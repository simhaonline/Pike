#ifndef OBJECT_H
#define OBJECT_H

#include "types.h"
#include "svalue.h"

/* a destructed object has no program */

struct object
{
  INT32 refs;                    /* Reference count, must be first. */
  struct program *prog;
  struct object *next;
  struct object *prev;
  char storage[1];
};

extern struct object fake_object;
extern struct object *first_object;

#define free_object(O) do{ struct object *o_=(O); if(!--o_->refs) really_free_object(o_); }while(0)
#define GLOBAL_FROM_INT(I) (fp->current_object->storage+INHERIT_FROM_INT(fp->current_object->prog,(I))->storage_offset+ID_FROM_INT(fp->current_object->prog,(I))->func.offset)

/* Prototypes begin here */
void setup_fake_object();
struct object *clone(struct program *p, int args);
struct object *get_master();
struct object *master();
void destruct(struct object *o);
void really_free_object(struct object *o);
void object_index_no_free(struct svalue *to,
			  struct object *o,
			  struct svalue *index);
void object_index(struct svalue *to,
		  struct object *o,
		  struct svalue *index);
void object_low_set_index(struct object *o,
			  int f,
			  struct svalue *from);
void object_set_index(struct object *o,
		      struct svalue *index,
		      struct svalue *from);
union anything *object_low_get_item_ptr(struct object *o,
					int f,
					TYPE_T type);
union anything *object_get_item_ptr(struct object *o,
				    struct svalue *index,
				    TYPE_T type);
void verify_all_objects(int pass);
int object_equal_p(struct object *a, struct object *b, struct processing *p);
void cleanup_objects();
/* Prototypes end here */

#endif /* OBJECT_H */

