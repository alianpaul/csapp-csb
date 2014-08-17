#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *riop);
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void serve_dynamic(int fd, char *filename, char *cgiargs);


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
  struct stat statbuf;
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
  
  if((stat(filename, &statbuf)) < 0)
    {
      clienterror(fd, filename, "404", "Not Found", 
		  "Tiny counldn't find this file");
      return;
    }
  if(is_static)
    {
      /*serve static content*/
      if(!(S_ISREG(statbuf.st_mode)) || !(statbuf.st_mode & S_IRUSR))
	{
	  clienterror(fd, filename, "403", "Fobidden",
		      "Tiny could not read the file");
	  return;
	}
      serve_static(fd, filename, statbuf.st_size);
    }
  else
    {
      /*serve dynamic content*/
      if(!(S_ISREG(statbuf.st_mode)) || !(statbuf.st_mode & S_IXUSR))
	{ /*cgi-bin is a directory file*/
	  clienterror(fd, filename, "403", "Fobidden",
		      "Tiny could not run the cgi progm");
	  return;
	}
      serve_dynamic(fd, filename, cgiargs);
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
  printf("\n");
  return;
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

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  //strstr(uri,"cgi-bin"); find the stings for the first time,and return the pointer
  char *ptr;
  
  if(!strstr(uri,"cgi-bin"))
    {
      /*static content, .uri*/
      strcpy(cgiargs, "");
      strcpy(filename,".");
      strcat(filename,uri);
      if(uri[strlen(uri) - 1] == '/')
	strcat(filename,"index.html");
      return 1;
    }
  else
    {
      /*not static*/
      ptr = index(uri, '?');
      if(ptr)
	{
	  strcpy(cgiargs, ptr+1);
	  *ptr = '\0';
	}
      else
	strcpy(cgiargs, "");
      strcpy(filename,".");
      strcat(filename,uri);
      return 0;
    }
}
void get_filetype(char *filename, char *filetype)
{
  if(strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if(strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if(strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}


void serve_static(int fd, char* filename, int filesize)
{
  int srcfd;
  char *srcp;
  /*form a header*/
  char buf[MAXBUF],filetype[MAXLINE];
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");   
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));      

  /*send response body*/
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);
}


void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork() == 0)
    {
      setenv("QUERY_STRING", cgiargs, 1);
      Dup2(fd, STDOUT_FILENO);
      Execve(filename, emptylist, environ);
    }
  Wait(NULL);
}
