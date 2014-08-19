/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

void doit(int fd);
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg);

int transmit_request(int cfd, int *sfd, char *uri);
int transmit_reply(int cfd, int sfd, int *rcvsize);

int open_clientfd_ts(char *hostname, int port); 
void *thread(void *vargp);

typedef struct {
  int clientfd,tID;
  struct sockaddr_in clientaddr;
}threadvargp;
/*
 *Global variables
 */
sem_t mtx_gethostbyname;
sem_t mtx_log;

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
  Signal(SIGPIPE, SIG_IGN);
  int listenfd,lport,mytid;
  pthread_t tid;
  threadvargp *thrargvp;
  socklen_t clientlen = sizeof(thrargvp->clientaddr);
  
  Sem_init(&mtx_gethostbyname, 0, 1);
  Sem_init(&mtx_log, 0, 1);
  /* Check arguments */
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
    exit(0);
  }
  lport = atoi(argv[1]);
  mytid = 0;
  listenfd = Open_listenfd(lport);
  while(1){
    thrargvp = Malloc(sizeof(threadvargp));
    thrargvp->clientfd = Accept(listenfd, (SA*)&(thrargvp->clientaddr), &clientlen);
    thrargvp->tID = mytid;
    Pthread_create(&tid, NULL, thread, thrargvp);
    mytid++;
  }
  exit(0);
}

void *thread(void *vargp)
{ 
  int clientfd = ((threadvargp *)vargp)->clientfd;
  //int mytid = ((threadvargp *)vargp)->tID;
  struct sockaddr_in sockaddr;
  int severfd,rcvsize = 0, rtn = 0, logfd;
  char uri[MAXLINE],logString[MAXLINE];
  memcpy(&sockaddr, &(((threadvargp *)vargp)->clientaddr), sizeof(sockaddr));
  Pthread_detach(pthread_self());

  Free(vargp);
  /*need to assign a ID for a thread for debug*/
 
  /*read request header,get the hostname and port from the uri*/
  if((rtn = transmit_request(clientfd, &severfd, uri)) < 0){
    printf("transmit request error\n");
    close(clientfd);
    return NULL;
  }
      
  /*receive the reply*/
  transmit_reply(clientfd, severfd, &rcvsize);
  close(clientfd);
  close(severfd);

  /*form the log file*/
  format_log_entry(logString, &sockaddr, uri, rcvsize);
  strcat(logString, "\r\n"); 
  P(&mtx_log);
  logfd = Open("proxy.log", O_APPEND | O_WRONLY, S_IWUSR);
  rio_writen(logfd, logString, strlen(logString));
  Close(logfd);
  V(&mtx_log);

  return NULL;
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;	
	strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}

/*extract the header from the HTTP request and return
 *so need to parse the "\r\n" to get the end of the header
 */
int transmit_request(int cfd, int *sfd, char *uri)
{
  rio_t rio;
  char hdrbuf[MAXBUF]; //store the request header
  char lbuf[MAXBUF];
  char *ctnt = "close\r\n";
  char method[MAXLINE],version[MAXLINE],hostname[MAXLINE],
    pathname[MAXLINE],tag[MAXLINE],content[MAXLINE];
  int port;
  
  *sfd = 0;

  /*read request line and headers*/
  rio_readinitb(&rio, cfd);
  bzero(hdrbuf,MAXBUF);
  /*the first line*/
  rio_readlineb(&rio, lbuf, MAXBUF);
  sscanf(lbuf, "%s %s %s", method,uri,version);
  //sprintf(lbuf, "%s %s %s",method,uri,vs);
  strcat(hdrbuf, lbuf);

  rio_readlineb(&rio, lbuf, MAXBUF);
  while(strcmp(lbuf, "\r\n")){
    sscanf(lbuf,"%s: %s",tag,content);
    if(!strcmp(tag,"Proxy-Connection:")){
      sprintf(lbuf,"%s: %s",tag,ctnt);
    }
    strcat(hdrbuf, lbuf);
    rio_readlineb(&rio,lbuf,MAXBUF);
  }
  strcat(hdrbuf, "\r\n");
  //printf("%s",hdrbuf);
  
  /*build a new conn to end sever*/
  if(parse_uri(uri, hostname, pathname, &port) < 0)
    return -1;
  
  //printf("%s, %d\n",hostname, port);

  if((*sfd = open_clientfd_ts(hostname, port)) == 0)
    return -1;
  
  /*send hdr to the end sever*/
  rio_writen(*sfd, hdrbuf, strlen(hdrbuf));

  return 0;
}

int transmit_reply(int cfd, int sfd, int *rcvsize)
{
  rio_t rio;
  char lbuf[4*MAXBUF];
  int n;
  
  *rcvsize = 0;

  rio_readinitb(&rio, sfd);

  bzero(lbuf, 2*MAXBUF);
  //printf("replying object\n");
  while ((n = rio_readnb(&rio, lbuf,4*MAXBUF)) > 0)
    {  
      if ((rio_writen(cfd, lbuf, n)) != n)
          break; 
      *rcvsize += n;
      bzero(lbuf, 4*MAXBUF);
    }
  return 0;
}


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

int open_clientfd_ts(char *hostname, int port) 
{
    int clientfd;
    struct hostent *hp, *sharehp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1; /* check errno for cause of error */

    /* Fill in the server's IP address and port */
    P(&mtx_gethostbyname);
    sharehp = gethostbyname(hostname);
    hp = sharehp;
    V(&mtx_gethostbyname);
    if (hp == NULL)
	return -2; /* check h_errno for cause of error */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0], 
	  (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    /* Establish a connection with the server */
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
	return -1;
    return clientfd;
}
