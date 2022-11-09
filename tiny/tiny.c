

/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg); 
void echo(int connfd);
/*
 * Tiny는 반복실행  서버로 명령줄에서 넘겨받은 포트로의 연결 요청을 듣는다.
 * open_listenfd 함수를 호출해서 듣기 소켓을 오픈한 후에, Tiny는 전형적인 무한 서버 루프를 실행하고,
 * 반복적으로 연결 요청을 접수하고, 트랜잭션을 수행하고, 자신 쪽의 연결 끝을 닫는다.
 */
int main(int argc, char **argv) { // para : argc -> 전달되는 데이터수(옵션 수) **argv -> 옵션 문자열의 배열
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // 포트 번호를 입력하지 않았을 때 종료
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);  // 듣기 소켓 열기 -> 인자로 받은 port에 listenfd 생성
  // 무한 서버 루프
  int n = 0;
  while (1) {
    // accept 함수 인자에 넣기 위한 주소 길이 계산
    clientlen = sizeof(clientaddr);
    // 연결 요청 접수
    // accept() 클라이언트로부터 연결요청 기다림 
    // para -> listenfd(듣기 식별자), 클라이언트 정보
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  

    // Getaddrinfo는 호스트 이름: 호스트 주소, 서비스 이름: 포트 번호의 스트링 표시를 소켓 주소 구조체로 변환
    // Getnameinfo는 위를 반대로 소켓 주소 구조체에서 스트링 표시로 변환.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    // echo(connfd);
    // transaction 수행
    doit(connfd);  
    // 서버쪽 연결 종료
    Close(connfd);

  }
}

// doit ->  한 개의 HTTP transaction 처리
/*
  즉, 한 개의 클라이언트의 요청을 처리해 클라이언트에게 컨텐츠를 제공한다.
*/
void doit(int fd)
{
  int is_static;  // 정적, 동적 컨텐츠를 나누는 변수
  struct stat sbuf; // 
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // Request Line을 읽고 분석한다. 
  Rio_readinitb(&rio, fd);  // &rio 주소를 가지는 읽기 버퍼를 만들고 fd와 연결하고 초기화.
  Rio_readlineb(&rio, buf, MAXLINE);  // rio내의 내부버퍼로 텍스트 한줄 읽고->메모리 위치 buf로 복사 (요청 헤더)
  printf("Request headers:\n");
  printf("%s", buf);  // 요청 헤더 출력
  sscanf(buf, "%s %s %s", method, uri, version);  // 버퍼(요청 헤더)에서 method, uri, version 가져옴.
  // GET 메소드 외에는 지원하지 않음 - 다른 요청이 오면 에러 띄우고 종료
  if (strcasecmp(method, "GET"))  // strcasecmp -> 두 문자열의 길이와 내용이 같다면 0 리턴
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  // GET메소드라면 읽어들이고, 다른 요청헤더들을 무시 -> 요청라인들을 지나친다.
  read_requesthdrs(&rio);

  // parse_uri()에 uri를 넣어줘서 filename(파일명)과 cgiargs(동적 인자?)를 업데이트하고, 정적, 동적 컨텐츠인지 구분함
  is_static = parse_uri(uri, filename, cgiargs);

  // stat() ->파일의 크기, 권한, 생성일시, 최종 변경일 등의 상태나 파일의 정보를 sbuf로 업데이트
  // 파일이 디스크 상에 있지 않다면(if stat() return  -> -1) 에러 띄우고 종료
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }
  // 정적 컨텐츠 일때
  if (is_static)
  { 
    // 일반파일(ISRES)이고, 읽기 권한중 하나라도 없으면 에러 처리
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 일반파일이고, 읽기권한이 있을때 정적 컨텐츠 클라이언트에 보내기
    serve_static(fd, filename, sbuf.st_size);
  }
  // 동적 컨텐츠 일때
  else
  { 
    // 일반파일(ISRES)인지, 실행 권한중 하나라도 없으면 에러 처리
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 일반 파일이고, 실행 권한이 있으면 동적 컨텐츠 클라이언트에 보내기
    serve_dynamic(fd, filename, cgiargs); 
  }
}

// read_requesthdrs() ->  요청 헤더를 한줄씩 읽어가면서 출력(결국 무시하는 작업)
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  // /r/n은 끝
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

