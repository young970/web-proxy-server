/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

/* 한개의 http 트랜잭션을 처리 */
void doit(int fd);

/* 요청 헤더를 내의 정보를 읽고 무시하는 함수 */
void read_requesthdrs(rio_t *rp);

/* 정적, 동적 콘텐츠인지를 구분하여 맞게 처리(분석) */
int parse_uri(char *uri, char *filename, char *cgiargs);

/* 서버에서 정적 콘텐츠를 처리 */
void serve_static(char method[MAXLINE], int fd, char *filename, int filesize);

/* 도메인을 분석해서 파일의 타입을 정의해주는 함수 */
void get_filetype(char *filename, char *filetype);

/* 서버에서 동적 콘텐츠를 처리해주는 함수 */
void serve_dynamic(char method[MAXLINE], int fd, char *filename, char *cgiargs);

/* 상황에 맞는 에러메세지를 출력해주는 함수 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);



int main(int argc, char **argv) {   //argc: 인자의 개수(기본이1 그리고 띄어쓰기로 구분)  argv: 값
  int listenfd, connfd;  // listenfd: 서버에서 만드는 듣기 식별자(소켓 식별자)   connfd: 서버에서 만드는 연결 식별자(소켓 식별자)
  char hostname[MAXLINE], port[MAXLINE]; // hostname: 주소(ip로도 올 수 있고 도메인)   port: ip만으로는 부족해서 부여하는 주소
  socklen_t clientlen;                   // clientlen: 클라이언트 주소 길이
  struct sockaddr_storage clientaddr;    // 클라이언트에 있는 소켓 주소

  /* Check command line args */
  if (argc != 2) {  //정상적인 주소 및 포트 입력이 아니라면(무조건 한개만 입력)
    fprintf(stderr, "usage: %s <port>\n", argv[0]);  //fprintf: 에러메시지를 저장한다음 출력하고 종료
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);                   //Open_listenfd: 듣기 식별자 생성 (추후 공부 필요)  fd는 식별자
  while (1) {                                          //무한 루프  서버는 항상 준비가 되어있어야 하기 때문에
    clientlen = sizeof(clientaddr);                    //clientaddr: 클라이언트 주소    clientaddr: Open_listenfd에서 get addrinfo에서 만들어짐 
    connfd = Accept(listenfd, (SA *)&clientaddr,       //클라이언트로부터 연결요청을 기다리고 받아들여주는 것  연결 식별자가 리턴됨(0보다 크거나 같음)
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,    //Getnameinfo: 소켓 구조체를 주소와 포트로 바꿔주고 이들을 host와 service버퍼로 복사  출력 리턴값이 0, 에러면 에러코드
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);  //클라이언트 ip와 포트 출력
    doit(connfd);   // 트랜잭션 수행
    Close(connfd);  // 자신쪽의 연결끝을 닫는다.
  }
}


