#include "csapp.h"

int main(int argc, char **argv)
{
  int port,clientfd;
  struct sockaddr_in severaddr;
  char buf[MAXLINE],*host;
  rio_t rio;
  size_t n;

  if(argc != 3)
    {
      fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
      exit(0);
    }
  host = argv[1];
  port = atoi(argv[2]);

  clientfd = Open_clientfd(host,port);
  Rio_readinitb(&rio, clientfd);


      
  while((Fgets(buf, MAXLINE, stdin)) != NULL)
	{
	  Rio_writen(clientfd,buf, strlen(buf));
	}
  Close(clientfd);
  exit(0);
  
  
}
