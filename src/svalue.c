/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
/**/
#include "global.h"
#include "main.h"
#include "svalue.h"
#include "stralloc.h"
#include "array.h"
#include "mapping.h"
#include "multiset.h"
#include "object.h"
#include "program.h"
#include "constants.h"
#include "error.h"
#include "dynamic_buffer.h"
#include "interpret.h"
#include "gc.h"
#include "pike_macros.h"
#include <ctype.h>
#include "queue.h"
#include "bignum.h"

RCSID("$Id: svalue.c,v 1.79 2000/07/02 14:54:07 grubba Exp $");

struct svalue dest_ob_zero = { T_INT, 0 };

/*
 * This routine frees a short svalue given a pointer to it and
 * its type.
 */

void really_free_short_svalue(union anything *s, TYPE_T type)
{
  union anything tmp=*s;
  s->refs=0; /* Prevent cyclic calls */
  switch(type)
  {
    case T_ARRAY:
      really_free_array(tmp.array);
      break;
      
    case T_MAPPING:
      really_free_mapping(tmp.mapping);
      break;
      
    case T_MULTISET:
      really_free_multiset(tmp.multiset);
      break;
      
    case T_OBJECT:
      schedule_really_free_object(tmp.object);
      break;
      
    case T_PROGRAM:
      really_free_program(tmp.program);
      break;
      
    case T_STRING:
      really_free_string(tmp.string);
      break;
      
#ifdef PIKE_DEBUG
    default:
	fatal("Bad type in free_short_svalue.\n");
#endif
  }
}

void really_free_svalue(struct svalue *s)
{
  int tmp=s->type;
  s->type=T_INT;
  switch(tmp)
  {
  case T_ARRAY:
    really_free_array(s->u.array);
#ifdef PIKE_DEBUG
    s->type = 99;
#endif
    break;
    
  case T_MAPPING:
    really_free_mapping(s->u.mapping);
#ifdef PIKE_DEBUG
    s->type = 99;
#endif
    break;
    
  case T_MULTISET:
    really_free_multiset(s->u.multiset);
#ifdef PIKE_DEBUG
    s->type = 99;
#endif
    break;
    
  case T_FUNCTION:
    if(s->subtype == FUNCTION_BUILTIN)
    {
      really_free_callable(s->u.efun);
      break;
    }
    /* fall through */
    
  case T_OBJECT:
    schedule_really_free_object(s->u.object);
    break;
    
  case T_PROGRAM:
    really_free_program(s->u.program);
#ifdef PIKE_DEBUG
    s->type = 99;
#endif
    break;
    
  case T_TYPE:
  case T_STRING:
    really_free_string(s->u.string);
#ifdef PIKE_DEBUG
    s->type = 99;
#endif
    break;
    
#ifdef PIKE_DEBUG
  default:
    fatal("Bad type in free_svalue.\n");
#endif
  }
}

void do_free_svalue(struct svalue *s)
{
  free_svalue(s);
}

/* Free a bunch of normal svalues.
 * We put this routine here so the compiler can optimize the call
 * inside the loop if it wants to
 */
void debug_free_svalues(struct svalue *s, size_t num, INT32 type_hint DMALLOC_LINE_ARGS)
{
  switch(type_hint)
  {
  case 0:
  case BIT_INT:
  case BIT_FLOAT:
  case BIT_FLOAT | BIT_INT:
    return;

#define DOTYPE(X,Y,Z) case X:						\
   while(num--) {							\
    DO_IF_DMALLOC(debug_malloc_update_location(s->u.Z, dmalloc_location));	\
    Y(s->u.Z);								\
    DO_IF_DMALLOC(s->u.Z=(void *)-1);					\
    s++;								\
   }return

    DOTYPE(BIT_STRING, free_string, string);
    DOTYPE(BIT_ARRAY, free_array, array);
    DOTYPE(BIT_MAPPING, free_mapping, mapping);
    DOTYPE(BIT_MULTISET, free_multiset, multiset);
    DOTYPE(BIT_OBJECT, free_object, object);
    DOTYPE(BIT_PROGRAM, free_program, program);

   case   3: case   5: case   6: case   7: case   9: case  10: case  11:
   case  12: case  13: case  14: case  15: case  17: case  18: case  19:
   case  20: case  21: case  22: case  23: case  24: case  25: case  26:
   case  27: case  28: case  29: case  30: case  31: case  33: case  34:
   case  35: case  36: case  37: case  38: case  39: case  40: case  41:
   case  42: case  43: case  44: case  45: case  46: case  47: case  48:
   case  49: case  50: case  51: case  52: case  53: case  54: case  55:
   case  56: case  57: case  58: case  59: case  60: case  61: case  62:
   case  63: case  65: case  66: case  67: case  68: case  69: case  70:
   case  71: case  72: case  73: case  74: case  75: case  76: case  77:
   case  78: case  79: case  80: case  81: case  82: case  83: case  84:
   case  85: case  86: case  87: case  88: case  89: case  90: case  91:
   case  92: case  93: case  94: case  95: case  96: case  97: case  98:
   case  99: case 100: case 101: case 102: case 103: case 104: case 105:
   case 106: case 107: case 108: case 109: case 110: case 111: case 112:
   case 113: case 114: case 115: case 116: case 117: case 118: case 119:
   case 120: case 121: case 122: case 123: case 124: case 125: case 126:
   case 127: case 129: case 130: case 131: case 132: case 133: case 134:
   case 135: case 136: case 137: case 138: case 139: case 140: case 141:
   case 142: case 143: case 144: case 145: case 146: case 147: case 148:
   case 149: case 150: case 151: case 152: case 153: case 154: case 155:
   case 156: case 157: case 158: case 159: case 160: case 161: case 162:
   case 163: case 164: case 165: case 166: case 167: case 168: case 169:
   case 170: case 171: case 172: case 173: case 174: case 175: case 176:
   case 177: case 178: case 179: case 180: case 181: case 182: case 183:
   case 184: case 185: case 186: case 187: case 188: case 189: case 190:
   case 191: case 192: case 193: case 194: case 195: case 196: case 197:
   case 198: case 199: case 200: case 201: case 202: case 203: case 204:
   case 205: case 206: case 207: case 208: case 209: case 210: case 211:
   case 212: case 213: case 214: case 215: case 216: case 217: case 218:
   case 219: case 220: case 221: case 222: case 223: case 224: case 225:
   case 226: case 227: case 228: case 229: case 230: case 231: case 232:
   case 233: case 234: case 235: case 236: case 237: case 238: case 239:
   case 240: case 241: case 242: case 243: case 244: case 245: case 246:
   case 247: case 248: case 249: case 250: case 251: case 252: case 253:
   case 254: case 255:

    while(num--)
    {
#ifdef DEBUG_MALLOC
      debug_malloc_update_location(s->u.refs  DMALLOC_PROXY_ARGS);
#endif
      if(--s->u.refs[0]<=0)
      {
	really_free_svalue(s);
	DO_IF_DMALLOC(s->u.refs=0);
      }
      s++;
    }
    break;

  case BIT_FUNCTION:
    while(num--)
    {
#ifdef DEBUG_MALLOC
      debug_malloc_update_location(s->u.refs  DMALLOC_PROXY_ARGS);
#endif
      if(--s->u.refs[0] <= 0)
      {
	if(s->subtype == FUNCTION_BUILTIN)
	  really_free_callable(s->u.efun);
	else
	  schedule_really_free_object(s->u.object);
	DO_IF_DMALLOC(s->u.refs=0);
      }
      s++;
    }
    return;

#undef DOTYPE
  default:
    while(num--)
      {
#ifdef DEBUG_MALLOC
	if(s->type <= MAX_REF_TYPE)
	  debug_malloc_update_location(s->u.refs  DMALLOC_PROXY_ARGS);
#endif
	free_svalue(s++);
      }
  }
}

