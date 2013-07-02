
/* bazaar/bstring.h */
#ifndef BAZAAR_BSTRING_H
#define BAZAAR_BSTRING_H 1

#include "bstrlib.h"
typedef bstring bstring_t;

#define btocstr(bs) (const char *)(bs)->data

/* TODO: add bcritbit functions here please */

#endif
