#include <stdlib.h>
#include "object.h"
#include "dynamic_buffer.h"
#include "interpret.h"
#include "program.h"
#include "stralloc.h"
#include "svalue.h"
#include "macros.h"
#include "memory.h"
#include "error.h"
#include "main.h"

struct object *master_object = 0;
struct object *first_object;

struct object fake_object = { 1 }; /* start with one reference */

void setup_fake_object()
{
  fake_object.prog=&fake_program;
  fake_object.next=0;
  fake_object.refs=0xffffff;
}

struct object *clone(struct program *p, int args)
{
  int e;
  struct object *o;
  struct frame frame;

  o=(struct object *)xalloc(sizeof(struct object)-1+p->storage_needed);
#ifdef DEBUG
  if(d_flag > 14) check_signals();
#endif

  o->prog=p;
  p->refs++;
  o->next=first_object;
  o->prev=0;
  if(first_object)
    first_object->prev=o;
  first_object=o;
  o->refs=1;

  frame.parent_frame=fp;
  frame.current_object=o;
  frame.locals=0;
  frame.fun=-1;
  frame.pc=0;
  fp= & frame;

  frame.current_object->refs++;

  /* clear globals and call C initializers */
  for(e=p->num_inherits-1; e>=0; e--)
  {
    int d;

    frame.context=p->inherits[e];
    frame.context.prog->refs++;
    frame.current_storage=o->storage+frame.context.storage_offset;

    for(d=0;d<frame.context.prog->num_identifiers;d++)
    {
      if(frame.context.prog->identifiers[d].flags & IDENTIFIER_FUNCTION) 
	continue;
      
      if(frame.context.prog->identifiers[d].run_time_type == T_MIXED)
      {
	struct svalue *s;
	s=(struct svalue *)(frame.current_storage +
			    frame.context.prog->identifiers[d].func.offset);
	s->type=T_INT;
	s->u.integer=0;
	s->subtype=0;
      }else{
	union anything *u;
	u=(union anything *)(frame.current_storage +
			     frame.context.prog->identifiers[d].func.offset);
	u->integer=0;
      }
    }

    if(frame.context.prog->init)
      frame.context.prog->init(frame.current_storage,o);

    free_program(frame.context.prog);
  }

  free_object(frame.current_object);
  fp = frame.parent_frame;

  if(!master_object) 
  {
    master_object=o;
    o->refs++;
  }

#ifdef DEBUG
    if(d_flag > 14) check_signals();
#endif

  apply(o,"__INIT",0);
  pop_stack();
  apply(o,"create",args);
  pop_stack();

#ifdef DEBUG
    if(d_flag > 14) check_signals();
#endif

  return o;
}


struct object *get_master()
{
  extern char *master_file;
  struct program *master_prog;
  struct lpc_string *master_name;
  static int inside=0;

  if(master_object && master_object->prog)
    return master_object;

  if(inside) return 0;

  if(master_object)
  {
    free_object(master_object);
    master_object=0;
  }

  inside = 1;
  master_name=make_shared_string(master_file);
  master_prog=compile_file(master_name);
  free_string(master_name);
  if(!master_prog) return 0;
  free_object(clone(master_prog,0));
  free_program(master_prog);
  inside = 0;
  return master_object;
}

struct object *master()
{
  struct object *o;
  o=get_master();
  if(!o) fatal("Couldn't load master object.\n");
  return o;
}


void destruct(struct object *o)
{
  int e;
  struct frame frame;
  struct program *p;

  if(!o || !(p=o->prog)) return; /* Object already destructed */
  o->prog=0;

  frame.parent_frame=fp;
  frame.current_object=o;
  frame.locals=0;
  frame.fun=-1;
  frame.pc=0;
  fp= & frame;

  frame.current_object->refs++;

  /* free globals and call C de-initializers */
  for(e=p->num_inherits-1; e>=0; e--)
  {
    int d;

    frame.context=p->inherits[e];
    frame.context.prog->refs++;
    frame.current_storage=o->storage+frame.context.storage_offset;

    if(frame.context.prog->exit)
      frame.context.prog->exit(frame.current_storage,o);

    for(d=0;d<frame.context.prog->num_identifiers;d++)
    {
      if(frame.context.prog->identifiers[d].flags & IDENTIFIER_FUNCTION) 
	continue;
      
      if(frame.context.prog->identifiers[d].run_time_type == T_MIXED)
      {
	struct svalue *s;
	s=(struct svalue *)(frame.current_storage +
			    frame.context.prog->identifiers[d].func.offset);
	free_svalue(s);
      }else{
	union anything *u;
	u=(union anything *)(frame.current_storage +
			     frame.context.prog->identifiers[d].func.offset);
	free_short_svalue(u, frame.context.prog->identifiers[d].run_time_type);
      }
    }
    free_program(frame.context.prog);
  }

  free_object(frame.current_object);
  fp = frame.parent_frame;

  free_program(p);
}

