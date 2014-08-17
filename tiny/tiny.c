#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *riop);
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int port,listenfd,connfd,clientlen;
  struct sockaddr_in clientaddr;
  /*
  char buf[MAXLINE];
  rio_t rio;
  size_t n;
  */

  if(argc != 2)
    {
      fprintf(stderr, "usage: %s <port>\n", argv[0]);
      exit(1);
    }
  
  port = atoi(argv[1]);

  listenfd = Open_listenfd(port);
  while(1)
    {
      clientlen = sizeof(clientaddr);
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
      doit(connfd);
      Close(connfd);
    }
  
  
}


void doit(int fd)
{
  rio_t rio;
  char buf[MAXLINE], method[MAXLINE], version[MAXLINE],uri[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  int is_static;
  /*Read request line and headers*/
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  sscanf(buf,"%s %s %s", method,uri,version); //format the input
  /*strcasecmp ignore upper lower case cmp string
   *return 0 if equal
   */
  if(strcasecmp(method, "GET"))
    {
      /*error method not inplement*/
      clienterror(fd, method, "501", "Not Implemented", 
		  "Tiny does not implement this method");
      return;
    }

  /*cool once the rio is init,we only need the rio 
  to readlineb*/
  read_requesthdrs(&rio);
  
  /*parse the uri from GET to determine static or dynamic 
   *contents the browser requested
   */
  is_static = parse_uri(uri, filename, cgiargs);
  
  if(is_static)
    {
      /*serve static content*/
    }
  else
    {
      /*serve dynamic content*/
    }
}

void read_requesthdrs(rio_t *riop)
{
  char buf[MAXLINE];
  Rio_readlineb(riop, buf, MAXLINE);
  while(strcmp(buf, "\r\n"))
    {
      printf("%s", buf);
      Rio_readlineb(riop, buf, MAXLINE);
    } 
  printf("/n");
  return;
}

int parse_uri(char *uri, char *filename, char*cgiargs)
{
 
  return 0;
}

/*client_error only send the error msg to brower*/
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE]; //use to form the header;
  char body[MAXBUF]; //use to form the body;

  /*build the reponse body*/
  sprintf(body, "<html><title>TINY ERROR</title>");
  sprintf(body,"%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body,"%s%s: %s\r\n",body,errnum,shortmsg);
  sprintf(body,"%s<p>%s: %s\r\n",body, longmsg, cause);
  sprintf(body,"%s<hr><em>The Tiny web server</em>\r\n",body);

  /*send the HTTP response*/
  sprintf(buf,"HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n",(int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
