
#include "userd.h"
#include <stdio.h>

int main(void) {

  void *ctx = userdclient_create("localhost:9999");
  int i;
  int yes = 0;
  int errors = 0;
  int total = 1000;
  int rc;
  bstring_t str;

  /* try adding */
  for (i=0; i < total; i++) {
    
    bdestroy(str);
    str = bformat("helloworld %d",i);
    rc = userdclient_addc(ctx,btocstr(str));
    if (rc <= -1)
      errors ++;
    else 
      yes += (rc) ? 1 : 0;
  }
  bdestroy(str);
  printf("add| %d failures, %d/%d added\n",errors,yes,total);
    

  errors = 0; yes = 0;

  for (i=0; i < total; i++) {

    bdestroy(str);
    str = bformat("helloworld %d",i);
    rc = userdclient_check(ctx,btocstr(str));

    if (rc <= -1)
      errors++;
    else 
      yes += (rc) ? 1 : 0;
    
  }
  bdestroy(str);
  printf("check| %d failures, %d/%d checked\n",errors,yes,total); 

  /*
  errors = 0; yes = 0;

  for (i=0; i < total; i++) {

    bdestroy(str);
    str = bformat("helloworld %d",i);
    rc = userdclient_removec(ctx,btocstr(str));
   
    if (rc <= -1)
      errors++;
    else 
      yes += (rc) ? 1 : 0;

  }
  bdestroy(str);
  
  printf("removec| %d failures %d/%d\n",errors,yes,total);
  */
  /*
  errors = 0;
  for (i=0; i < total; i++) {

    bdestroy(str);
    str = bformat("helloworld %d",i);
    rc = userdclient_swap(ctx,btocstr(str),btocstr(str));
    errors += (rc <= -1) ? 1 : 0;
  }
  bdestroy(str);
  
  printf("swap| %d/%d failures\n",errors,total);
  */


  userdclient_destroy (ctx);
  return 0;
}