void doit(int fd)  //이 함수에서 fd는 연결 식별자 connfd
{
  int is_static;       // 동적인지 정적 콘텐츠 인지 알려주는 변수  is_static이 1이면 정적 0이라면 동적 컨텐츠 
  struct stat sbuf;   //소켓 버퍼 (임시저장 변수)
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];    //uri: 파일 이름과 옵션인 인자들을 포함하는 URL의 접미어  version:HTTP1.0인지 1.1인지
  char filename[MAXLINE], cgiargs[MAXLINE];  //cgiargs 는 ?뒤에 나오는 것으로 &로 구분한다.(?앞에는 파일명)
  rio_t rio; // 

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);  // connectfd와 rio 버퍼를 연결해준다.
  Rio_readlineb(&rio, buf, MAXLINE);  //rio버퍼에 있는것을 읽고(maxlien -1 만큼) 우리 버퍼에 저장
  printf("Request headers:\n");
  printf("%s", buf);                               // 버퍼에는 메소드와 uri와 http버전이 띄어쓰기로 나열되어 있다.
  sscanf(buf, "%s %s %s", method, uri, version);   // 버퍼에 있는 출력값들을 메쏘드, uri, 버전 변수에 정의

  if (!((strcasecmp(method, "GET") == 0) || (strcasecmp(method, "HEAD") == 0))) {                 // 메소드의 인자 1과 인자2가 같으면 0을 리턴 
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);                          // 메소드가 GET으로 들어오면 읽어들이고 다른 연결요청 헤더 무시

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);    // URI를 파일 이름과 비어 있을 수도 있는 CGI인자 스트링으로 분석하고, 요청이 정적 또는 동적 컨텔츠를 위한 것인지 나타내는 플레그를 설정.
  if (stat(filename, &sbuf) < 0) {   // stat: 파일의 정보를 가져와서 두번째 인자인 sbuf에 넣어준다.(0보다 작으면 파일이 없다.)
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");    
    return;
  }

  if (is_static) { /* Serve static content */   // 정적 컨텐츠라면 
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {          // 일반파일이거나 읽기권한이 없으면 에러를 띄운다     요청이 정적 컨텐츠를 위한 것이면 이 파일이 보통파일 이라는 것과 읽기 권한을 가지고 있는지 검증.
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(method, fd, filename, sbuf.st_size);     // 정적 컨텐츠 제공
  }
  else { /* Serve dynamic content */            // 정적 컨텐츠
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {          // 앞에꺼가 일반파일 여부
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(method, fd, filename, cgiargs);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg,    //HTTP응답을 응답 라인에 적절한 상태 코드와 상태 메시지와 함께 클라이언트에 보내며 에러를 설명하는 응답 본체에 html파일도 함께 보낸다.
                 char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/thml\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)  // rp는 rio버퍼
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);   // rb에서 택스트를 읽고 buf에 복사
  while (strcmp(buf, "\r\n")) {      // strcmp: 문자열 비교 
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { /* Static content */    // 요청이 정적컨텐츠라면 (uri에 cgi-bin이 없다)
    strcpy(cgiargs, "");          // cgiargs를 지우고
    strcpy(filename, ".");        
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else { /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(char method[MAXLINE], int fd, char *filename, int filesize)      // 정적 컨텐츠 제공 
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));    // 식별자 fd로 buf내용 전송     //웹 브라우저에 전송
  printf("Response headers:\n");
  printf("%s", buf);
  
  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);                            // srcfd: (임시 저장장치)   Open: 파일을 열고 성공시 파일 식별자 반환
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);     // Mmap: srcp포인터에서 부터 파일 내용을 복사해서 넣는다.  각각 0은 븥여넣어질 그리고 복사할 시작 주소
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);                                 // srcp 에서 fd로 filesize만큼 전송  (클라이언트로 전송)
  Munmap(srcp, filesize);                                         // free랑 같은 기능

  // if (!(strcasecmp(method, "GET"))){
  //   srcfd = Open(filename, O_RDONLY, 0);
  //   srcp = (void*)malloc(sizeof(char)*filesize);
  //   Rio_readn(srcfd, srcp, filesize);
  //   Close(srcfd);
  //   Rio_writen(fd, srcp, filesize);
  //   free(srcp);
  // }
}

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
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(char method[MAXLINE], int fd, char *filename, char *cgiargs)    // fd: 연결 식별자
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);     // 환경변수 값을 설정 "QUERY_STRING"에 cgiarg를 넣어준다 1이면 덮어쓰고 0이면 덧붙인다.
    // method를 cgi-bin/adder.c에 넘겨주기 위해 환경변수 set
    setenv("REQUEST_METHOD", method, 1);
    Dup2(fd, STDOUT_FILENO);            /* Redirect stdout to client */   // 자식프로세스를 fd한테 연결 STDOUT_FILENO는 표준출력
    Execve(filename, emptylist, environ);  /* Run CGI program */           //대문자는 유닉스에서 에러까지 포함하는것
  }
  Wait(NULL); /* Parent waits for and reaps child */      // 자식이 끝날때 까지 기다리다 끝나면 자식 종료
}
