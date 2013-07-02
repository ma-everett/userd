
/* bazaar/userd.c 
 * 
 * in-memory user auth check
 */

#include "userd.h"

#include <dlfcn.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h> /* for getaddrinfo */

#define BUFLEN 512
#define PORT 9999

void err (char *str) {
  
  perror(str);
  exit(1);
}


/* three interfaces : 
 *
 * 0. check user, Nanomsg REQ/REP
 *    recv: hash -> check if hash present in store
 *          return YES or NO
 *
 * 1. control, Nanomsg REQ/REP
 *    add: hash
 *    delete: hash
 *    set: hash hash { where first set is to be replaced
 *
 * 2. publish events, Nanomsg PUB/SUB
 *    events: check,add,delete,set,stats,memoryuse etc
 */

int64_t readable_timeo(int64_t fd,int64_t ms) {
  /* from unix network porgramming - pg385 */
  fd_set rset;
  struct timeval tv;
  FD_ZERO(&rset);
  FD_SET(fd,&rset);

  tv.tv_sec = 0;
  tv.tv_usec = ms * 1000;

  return (select(fd + 1,&rset,NULL,NULL,&tv));
  /* > 0 if descriptor is readable*/
}

int64_t udp_bind(const char *host,const char *serv,socklen_t *addrlenp) {
  /* from unix network programming, pg338 */
  int64_t sockfd,n;
  const int64_t on = 1;
  struct addrinfo hints,*res,*ressave;

  memset(&hints,0,sizeof(struct addrinfo));
  // bzero(&hints,sizeof(struct addrinfo));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if ((n = getaddrinfo(host,serv,&hints,&res)) != 0)
    err("udp getaddrinfo err");

  ressave = res;
  do {
    sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if (sockfd < 0)
      continue;
    
    if (bind(sockfd,res->ai_addr,res->ai_addrlen) == 0)
      break;
    
    close(sockfd);
  } while ((res = res->ai_next) != NULL);

  if (res == NULL)
    err("udp bind error");

  if (addrlenp)
    *addrlenp = res->ai_addrlen;

  freeaddrinfo(ressave);
  return (sockfd);
}


#define DLASSERT() \
  if ((error = dlerror()) != NULL) {\
    fprintf(stderr,"%s\n",error);  \
    exit(1);}

uint32_t u_term;

void signalhandler (int32_t signo)
{
  if (signo == SIGINT || signo == SIGTERM || signo == SIGHUP)
    u_term = 0;
}

uint32_t checksignal (void) 
{
  return (u_term);
}