void assign_svalues_no_free(struct svalue *to,
			    struct svalue *from,
			    size_t num,
			    INT32 type_hint)
{
#ifdef PIKE_DEBUG
  if(d_flag)
  {
    size_t e;
    for(e=0;e<num;e++)
      if(!(type_hint & (1<<from[e].type)))
	 fatal("Type hint lies (%ld %ld %d)!\n",(long)e,(long)type_hint,from[e].type);
  }
#endif
  if((type_hint & ((2<<MAX_REF_TYPE)-1)) == 0)
  {
    MEMCPY((char *)to, (char *)from, sizeof(struct svalue) * num);
    return;
  }

  if((type_hint & ((2<<MAX_REF_TYPE)-1)) == type_hint)
  {
    while(num--)
    {
      struct svalue tmp;
      tmp=*(from++);
      *(to++)=tmp;
      add_ref( tmp.u.array );
    }
    return;
  }

  while(num--) assign_svalue_no_free(to++,from++);
}

void assign_svalues(struct svalue *to,
		    struct svalue *from,
		    size_t num,
		    TYPE_FIELD types)
{
  free_svalues(to,num,BIT_MIXED);
  assign_svalues_no_free(to,from,num,types);
}

void assign_to_short_svalue(union anything *u,
			    TYPE_T type,
			    struct svalue *s)
{
  check_type(s->type);
  check_refs(s);

  if(s->type == type)
  {
    INT32 *tmp;
    switch(type)
    {
      case T_INT: u->integer=s->u.integer; break;
      case T_FLOAT: u->float_number=s->u.float_number; break;
      default:
	if(u->refs && --*(u->refs) <= 0) really_free_short_svalue(u,type);
	u->refs = tmp = s->u.refs;
	tmp[0]++;
    }
  }else if(type<=MAX_REF_TYPE && IS_ZERO(s)){
    if(u->refs && --*(u->refs) <= 0) really_free_short_svalue(u,type);
    u->refs=0;
  }else{
    error("Wrong type in assignment, expected %s, got %s.\n",
	  get_name_of_type(type),
	  get_name_of_type(s->type));
  }
}

void assign_to_short_svalue_no_free(union anything *u,
				    TYPE_T type,
				    struct svalue *s)
{
  check_type(s->type);
  check_refs(s);

  if(s->type == type)
  {
    INT32 *tmp;
    switch(type)
    {
      case T_INT: u->integer=s->u.integer; break;
      case T_FLOAT: u->float_number=s->u.float_number; break;
      default:
	u->refs = tmp = s->u.refs;
	tmp[0]++;
    }
  }else if(type<=MAX_REF_TYPE && IS_ZERO(s)){
    u->refs=0;
  }else{
    error("Wrong type in assignment, expected %s, got %s.\n",
	  get_name_of_type(type),
	  get_name_of_type(s->type));
  }
}


void assign_from_short_svalue_no_free(struct svalue *s,
				      union anything *u,
				      TYPE_T type)
{
  check_type(type);
  check_refs2(u,type);

  s->type = type;
  s->subtype = 0;

  switch(type)
  {
    case T_FLOAT: s->u.float_number=u->float_number; break;
    case T_INT: s->u.integer=u->integer; break;
    default:
      if((s->u.refs=u->refs))
      {
	add_ref( u->array );
      }else{
	s->type=T_INT;
	s->subtype=NUMBER_NUMBER;
	s->u.integer=0;
      }
  }
}

void assign_short_svalue_no_free(union anything *to,
				 union anything *from,
				 TYPE_T type)
{
  INT32 *tmp;
  check_type(type);
  check_refs2(from,type);

  switch(type)
  {
    case T_INT: to->integer=from->integer; break;
    case T_FLOAT: to->float_number=from->float_number; break;
    default:
      to->refs = tmp = from->refs;
      if(tmp) tmp[0]++;
  }
}

