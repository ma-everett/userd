
/* userd_client.c library */
#include "userd.h"

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
  bstring_t yes;
  bstring_t check;
  bstring_t removec;
  bstring_t addc;
  bstring_t swapc;

} context_t;

void * userdclient_create(const char *serverlist) {

  context_t *ctx = (context_t *)malloc(sizeof(context_t));
  if (!ctx)
    return NULL;

  ctx->sockfd = -1;
  ctx->using = -1;
  bstring_t sl = bfromcstr(serverlist);
  ctx->servers = bsplit(sl,';');

  ctx->yes = bfromcstr("yes");
  ctx->check = bfromcstr("check");
  ctx->removec = bfromcstr("removec");
  ctx->addc = bfromcstr("addc");
  ctx->swapc = bfromcstr("swapc");

  bdestroy(sl);
  
  return ctx; 
}

void userdclient_destroy(void *_ctx) {

  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return;

  if (ctx->sockfd)
    close(ctx->sockfd);

  bdestroy(ctx->yes);
  bdestroy(ctx->check);
  bdestroy(ctx->addc);
  bdestroy(ctx->removec);
  bdestroy(ctx->swapc);

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
  /*FIXME
  if ( (n = getaddrinfo(host,serv,&hints,&res)) != 0)
    err("udp_connect error");
  */

  n = getaddrinfo(host,serv,&hints,&res);

  ressave = res;

  do {
    sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if (sockfd < 0)
      continue;

    if (connect(sockfd,res->ai_addr,res->ai_addrlen) == 0)
      break;

    close(sockfd);
  } while ((res = res->ai_next) != NULL);

  /* FIXME
  if (res == NULL)
    err("udp_connect error");
  */
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

int init(context_t *ctx) {

  if (ctx->sockfd != -1)
    return ctx->sockfd;

  int i;

  for (i=0; i < ctx->servers->qty; i++) {
    
    struct bstrList *server = bsplit(ctx->servers->entry[i],':');
    if (server->qty != 2) {
      printf("server entry incorrect! %s\n",btocstr(ctx->servers->entry[i]));
      bstrListDestroy(server);
      continue;
    }
    
    //printf("connecting to %s:%s\n",btocstr(server->entry[0]),btocstr(server->entry[1]));
    int sockfd = udp_connect(btocstr(server->entry[0]),btocstr(server->entry[1]));
    bstrListDestroy(server);
    if (sockfd >= 0) {
      
      ctx->sockfd = sockfd;
      ctx->using = i;
      //printf("\tconnected\n");
      break;
    }
    
    //printf("\tfailed to connect\n");
  }
  
  return ctx->sockfd;  
}


int userdclient_check(void *_ctx,const char *hash) {

  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return -1;

  char buf[BUFLEN];
  int i;
  int rc;

  if (ctx->sockfd == -1) 
    if (!init(ctx))
      return -1;
 
  /* try a check */

  bstring_t msg = bformat("check>%s",hash);
  memcpy(&buf,btocstr(msg),blength(msg));
  write(ctx->sockfd,buf,blength(msg)); /*FIXME: check the rc */
  bdestroy(msg);

  if (readable_timeo(ctx->sockfd,100) <= 0) {
 
    return -1;
  }
  bstring_t h = bfromcstr(hash);
  memset(&buf,'\0',BUFLEN);
  read(ctx->sockfd,&buf,BUFLEN);
  bstring_t rep = bfromcstr(buf);
 
  struct bstrList *reply;
  reply = bsplit(rep,'>');
  bdestroy(rep);

  if (reply->qty != 3)
    goto error;

  if (!biseq(ctx->check,reply->entry[0]))
    goto error;

  if (!biseq(h,reply->entry[1]))
    goto error;

  bdestroy(h);
  rc = biseq(ctx->yes,reply->entry[2]);
  bstrListDestroy(reply);
 
  return (rc);

 error:
  bdestroy(h);
  bstrListDestroy(reply);
  return -1;
}

int write_toserver(context_t *ctx,const char *cmd,const char *hash0,const char *hash1) {

  if (ctx->sockfd == -1)
    if (!init(ctx))
      return -1;

  char buf[BUFLEN];
  int rc;
  int len;
  bstring msg;
  
  if (hash1 == NULL) {
    
    msg = bformat("%s>%s",cmd,hash0);
  } else {

    msg = bformat("%s>%s>%s",cmd,hash0,hash1);
  }

  len = blength(msg);
  memcpy(&buf,btocstr(msg),len);
  rc = write(ctx->sockfd,buf,len);
 
  bdestroy(msg);

  return (rc == len);
}


int userdclient_remove(void *_ctx,const char *hash) {
  
  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return -1;
 
  return write_toserver(ctx,"remove",hash,NULL);
}

int userdclient_removec(void *_ctx,const char *hash) {

  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return -1;

  int rc = write_toserver(ctx,"removec",hash,NULL);
  if (rc <= -1)
    return rc;

  if (readable_timeo(ctx->sockfd,100) <= 0) {
    
    return -1; /*FIXME*/
  }

  char buf[BUFLEN];
  memset(&buf,'\0',BUFLEN);
  bstring_t h = bfromcstr(hash);

  read(ctx->sockfd,&buf,BUFLEN);
  bstring_t rep = bfromcstr(buf);
 
  struct bstrList *reply;
  reply = bsplit(rep,'>');
  bdestroy(rep);

  if (reply->qty != 3)
    goto error;

  if (!biseq(ctx->removec,reply->entry[0]))
    goto error;

  if (!biseq(h,reply->entry[1]))
    goto error;

  bdestroy(h);
 
  rc = biseq(ctx->yes,reply->entry[2]);
  bstrListDestroy(reply);
 
  return (rc);

 error:
  bdestroy(h);
  bstrListDestroy(reply);
  return -1;
}

int userdclient_add(void *_ctx,const char *hash) { 

  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return -1;

  return write_toserver(ctx,"add",hash,NULL);
}

int userdclient_addc(void *_ctx,const char *hash) {

  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return -1;

  int rc = write_toserver(ctx,"addc",hash,NULL);
  if (rc <= -1)
    return rc;

  if (readable_timeo(ctx->sockfd,100) <= 0) {
    
    return -1; /*FIXME*/
  }

  char buf[BUFLEN];
  memset(&buf,'\0',BUFLEN);
  bstring_t h = bfromcstr(hash);

  read(ctx->sockfd,&buf,BUFLEN);
  bstring_t rep = bfromcstr(buf);
 
  struct bstrList *reply;
  reply = bsplit(rep,'>');
  bdestroy(rep);

  if (reply->qty != 3)
    goto error;

  if (!biseq(ctx->addc,reply->entry[0]))
    goto error;

  if (!biseq(h,reply->entry[1]))
    goto error;

  bdestroy(h);
 
  rc = biseq(ctx->yes,reply->entry[2]);
  bstrListDestroy(reply);
 
  return (rc);

 error:
  bdestroy(h);
  bstrListDestroy(reply);
  return -1;
}

int userdclient_purge(void *_ctx) { 
  
  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return -1;

  return write_toserver(ctx,"purge","all",NULL);
}

int userdclient_swap(void *_ctx,const char *hash0,const char *hash1) {

  context_t *ctx = (context_t *)_ctx;
  if (!ctx)
    return -1;

  return write_toserver(ctx,"swap",hash0,hash1);
}
 
