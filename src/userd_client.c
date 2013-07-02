
/* userd_client.c library */
#include "userd_client.h"

#include "bstring.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

typedef struct {

  struct bstrList *servers;
  int using;
  int sockfd;  

} context_t;

void * userdclient_create(const char *serverlist,int len) {

  context_t *ctx = (context_t *)malloc(sizeof(context_t));
  if (!ctx)
    return NULL;

  ctx->sockfd = -1;
  ctx->using = -1;
  bstring_t sl = bfromcstr(serverlist);
  ctx->servers = bsplit(sl,';');
  bdestroy(sl);
  
  return ctx; 
}

void userdclient_destroy(void *_ctx) {

  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return;

  if (ctx->sockfd)
    close(ctx->sockfd);

  bstrListDestroy(ctx->servers);
  free(ctx);
}


int udp_connect(const char *host,const char *serv) {
  /* from unix network programming - pg337 */
  int sockfd,n;
  struct addrinfo hints,*res,*ressave;

  memset(&hints,0,sizeof(struct addrinfo));
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

int readable_timeo(int fd,int ms) {
 
  fd_set rset;
  struct timeval tv;
  FD_ZERO(&rset);
  FD_SET(fd,&rset);

  tv.tv_sec = 0;
  tv.tv_usec = ms * 1000;

  return (select(fd + 1,&rset,NULL,NULL,&tv));
  /* > 0 if descriptor is readable*/
}

#define BUFLEN 512

int userdclient_check(void *_ctx,const char *hash,int len) {

  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return -1;

  char buf[BUFLEN];
  int i;

  if (ctx->sockfd == -1) { /* then connect */
    
    for (i=0; i < ctx->servers->qty; i++) {

      struct bstrList *server = bsplit(ctx->servers->entry[i],':');
      if (server->qty != 2) {
	printf("server entry incorrect! %s\n",btocstr(ctx->servers->entry[i]));
	bstrListDestroy(server);
	continue;
      }
      
      printf("connecting to %s:%s\n",btocstr(server->entry[0]),btocstr(server->entry[1]));
      int sockfd = udp_connect(btocstr(server->entry[0]),btocstr(server->entry[1]));
      bstrListDestroy(server);
      if (sockfd >= 0) {
	
	ctx->sockfd = sockfd;
	ctx->using = i;
	printf("\tconnected\n");
	break;
      }

      printf("\tfailed to connect\n");
    }

    if (ctx->sockfd == -1)
      return -1;
  }
  
  /* try a check */
  bstring_t msg = bformat("check>%s",hash);
  memcpy(&buf,btocstr(msg),blength(msg));
  write(ctx->sockfd,buf,blength(msg)); /*FIXME: check the rc */
  
  if (readable_timeo(ctx->sockfd,100) <= 0) {
    
    printf("\ttimeout for read\n");
    return -1;
  }    

  read(ctx->sockfd,&buf,BUFLEN);
  bstring_t rep = bfromcstr(buf);
  printf("\trecv'd echo of %s\n",btocstr(rep));
 
  struct bstrList *reply;
  reply = bsplit(rep,'>');
  bdestroy(rep);

  if (reply->qty != 3) {

    bstrListDestroy(reply);
    return -1;
  }

  bstring_t yes = bfromcstr("yes");
  if (biseq(yes,reply->entry[2])) {
   
    bstrListDestroy(reply);
    bdestroy(yes);
    return 1;
  } 
  
  bstrListDestroy(reply);
  bdestroy(yes);
  return 0;
}
