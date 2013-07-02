
/* userd_test */
#include "userd.h"

#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#define BUFLEN 512
#define PORT 9999

void err(char *s) {

  perror(s);
  exit(1);
}

int64_t udp_connect(const char *host,const char *serv) {
  /* from unix network programming - pg337 */
  int64_t sockfd,n;
  struct addrinfo hints,*res,*ressave;

  memset(&hints,0,sizeof(struct addrinfo));
  //bzero(&hints,sizeof(struct addrinfo)); /*FIXME: use memset */
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  
  if ( (n = getaddrinfo(host,serv,&hints,&res)) != 0)
    err("udp_connect error");

  ressave = res;

  do {
    sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if (sockfd < 0)
      continue;

    if (connect(sockfd,res->ai_addr,res->ai_addrlen) == 0)
      break;

    close(sockfd);
  } while ((res = res->ai_next) != NULL);

  if (res == NULL)
    err("udp_connect error");

  freeaddrinfo(ressave);

  return (sockfd);
}

int64_t readable_timeo(int64_t fd,int64_t ms) {
 
  fd_set rset;
  struct timeval tv;
  FD_ZERO(&rset);
  FD_SET(fd,&rset);

  tv.tv_sec = 0;
  tv.tv_usec = ms * 1000;

  return (select(fd + 1,&rset,NULL,NULL,&tv));
  /* > 0 if descriptor is readable*/
}


void signalhandler (int32_t signo)
{
  if (signo == SIGINT || signo == SIGTERM || signo == SIGHUP) {
    
    printf("interrupted\n");
    exit(0);
  }    
}


int main(void) {

  signal (SIGINT,signalhandler);
  signal (SIGTERM,signalhandler);

  int64_t sockfd;
  char buf[BUFLEN];
  memset(&buf,'\0',BUFLEN);

  sockfd = udp_connect("localhost","9999");
  
  /* try add */
  bstring_t msg = bfromcstr("add>hellothere");
  memcpy(&buf,btocstr(msg),blength(msg));
  write(sockfd,buf,blength(msg));
   /* don't expect response back */
 
  /* try a check */
  msg = bfromcstr("check>hellothere");
  memcpy(&buf,btocstr(msg),blength(msg));
  write(sockfd,buf,blength(msg));

  int64_t rc = readable_timeo(sockfd,100);
  if (rc <= 0) {
    
    printf("timeout for read\n");
  } else {    

    read(sockfd,&buf,BUFLEN);
    bstring_t rep = bfromcstr(buf);
    printf("recv'd echo of %s\n",btocstr(rep));
    bdestroy(rep);
  }

  /* try a swap */
  msg = bfromcstr("swap>hellothere>bynow");
  memcpy(&buf,btocstr(msg),blength(msg));
  write(sockfd,buf,blength(msg));
  /* don't expect response back */
  
  /* try a remove */
  msg = bfromcstr("remove>hellothere");
  memcpy(&buf,btocstr(msg),blength(msg));
  write(sockfd,buf,blength(msg));
  /* don't expect response back */

  /* try a purge */
  msg = bfromcstr("purge>all");
  memcpy(&buf,btocstr(msg),blength(msg));
  write(sockfd,buf,blength(msg));
 
  


  close(sockfd);
  bdestroy(msg);


  return 0;
}

