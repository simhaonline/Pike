/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
/**/
#include "global.h"
RCSID("$Id: mapping.c,v 1.47 1999/11/23 10:38:16 mast Exp $");
#include "main.h"
#include "object.h"
#include "mapping.h"
#include "svalue.h"
#include "array.h"
#include "pike_macros.h"
#include "language.h"
#include "error.h"
#include "pike_memory.h"
#include "dynamic_buffer.h"
#include "interpret.h"
#include "las.h"
#include "gc.h"
#include "stralloc.h"
#include "security.h"

#define AVG_LINK_LENGTH 4
#define MIN_LINK_LENGTH 1
#define MAP_SLOTS(X) ((X)?((X)+((X)>>4)+8):0)

struct mapping *first_mapping;


#ifdef PIKE_DEBUG
/* This function checks that the type field isn't lacking any bits.
 * It is used for debugging purposes only.
 */
static void check_mapping_type_fields(struct mapping *m)
{
  INT32 e;
  struct keypair *k,**prev;
  TYPE_FIELD ind_types, val_types;

  ind_types=val_types=0;

  MAPPING_LOOP(m) 
    {
      val_types |= 1 << k->val.type;
      ind_types |= 1 << k->ind.type;
    }

  if(val_types & ~(m->val_types))
    fatal("Mapping value types out of order!\n");

  if(ind_types & ~(m->ind_types))
    fatal("Mapping indices types out of order!\n");
}
#endif


/* This function allocates the hash table and svalue space for a mapping
 * struct. The size is the max number of indices that can fit in the
 * allocated space.
 */
static void init_mapping(struct mapping *m, INT32 size)
{
  char *tmp;
  INT32 e;
  INT32 hashsize,hashspace;

#ifdef PIKE_DEBUG
  if(size < 0) fatal("init_mapping with negative value.\n");
#endif
  if(size)
  {
    hashsize=size / AVG_LINK_LENGTH + 1;
    if(!(hashsize & 1)) hashsize++;
    hashspace=hashsize+1;
    e=sizeof(struct keypair)*size+ sizeof(struct keypair *)*hashspace;
    tmp=(char *)xalloc(e);
    
    m->hash=(struct keypair **) tmp;
    m->hashsize=hashsize;
    
    tmp+=sizeof(struct keypair *)*hashspace;
    
    MEMSET((char *)m->hash, 0, sizeof(struct keypair *) * m->hashsize);
    
    m->free_list=(struct keypair *) tmp;
    for(e=1;e<size;e++)
      m->free_list[e-1].next = m->free_list + e;
    m->free_list[e-1].next=0;
  }else{
    m->hashsize=0;
    m->hash=0;
    m->free_list=0;
  }
  m->ind_types = 0;
  m->val_types = 0;
  m->size = 0;
}

/* This function allocates an empty mapping with room for 'size' values
 */
struct mapping *allocate_mapping(int size)
{
  struct mapping *m;

  GC_ALLOC();

  m=ALLOC_STRUCT(mapping);

  INITIALIZE_PROT(m);
  init_mapping(m,size);

  m->next = first_mapping;
  m->prev = 0;
  m->refs = 1;
  m->flags = 0;

  if(first_mapping) first_mapping->prev = m;
  first_mapping=m;

  return m;
}

/* This function should only be called by the free_mapping() macro.
 * It frees the storate used by the mapping.
 */
void really_free_mapping(struct mapping *m)
{
  INT32 e;
  struct keypair *k;
#ifdef PIKE_DEBUG
  if(m->refs)
    fatal("really free mapping on mapping with nonzero refs.\n");
#endif

  FREE_PROT(m);

  MAPPING_LOOP(m)
  {
    free_svalue(& k->val);
    free_svalue(& k->ind);
  }

  if(m->prev)
    m->prev->next = m->next;
  else
    first_mapping = m->next;

  if(m->next) m->next->prev = m->prev;

  if(m->hash)
    free((char *)m->hash);
  free((char *)m);

  GC_FREE();
}

/* This function is used to rehash a mapping without loosing the internal
 * order in each hash chain. This is to prevent mappings from becoming
 * very inefficient just after being rehashed.
 */