void assign_short_svalue(union anything *to,
			 union anything *from,
			 TYPE_T type)
{
  INT32 *tmp;
  check_type(type);
  check_refs2(from,type);

  switch(type)
  {
    case T_INT: to->integer=from->integer; break;
    case T_FLOAT: to->float_number=from->float_number; break;
    default:
      if(to->refs && --*(to->refs) <= 0) really_free_short_svalue(to,type);
      to->refs = tmp = from->refs;
      if(tmp) tmp[0]++;
  }
}

unsigned INT32 hash_svalue(struct svalue *s)
{
  unsigned INT32 q;

  check_type(s->type);
  check_refs(s);

  switch(s->type)
  {
  case T_OBJECT:
    if(!s->u.object->prog)
    {
      q=0;
      break;
    }

    if(FIND_LFUN(s->u.object->prog,LFUN___HASH) != -1)
    {
      safe_apply_low(s->u.object, FIND_LFUN(s->u.object->prog,LFUN___HASH), 0);
      if(sp[-1].type == T_INT)
      {
	q=sp[-1].u.integer;
      }else{
	q=0;
      }
      pop_stack();
      break;
    }

  default:      q=(unsigned INT32)((long)s->u.refs >> 2); break;
  case T_INT:   q=s->u.integer; break;
  case T_FLOAT: q=(unsigned INT32)(s->u.float_number * 16843009.731757771173); break;
  }
  q+=q % 997;
  q+=((q + s->type) * 9248339);
  
  return q;
}

int svalue_is_true(struct svalue *s)
{
  unsigned INT32 q;
  check_type(s->type);
  check_refs(s);

  switch(s->type)
  {
  case T_INT:
    if(s->u.integer) return 1;
    return 0;

  case T_FUNCTION:
    if(!s->u.object->prog) return 0;
    return 1;

  case T_OBJECT:
    if(!s->u.object->prog) return 0;

    if(FIND_LFUN(s->u.object->prog,LFUN_NOT)!=-1)
    {
      safe_apply_low(s->u.object,FIND_LFUN(s->u.object->prog,LFUN_NOT),0);
      if(sp[-1].type == T_INT && sp[-1].u.integer == 0)
      {
	pop_stack();
	return 1;
      } else {
	pop_stack();
	return 0;
      }
    }

  default:
    return 1;
  }
    
}

#define TWO_TYPES(X,Y) (((X)<<8)|(Y))

int is_identical(struct svalue *a, struct svalue *b)
{
  if(a->type != b->type) return 0;
  switch(a->type)
  {
  case T_TYPE:
  case T_STRING:
  case T_OBJECT:
  case T_MULTISET:
  case T_PROGRAM:
  case T_ARRAY:
  case T_MAPPING:
    return a->u.refs == b->u.refs;

  case T_INT:
    return a->u.integer == b->u.integer;

  case T_FUNCTION:
    return (a->subtype == b->subtype && a->u.object == b->u.object);
      
  case T_FLOAT:
    return a->u.float_number == b->u.float_number;

  default:
    fatal("Unknown type %x\n",a->type);
    return 0; /* make gcc happy */
  }

}

int is_eq(struct svalue *a, struct svalue *b)
{
  check_type(a->type);
  check_type(b->type);
  check_refs(a);
  check_refs(b);

  safe_check_destructed(a);
  safe_check_destructed(b);

  if (a->type != b->type)
  {
    switch(TWO_TYPES((1<<a->type),(1<<b->type)))
    {
    case TWO_TYPES(BIT_FUNCTION,BIT_PROGRAM):
      return program_from_function(a) == b->u.program;
    
    case TWO_TYPES(BIT_PROGRAM,BIT_FUNCTION):
      return program_from_function(b) == a->u.program;

    case TWO_TYPES(BIT_OBJECT, BIT_ARRAY):
    case TWO_TYPES(BIT_OBJECT, BIT_MAPPING):
    case TWO_TYPES(BIT_OBJECT, BIT_MULTISET):
    case TWO_TYPES(BIT_OBJECT, BIT_OBJECT):
    case TWO_TYPES(BIT_OBJECT, BIT_FUNCTION):
    case TWO_TYPES(BIT_OBJECT, BIT_PROGRAM):
    case TWO_TYPES(BIT_OBJECT, BIT_STRING):
    case TWO_TYPES(BIT_OBJECT, BIT_INT):
    case TWO_TYPES(BIT_OBJECT, BIT_FLOAT):
      if(FIND_LFUN(a->u.object->prog,LFUN_EQ) != -1)
      {
      a_is_obj:
	assign_svalue_no_free(sp, b);
	sp++;
	apply_lfun(a->u.object, LFUN_EQ, 1);
	if(IS_ZERO(sp-1))
	{
	  pop_stack();
	  return 0;
	}else{
	  pop_stack();
	  return 1;
	}
      }
    if(b->type != T_OBJECT) return 0;

    case TWO_TYPES(BIT_ARRAY,BIT_OBJECT):
    case TWO_TYPES(BIT_MAPPING,BIT_OBJECT):
    case TWO_TYPES(BIT_MULTISET,BIT_OBJECT):
    case TWO_TYPES(BIT_FUNCTION,BIT_OBJECT):
    case TWO_TYPES(BIT_PROGRAM,BIT_OBJECT):
    case TWO_TYPES(BIT_STRING,BIT_OBJECT):
    case TWO_TYPES(BIT_INT,BIT_OBJECT):
    case TWO_TYPES(BIT_FLOAT,BIT_OBJECT):
      if(FIND_LFUN(b->u.object->prog,LFUN_EQ) != -1)
      {
      b_is_obj:
	assign_svalue_no_free(sp, a);
	sp++;
	apply_lfun(b->u.object, LFUN_EQ, 1);
	if(IS_ZERO(sp-1))
	{
	  pop_stack();
	  return 0;
	}else{
	  pop_stack();
	  return 1;
	}
      }
    }

    return 0;
  }
  switch(a->type)
  {
  case T_OBJECT:
    if(FIND_LFUN(a->u.object->prog,LFUN_EQ) != -1)
      goto a_is_obj;

    if(FIND_LFUN(b->u.object->prog,LFUN_EQ) != -1)
      goto b_is_obj;

  case T_MULTISET:
  case T_PROGRAM:
  case T_ARRAY:
  case T_MAPPING:
    return a->u.refs == b->u.refs;

  case T_INT:
    return a->u.integer == b->u.integer;

  case T_STRING:
    return is_same_string(a->u.string,b->u.string);

  case T_TYPE:
    return pike_types_le(a->u.string, b->u.string) &&
      pike_types_le(b->u.string, a->u.string);

  case T_FUNCTION:
    return (a->subtype == b->subtype && a->u.object == b->u.object);
      
  case T_FLOAT:
    return a->u.float_number == b->u.float_number;

  default:
    fatal("Unknown type %x\n",a->type);
    return 0; /* make gcc happy */
  }
}