int main(void) {

  printf("bazaar userd\n");

  void *hdle;
  bstring (*namef)(void);
  void *(*createf)(void);
  void (*destroyf)(void *);
  user_t *(*addf) (bstring_t,void *);
  uint8_t (*removef)(bstring_t,void *);
  uint8_t (*checkf)(bstring_t,void *);
  uint8_t (*purgef)(void *);
  user_t *(*swapf)(bstring_t,bstring_t,void *);
  void (*statsf)(stats_t *,void *);
  char *error;


  hdle = dlopen("./libuserd_array.so",RTLD_LAZY);
  if (!hdle) {
    
    fprintf(stderr,"%s\n",dlerror());
    exit(1);
  }

  namef = dlsym(hdle,"userd_name");
  if ((error = dlerror()) != NULL) {

    fprintf(stderr,"%s\n",error);
    exit(1);
  }

  bstring_t name = (*namef)();
  printf("using module %s\n",btocstr(name));
  bdestroy (name);

  createf = dlsym(hdle,"userd_create");
  DLASSERT();

  destroyf = dlsym(hdle,"userd_destroy");
  DLASSERT();

  addf = dlsym(hdle,"userd_add");
  DLASSERT();

  removef = dlsym(hdle,"userd_remove");
  DLASSERT();

  swapf = dlsym(hdle,"userd_swap");
  DLASSERT();

  checkf = dlsym(hdle,"userd_check");
  DLASSERT();
  
  purgef = dlsym(hdle,"userd_purge");
  DLASSERT();

  statsf = dlsym(hdle,"userd_stats");
  DLASSERT();

  /* create module */

  void * ctx = (*createf)();
  uint8_t r;

  /* run some quick tests : */
  printf("\trunning tests\n");
  uint8_t failed = 0;

  /* attempt to add a hash */
  bstring test = bfromcstr("hellothere");

  user_t *A = (*addf)(test,ctx);
  printf("\t\t1/4: add\tA:hellothere\t\t%s(yes)\n",(A == NULL) ? " no" : "yes");
  failed += (A == NULL) ? 1 : 0;

  /* do a check against hash */
  r = (*checkf)(test,ctx);
  printf("\t\t2/4: check\tA:hellothere\t\t%s(yes)\n",(r == 0) ? " no" : "yes");
  failed += (r == 0) ? 1 : 0;

  /* attempt to remove an existing hash */
  r = (*removef)(test,ctx);
  printf("\t\t3/4: remove\tA:hellothere\t\t%s(yes)\n",(r == 0) ? " no" : "yes");
  failed += (r == 0) ? 1 : 0;

  r = (*checkf)(test,ctx);
  printf("\t\t4/4: check\tA:hellothere\t\t%s(no)\n",(r == 1) ? "yes" : " no");
  failed += (r == 1) ? 1 : 0;

  bdestroy(test);

  if (failed) {
   
    printf("\tfailed %d/4 - exiting\n",failed);
    (*destroyf)(ctx);
    dlclose(hdle);
    exit(1);
  }
 
  int64_t sockfd;
  struct sockaddr_in raddr;
  socklen_t slen=sizeof(raddr);
  char buf[BUFLEN];

  sockfd = udp_bind("localhost","9999",NULL); /* UNSURE if localhost is best pratice here ?*/

  u_term = 1;
  signal (SIGINT,signalhandler);
  signal (SIGTERM,signalhandler);

  bstring_t msg = NULL;
  bstring_t log = NULL;
  struct bstrList *parts = NULL;

  bstring flag_check = bfromcstr("CHECK");
  bstring flag_add = bfromcstr("ADD");
  bstring flag_remove = bfromcstr("REMOVE");
  bstring flag_swap = bfromcstr("SWAP");
  bstring flag_purge = bfromcstr("PURGE");

  while(1) {

    if (!checksignal())
      break;

    int64_t rc = readable_timeo(sockfd,100);
    if (rc <= 0) /* timeout */
      continue;

    rc = recvfrom(sockfd,buf,BUFLEN,0,(struct sockaddr*)&raddr,&slen);
    if (rc == -1) {

      err("recvfrom");
    }

    /* subtle error? should not get here */
    if (rc == 0) {
      continue;
    }
    /* terminate the end of a c string */
    buf[rc] = '\0'; 

    bdestroy(msg);
    msg = bfromcstr(buf);
    btrimws(msg);
    
    bstrListDestroy(parts);
    parts = bsplit(msg,'>');

    bdestroy(log);
    
    log = bformat("from %s:%d - ",inet_ntoa(raddr.sin_addr),ntohs(raddr.sin_port));
      
    if (parts->qty < 2) {

      printf("%s message not complete, rejecting\n",btocstr(log));
      continue;
    }

    btoupper(parts->entry[0]);

    /* CHECK */
    if (biseq(flag_check,parts->entry[0])) {
  
      uint8_t present = (*checkf)(parts->entry[1],ctx);
      printf("%s checked [%s] - %s\n",btocstr(log),
	     btocstr(parts->entry[1]),(present) ? "present" : "not present");

      bstring reply = bformat("check>%s>%s",btocstr(parts->entry[1]),(present) ? "yes" : "no");

      rc = sendto(sockfd,btocstr(reply),blength(reply),0,(struct sockaddr*)&raddr,slen);
      bdestroy(reply);
      continue;

    } 

    /* ADD */
    if (biseq(flag_add,parts->entry[0])) {
      
      user_t *user = (*addf)(parts->entry[1],ctx);
      printf("%s %s [%s]\n",btocstr(log),(user) ? "added" : "did not add",
	     btocstr(parts->entry[1]));
     
      continue;
    }

    /* REMOVE */
    if (biseq(flag_remove,parts->entry[0])) {

      uint8_t removed = (*removef)(parts->entry[1],ctx);
      printf("%s %s [%s]\n",btocstr(log),(removed) ? "removed" : "did not remove",
	     btocstr(parts->entry[1]));
     
      continue;
    }

    /* SWAP */
    if (biseq(flag_swap,parts->entry[0])) {
      
      /* check for extra hash */
      if (parts->qty != 3) {
	
	printf("%s swap message incomplete,rejecting\n",btocstr(log));
	continue;
      }

      user_t *user = (*swapf)(parts->entry[1],parts->entry[2],ctx);
      printf("%s %s [%s] with [%s]\n",btocstr(log),(user) ? "replaced" : "did not replace",
	     btocstr(parts->entry[1]),btocstr(parts->entry[2]));

      continue;
    }

    /* PURGE */
    if (biseq(flag_purge,parts->entry[0])) {

      uint8_t rc = (*purgef)(ctx);
      printf("%s %s all hashes\n",btocstr(log),(rc) ? "purged" : "unable to purge");

      continue;
    }

    printf("%s unknown command - %s, rejecting\n",btocstr(log),btocstr(parts->entry[0]));
  }

  bdestroy(flag_check);
  bdestroy(flag_add);
  bdestroy(flag_remove);
  bdestroy(flag_swap);
  bdestroy(flag_purge);


  bdestroy(msg);
  bstrListDestroy(parts);
  bdestroy(log);

  printf("closing down\n");
  close(sockfd);
 
  stats_t stats;
  stats.memused = 0;
  stats.count = 0;
  (*statsf)(&stats,ctx);
  printf("stats\t%dbytes of memory used for %d hashes\n",stats.memused,stats.count);

  (*destroyf)(ctx);
    
  dlclose(hdle);
  
  return 0;
}