static void mapping_rehash_backwards(struct mapping *m, struct keypair *p)
{
  unsigned INT32 h;
  struct keypair *tmp;

  if(!p) return;
  mapping_rehash_backwards(m,p->next);
  h=hash_svalue(& p->ind) % m->hashsize;
  tmp=m->free_list;
  m->free_list=tmp->next;
  tmp->next=m->hash[h];
  m->hash[h]=tmp;
  tmp->ind=p->ind;
  tmp->val=p->val;
  m->size++;
  m->ind_types |= 1 << p->ind.type;
  m->val_types |= 1 << p->val.type;
}

/* This function re-allocates a mapping. It adjusts the max no. of
 * values can be fitted into the mapping. It takes a bit of time to
 * run, but is used seldom enough not to degrade preformance significantly.
 */
static struct mapping *rehash(struct mapping *m, int new_size)
{
#ifdef PIKE_DEBUG
  INT32 tmp=m->size;
#endif
  INT32 e,hashsize;
  struct keypair *k,**hash;

  hashsize=m->hashsize;
  hash=m->hash;

  init_mapping(m, new_size);

  if(hash)
  {
    for(e=0;e<hashsize;e++)
      mapping_rehash_backwards(m, hash[e]);
    
    free((char *)hash);
  }

#ifdef PIKE_DEBUG
    if(m->size != tmp)
      fatal("Rehash failed, size not same any more.\n");
#endif
    
#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif

  return m;
}

/* This function brings the type fields in the mapping up to speed.
 * I only use it when the type fields MUST be correct, which is not
 * very often.
 */
void mapping_fix_type_field(struct mapping *m)
{
  INT32 e;
  struct keypair *k;
  TYPE_FIELD ind_types, val_types;

  val_types = ind_types = 0;

  MAPPING_LOOP(m)
    {
      val_types |= 1 << k->val.type;
      ind_types |= 1 << k->ind.type;
    }

#ifdef PIKE_DEBUG
  if(val_types & ~(m->val_types))
    fatal("Mapping value types out of order!\n");

  if(ind_types & ~(m->ind_types))
    fatal("Mapping indices types out of order!\n");
#endif
  m->val_types = val_types;
  m->ind_types = ind_types;
}

/* This function inserts key:val into the mapping m.
 * Same as doing m[key]=val; in pike.
 */
void mapping_insert(struct mapping *m,
		    struct svalue *key,
		    struct svalue *val)
{
  unsigned INT32 h,h2;
  struct keypair *k, **prev;

  h2=hash_svalue(key);
  if(m->hashsize)
  {
    h=h2 % m->hashsize;
  
#ifdef PIKE_DEBUG
    if(d_flag > 1) check_mapping_type_fields(m);
#endif
    if(m->ind_types & (1 << key->type))
    {
      for(prev= m->hash + h;(k=*prev);prev=&k->next)
      {
	if(is_eq(& k->ind, key))
	{
	  *prev=k->next;
	  k->next=m->hash[h];
	  m->hash[h]=k;
	  
	  m->val_types |= 1 << val->type;
	  assign_svalue(& k->val, val);
	  
#ifdef PIKE_DEBUG
	  if(d_flag > 1) check_mapping_type_fields(m);
#endif
	  return;
	}
      }
    }
  }else{
    h=0;
  }
    
  if(!(k=m->free_list))
  {
    rehash(m, m->size * 2 + 2);
    h=h2 % m->hashsize;
    k=m->free_list;
  }

  m->free_list=k->next;
  k->next=m->hash[h];
  m->hash[h]=k;
  m->ind_types |= 1 << key->type;
  m->val_types |= 1 << val->type;
  assign_svalue_no_free(& k->ind, key);
  assign_svalue_no_free(& k->val, val);
  m->size++;

#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif
}