void really_free_object(struct object *o)
{
  if(o->prog)
  {
    /* prevent recursive calls to really_free_object */
    o->refs++;
    destruct(o);
    o->refs--;
  }

  if(o->prev)
    o->prev->next=o->next;
  else
    first_object=o->next;

  if(o->next) o->next->prev=o->prev;
  
  free((char *)o);
}

void object_index_no_free(struct svalue *to,
			  struct object *o,
			  struct svalue *index)
{
  struct program *p;
  int f;

  if(!o || !(p=o->prog))
    error("Lookup in destructed object.\n");

  if(index->type != T_STRING)
    error("Lookup on non-string value.\n");

  f=find_shared_string_identifier(index->u.string, p);
  if(f < 0)
  {
    to->type=T_INT;
    to->subtype=NUMBER_UNDEFINED;
    to->u.integer=0;
  }else{
    struct identifier *i;
    i=ID_FROM_INT(p, f);

    if(i->flags & IDENTIFIER_FUNCTION)
    {
      to->type=T_FUNCTION;
      to->subtype=f;
      to->u.object=o;
      o->refs++;
    }
    else if(i->run_time_type == T_MIXED)
    {
      struct svalue *s;
      s=(struct svalue *)(o->storage+
			  INHERIT_FROM_INT(p, f)->storage_offset +
			  i->func.offset);
      check_destructed(s);
      assign_svalue_no_free(to,s);
    }
    else
    {
      union anything *u;
      u=(union anything *)(o->storage+
			   INHERIT_FROM_INT(p, f)->storage_offset +
			   i->func.offset);
      check_short_destructed(u,i->run_time_type);
      assign_from_short_svalue_no_free(to,u,i->run_time_type);
    }
  }
}

void object_index(struct svalue *to,
		  struct object *o,
		  struct svalue *index)
{
  struct program *p;
  int f;

  if(!o || !(p=o->prog))
    error("Lookup in destructed object.\n");

  if(index->type != T_STRING)
    error("Lookup on non-string value.\n");

  f=find_shared_string_identifier(index->u.string, p);
  if(f < 0)
  {
    free_svalue(to);
    to->type=T_INT;
    to->subtype=NUMBER_UNDEFINED;
    to->u.integer=0;
  }else{
    struct identifier *i;
    i=ID_FROM_INT(p, f);

    if(i->flags & IDENTIFIER_FUNCTION)
    {
      free_svalue(to);
      to->type=T_FUNCTION;
      to->subtype=f;
      to->u.object=o;
      o->refs++;
    }
    else if(i->run_time_type == T_MIXED)
    {
      struct svalue *s;
      s=(struct svalue *)(o->storage+
			  INHERIT_FROM_INT(p, f)->storage_offset +
			  i->func.offset);
      check_destructed(s);

      assign_svalue(to, s);
    }
    else
    {
      union anything *u;
      u=(union anything *)(o->storage+
			   INHERIT_FROM_INT(p, f)->storage_offset +
			   i->func.offset);
      check_short_destructed(u,i->run_time_type);
      free_svalue(to);
      assign_from_short_svalue_no_free(to, u, i->run_time_type);
    }
  }
}

void object_low_set_index(struct object *o,
			  int f,
			  struct svalue *from)
{
  struct identifier *i;
  struct program *p;

  if(!o || !(p=o->prog))
    error("Lookup in destructed object.\n");

  check_destructed(from);

  i=ID_FROM_INT(p, f);

  if(i->flags & IDENTIFIER_FUNCTION)
  {
    error("Cannot assign functions.\n");
  }
  else if(i->run_time_type == T_MIXED)
  {
    assign_svalue((struct svalue *) 
		  (o->storage+
		   INHERIT_FROM_INT(p, f)->storage_offset+
		   i->func.offset),
		  from);
  }
  else
  {
    assign_to_short_svalue((union anything *) 
			   (o->storage+
			    INHERIT_FROM_INT(p, f)->storage_offset+
			    i->func.offset),
			   i->run_time_type,
			   from);
  }
}

void object_set_index(struct object *o,
		      struct svalue *index,
		      struct svalue *from)
{
  struct program *p;
  int f;

  if(!o || !(p=o->prog))
    error("Lookup in destructed object.\n");

  if(index->type != T_STRING)
    error("Lookup on non-string value.\n");

  f=find_shared_string_identifier(index->u.string, p);
  if(f < 0)
  {
    error("No such variable in object.\n");
  }else{
    object_low_set_index(o, f, from);
  }
}


union anything *object_low_get_item_ptr(struct object *o,
					int f,
					TYPE_T type)
{
  struct identifier *i;
  struct program *p;

  if(!o || !(p=o->prog))
    error("Lookup in destructed object.\n");

  i=ID_FROM_INT(p, f);

  if(i->flags & IDENTIFIER_FUNCTION)
  {
    error("Cannot assign functions.\n");
  }
  else if(i->run_time_type == T_MIXED)
  {
    struct svalue *s;
    s=(struct svalue *)(o->storage+
			INHERIT_FROM_INT(p, f)->storage_offset +
			i->func.offset);
    if(s->type == type) return & s->u;
  }
  else if(i->run_time_type == type)
  {
    return (union anything *) (o->storage+
			       INHERIT_FROM_INT(p, f)->storage_offset +
			       i->func.offset);
  }
  return 0;
}