int low_is_equal(struct svalue *a,
		 struct svalue *b,
		 struct processing *p)
{
  check_type(a->type);
  check_type(b->type);
  check_refs(a);
  check_refs(b);

  if(a->type == T_OBJECT && a->u.object->prog)
  {
    int f=FIND_LFUN(a->u.object->prog, LFUN__EQUAL);
    if(f != -1)
    {
      push_svalue(b);
      apply_lfun(a->u.object, f, 1);
      if(IS_ZERO(sp-1)) 
      {
	pop_stack();
	return 0;
      }else{
	pop_stack();
	return 1;
      }
    }
  }

  if(b->type == T_OBJECT && b->u.object->prog)
  {
    int f=FIND_LFUN(b->u.object->prog, LFUN__EQUAL);
    if(f != -1)
    {
      push_svalue(a);
      apply_lfun(b->u.object, f, 1);
      if(IS_ZERO(sp-1)) 
      {
	pop_stack();
	return 0;
      }else{
	pop_stack();
	return 1;
      }
    }
  }

  if(is_eq(a,b)) return 1;

  if(a->type != b->type) return 0;

  switch(a->type)
  {
    case T_INT:
    case T_STRING:
    case T_FLOAT:
    case T_FUNCTION:
    case T_PROGRAM:
      return 0;

    case T_OBJECT:
      return object_equal_p(a->u.object, b->u.object, p);

    case T_ARRAY:
      check_array_for_destruct(a->u.array);
      check_array_for_destruct(b->u.array);
      return array_equal_p(a->u.array, b->u.array, p);

    case T_MAPPING:
      return mapping_equal_p(a->u.mapping, b->u.mapping, p);

    case T_MULTISET:
      return multiset_equal_p(a->u.multiset, b->u.multiset, p);
      
    default:
      fatal("Unknown type in is_equal.\n");
  }
  return 1; /* survived */
}

int low_short_is_equal(const union anything *a,
		       const union anything *b,
		       TYPE_T type,
		       struct processing *p)
{
  struct svalue sa,sb;

  check_type(type);
  check_refs2(a,type);
  check_refs2(b,type);

  switch(type)
  {
    case T_INT: return a->integer == b->integer;
    case T_FLOAT: return a->float_number == b->float_number;
  }

  if((sa.u.refs=a->refs))
  {
    sa.type=type;
  }else{
    sa.type=T_INT;
    sa.u.integer=0;
  }

  if((sb.u.refs=a->refs))
  {
    sb.type=type;
  }else{
    sb.type=T_INT;
    sa.u.integer=0;
  }

  return low_is_equal(&sa,&sb,p);
}

int is_equal(struct svalue *a,struct svalue *b)
{
  return low_is_equal(a,b,0);
}