union anything *mapping_get_item_ptr(struct mapping *m,
				     struct svalue *key,
				     TYPE_T t)
{
  unsigned INT32 h, h2;
  struct keypair *k, **prev;

  h2=hash_svalue(key);

  if(m->hashsize)
  {
    h=h2 % m->hashsize;

#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif

    for(prev= m->hash + h;(k=*prev);prev=&k->next)
    {
      if(is_eq(& k->ind, key))
      {
	*prev=k->next;
	k->next=m->hash[h];
	m->hash[h]=k;
	
	if(k->val.type == t) return & ( k->val.u );
	
#ifdef PIKE_DEBUG
	if(d_flag > 1) check_mapping_type_fields(m);
#endif
	
	return 0;
      }
    }
  }else{
    h=0;
  }
  
  if(!(k=m->free_list))
  {
    rehash(m, m->size * 2 + 2);
    h=h2 % m->hashsize;
    k=m->free_list;
  }

  m->free_list=k->next;
  k->next=m->hash[h];
  m->hash[h]=k;
  assign_svalue_no_free(& k->ind, key);
  k->val.type=T_INT;
  k->val.subtype=NUMBER_NUMBER;
  k->val.u.integer=0;
  m->ind_types |= 1 << key->type;
  m->val_types |= BIT_INT;
  m->size++;

  if(t != T_INT) return 0;

#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif

  return & ( k->val.u );
}

void map_delete_no_free(struct mapping *m,
			struct svalue *key,
			struct svalue *to)
{
  unsigned INT32 h;
  struct keypair *k, **prev;

  if(!m->size) return;

  h=hash_svalue(key) % m->hashsize;
  
  for(prev= m->hash + h;(k=*prev);prev=&k->next)
  {
    if(is_eq(& k->ind, key))
    {
      *prev=k->next;
      free_svalue(& k->ind);
      if(to)
	to[0]=k->val;
      else
	free_svalue(& k->val);
      k->next=m->free_list;
      m->free_list=k;
      m->size--;

      if(m->size < (m->hashsize + 1) * MIN_LINK_LENGTH)
	rehash(m, MAP_SLOTS(m->size));

#ifdef PIKE_DEBUG
      if(d_flag > 1) check_mapping_type_fields(m);
#endif
      return;
    }
  }
  if(to)
  {
    to->type=T_INT;
    to->subtype=NUMBER_UNDEFINED;
    to->u.integer=0;
  }
}

void check_mapping_for_destruct(struct mapping *m)
{
  INT32 e;
  struct keypair *k, **prev;
  TYPE_FIELD ind_types, val_types;

#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif

  if(!m->size) return;

  if((m->ind_types | m->val_types) & (BIT_OBJECT | BIT_FUNCTION))
  {
    val_types = ind_types = 0;
    m->val_types |= BIT_INT;
    for(e=0;e<m->hashsize;e++)
    {
      for(prev= m->hash + e;(k=*prev);)
      {
	check_destructed(& k->val);
	
	if((k->ind.type == T_OBJECT || k->ind.type == T_FUNCTION) &&
	   !k->ind.u.object->prog)
	{
	  *prev=k->next;
	  free_svalue(& k->ind);
	  free_svalue(& k->val);
	  k->next=m->free_list;
	  m->free_list=k;
	  m->size--;
	}else{
	  val_types |= 1 << k->val.type;
	  ind_types |= 1 << k->ind.type;
	  prev=&k->next;
	}
      }
    }
    if(MAP_SLOTS(m->size) < m->hashsize * MIN_LINK_LENGTH)
      rehash(m, MAP_SLOTS(m->size));

    m->val_types = val_types;
    m->ind_types = ind_types;
#ifdef PIKE_DEBUG
    if(d_flag > 1) check_mapping_type_fields(m);
#endif
  }
}

struct svalue *low_mapping_lookup(struct mapping *m,
				  struct svalue *key)
{
  unsigned INT32 h;
  struct keypair *k, **prev;

#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif
  if(!m->size) return 0;

  if((1 << key->type) & m->ind_types)
  {
    h=hash_svalue(key) % m->hashsize;
  
    for(prev= m->hash + h;(k=*prev);prev=&k->next)
    {
      if(is_eq(& k->ind, key))
      {
	*prev=k->next;
	k->next=m->hash[h];
	m->hash[h]=k;
	
	return &k->val;
      }
    }
  }

  return 0;
}

