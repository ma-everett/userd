
#ifndef USERD_CLIENT_H
#define USERD_CLIENT_H

void * userdclient_create(const char *serverlist,int len);
void userdclient_destroy(void *ctx);

int userdclient_check(void *ctx,const char *hash,int len);


#endif