int is_lt(struct svalue *a,struct svalue *b)
{
  check_type(a->type);
  check_type(b->type);
  check_refs(a);
  check_refs(b);

  safe_check_destructed(a);
  safe_check_destructed(b);

  if (((a->type == T_TYPE) ||
       (a->type == T_FUNCTION) || (a->type == T_PROGRAM)) &&
      ((b->type == T_FUNCTION) ||
       (b->type == T_PROGRAM) || (b->type == T_TYPE))) {
    if (a->type != T_TYPE) {
      /* Try converting a to a program, and then to a type. */
      struct svalue aa;
      int res;
      aa.u.program = program_from_svalue(a);
      if (!aa.u.program) {
	error("Bad argument to comparison.");
      }
      type_stack_mark();
      push_type_int(aa.u.program->id);
      push_type(0);
      push_type(T_OBJECT);
      aa.u.string = pop_unfinished_type();
      aa.type = T_TYPE;
      res = is_lt(&aa, b);
      free_string(aa.u.string);
      return res;
    }
    if (b->type != T_TYPE) {
      /* Try converting b to a program, and then to a type. */
      struct svalue bb;
      int res;
      bb.u.program = program_from_svalue(b);
      if (!bb.u.program) {
	error("Bad argument to comparison.");
      }
      type_stack_mark();
      push_type_int(bb.u.program->id);
      push_type(0);
      push_type(T_OBJECT);
      bb.u.string = pop_unfinished_type();
      bb.type = T_TYPE;
      res = is_lt(a, &bb);
      free_string(bb.u.string);
      return res;
    }

    /* At this point both a and b have type T_TYPE */
#ifdef PIKE_DEBUG
    if ((a->type != T_TYPE) || (b->type != T_TYPE)) {
      fatal("Unexpected types in comparison.\n");
    }
#endif /* PIKE_DEBUG */
    return !pike_types_le(b->u.string, a->u.string);
  }

  if (a->type != b->type)
  {
    if(a->type == T_FLOAT && b->type==T_INT)
      return a->u.float_number < (FLOAT_TYPE)b->u.integer;

    if(a->type == T_INT && b->type==T_FLOAT)
      return (FLOAT_TYPE)a->u.integer < b->u.float_number;

    if(a->type == T_OBJECT)
    {
    a_is_object:
      if(!a->u.object->prog)
	error("Comparison on destructed object.\n");
      if(FIND_LFUN(a->u.object->prog,LFUN_LT) == -1)
	error("Object lacks `<\n");
      assign_svalue_no_free(sp, b);
      sp++;
      apply_lfun(a->u.object, LFUN_LT, 1);
      if(IS_ZERO(sp-1))
      {
	pop_stack();
	return 0;
      }else{
	pop_stack();
	return 1;
      }
    }

    if(b->type == T_OBJECT)
    {
      if(!b->u.object->prog)
	error("Comparison on destructed object.\n");
      if(FIND_LFUN(b->u.object->prog,LFUN_GT) == -1)
	error("Object lacks `>\n");
      assign_svalue_no_free(sp, a);
      sp++;
      apply_lfun(b->u.object, LFUN_GT, 1);
      if(IS_ZERO(sp-1))
      {
	pop_stack();
	return 0;
      }else{
	pop_stack();
	return 1;
      }
    }
    
    error("Cannot compare different types.\n");
  }
  switch(a->type)
  {
  case T_OBJECT:
    goto a_is_object;

  default:
    error("Bad type to comparison.\n");

  case T_INT:
    return a->u.integer < b->u.integer;

  case T_STRING:
    return my_strcmp(a->u.string, b->u.string) < 0;

  case T_FLOAT:
    return a->u.float_number < b->u.float_number;

  }
}

void describe_svalue(struct svalue *s,int indent,struct processing *p)
{
  char buf[50];

  check_type(s->type);
  check_refs(s);

  indent+=2;
  switch(s->type)
  {
    case T_LVALUE:
      my_strcat("lvalue");
      break;

    case T_INT:
      sprintf(buf,"%ld",(long)s->u.integer);
      my_strcat(buf);
      break;

    case T_TYPE:
      low_describe_type(s->u.string->str);
      break;

    case T_STRING:
      {
	int i,j=0;
        my_putchar('"');
	for(i=0; i < s->u.string->len; i++)
        {
          switch(j=index_shared_string(s->u.string,i))
          {
	  case '\n':
	    my_putchar('\\');
	    my_putchar('n');
	    break;

	  case '\t':
	    my_putchar('\\');
	    my_putchar('t');
	    break;

	  case '\b':
	    my_putchar('\\');
	    my_putchar('b');
	    break;

	  case '\r':
	    my_putchar('\\');
	    my_putchar('r');
	    break;


            case '"':
            case '\\':
              my_putchar('\\');
              my_putchar(j);
	      break;

            default:
	      if(j>=0 && j<256 && isprint(j))
	      {
		my_putchar(j);
		break;
	      }

	      my_putchar('\\');
	      sprintf(buf,"%o",j);
	      my_strcat(buf);

	      switch(index_shared_string(s->u.string,i+1))
	      {
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
		case '8': case '9':
		  my_putchar('"');
		  my_putchar('"');
	      }
	      break;
          } 
        }
        my_putchar('"');
      }
      break;


    case T_FUNCTION:
      /* FIXME: What if the functionname is a wide-string?
       * /grubba 1999-10-21
       */
      if(s->subtype == FUNCTION_BUILTIN)
      {
	my_binary_strcat(s->u.efun->name->str,s->u.efun->name->len);
      }else{
	if(s->u.object->prog)
	{
	  struct pike_string *name;
	  name=ID_FROM_INT(s->u.object->prog,s->subtype)->name;
	  my_binary_strcat(name->str,name->len);
	}else{
	  my_strcat("0");
	}
      }
      break;

    case T_OBJECT:
      if(s->u.object->prog)
      {
	int fun=FIND_LFUN(s->u.object->prog, LFUN__SPRINTF);
	if(fun != -1)
	{
	  /* We require some tricky coding to make this work
	   * with tracing...
	   */
	  int save_t_flag=t_flag;
	  string save_buffer=complex_free_buf();

	  t_flag=0;
	  

	  push_int('O');
	  push_constant_text("indent");
	  push_int(indent);
	  f_aggregate_mapping(2);					      
	  safe_apply_low(s->u.object, fun ,2);

	  if(!IS_ZERO(sp-1))
	  {
	    struct pike_string *str;
	    int i;
	    if(sp[-1].type != T_STRING)
	    {
	      pop_stack();
	      push_text("(object returned illegal value from _sprintf)");
	    }

	    init_buf_with_string(save_buffer);
	    t_flag=save_t_flag;

	    str=sp[-1].u.string;
	    
	    switch(str->size_shift)
	    {
	      case 0:
		my_binary_strcat((char *)STR0(str), str->len);
		break;

	      case 1:
	      {
		p_wchar1 *cp=STR1(str);
		for(i=0;i<str->len;i++)
		{
		  int c=cp[i];
		  if(c<256) 
		    my_putchar(c);
		  else
		  {
		    sprintf(buf,"<%d>",c);
		    my_strcat(buf);
		  }
		}
		  break;
	      }

	      case 2:
	      {
		p_wchar2 *cp=STR2(str);
		for(i=0;i<str->len;i++)
		{
		  int c=cp[i];
		  if(c<256) 
		    my_putchar(c);
		  else
		  {
		    sprintf(buf,"<%d>",c);
		    my_strcat(buf);
		  }
		}
		break;
	      }
	    }
	    pop_stack();
	    break;
	  }

	  init_buf_with_string(save_buffer);
	  t_flag=save_t_flag;
	  pop_stack();
	}
      }
      
      my_strcat("object");
      break;

    case T_PROGRAM:
      my_strcat("program");
      break;

    case T_FLOAT:
      sprintf(buf,"%f",(double)s->u.float_number);
      my_strcat(buf);
      break;

    case T_ARRAY:
      describe_array(s->u.array, p, indent);
      break;

    case T_MULTISET:
      describe_multiset(s->u.multiset, p, indent);
      break;

    case T_MAPPING:
      describe_mapping(s->u.mapping, p, indent);
      break;

    default:
      sprintf(buf,"<Unknown %d>",s->type);
      my_strcat(buf);
  }
}