struct svalue *low_mapping_string_lookup(struct mapping *m,
					 struct pike_string *p)
{
  struct svalue tmp;
  tmp.type=T_STRING;
  tmp.u.string=p;
  return low_mapping_lookup(m, &tmp);
}

void mapping_string_insert(struct mapping *m,
			   struct pike_string *p,
			   struct svalue *val)
{
  struct svalue tmp;
  tmp.type=T_STRING;
  tmp.u.string=p;
  mapping_insert(m, &tmp, val);
}

void mapping_string_insert_string(struct mapping *m,
				  struct pike_string *p,
				  struct pike_string *val)
{
  struct svalue tmp;
  tmp.type=T_STRING;
  tmp.u.string=val;
  mapping_string_insert(m, p, &tmp);
}

struct svalue *simple_mapping_string_lookup(struct mapping *m,
					    char *p)
{
  struct pike_string *tmp;
  if((tmp=findstring(p)))
    return low_mapping_string_lookup(m,tmp);
  return 0;
}

struct svalue *mapping_mapping_lookup(struct mapping *m,
				      struct svalue *key1,
				      struct svalue *key2,
				      int create)
{
  struct svalue tmp;
  struct mapping *m2;
  struct svalue *s=low_mapping_lookup(m, key1);
  debug_malloc_touch(m);

  if(!s || !s->type==T_MAPPING)
  {
    if(!create) return 0;
    tmp.u.mapping=allocate_mapping(5);
    tmp.type=T_MAPPING;
    mapping_insert(m, key1, &tmp);
    debug_malloc_touch(m);
    debug_malloc_touch(tmp.u.mapping);
    free_mapping(tmp.u.mapping); /* There is one ref in 'm' */
    s=&tmp;
  }

  m2=s->u.mapping;
  debug_malloc_touch(m2);
  s=low_mapping_lookup(m2, key2);
  if(s) return s;
  if(!create) return 0;

  tmp.type=T_INT;
  tmp.subtype=NUMBER_UNDEFINED;
  tmp.u.integer=0;

  mapping_insert(m2, key2, &tmp);
  debug_malloc_touch(m2);

  return low_mapping_lookup(m2, key2);
}


struct svalue *mapping_mapping_string_lookup(struct mapping *m,
				      struct pike_string *key1,
				      struct pike_string *key2,
				      int create)
{
  struct svalue k1,k2;
  k1.type=T_STRING;
  k1.u.string=key1;
  k2.type=T_STRING;
  k2.u.string=key2;
  return mapping_mapping_lookup(m,&k1,&k2,create);
}



void mapping_index_no_free(struct svalue *dest,
			   struct mapping *m,
			   struct svalue *key)
{
  struct svalue *p;

  if((p=low_mapping_lookup(m,key)))
  {
    if(p->type==T_INT)
      p->subtype=NUMBER_NUMBER;

    assign_svalue_no_free(dest, p);
  }else{
    dest->type=T_INT;
    dest->u.integer=0;
    dest->subtype=NUMBER_UNDEFINED;
  }
}

struct array *mapping_indices(struct mapping *m)
{
  INT32 e;
  struct array *a;
  struct svalue *s;
  struct keypair *k;

  a=allocate_array(m->size);
  s=ITEM(a);

  MAPPING_LOOP(m) assign_svalue(s++, & k->ind);

  a->type_field = m->ind_types;
  
#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif

  return a;
}

struct array *mapping_values(struct mapping *m)
{
  INT32 e;
  struct keypair *k;
  struct array *a;
  struct svalue *s;

  a=allocate_array(m->size);
  s=ITEM(a);

  MAPPING_LOOP(m) assign_svalue(s++, & k->val);

  a->type_field = m->val_types;

#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif
  
  return a;
}

struct array *mapping_to_array(struct mapping *m)
{
  INT32 e;
  struct keypair *k;
  struct array *a;
  struct svalue *s;
  a=allocate_array(m->size);
  s=ITEM(a);
  MAPPING_LOOP(m)
    {
      struct array *b=allocate_array(2);
      assign_svalue(b->item+0, & k->ind);
      assign_svalue(b->item+1, & k->val);
      s->u.array=b;
      s->type=T_ARRAY;
      s++;
    }
  a->type_field = BIT_ARRAY;

