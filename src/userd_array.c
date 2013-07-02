
#include "userd.h"

#define MODULE "array"
#define VERSION 0

typedef struct {

  user_t *users;
  uint64_t numof;

  void *next;
} users_t;

users_t *create_usersarray (uint64_t count) {

  users_t *u = (users_t *)malloc(sizeof(users_t));
  u->numof = count;
  u->users = (user_t *)malloc(sizeof(user_t) * count);
  int i;
  for (i = 0; i < count; i++) {
   
    u->users[i].hash = NULL;
    u->users[i].prev = NULL;
    u->users[i].next = NULL;
  }
  return u;
}

void destroy_usersarray (users_t *u) {

  int i;
  for (i = 0; i < u->numof; i++) {
    
    if (u->users[i].hash)
      bdestroy (u->users[i].hash);
  }
  free (u->users);
  free (u);
}

user_t * usefrom_usersarray (users_t *u) {
  
  user_t *user = NULL;
  int i;
  for (i=0; i < u->numof; i++) {

    user = &u->users[i];
    if (!user->hash) {
   
      /* then empty */
      user->prev = NULL;
      user->next = NULL;
      return user;
    }
  }

  if (!u->next) {
    
    users_t *nu = create_usersarray (u->numof);
    u->next = (void *)nu;
  }
  /* FIXME: check that memory was allocated ^^^ */
  return usefrom_usersarray ((users_t *)u->next);
}

void berid (user_t *user) {
   
  bdestroy(user->hash);
  user->hash = NULL;
    
  user->next = NULL;
  user->prev = NULL;
}
 
typedef struct {

  users_t *users;
  user_t *head;
  user_t *tail;

} space_t;


user_t * find_user (space_t *s,bstring_t hash) {

  user_t *user = s->head;
  while(user != NULL) {

    if (user->hash)
      if (biseq(hash,user->hash))
	break;
    
    user = (user_t *)user->next;
  }

  return user;
}

user_t * add_user (space_t *s,bstring_t hash) {

  user_t *user = find_user(s,hash);
  if (user != NULL) { /*already present, technically an error! */
    //printf("[%s:%d] warning - %d:%s already present\n",MODULE,__LINE__,
    //	   blength(hash),btocstr(hash));
    return user;
  }

  user = usefrom_usersarray (s->users);
  if (!user) {
    //printf("[%s:%d] error - unable to create new user!\n",MODULE,__LINE__);
    return NULL;
  }

  //printf("[add_user] - %d:%s\n",blength(hash),btocstr(hash));
  user->hash = bstrcpy(hash);
   
  if (s->tail == NULL) {

    s->head = user;
    s->tail = user;
  } else {

    s->tail->next = (void *)user;
    user->prev = (void *)s->tail;
    s->tail = user;
  }

  return user;
}

uint8_t delete_user (space_t *s,bstring_t hash) {

  user_t *user = find_user(s,hash);
  if (!user) {
    //printf("[%s:%d] warning - %d:%s not found\n",MODULE,__LINE__,blength(hash),btocstr(hash));
    return 0;
  }

  /* check that the user found is not the head nor tail of the space */
  if (user == s->head) {

    s->head = (user_t *)user->next;
  }

  if (user == s->tail) {

    s->tail = (user_t *)user->prev;
  }

  user_t *prev = (user_t *)user->prev;
  user_t *next = (user_t *)user->next;

  if (prev)
    prev->next = (void *)next;
  if (next)
    next->prev = (void *)prev;

  //printf("[delete_user] - deleted %d:%s\n",blength(hash),btocstr(hash));
  berid (user);
  return 1;
}

uint32_t check_user (space_t *s,bstring_t hash) {
  
  user_t *user = find_user(s,hash);
  return (!user) ? 0 : 1;
}

uint8_t purge_allusers (space_t *s) {

  user_t *user = s->head;
  while(user) {
    
    user_t *c = user;
    user = (user_t *)user->next;
    berid(c);
  }

  s->head = NULL;
  s->tail = NULL;

  return 1;
}






user_t * userd_add (bstring_t hash,void * ctx) {

  space_t *space = (space_t *)ctx;
  if (!space)
    return NULL;

  return add_user(space,hash);
}

uint8_t userd_remove (bstring_t hash,void * ctx) {

  space_t *space = (space_t *)ctx;
  if (!space)
    return 0;

  return delete_user(space,hash);
}

uint8_t userd_check (bstring_t hash,void * ctx) {

  space_t *space = (space_t *)ctx;
  if (!space)
    return 0;

  return check_user(space,hash);
}

uint8_t userd_purge (void * ctx) {
  
  space_t *space = (space_t *)ctx;
  
  return purge_allusers(space);
}

user_t * userd_swap (bstring_t hash0,bstring_t hash1,void *ctx) {

  space_t *space = (space_t *)ctx;
  if (!space)
    return NULL;

  /* TODO: this is pretty slow, considering both 
   *       delete and add internally do a find op!
   */
  delete_user(space,hash0);
  return add_user(space,hash1);
}

void userd_stats (stats_t *stats,void *ctx) {

  space_t *space = (space_t *)ctx;
  if (!space)
    return;

  size_t used;
  users_t *users;
  
  used = sizeof(space_t);
  users = space->users;
  
  while(users != NULL) {

    used += sizeof(users_t) + (sizeof(user_t) * users->numof);
    users = (users_t *)users->next;
  }
 
  uint64_t count = 0;

  user_t *user = space->head;
  while(user) {

    count++;
    used += blength(user->hash);
    user = (user_t *)user->next;
  }

  stats->memused = used;
  stats->count = count;  
}

bstring userd_name (void) {

  return bfromcstr("array");
}

void * userd_create (void) {

  space_t *space = (space_t *)malloc(sizeof(space_t));
  if (!space)
    return NULL;

  space->users = create_usersarray(5);
  space->head = NULL;
  space->tail = NULL;

  return (void *)space;
}

void userd_destroy (void * ctx) {

  space_t *space = (space_t *)ctx;
  if (!space)
    return;

  destroy_usersarray(space->users);
  free (space);  
}



