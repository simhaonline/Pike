/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
#ifndef GLOBAL_H
#define GLOBAL_H

/*
 * Some structure forward declarations are needed.
 */

/* This is needed for linux */
#ifdef MALLOC_REPLACED
#define NO_FIX_MALLOC
#endif

#ifndef STRUCT_PROGRAM_DECLARED
#define STRUCT_PROGRAM_DECLARED
struct program;
#endif

struct function;
#ifndef STRUCT_SVALUE_DECLARED
#define STRUCT_SVALUE_DECLARED
struct svalue;
#endif
struct sockaddr;
struct object;
struct array;
struct svalue;

#include "machine.h"

/*
 * Max number of local variables in a function.
 * Currently there is no support for more than 256
 */
#define MAX_LOCAL	256

/*
 * define NO_GC to get rid of garbage collection
 */
#ifndef NO_GC
#define GC2
#endif

#if defined(i386)
#ifndef HANDLES_UNALIGNED_MEMORY_ACCESS
#define HANDLES_UNALIGNED_MEMORY_ACCESS
#endif
#endif

/* AIX requires this to be the first thing in the file.  */
#ifdef __GNUC__
# ifdef alloca
#  undef alloca
# endif
# define alloca __builtin_alloca
#else
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#undef HAVE_STDLIB_H
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#undef HAVE_MALLOC_H
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#undef HAVE_UNISTD_H
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#undef HAVE_STRING_H
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#undef HAVE_LIMITS_H
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#undef HAVE_SYS_TYPES_H
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#undef HAVE_MEMORY_H
#endif


/* we here define a few types with more defined values */

#define INT64 long long

#if SIZEOF_SHORT >= 4
#define INT32 short
#else
#if SIZEOF_INT >= 4
#define INT32 int
#else
#define INT32 long
#endif
#endif

#define INT16 short
#define INT8 char

#define SIZE_T unsigned INT32

#define TYPE_T unsigned INT8
#define TYPE_FIELD unsigned INT16

#define FLOAT_TYPE float




#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#define RCSID(X) \
 static char *rcsid __attribute__ ((unused)) =X
#elif __GNUC__ == 2
#define RCSID(X) \
 static char *rcsid = X; \
 static void *use_rcsid=(&use_rcsid, (void *)&rcsid)
#else
#define RCSID(X) \
 static char *rcsid = X
#endif

#if defined(__GNUC__) && !defined(DEBUG) && !defined(lint)
#define INLINE inline
#else
#define INLINE
#endif

#include "port.h"


#ifdef BUFSIZ
#define PROT_STDIO(x) PROT(x)
#else
#define PROT_STDIO(x) ()
#endif

#ifdef __STDC__
#define PROT(x) x
#else
#define PROT(x) ()
#endif

#ifdef MALLOC_DECL_MISSING
char *malloc PROT((int));
char *realloc PROT((char *,int));
void free PROT((char *));
char *calloc PROT((int,int));
#endif

#ifdef GETPEERNAME_DECL_MISSING
int getpeername PROT((int, struct sockaddr *, int *));
#endif

#ifdef GETHOSTNAME_DECL_MISSING
void gethostname PROT((char *,int));
#endif

#ifdef POPEN_DECL_MISSING
FILE *popen PROT((char *,char *));
#endif

#ifdef GETENV_DECL_MISSING
char *getenv PROT((char *));
#endif

#endif