void print_svalue (FILE *out, struct svalue *s)
{
  string orig_str;
  string str;
  orig_str = complex_free_buf();
  init_buf();
  describe_svalue (s, 0, 0);
  str = complex_free_buf();
  if (orig_str.str) init_buf_with_string (orig_str);
  fwrite (str.str, str.len, 1, out);
  free (str.str);
}

/* NOTE: Must handle num being negative. */
void clear_svalues(struct svalue *s, ptrdiff_t num)
{
  struct svalue dum;
  dum.type=T_INT;
  dum.subtype=NUMBER_NUMBER;
  dum.u.refs=0;
  dum.u.integer=0;
  while(num-- > 0) *(s++)=dum;
}

/* NOTE: Must handle num being negative. */
void clear_svalues_undefined(struct svalue *s, ptrdiff_t num)
{
  struct svalue dum;
  dum.type=T_INT;
  dum.subtype=NUMBER_UNDEFINED;
  dum.u.refs=0;
  dum.u.integer=0;
  while(num-- > 0) *(s++)=dum;
}

void copy_svalues_recursively_no_free(struct svalue *to,
				      struct svalue *from,
				      size_t num,
				      struct processing *p)
{
  while(num--)
  {
    check_type(from->type);
    check_refs(from);

    switch(from->type)
    {
    default:
      *to=*from;
      if(from->type <= MAX_REF_TYPE) add_ref(from->u.array);
      break;

    case T_ARRAY:
      to->u.array=copy_array_recursively(from->u.array,p);
      to->type=T_ARRAY;
      break;

    case T_MAPPING:
      to->u.mapping=copy_mapping_recursively(from->u.mapping,p);
      to->type=T_MAPPING;
      break;

    case T_MULTISET:
      to->u.multiset=copy_multiset_recursively(from->u.multiset,p);
      to->type=T_MULTISET;
      break;
    }
    to++;
    from++;
  }
}

#ifdef PIKE_DEBUG
static void low_check_short_svalue(union anything *u, TYPE_T type)
{
  static int inside=0;

  check_type(type);
  if ((type > MAX_REF_TYPE)||(!u->refs)) return;

  switch(type)
  {
  case T_STRING:
    if(!debug_findstring(u->string))
      fatal("Shared string not shared!\n");
    break;

  default:
    if(d_flag > 50)
    {
      if(inside) return;
      inside=1;

      switch(type)
      {
      case T_MAPPING: check_mapping(u->mapping); break;
      case T_ARRAY: check_array(u->array); break;
      case T_PROGRAM: check_program(u->program); break;
      case T_OBJECT: check_object(u->object); break;
/*      case T_MULTISET: check_multiset(u->multiset); break; */
      }
      inside=0;
    }
  }
}

void check_short_svalue(union anything *u, TYPE_T type)
{
  if(type<=MAX_REF_TYPE && (3 & (long)(u->refs)))
    fatal("Odd pointer! type=%d u->refs=%p\n",type,u->refs);

  check_refs2(u,type);
  low_check_short_svalue(u,type);
}

void debug_check_svalue(struct svalue *s)
{
  check_type(s->type);
  if(s->type<=MAX_REF_TYPE && (3 & (long)(s->u.refs)))
    fatal("Odd pointer! type=%d u->refs=%p\n",s->type,s->u.refs);

  check_refs(s);
  low_check_short_svalue(& s->u, s->type);
}

#endif

#ifdef PIKE_DEBUG
void real_gc_xmark_svalues(struct svalue *s, size_t num)
{
  size_t e;

  if (!s) {
    return;
  }

  for(e=0;e<num;e++,s++)
  {
    check_svalue(s);
    
    gc_svalue_location=(void *)s;

    if(s->type <= MAX_REF_TYPE)
      gc_external_mark2(s->u.refs,0,0);
  }
  gc_svalue_location=0;
}
#endif


/* Macro mania follows. We construct the gc 1) check, 2) mark, and 3)
 * cycle check functions on 1) svalues and 2) short svalues containing
 * 1) normal or 2) weak references. I.e. 12 very similar functions. */

#define GC_CHECK_SWITCH(U, T, ZAP, GC_DO, PRE, DO_FUNC, DO_OBJ)		\
  switch (T) {								\
    case T_FUNCTION:							\
      PRE DO_FUNC(U, T, ZAP, GC_DO)					\
    case T_OBJECT:							\
      PRE DO_OBJ(U, T, ZAP, GC_DO)					\
    case T_STRING:							\
      PRE DO_IF_DEBUG(if (d_flag) gc_check(U.string);) break;		\
    case T_PROGRAM:							\
    case T_ARRAY:							\
    case T_MULTISET:							\
    case T_MAPPING:							\
      PRE GC_DO(U.refs); break;						\
  }

#define DO_CHECK_FUNC_SVALUE(U, T, ZAP, GC_DO)				\
      if (s->subtype == FUNCTION_BUILTIN) {				\
	DO_IF_DEBUG(							\
	  if (d_flag && !gc_check(s->u.efun)) {				\
	    gc_check(s->u.efun->name);					\
	    gc_check(s->u.efun->type);					\
	  }								\
	)								\
	break;								\
      }									\
      /* Fall through to T_OBJECT. */