  return a;
}

void mapping_replace(struct mapping *m,struct svalue *from, struct svalue *to)
{
  INT32 e;
  struct keypair *k;

  MAPPING_LOOP(m)
    if(is_eq(& k->val, from))
      assign_svalue(& k->val, to);

  m->val_types |= 1 << to->type;

#ifdef PIKE_DEBUG
  if(d_flag > 1) check_mapping_type_fields(m);
#endif

}

struct mapping *mkmapping(struct array *ind, struct array *val)
{
  struct mapping *m;
  struct svalue *i,*v;
  INT32 e;

#ifdef PIKE_DEBUG
  if(ind->size != val->size)
    fatal("mkmapping on different sized arrays.\n");
#endif

  m=allocate_mapping(MAP_SLOTS(ind->size));
  i=ITEM(ind);
  v=ITEM(val);
  for(e=0;e<ind->size;e++) mapping_insert(m, i++, v++);

  return m;
}

struct mapping *copy_mapping(struct mapping *m)
{
  INT32 e;
  struct mapping *n;
  struct keypair *k;

  n=allocate_mapping(MAP_SLOTS(m->size));

  MAPPING_LOOP(m) mapping_insert(n, &k->ind, &k->val);
  
  return n;
}

struct mapping *merge_mappings(struct mapping *a, struct mapping *b, INT32 op)
{
  struct array *ai, *av;
  struct array *bi, *bv;
  struct array *ci, *cv;
  INT32 *zipper;
  struct mapping *m;

  ai=mapping_indices(a);
  av=mapping_values(a);
  if(ai->size > 1)
  {
    zipper=get_set_order(ai);
    order_array(ai, zipper);
    order_array(av, zipper);
    free((char *)zipper);
  }

  bi=mapping_indices(b);
  bv=mapping_values(b);
  if(bi->size > 1)
  {
    zipper=get_set_order(bi);
    order_array(bi, zipper);
    order_array(bv, zipper);
    free((char *)zipper);
  }

  zipper=merge(ai,bi,op);

  ci=array_zip(ai,bi,zipper);
  free_array(ai);
  free_array(bi);

  cv=array_zip(av,bv,zipper);
  free_array(av);
  free_array(bv);
  
  free((char *)zipper);

  m=mkmapping(ci, cv);
  free_array(ci);
  free_array(cv);

  return m;
}

struct mapping *add_mappings(struct svalue *argp, INT32 args)
{
  INT32 e,d;
  struct mapping *ret;
  struct keypair *k;

  for(e=d=0;d<args;d++) e+=argp[d].u.mapping->size;
  ret=allocate_mapping(MAP_SLOTS(e));
  for(d=0;d<args;d++)
    MAPPING_LOOP(argp[d].u.mapping)
      mapping_insert(ret, &k->ind, &k->val);
  return ret;
}

int mapping_equal_p(struct mapping *a, struct mapping *b, struct processing *p)
{
  struct processing curr;
  struct keypair *k;
  INT32 e;

  if(a==b) return 1;
  if(a->size != b->size) return 0;

  curr.pointer_a = a;
  curr.pointer_b = b;
  curr.next = p;

  for( ;p ;p=p->next)
    if(p->pointer_a == (void *)a && p->pointer_b == (void *)b)
      return 1;

  check_mapping_for_destruct(a);
  check_mapping_for_destruct(b);
  
  MAPPING_LOOP(a)
  {
    struct svalue *s;
    if((s=low_mapping_lookup(b, & k->ind)))
    {
      if(!low_is_equal(s, &k->val, &curr)) return 0;
    }else{
      return 0;
    }
  }
  return 1;
}

