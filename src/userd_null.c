
/* userd/null module 
 * histroy: 
 * version 0
 */

#include "userd.h"

user_t * userd_add (bstring_t hash,void * ctx) {

  return NULL;
}

uint8_t userd_remove (bstring_t hash,void * ctx) {

  return 0;
}

uint8_t userd_check (bstring_t hash,void * ctx) {

  return 0;
}

uint8_t userd_purge (void * ctx) {

  return 0;
}

user_t * userd_swap (bstring_t hash0,bstring_t hash1,void *ctx) {

  return NULL;
}

void userd_stats (stats_t *stats,void *ctx) {

}

bstring userd_name (void) {

  return bfromcstr("null version 0");
}

void * userd_create (void) {

  return NULL;
}

void userd_destroy (void * ctx) {

  
}