union anything *object_get_item_ptr(struct object *o,
				    struct svalue *index,
				    TYPE_T type)
{

  struct program *p;
  int f;

  if(!o || !(p=o->prog))
    error("Lookup in destructed object.\n");

  if(index->type != T_STRING)
    error("Lookup on non-string value.\n");

  f=find_shared_string_identifier(index->u.string, p);
  if(f < 0)
  {
    error("No such variable in object.\n");
  }else{
    return object_low_get_item_ptr(o, f, type);
  }
  return 0;
}

#ifdef DEBUG
void verify_all_objects(int pass)
{
  struct object *o;
  struct frame frame;

  for(o=first_object;o;o=o->next)
  {
    if(pass)
    {
      if(checked((void *)o, 0) != o->refs)
      {
	fatal("Object has wrong number of refs.\n");
      }
      continue;
    }

    if(o->next && o->next->prev !=o)
      fatal("Object check: o->next->prev != o\n");

    if(o->prev)
    {
      if(o->prev->next != o)
	fatal("Object check: o->prev->next != o\n");

      if(o == first_object)
	fatal("Object check: o->prev !=0 && first_object == o\n");
    } else {
      if(first_object != o)
	fatal("Object check: o->prev ==0 && first_object != o\n");
    }

    if(o->refs <=0)
      fatal("Object refs <= zero.\n");

    if(o->prog)
    {
      extern struct program *first_program;
      struct program *p;
      int e;

      for(p=first_program;p!=o->prog;p=p->next)
	if(!p)
	  fatal("Object's program not in program list.\n");

      for(e=0;e<o->prog->num_identifiers;e++)
      {
	struct identifier *i;
	i=ID_FROM_INT(o->prog, e);
	if(i->flags & IDENTIFIER_FUNCTION)
	  continue;

	if(i->run_time_type == T_MIXED)
	{
	  check_svalue((struct svalue *)
		       (o->storage+
			INHERIT_FROM_INT(o->prog, e)->storage_offset+
			i->func.offset));
	}else{
	  check_short_svalue((union anything *)
			     (o->storage+
			      INHERIT_FROM_INT(o->prog, e)->storage_offset+
			      i->func.offset),
			     i->run_time_type);
	}
      }

      frame.parent_frame=fp;
      frame.current_object=o;
      frame.locals=0;
      frame.fun=-1;
      frame.pc=0;
      fp= & frame;

      frame.current_object->refs++;

      for(e=0;e<o->prog->num_inherits;e++)
      {
	frame.context=o->prog->inherits[e];
	frame.context.prog->refs++;
	frame.current_storage=o->storage+frame.context.storage_offset;

	if(frame.context.prog->checkrefs)
	  frame.context.prog->checkrefs(frame.current_storage,o,pass);
      }

      free_object(frame.current_object);
      fp = frame.parent_frame;
    }
  }
}
#endif

int object_equal_p(struct object *a, struct object *b, struct processing *p)
{
  struct processing curr;

  if(a == b) return 1;
  if(a->prog != b->prog) return 0;

  curr.pointer_a = a;
  curr.pointer_b = b;
  curr.next = p;

  for( ;p ;p=p->next)
    if(p->pointer_a == (void *)a && p->pointer_b == (void *)b)
      return 1;


  if(a->prog)
  {
    int e;
    for(e=0;e<a->prog->num_identifiers;e++)
    {
      struct identifier *i;
      i=ID_FROM_INT(a->prog, e);
      if(i->flags & IDENTIFIER_FUNCTION)
	continue;

      if(i->run_time_type == T_MIXED)
      {
	if(!low_is_equal((struct svalue *)
			(a->storage+
			 INHERIT_FROM_INT(a->prog, e)->storage_offset+
			 i->func.offset),
			(struct svalue *)
			(b->storage+
			 INHERIT_FROM_INT(a->prog, e)->storage_offset+
			 i->func.offset),
			&curr))
	  return 0;
      }else{
	if(!low_short_is_equal((union anything *)
			       (a->storage+
				INHERIT_FROM_INT(a->prog, e)->storage_offset+
				i->func.offset),
			       (union anything *)
			       (b->storage+
				INHERIT_FROM_INT(a->prog, e)->storage_offset+
				i->func.offset),
			       i->run_time_type,
			       &curr))
	  return 0;
      }
    }
  }

  return 1;
}

void cleanup_objects()
{
  struct object *o,*next;
  for(o=first_object;o;o=next)
  {
    o->refs++;
    destruct(o);
    next=o->next;
    free_object(o);
  }

  free_object(master_object);
  master_object=0;
}