void describe_mapping(struct mapping *m,struct processing *p,int indent)
{
  struct processing doing;
  INT32 e,d,q;
  struct keypair *k;
  char buf[40];

  if(! m->size)
  {
    my_strcat("([ ])");
    return;
  }

  doing.next=p;
  doing.pointer_a=(void *)m;
  for(e=0;p;e++,p=p->next)
  {
    if(p->pointer_a == (void *)m)
    {
      sprintf(buf,"@%ld",(long)e);
      my_strcat(buf);
      return;
    }
  }
  
  sprintf(buf, m->size == 1 ? "([ /* %ld element */\n" :
	                      "([ /* %ld elements */\n",
	  (long)m->size);
  my_strcat(buf);

  q=0;

  MAPPING_LOOP(m)
  {
    if(q)
    {
      my_putchar(',');
      my_putchar('\n');
    } else {
      q=1;
    }
    for(d=0; d<indent; d++) my_putchar(' ');
    describe_svalue(& k->ind, indent+2, &doing);
    my_putchar(':');
    describe_svalue(& k->val, indent+2, &doing);
  }

  my_putchar('\n');
  for(e=2; e<indent; e++) my_putchar(' ');
  my_strcat("])");
}

node * make_node_from_mapping(struct mapping *m)
{
  struct keypair *k;
  INT32 e;

  mapping_fix_type_field(m);

  if((m->ind_types | m->val_types) & (BIT_FUNCTION | BIT_OBJECT))
  {
    struct array *ind, *val;
    node *n;
    ind=mapping_indices(m);
    val=mapping_values(m);
    n=mkefuncallnode("mkmapping",
		     mknode(F_ARG_LIST,
			    make_node_from_array(ind),
			    make_node_from_array(val)));
    free_array(ind);
    free_array(val);
    return n;
  }else{
    struct svalue s;

    if(!m->size)
      return mkefuncallnode("aggregate_mapping",0);

    s.type=T_MAPPING;
    s.subtype=0;
    s.u.mapping=m;
    return mkconstantsvaluenode(&s);
  }
}

void f_m_delete(INT32 args)
{
  if(args < 2)
    error("Too few arguments to m_delete.\n");
  if(sp[-args].type != T_MAPPING)
    error("Bad argument 1 to m_delete.\n");

  map_delete(sp[-args].u.mapping,sp+1-args);
  pop_n_elems(args-1);
}

void f_aggregate_mapping(INT32 args)
{
  INT32 e;
  struct keypair *k;
  struct mapping *m;

  if(args & 1)
    error("Uneven number of arguments to aggregate_mapping.\n");

  m=allocate_mapping(MAP_SLOTS(args / 2));

  for(e=-args;e<0;e+=2) mapping_insert(m, sp+e, sp+e+1);
  pop_n_elems(args);
  push_mapping(m);
}

struct mapping *copy_mapping_recursively(struct mapping *m,
					 struct processing *p)
{
  struct processing doing;
  struct mapping *ret;
  INT32 e;
  struct keypair *k;

  doing.next=p;
  doing.pointer_a=(void *)m;
  for(;p;p=p->next)
  {
    if(p->pointer_a == (void *)m)
    {
      add_ref(ret=(struct mapping *)p->pointer_b);
      return ret;
    }
  }

  ret=allocate_mapping(MAP_SLOTS(m->size));
  ret->flags=m->flags;
  doing.pointer_b=ret;

  check_stack(2);

  MAPPING_LOOP(m)
  {
    copy_svalues_recursively_no_free(sp,&k->ind, 1, p);
    sp++;
    copy_svalues_recursively_no_free(sp,&k->val, 1, p);
    sp++;
    
    mapping_insert(ret, sp-2, sp-1);
    pop_n_elems(2);
  }

  return ret;
}


void mapping_search_no_free(struct svalue *to,
			    struct mapping *m,
			    struct svalue *look_for,
			    struct svalue *start)
{
  unsigned INT32 h;
  struct keypair *k;

  if(m->size)
  {
    h=0;
    k=m->hash[h];
    if(start)
    {
      h=hash_svalue(start) % m->hashsize;
      for(k=m->hash[h];k;k=k->next)
	if(is_eq(&k->ind, start))
	  break;
      
      if(!k)
      {
	to->type=T_INT;
	to->subtype=NUMBER_UNDEFINED;
	to->u.integer=0;
	return;
      }
      k=k->next;
    }
    
    while(h < (unsigned INT32)m->hashsize)
    {
      while(k)
      {
	if(is_eq(look_for, &k->val))
	{
	  assign_svalue_no_free(to,&k->ind);
	  return;
	}
	k=k->next;
      }
      k=m->hash[++h];
  }
  }

