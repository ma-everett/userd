
#ifndef USERD_H
#define USERD_H 1

#include "bstring.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* server interface */

typedef struct {

  bstring_t hash;
  void *prev,*next;

} user_t;

typedef struct {
  
  size_t memused;
  uint64_t count;
} stats_t;


extern user_t * userd_add (bstring_t,void *);
extern uint8_t userd_remove(bstring_t,void *);
extern uint8_t userd_check (bstring_t,void *);
extern uint8_t userd_purge (void *);
extern user_t * usered_swap (bstring_t,bstring_t,void *);
extern void userd_stats(stats_t *,void *);

extern bstring userd_name (void);
extern void *userd_create (void);
extern void userd_destroy (void *);

/* client interface */

void * userdclient_create(const char *serverlist);
void userdclient_destroy(void *ctx);

int userdclient_check(void *ctx,const char *hash);
int userdclient_remove(void *ctx,const char *hash);
int userdclient_removec(void *ctx,const char *hash);
int userdclient_add(void *ctx,const char *hash);
int userdclient_addc(void *ctx,const char *hash);
int userdclient_swap(void *ctx,const char *hash0,const char *hash1);
int userdclient_purge(void *ctx);

#endif
