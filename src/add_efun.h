#ifndef ADD_EFUN_H
#define ADD_EFUN_H

#include "svalue.h"
#include "hashtable.h"

struct efun
{
  struct svalue function;
  struct hash_entry link;
};

typedef void (*c_fun)(INT32);

struct callable
{
  INT32 refs;
  c_fun function;
  struct lpc_string *type;
  struct lpc_string *name;
  INT16 flags;
};

/* Prototypes begin here */
struct efun *lookup_efun(struct lpc_string *name);
void low_add_efun(struct lpc_string *name, struct svalue *fun);
struct callable *make_callable(c_fun fun,char *name, char *type, INT16 flags);
void really_free_callable(struct callable *fun);
void add_efun(char *name, c_fun fun, char *type, INT16 flags);
void push_all_efuns_on_stack();
void cleanup_added_efuns();
/* Prototypes end here */

#endif