  to->type=T_INT;
  to->subtype=NUMBER_UNDEFINED;
  to->u.integer=0;
}


#ifdef PIKE_DEBUG

void check_mapping(struct mapping *m)
{
  int e,num;
  struct keypair *k;

  if(m->refs <=0)
    fatal("Mapping has zero refs.\n");

  if(m->next && m->next->prev != m)
    fatal("Mapping ->next->prev != mapping.\n");

  if(m->prev)
  {
    if(m->prev->next != m)
      fatal("Mapping ->prev->next != mapping.\n");
  }else{
    if(first_mapping != m)
      fatal("Mapping ->prev == 0 but first_mapping != mapping.\n");
  }

  if(m->hashsize < 0)
    fatal("Assert: I don't think he's going to make it Jim.\n");

  if(m->size < 0)
    fatal("Core breach, evacuate ship!\n");

  if(m->size > (m->hashsize + 3) * AVG_LINK_LENGTH)
    fatal("Pretty mean hashtable there buster!.\n");
  
  if(m->size > 0 && (!m->ind_types || !m->val_types))
    fatal("Mapping type fields are... wrong.\n");

  if(!m->hash && m->size)
    fatal("Hey! where did my hashtable go??\n");

  num=0;
  MAPPING_LOOP(m)
    {
      num++;

      if(! ( (1 << k->ind.type) & (m->ind_types) ))
	fatal("Mapping indices type field lies.\n");

      if(! ( (1 << k->val.type) & (m->val_types) ))
	fatal("Mapping values type field lies.\n");

      check_svalue(& k->ind);
      check_svalue(& k->val);
    }
  
  if(m->size != num)
    fatal("Shields are failing, hull integrity down to 20%%\n");
}

void check_all_mappings(void)
{
  struct mapping *m;
  for(m=first_mapping;m;m=m->next)
    check_mapping(m);
}
#endif


void gc_mark_mapping_as_referenced(struct mapping *m)
{
  INT32 e;
  struct keypair *k;

  if(gc_mark(m))
  {
    if((m->ind_types | m->val_types) & BIT_COMPLEX)
    {
      MAPPING_LOOP(m)
      {
	/* We do not want to count this key:index pair if
	 * the index is a destructed object or function
	 */
	if(((1 << k->ind.type) & (BIT_OBJECT | BIT_FUNCTION)) &&
	   !(k->ind.u.object->prog))
	  continue;

	if (m->flags & MAPPING_FLAG_WEAK)
	{
	  if (k->ind.type == T_OBJECT &&
	      k->ind.u.object->prog->flags & PROGRAM_NO_WEAK_FREE)
	    gc_mark_svalues(&k->ind, 1);
	  if (k->val.type == T_OBJECT && k->val.u.object->prog &&
	      k->val.u.object->prog->flags & PROGRAM_NO_WEAK_FREE)
	    gc_mark_svalues(&k->val, 1);
	}
	else {
	  gc_mark_svalues(&k->ind, 1);
	  gc_mark_svalues(&k->val, 1);
	}
      }
    }
  }
}

void gc_check_all_mappings(void)
{
  INT32 e;
  struct keypair *k;
  struct mapping *m;

  for(m=first_mapping;m;m=m->next)
  {
    if((m->ind_types | m->val_types) & BIT_COMPLEX)
    {
      MAPPING_LOOP(m)
      {
	/* We do not want to count this key:index pair if
	 * the index is a destructed object or function
	 */
	if(((1 << k->ind.type) & (BIT_OBJECT | BIT_FUNCTION)) &&
	   !(k->ind.u.object->prog))
	  continue;
	  
	debug_gc_check_svalues(&k->ind, 1, T_MAPPING, m);
	m->val_types |= debug_gc_check_svalues(&k->val, 1, T_MAPPING, m);
      }

#ifdef PIKE_DEBUG
      if(d_flag > 1) check_mapping_type_fields(m);
#endif

    }
  }
}

void gc_mark_all_mappings(void)
{
  struct mapping *m;
  for(m=first_mapping;m;m=m->next)
    if(gc_is_referenced(m))
      gc_mark_mapping_as_referenced(m);
}