#define DO_FUNC_SHORT_SVALUE(U, T, ZAP, GC_DO)				\
      fatal("Cannot have a function in a short svalue.\n");

#define DO_CHECK_OBJ(U, T, ZAP, GC_DO)					\
      if (U.object->prog)						\
	GC_DO(U.object);						\
      else ZAP();							\
      break;

#define DO_CHECK_OBJ_WEAK(U, T, ZAP, GC_DO)				\
      if(U.object->prog)						\
	if (U.object->prog->flags & PROGRAM_NO_WEAK_FREE)		\
	  gc_check(U.object);						\
	else								\
	  gc_check_weak(U.object);					\
      else ZAP();							\
      break;

#define ZAP_SVALUE()							\
      do {								\
	free_svalue(s);							\
	s->type = T_INT;						\
	s->u.integer = 0;						\
	s->subtype = NUMBER_DESTRUCTED;					\
      } while (0)

#define ZAP_SHORT_SVALUE()						\
      do {								\
	free_short_svalue(u, type);					\
	u->refs = 0;							\
      } while (0)

#define SET_SUB_SVALUE(V) s->subtype = (V)

#define SET_SUB_SHORT_SVALUE(V)

TYPE_FIELD real_gc_check_svalues(struct svalue *s, size_t num)
{
#ifdef PIKE_DEBUG
  extern void * check_for;
#endif
  size_t e;
  TYPE_FIELD f;
  f=0;
  for(e=0;e<num;e++,s++)
  {
    check_svalue(s);
#ifdef PIKE_DEBUG
    gc_svalue_location=(void *)s;
#endif
    GC_CHECK_SWITCH((s->u), (s->type), ZAP_SVALUE, gc_check,
		    {}, DO_CHECK_FUNC_SVALUE,
		    DO_CHECK_OBJ);
    f|= 1 << s->type;
  }
#ifdef PIKE_DEBUG
  gc_svalue_location=0;
#endif
  return f;
}

TYPE_FIELD gc_check_weak_svalues(struct svalue *s, size_t num)
{
#ifdef PIKE_DEBUG
  extern void * check_for;
#endif
  size_t e;
  TYPE_FIELD f;
  f=0;
  for(e=0;e<num;e++,s++)
  {
    check_svalue(s);
#ifdef PIKE_DEBUG
    gc_svalue_location=(void *)s;
#endif
    GC_CHECK_SWITCH((s->u), (s->type), ZAP_SVALUE, gc_check_weak,
		    {}, DO_CHECK_FUNC_SVALUE,
		    DO_CHECK_OBJ_WEAK);
    f|= 1 << s->type;
  }
#ifdef PIKE_DEBUG
  gc_svalue_location=0;
#endif
  return f;
}

void real_gc_check_short_svalue(union anything *u, TYPE_T type)
{
#ifdef PIKE_DEBUG
  extern void * check_for;
  gc_svalue_location=(void *)u;
#endif
  debug_malloc_touch(u);
  GC_CHECK_SWITCH((*u), type, ZAP_SHORT_SVALUE, gc_check,
		  {if (!u->refs) return;}, DO_FUNC_SHORT_SVALUE,
		  DO_CHECK_OBJ);
#ifdef PIKE_DEBUG
  gc_svalue_location=0;
#endif
}

void gc_check_weak_short_svalue(union anything *u, TYPE_T type)
{
#ifdef PIKE_DEBUG
  extern void * check_for;
  gc_svalue_location=(void *)u;
#endif
  debug_malloc_touch(u);
  GC_CHECK_SWITCH((*u), type, ZAP_SHORT_SVALUE, gc_check_weak,
		  {if (!u->refs) return;}, DO_FUNC_SHORT_SVALUE,
		  DO_CHECK_OBJ_WEAK);
#ifdef PIKE_DEBUG
  gc_svalue_location=0;
#endif
}

#define GC_RECURSE_SWITCH(U,T,ZAP,FREE_WEAK,GC_DO,PRE,DO_FUNC,DO_STR)	\
  switch (T) {								\
    case T_FUNCTION:							\
      PRE DO_FUNC(U, T, ZAP, GC_DO)					\
    case T_OBJECT:							\
      PRE								\
      DO_IF_DEBUG(							\
	if (!U.object->prog)						\
	  gc_fatal(U.object, 0,						\
		   "Unfreed destructed object in mark pass.\n");	\
      )									\
      FREE_WEAK(U, T, ZAP) GC_DO(U, object);				\
      break;								\
    case T_STRING:							\
      DO_STR(U); break;							\
    case T_PROGRAM:							\
      PRE FREE_WEAK(U, T, ZAP) GC_DO(U, program); break;		\
    case T_ARRAY:							\
      PRE FREE_WEAK(U, T, ZAP) GC_DO(U, array); break;			\
    case T_MULTISET:							\
      PRE FREE_WEAK(U, T, ZAP) GC_DO(U, multiset); break;		\
    case T_MAPPING:							\
      PRE FREE_WEAK(U, T, ZAP) GC_DO(U, mapping); break;		\
  }

#define DO_MARK_FUNC_SVALUE(U, T, ZAP, GC_DO)				\
      if (s->subtype == FUNCTION_BUILTIN) {				\
	DO_IF_DEBUG(							\
	  if (d_flag) {							\
	    gc_mark(s->u.efun->name);					\
	    gc_mark(s->u.efun->type);					\
	  }								\
	)								\
	break;								\
      }									\
      /* Fall through to T_OBJECT. */

#define DO_MARK_STRING(U)						\
      DO_IF_DEBUG(if (U.refs && d_flag) gc_mark(U.string))