// parse_uri: HTTP URI를 가져와서 filename(파일명)과 cgiargs(동적인자) 업데이트 및 정적, 동적 컨텐츠 구분
// return -> 1(정적 컨텐츠) , 0(동적 컨텐츠)
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  /* 실행파일의 홈 디렉토리 : cgi-bin */
  /* cgi-bin을 포함하는 uri는 정적컨텐츠임을 가정 */
  // 정적 컨텐츠 일때
  if (!strstr(uri, "cgi-bin"))  // strstr() -> uri(para1)에 cgi-bin(para2)가 있는지 확인
  { 
    strcpy(cgiargs, ""); // 정적 컨텐츠이므로 cgiargs 비워줌
    strcpy(filename, ".");  // filename = '' -> '.'
    strcat(filename, uri);   // uri를 상대 리눅스 경로이름으로 변환 filename = '.' -> './index.html'  
    if (uri[strlen(uri) - 1] == '/') // uri가 '/'로 끝난다면
      strcat(filename, "home.html"); // 기본 파일이름 추가
    return 1;

        /* 예시
      uri : /godzilla.jpg
      ->
      cgiargs : 
      filename : ./godzilla.jpg
    */
    
  }

  // 동적 컨텐츠 일때
  else
  { 
    ptr = index(uri, '?');  // 좌표값(ptr) uri의 안의 '?'의 인덱스로 설정
    // ptr이 0이 아니면(uri에 '?'가 존재하면) 뒤에 인자값이 있다는 것이므로 인자 가져오기
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1); // '?'이후의 값 -> cgiargs
      *ptr = '\0';  // '?' 해당 인덱스 NULL 처리
    }
    // ptr이 0이면 뒤에 인자값 X ->  빈 cgiargs
    else
      strcpy(cgiargs, "");
    // 인자를 제외한 filename 만들기
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
      /* 예시
      uri : /cgi-bin/adder?123&123
      ->
      cgiargs : 123&123
      filename : ./cgi-bin/adder
    */
}

// serve_static() -> 클라이언트에 정적 컨텐츠 전달 
// 서버의 local file을 body로 가진 HTTP response를 클라이언트에게 전달
// Tiny는 5개의 정적 컨텐츠 파일을 지원함 : HTML, unformatted text file, GIF, PNG, JPEG
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // get_filetype() -> filex
  get_filetype(filename, filetype);
  /* 클라이언트에 response line과 header를 보내기 */
  
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("정적 컨텐츠 입니다. %s\n", filename);
  printf("Response headers:\n");
  printf("%s", buf);

  /* 클라이언트에 response body 보내기 */
  srcfd = Open(filename, O_RDONLY, 0);                        /* read를 위해 filename을 open하고 file descriptor를 얻어옴 */
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); /* 요청한 파일을 가상메모리 영억으로 mapping */
  Close(srcfd);                                               /* mapping 후 파일을 닫는다 - 메모리 누수를 막기 위해 */
  Rio_writen(fd, srcp, filesize);                             /* 파일을 클라이언트에 전달 */
  Munmap(srcp, filesize);                                     /* mapping된 가상메모리를 free 한다 - 메모리 누수를 막기 위해 */
}

/*
 * get_filetype - Derive file type from filename
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if(strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

/* serve_dynamic : child process를 fork하고, CGI program을 child context에서 실행함 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* 연결 성공을 알리는 response line을 보내기 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)
  { /* Child process를 fork 하기 */
    /* 실제 서버는 모든 CGI 환경 변수를 여기서 설정하나, Tiny에서는 생략함 */
    setenv("QUERY_STRING", cgiargs, 1);   /* URI의 CGI argument를 이용해 QUERY_STRING 환경변수 초기화 */
    Dup2(fd, STDOUT_FILENO);              /* child의 stdout을 file descriptor로 redirect */
    Execve(filename, emptylist, environ); /* CGI program을 실행시킴 */
  }
  Wait(NULL); /* Parent process는 child process의 종료를 기다림 */
}

/* clienterror : 에러 메시지를 클라이언트에게 전송 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void echo(int connfd) {
  size_t n;
  char buf[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, connfd);
  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    if (strcmp(buf, "\r\n") == 0)
      break;
    Rio_writen(connfd, buf, n);
  }
}