void gc_free_all_unreferenced_mappings(void)
{
  INT32 e;
  struct keypair *k,**prev;
  struct mapping *m,*next;

  for(m=first_mapping;m;m=next)
  {
    check_mapping_for_destruct(m);
    if(gc_do_free(m))
    {
      add_ref(m);

      for(e=0;e<m->hashsize;e++)
      {
	k=m->hash[e];
	while(k)
	{
	  free_svalue(&k->ind);
	  free_svalue(&k->val);

	  if(k->next)
	  {
	    k = k->next;
	  } else {
	    k->next=m->free_list;
	    m->free_list=m->hash[e];
	    m->hash[e]=0;
	    break;
	  }
	}
      }
      m->size=0;

      next=m->next;

      free_mapping(m);
    }
    else if(m->flags & MAPPING_FLAG_WEAK)
    {
      add_ref(m);
      for(e=0;e<m->hashsize;e++)
      {
	for(prev= m->hash + e;(k=*prev);)
	{
	  if((k->val.type <= MAX_COMPLEX &&
	      !(k->val.type == T_OBJECT &&
		k->val.u.object->prog->flags & PROGRAM_NO_WEAK_FREE) &&
	      gc_do_free(k->val.u.refs)) ||
	     (k->ind.type <= MAX_COMPLEX &&
	      !(k->ind.type == T_OBJECT &&
		k->ind.u.object->prog->flags & PROGRAM_NO_WEAK_FREE) &&
	      gc_do_free(k->ind.u.refs)))
	  {
	    *prev=k->next;
	    free_svalue(& k->ind);
	    free_svalue(& k->val);
	    k->next=m->free_list;
	    m->free_list=k;
	    m->size--;
	  }else{
	    prev=&k->next;
	  }
	}
      }
      if(MAP_SLOTS(m->size) < m->hashsize * MIN_LINK_LENGTH)
	rehash(m, MAP_SLOTS(m->size));
      next=m->next;
      free_mapping(m);
    }
    else
    {
      next=m->next;
    }
  }
}

#ifdef PIKE_DEBUG

void simple_describe_mapping(struct mapping *m)
{
  char *s;
  init_buf();
  describe_mapping(m,0,2);
  s=simple_free_buf();
  fprintf(stderr,"%s\n",s);
  free(s);
}


void debug_dump_mapping(struct mapping *m)
{
  fprintf(stderr,"Refs=%d, next=%p, prev=%p, size=%d, hashsize=%d\n",
	  m->refs,
	  m->next,
	  m->prev,
	  m->size,
	  m->hashsize);
  fprintf(stderr,"Indices type field = ");
  debug_dump_type_field(m->ind_types);
  fprintf(stderr,"\n");
  fprintf(stderr,"Values type field = ");
  debug_dump_type_field(m->val_types);
  fprintf(stderr,"\n");
  simple_describe_mapping(m);
}
#endif

void zap_all_mappings(void)
{
  INT32 e;
  struct keypair *k;
  struct mapping *m,*next;

  for(m=first_mapping;m;m=next)
  {
    add_ref(m);

#if defined(PIKE_DEBUG) && defined(DEBUG_MALLOC)
    if(verbose_debug_exit)
      debug_dump_mapping(m);
#endif

    for(e=0;e<m->hashsize;e++)
    {
      while((k=m->hash[e]))
      {
	m->hash[e]=k->next;
	k->next=m->free_list;
	m->free_list=k;
	free_svalue(&k->ind);
	free_svalue(&k->val);
      }
    }
    m->size=0;
    
    next=m->next;
    
    /* free_mapping(m); */
  }
}

void count_memory_in_mappings(INT32 *num_, INT32 *size_)
{
  INT32 num=0, size=0;
  struct mapping *m;
  for(m=first_mapping;m;m=m->next)
  {
    struct keypair *k;
    num++;
    size+=sizeof(struct mapping)+
      sizeof(struct keypair *) * m->hashsize+
      sizeof(struct keypair) *  m->size;

    for(k=m->free_list;k;k=k->next)
      size+=sizeof(struct keypair);
  }

  *num_=num;
  *size_=size;
}