#define GC_DO_MARK(U, TN)						\
  enqueue(&gc_mark_queue,						\
	  (queue_call) PIKE_CONCAT3(gc_mark_, TN, _as_referenced),	\
	  U.TN)

#define DONT_FREE_WEAK(U, T, ZAP)

#define FREE_WEAK(U, T, ZAP)						\
    if (gc_do_weak_free(U.refs)) {					\
      ZAP();								\
      freed = 1;							\
      break;								\
    }

void real_gc_mark_svalues(struct svalue *s, size_t num)
{
  size_t e;
  for(e=0;e<num;e++,s++)
  {
    dmalloc_touch_svalue(s);
    GC_RECURSE_SWITCH((s->u), (s->type), ZAP_SVALUE, DONT_FREE_WEAK,
		      GC_DO_MARK, {},
		      DO_MARK_FUNC_SVALUE, DO_MARK_STRING);
  }
}

TYPE_FIELD gc_mark_weak_svalues(struct svalue *s, size_t num)
{
  TYPE_FIELD t = 0;
  int freed = 0;
  size_t e;
  for(e=0;e<num;e++,s++)
  {
    dmalloc_touch_svalue(s);
    GC_RECURSE_SWITCH((s->u), (s->type), ZAP_SVALUE, FREE_WEAK,
		      GC_DO_MARK, {},
		      DO_MARK_FUNC_SVALUE, DO_MARK_STRING);
    t |= 1 << s->type;
  }
  return freed ? t : 0;
}

void real_gc_mark_short_svalue(union anything *u, TYPE_T type)
{
  debug_malloc_touch(u);
  GC_RECURSE_SWITCH((*u), type, ZAP_SHORT_SVALUE, DONT_FREE_WEAK,
		    GC_DO_MARK, {if (!u->refs) return;},
		    DO_FUNC_SHORT_SVALUE, DO_MARK_STRING);
}

int gc_mark_weak_short_svalue(union anything *u, TYPE_T type)
{
  int freed = 0;
  debug_malloc_touch(u);
  GC_RECURSE_SWITCH((*u), type, ZAP_SHORT_SVALUE, FREE_WEAK,
		    GC_DO_MARK, {if (!u->refs) return 0;},
		    DO_FUNC_SHORT_SVALUE, DO_MARK_STRING);
  return freed;
}

#define DO_CYCLE_CHECK_FUNC_SVALUE(U, T, ZAP, GC_DO)			\
      if (s->subtype == FUNCTION_BUILTIN) break;			\
      /* Fall through to T_OBJECT. */

#define DO_CYCLE_CHECK_STRING(U)

#define GC_DO_CYCLE_CHECK(U, TN) PIKE_CONCAT(gc_cycle_check_, TN)(U.TN)
#define GC_DO_CYCLE_CHECK_WEAK(U, TN) PIKE_CONCAT3(gc_cycle_check_, TN, _weak)(U.TN)

void real_gc_cycle_check_svalues(struct svalue *s, size_t num)
{
  size_t e;
  for(e=0;e<num;e++,s++)
  {
    dmalloc_touch_svalue(s);
    GC_RECURSE_SWITCH((s->u), (s->type), ZAP_SVALUE, DONT_FREE_WEAK,
		      GC_DO_CYCLE_CHECK, {},
		      DO_CYCLE_CHECK_FUNC_SVALUE, DO_CYCLE_CHECK_STRING);
  }
}

TYPE_FIELD gc_cycle_check_weak_svalues(struct svalue *s, size_t num)
{
  TYPE_FIELD t = 0;
  int freed = 0;
  size_t e;
  for(e=0;e<num;e++,s++)
  {
    dmalloc_touch_svalue(s);
    GC_RECURSE_SWITCH((s->u), (s->type), ZAP_SVALUE, FREE_WEAK,
		      GC_DO_CYCLE_CHECK_WEAK, {},
		      DO_CYCLE_CHECK_FUNC_SVALUE, DO_CYCLE_CHECK_STRING);
    t |= 1 << s->type;
  }
  return freed ? t : 0;
}

void real_gc_cycle_check_short_svalue(union anything *u, TYPE_T type)
{
  debug_malloc_touch(u);
  GC_RECURSE_SWITCH((*u), type, ZAP_SHORT_SVALUE, DONT_FREE_WEAK,
		    GC_DO_CYCLE_CHECK, {if (!u->refs) return;},
		    DO_FUNC_SHORT_SVALUE, DO_CYCLE_CHECK_STRING);
}

int gc_cycle_check_weak_short_svalue(union anything *u, TYPE_T type)
{
  int freed = 0;
  debug_malloc_touch(u);
  GC_RECURSE_SWITCH((*u), type, ZAP_SHORT_SVALUE, FREE_WEAK,
		    GC_DO_CYCLE_CHECK_WEAK, {if (!u->refs) return 0;},
		    DO_FUNC_SHORT_SVALUE, DO_CYCLE_CHECK_STRING);
  return freed;
}

INT32 pike_sizeof(struct svalue *s)
{
  switch(s->type)
  {
  case T_STRING: return s->u.string->len;
  case T_ARRAY: return s->u.array->size;
  case T_MAPPING: return m_sizeof(s->u.mapping);
  case T_MULTISET: return l_sizeof(s->u.multiset);
  case T_OBJECT:
    if(!s->u.object->prog)
      error("sizeof() on destructed object.\n");
    if(FIND_LFUN(s->u.object->prog,LFUN__SIZEOF) == -1)
    {
      return s->u.object->prog->num_identifier_index;
    }else{
      apply_lfun(s->u.object, LFUN__SIZEOF, 0);
      if(sp[-1].type != T_INT)
	error("Bad return type from o->_sizeof() (not int)\n");
      sp--;
      return sp->u.integer;
    }
  default:
    error("Bad argument 1 to sizeof().\n");
    return 0; /* make apcc happy */
  }
}
