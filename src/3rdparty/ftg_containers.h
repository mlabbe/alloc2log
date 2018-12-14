/* ftg_containers  - public domain library
   no warranty implied; use at your own risk

   C89 container library.  Tries to implement actually useful
   container types in real code, not just interpret other container
   libraries.

   FEATURES
   
   1. Resizable arrays -- C style access to arrays that quietly resize
   when needed.

   2. Hashindex -- Tool to implement dictionaries.  Handles hashing,
   linear searching, expansion.  Returns array index.

   3. Variants -- Values which can change their type at runtime.

   4. Dict -- Key/value pairs, where the key is a string and the value
   is a variant.

   USAGE

   Scroll down to prototypes below to see documentation on each container.

   Do this:
   #define FTG_IMPLEMENT_CONTAINERS

   before you include this file in one C or C++ file to create the
   implementation.

   It should look like this:
   #include ...
   #include ...
   #include ...
   #define FTG_IMPLEMENT_CONTAINERS
   #include "ftg_containers.h"

   REVISION HISTORY

   0.1  oct 27, 2016   Resizable arrays
   0.2  mar 1st, 2017  Hashindex, Variants

   LICENSE

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   Special thanks to Sean T. Barrett (@nothings) for the idea to use single
   header format for libraries.
*/

/*
  todo: 
  DICT
   - implement set int for dict
   - separate out the hashkey search code from setstring, because we can't just use atoi()
   - fix leak in ftgc_hashindex_free

  VARIANT
   - look into inlining a lot of the getters and setters

   - docs
   - copy all unit tests from unit_dict.h
   - convert this to premake!
   - consider allocation scheme load on parse
     https://deplinenoise.wordpress.com/2012/10/20/toollibrary-memory-management-youre-doing-it-wrong/

 */

/*
Other containers/types to consider:
 - dictionary
 - scratch buffer
 - linked list, 2x linked list
 - ring buffer
 - u8strpool
 - scratch buffer
 */

#ifndef FTGC__INCLUDE_CONTAINERS_H
#define FTGC__INCLUDE_CONTAINERS_H

#include <stddef.h>

#ifdef FTGC_CONTAINERS_STATIC
#define FTGCDEF static
#else
#define FTGCDEF extern
#endif

#ifdef FTGC_CONTAINERS_STATIC
#  define FTGCDEFDATA static
#else
#  define FTGCDEFDATA
#endif


#if defined(FTGC_MALLOC) && defined(FTGC_FREE) && defined(FTGC_REALLOC)
// okay
#elif !defined(FTGC_MALLOC) && !defined(FTGC_FREE) && !defined(FTGC_REALLOC)
// also okay
#else
#  error "Must define all or none of FTGC_MALLOC, FTGC_FREE and FTGC_REALLOC."
#endif


#ifndef FTGC_MALLOC
#include <stdlib.h>
#  define FTGC_MALLOC(size)        malloc(size)
#  define FTGC_REALLOC(ptr, size)  realloc(ptr, size)
#  define FTGC_FREE(ptr)           free(ptr)
#endif

// include ftg_core.h ahead of this header to debug it
#ifdef FTG_ASSERT
#  define FTGC__ASSERT(exp)      FTG_ASSERT(exp)
#  define FTGC__ASSERT_FAIL(exp) FTG_ASSERT_FAIL(exp)    
#else
#  define FTGC__ASSERT(exp)      ((void)0)
#  define FTGC__ASSERT_FAIL(exp) ((void)0)
#endif

#ifdef __cplusplus
# extern "C"
#endif

// API declaration starts here

typedef void ftgc_data_t;
typedef signed int ftgc_sint32;
typedef unsigned int ftgc_uint32;

/*
  resizable arrays usage:

  1. declare:    int *i = array_init(i, num_elements)
  2. loop over:  while(condition) {array_append(i, some_val);}
  3. access:     i[j];  array_last(i);  array_count(i);
  4. free:       array_free(i);

  iteration without indexing:

  // assume *i is a populated array of integers (or NULL)
  for (int *it = i; it != array_end(i); i++)
       printf("%d", *it);


  Note that the i ptr itself can change whenever array_append is called.
  This resizable array scheme does not work for long lived, mutable, resizable
  arrays.  Only store the pointer somewhere permanent after all appends
  have completed.


  Usage notes:
   - a NULL ptr is idiomatically equivalent to 0 elements
   - it's okay to store pointers to strings as long as those pointers are
     valid.  For example:
       const char **str_list = array_init(str_list, 1);
       array_append(str_list, "append local stack ptr");
     This is a pointer copy and not a string copy.
 
   
 */

// override this to define a function that specifies how many elements to allocate on an array realloc
#ifndef FTGC_ARRAY_NEW_ELEMENT_COUNT
#  define FTGC_ARRAY_NEW_ELEMENT_COUNT(old, incr) \
    ftgc__default_new_elem_count(old, incr);
#endif

// given a count of elements and an increased number of elements,
// return the number of elements to allocate on a resize.
// incr is always positive and return must always be greater than old_elem_count
static size_t
ftgc__default_new_elem_count(size_t old_elem_count, size_t incr_elems)
{
    size_t expand_new_elem_count = (int)((float)old_elem_count * 1.5f);
    size_t min_new_elem_count = old_elem_count + incr_elems;

    if (min_new_elem_count > expand_new_elem_count)
        return min_new_elem_count;
    else
        return expand_new_elem_count;
}



#define ftgc__array_raw(a)  ((size_t*)(a) - 2)
#define ftgc__array_size(a) (ftgc__array_raw(a)[0])         // returns elem capacity
#define ftgc_array_count(a) ((a)?ftgc__array_raw(a)[1]:0)   // returns num stored

// test if array a needs to grow to fit n more elements
#define ftgc__array_test_needs_to_grow(a,n) \
    ((a)==NULL || ftgc_array_count(a)+(n) > ftgc__array_size(a))

// test if array a should grow by n
#define ftgc__array_grow_if_needed(a,n) \
    (ftgc__array_test_needs_to_grow((a),(n)) ? ftgc__array_grow((ftgc_data_t**)(&(a)),(n),sizeof(*(a))) : 0)

#define ftgc_array_append(a,d)                  \
    (ftgc__array_grow_if_needed((a), 1),        \
     (a)[ftgc__array_raw(a)[1]++] = (d));

// initialize array a, starting with n elements
#define ftgc_array_init(a,n) \
    ((a)=NULL, ftgc_array_reserve((a),(n)))

// add n more uninitialized elements to already-initialized array a
#define ftgc_array_reserve(a,n) \
    (ftgc__array_grow((ftgc_data_t**)(&(a)),(n),sizeof(*(a))))

// return the last element of an array
#define ftgc_array_last(a) ((a)[ftgc_array_count(a)-1])

// get the pointer to the last element, useful for iteration
#define ftgc_array_end(a) ((a) + ftgc_array_count(a))

// free the array and set it to NULL
#define ftgc_array_free(a) ((a)?(FTGC_FREE(ftgc__array_raw(a)), (a)=NULL):0)


#ifndef FTGC_ARRAY_DISABLE_PREFIX
#define array_init     ftgc_array_init
#define array_reserve  ftgc_array_reserve
#define array_append   ftgc_array_append
#define array_free     ftgc_array_free
#define array_count    ftgc_array_count
#define array_last     ftgc_array_last
#define array_end      ftgc_array_end
#endif

// grow *array by incr_elems of size item_sz
// if realloc is sucessful, return ptr to *array[0]
// with housekeeping size updated
static ftgc_data_t *
ftgc__array_grow(ftgc_data_t **array,
                 size_t incr_elems,
                 size_t item_sz)
{
    size_t old_elem_count = *array ? ftgc__array_size(*array) : 0;
    const size_t housekeeping_size = sizeof(size_t)*2;
    ftgc_data_t *new_raw;
    size_t new_bytes;

    size_t new_elem_count = FTGC_ARRAY_NEW_ELEMENT_COUNT(old_elem_count, incr_elems);

    FTGC__ASSERT(new_elem_count > 0);
    FTGC__ASSERT(new_elem_count > old_elem_count);
    FTGC__ASSERT(incr_elems > 0);
    
    new_bytes = (new_elem_count * item_sz) + housekeeping_size;
    new_raw = (ftgc_data_t*)FTGC_REALLOC(*array ? ftgc__array_raw(*array) : NULL,
                                         new_bytes);
    if (!new_raw)
    {
        /* keep old data on alloc fail */        
        FTGC__ASSERT(0);
        return *array;
    }

    *array = &((size_t*)(new_raw))[2];
    if (old_elem_count == 0) {
        ftgc__array_raw(*array)[0] = 0;
        ftgc__array_raw(*array)[1] = 0;
    }
    
    ftgc__array_size(*array) += incr_elems;

    return *array;
}


/*
  hashindex is the core of a container that uses hash lookups.
  rather than return what is being contained, it returns an array
  index.  hashindex is used to implement type-specific containers,
  where the owning container holds a linear array of types and
  hashindex resolves it.
  
  Hashindex has no upper bounds on the number of indices that can be
  stored in it.

  INIT

  When you initialize a hashindex, you specify the storage count.
  This value trades memory usage for collisions.
  
      ftgc_hashindex_s table;
      ftgc_hashindex_init(&table, 32);

  In order to store a value in the hashindex, it must first generate a
  key.  Then, the value can be added by that key.

      ftgc_hashkey_t key;
      key = ftgc_hashindex_generate_key_string(&table, "sample_key");
      ftgc_hashindex_add_key(&table, key, some_index_lookup_value);

  Retrieving a value from the hashindex is done by deriving a key from
  a string, then searching for that key by performing a linear search
  through all of the hash-collided values for that key.  This is done
  by using an iterator.

      char requested_lookup[] = "some_value";
      ftgc_hashindex_iter_s iter;
      ftgc_hashkey_t key;

      key = ftgc_hashindex_generate_key_string(&table, requested_lookup);

      iter = ftgc_hashindex_get_iter(&table);

      for (key = ftgc_hashindex_iter_get_first(&table, &iter, key);
           key != FTGC_HASHINDEX_UNUSED;
           key = ftgc_hashindex_iter_get_next(&iter))
      {
          if (key == FTGC_HASHINDEX_DELETED)
              continue;

          // find a key match in some containing array, and return
          // its value.
          if (strcmp(requested_lookup, some_array[key].lookup)==0)
              return some_array[key].value;
      }
*/

#define FTGC_HASHINDEX_UNUSED -1
#define FTGC_HASHINDEX_DELETED -2

typedef ftgc_sint32 ftgc_hashkey_t;

struct ftgc__hashlink_s {
    int value;
    struct ftgc__hashlink_s *next;
};

typedef struct {
    int *table;
    struct ftgc__hashlink_s *chain;
    ftgc_sint32 table_size;
    ftgc_sint32 hash_mask;
} ftgc_hashindex_s;

typedef struct {
    struct ftgc__hashlink_s *current_node;
    struct ftgc__hashlink_s *prev_node;
}ftgc_hashindex_iter_s;

FTGCDEF int
ftgc_hashindex_init(ftgc_hashindex_s *context, int vert_size);

FTGCDEF void
ftgc_hashindex_free(ftgc_hashindex_s *context);

FTGCDEF ftgc_hashkey_t
ftgc_hashindex_generate_key_string(const ftgc_hashindex_s *context,
                                   const char *str);

FTGCDEF ftgc_hashkey_t
ftgc_hashindex_generate_key_int(const ftgc_hashindex_s *context, int value);

FTGCDEF int
ftgc_hashindex_add_key(ftgc_hashindex_s *context, ftgc_hashkey_t key, int value);

FTGCDEF void
ftgc_hashindex_remove_first(ftgc_hashindex_s *context, ftgc_hashkey_t key);

FTGCDEF int
ftgc_hashindex_iter_get_first(const ftgc_hashindex_s *context,
                              ftgc_hashindex_iter_s *iter,
                              ftgc_hashkey_t key);

FTGCDEF void
ftgc_hashindex_iter_remove_current(ftgc_hashindex_iter_s *iter);

FTGCDEF int
ftgc_hashindex_iter_get_next(ftgc_hashindex_iter_s *iter);

/*
  Variant is a variable that can be changed at runtime to one of
  any of the types represented in enum ftgc_variant_type_t.

  Various ftgc_variant_get_* and ftgc_variant_set_* apis exist.
  Check the type with variant.field_type.
   
  String handling is noteworthy: you can either heap-allocate memory
  and store the string directly in the variant using
  ftgc_variant_set_string(), or you can store a pointer to a string
  with ftgc_variant_set_string_ptr().

  Variants are of type FTGC_VARTYPE_VOID after init() is called.
*/

typedef enum {
    FTGC_VARTYPE_VOID,
    FTGC_VARTYPE_VOIDPTR,
    FTGC_VARTYPE_BOOL,
    FTGC_VARTYPE_SINT32,
    FTGC_VARTYPE_UINT32,
    FTGC_VARTYPE_FLOAT,
    FTGC_VARTYPE_VEC2,
    FTGC_VARTYPE_VEC3,
    FTGC_VARTYPE_STRING
}ftgc_variant_type_t;

typedef float ftgc__variant_float3_t[3];
typedef struct {
    union
    {
        void                    *val_ptr;
        int                      val_bool;
        ftgc_sint32              val_sint32;
        ftgc_uint32              val_uint32;
        ftgc__variant_float3_t   val_float3;
        struct ftgc__varstring_s
        {
            const char          *external_storage;
            char                *internal_storage;
        }val_str;
    };

    ftgc_variant_type_t field_type;
}ftgc_variant_s;

// todo: inline this
FTGCDEF void
ftgc_variant_init(ftgc_variant_s *context);

FTGCDEF void
ftgc_variant_free(ftgc_variant_s *context);

FTGCDEF void
ftgc_variant_set_void_ptr(ftgc_variant_s *ctx, void *ptr);

FTGCDEF void
ftgc_variant_set_bool(ftgc_variant_s *ctx, int v);

FTGCDEF void
ftgc_variant_set_sint32(ftgc_variant_s *ctx, ftgc_sint32 v);

FTGCDEF void
ftgc_variant_set_uint32(ftgc_variant_s *ctx, ftgc_uint32 v);

FTGCDEF void
ftgc_variant_set_float(ftgc_variant_s *ctx, float v);

FTGCDEF void
ftgc_variant_set_vec2(ftgc_variant_s *ctx, float *v);

FTGCDEF void
ftgc_variant_set_vec3(ftgc_variant_s *ctx, float *v);

// copy *str to variant, allocating the appropriate memory
FTGCDEF int
ftgc_variant_set_string(ftgc_variant_s *ctx, const char *str);

// store the pointer to p_str internally, not allocating any memory
FTGCDEF void
ftgc_variant_set_string_ptr(ftgc_variant_s *ctx, const char *p_str);

FTGCDEF void
ftgc_variant_set_from_variant(ftgc_variant_s *ctx, const ftgc_variant_s *other);

FTGCDEF void *
ftgc_variant_get_void_ptr(const ftgc_variant_s *ctx);

FTGCDEF int
ftgc_variant_get_bool(const ftgc_variant_s *ctx);

FTGCDEF int
ftgc_variant_get_sint32(const ftgc_variant_s *ctx);

FTGCDEF int
ftgc_variant_get_uint32(const ftgc_variant_s *ctx);

FTGCDEF float
ftgc_variant_get_float(const ftgc_variant_s *ctx);

FTGCDEF float*
ftgc_variant_get_vec2(const ftgc_variant_s *ctx);

FTGCDEF float*
ftgc_variant_get_vec3(const ftgc_variant_s *ctx);

FTGCDEF char *
ftgc_variant_get_string(const ftgc_variant_s *ctx);

/*
  dict is a string key to arbitrary value dictionary. 


*/
#ifndef FTGC_DICT_KEY_BYTES
#  define FTGC_DICT_KEY_BYTES 9
#endif

#ifndef FTGC_DICT_EXPANSION_GRANULARITY
#  define FTGC_DICT_EXPANSION_GRANULARITY 12
#endif

// define FTGC_DICT_CASE_SENSITIVE to 1 to avoid ASCII case-folding
// string compare.  Use this if keys are not 7-bit ascii.
#ifndef FTGC_DICT_CASE_SENSITIVE
#  define FTGC_DICT_CASE_SENSITIVE 0
#endif

struct ftgc__dict_pairs_s {
    char *keys;
    ftgc_variant_s *values;
};

typedef struct {
    int dict_size;
    int num_pairs;
    struct ftgc__dict_pairs_s pairs;
    ftgc_hashindex_s hash_index;
}ftgc_dict_s;

FTGCDEF void
ftgc_dict_init(ftgc_dict_s *dict, int size, int hash_size);

//
// End of header file
//
#endif /* FTGC__INCLUDE_CONTAINERS_H */

/* implementation */
#if defined(FTG_IMPLEMENT_CONTAINERS)

#include <ctype.h>
#include <string.h>

//
// hashindex
//

static ftgc_sint32
ftgc__pow2_roundup(ftgc_sint32 v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    
    return v;
}

static struct ftgc__hashlink_s *
ftgc__get_chain_end(struct ftgc__hashlink_s *node)
{
    struct ftgc__hashlink_s *current;

    for (current = node; current->next; current = current->next)
        ;

    FTGC__ASSERT(current);
    return current;
}

static int
ftgc__is_value_in_chain(const struct ftgc__hashlink_s *start, int value)
{
    const struct ftgc__hashlink_s *current;

    for (current = start; current->next; current = current->next)
    {
        if (current->value == value)
            return 1;
    }

    return 0;
}

static void
ftgc__append_node(struct ftgc__hashlink_s *chain_end)
{
    struct ftgc__hashlink_s *new_end;
    
    FTGC__ASSERT(chain_end != NULL);
    FTGC__ASSERT(chain_end->next == NULL);

    new_end = (struct ftgc__hashlink_s*)FTGC_MALLOC(sizeof(struct ftgc__hashlink_s));
	if (!new_end)
	{
		FTGC__ASSERT(new_end);
		chain_end->next = NULL;
		return;
	}

    new_end->value = FTGC_HASHINDEX_UNUSED;
    new_end->next = NULL;
    
    chain_end->next = new_end;
}

// required init of the hashindex
//
// vert_size is the number of cells that your value will be hashed into.
// it is rounded up to the next power of 2.
// higher values allocate more memory but result in less collisions
// returns false on failure to allocate memory
FTGCDEF int
ftgc_hashindex_init(ftgc_hashindex_s *ctx, int vert_size)
{
    int i;
    
	FTGC__ASSERT(vert_size > 1);

    ctx->table_size = ftgc__pow2_roundup(vert_size);
	ctx->hash_mask = ctx->table_size - 1;

    ctx->table = (int*)FTGC_MALLOC(sizeof(int)*ctx->table_size);
	if (!ctx->table)
	{
		return 0;
	}

    memset(ctx->table, FTGC_HASHINDEX_UNUSED, sizeof(int)*ctx->table_size);
    
    ctx->chain = (struct ftgc__hashlink_s*)FTGC_MALLOC(sizeof(struct ftgc__hashlink_s) * ctx->table_size);
	if (!ctx->chain)
	{
		FTGC__ASSERT(ctx->chain);
		FTGC_FREE(ctx->table);
		return 0;
	}

    for (i=0; i<ctx->table_size; i++)
    {
        ctx->chain[i].value = FTGC_HASHINDEX_UNUSED;
        ctx->chain[i].next = NULL;
    }

	return 1;
}


// required free all heap allocated memory allocated inside the hashindex
FTGCDEF void
ftgc_hashindex_free(ftgc_hashindex_s *ctx)
{
    FTGC_FREE(ctx->table);
    FTGC_FREE(ctx->chain); // fixme: this does not clear the linked lists! leak
    
    ctx->table = NULL;
    ctx->chain = NULL; 
    ctx->table_size = 0;
    ctx->hash_mask = 0;
}

// return a hash key for the string, which can then be searched or added to the index
// in a subsequent operation.
FTGCDEF ftgc_hashkey_t
ftgc_hashindex_generate_key_string(const ftgc_hashindex_s *ctx, const char *str)
{
    ftgc_uint32 hash = 0, x = 0;
    const char *p;
    
    for (p=str;*p;p++)
    {
        hash = (hash << 4) + (ftgc_uint32)*p;
        if ((x = hash & 0xF0000000L) != 0)
            hash ^= (x >> 24);

        hash &= ~x;
    }

    return hash & ctx->hash_mask;
}


// as key_string, for int
FTGCDEF ftgc_hashkey_t
ftgc_hashindex_generate_key_int(const ftgc_hashindex_s *ctx, int value)
{
    value = ((value >> 16) ^ value) * 0x45d9f3b;
    value = ((value >> 16) ^ value) * 0x45d9f3b;
    value = (value >> 16) ^ value;
    
    return value & ctx->hash_mask;
}


FTGCDEF ftgc_hashkey_t
ftgc_hashindex_generate_key_ptr(const ftgc_hashindex_s *ctx, void *ptr)
{
    union intptr_u {
        void *p;
        uint32_t i[2];
        char c[8];
    }intptr;

    intptr.p = ptr;

    if (sizeof(ptr) == 4)
    {
        return ftgc_hashindex_generate_key_int(ctx, intptr.i[0]);
    }
    else if (sizeof(ptr) == 8)
    {
#define FTGC__SWAP(a,b)            \
        intptr.c[a] ^= intptr.c[b];             \
        intptr.c[b] ^= intptr.c[a];             \
        intptr.c[a] ^= intptr.c[b];

        FTGC__SWAP(1, 5);
        FTGC__SWAP(3, 7)
#undef FTGC__SWAP

        return ftgc_hashindex_generate_key_int(ctx, intptr.i[0] &~ intptr.i[1]);
    }
}


// Add a new value to the hashindex.  The key is built from a call
// to ftgc_hashindex_generate_key_* functions.
//
// This returns 1 if there was a collision, or 0 if not.  -1 if oom error allocating.
// Collision is safe to ignore; no duplicate value can be added to the chain.
FTGCDEF int
ftgc_hashindex_add_key(ftgc_hashindex_s *ctx, ftgc_hashkey_t key, int value)
{
    struct ftgc__hashlink_s *chain_end;

    // simply add it if its empty
    if (ctx->table[key] == FTGC_HASHINDEX_UNUSED ||
        ctx->table[key] == FTGC_HASHINDEX_DELETED)
    {
        ctx->table[key] = value;
        return 0;
    }

    // collision.  exit if we are adding something that exists
    if (ctx->table[key] == value)
        return 1;

    // new value with the same key.  check to see if it exists in the chain already.
    if (ftgc__is_value_in_chain(&ctx->chain[key], value))
        return 1;

    // it does not exist, we will append it to the chain.
    chain_end = ftgc__get_chain_end(&ctx->chain[key]);
    chain_end->value = value;

    // create new node and link it up.  there is always one unused node.
    ftgc__append_node(chain_end);
	if (chain_end->next == NULL)
		return -1; /* oom */

    return 1;
}


// Remove the first entry in a chain.
//
// Internally, this invalidates the data but does not free any memory.
FTGCDEF void
ftgc_hashindex_remove_first(ftgc_hashindex_s *ctx, ftgc_hashkey_t key)
{
    FTGC__ASSERT(key <= ctx->table_size);
    FTGC__ASSERT(key >= 0);
    
    ctx->table[key] = FTGC_HASHINDEX_DELETED;
}


// Get the first entry in this chain of the hash table.
// If FTGC_HASHINDEX_UNUSED is returned, this table entry is not used.
// This means no subsequent entries were used, either.
//
// If FTGC_HASHINDEX_DELETED is returned, the table entry was used
// and has been deleted.  Subsequent entries in the chain could possibly
// have values.
//
// Call ftgc_hashindex_iter_get_first() with an iterator once, and then
// call ftgc_hashindex_iter_get_next() from thereon out.
//
// Pass NULL for *iter if you just want the first entry and do not intend
// to call ftgc_hashindex_iter_get_next()
FTGCDEF int
ftgc_hashindex_iter_get_first(const ftgc_hashindex_s *ctx,
                              ftgc_hashindex_iter_s *iter, ftgc_hashkey_t key)
{
    FTGC__ASSERT(key >= 0);
    FTGC__ASSERT(key <= ctx->table_size);

    if (iter)
    {
        iter->current_node = &ctx->chain[key];
        iter->prev_node = NULL;
    }

    return ctx->table[key];
}

// Get the next entry for the current horizontal set of the hash chain.
//
// If FTGC_HASHINDEX_UNUSED is returned, this table entry is not used.
// This means no subsequent entries were used, either.
//
// If HASHINDEX_DELETED is returned, the table entry was used and has
// been deleted.  Subsequent entries in the chain could possibly have
// values.
//
// Returns the value, FTGC_HASHINDEX_UNUSED or FTGC_HASHINDEX_DELETED.
FTGCDEF int
ftgc_hashindex_iter_get_next(ftgc_hashindex_iter_s *iter)
{
    // Skip over all deleted entries.
    while (iter->current_node)
    {
        if (iter->current_node->value != FTGC_HASHINDEX_DELETED)
            break;

        iter->current_node = iter->current_node->next;
    }

    // this should never be reached; we always have an empty node waiting to be
    // filled at the next call to add().  The return at the bottom of the function
    // should return -1 which signifies there is no more chained data.
    if (iter->current_node == NULL)
        return -1;

    iter->prev_node = iter->current_node;
    iter->current_node = iter->current_node->next;
    
    return iter->prev_node->value;
}

// Remove the current node of the linked list.  This should only be
// called after get_next() during an iteration loop.
//
// Calling this does not advance the iterator.  A call to get_next()
// is still necessary to get the next node.
FTGCDEF void
ftgc_hashindex_iter_remove_current(ftgc_hashindex_iter_s *iter)
{
    FTGC__ASSERT(iter->prev_node != NULL);

    iter->prev_node->value = FTGC_HASHINDEX_DELETED;
}

//
// variant
// 

#define FTGC__VARIANT_ZERO(x) {x->val_float3[0] =                   \
                               x->val_float3[1] =                   \
                               x->val_float3[2] = 0.0f;             \
        x->val_str.internal_storage = NULL;                         \
        x->val_str.external_storage = NULL;}

#define FTGC__VARIANT_CLEAR(x) {                        \
    if (x->field_type == FTGC_VARTYPE_STRING &&         \
        x->val_str.internal_storage)                    \
    {                                                   \
        FTGC_FREE(x->val_str.internal_storage);         \
    }                                                   \
                                                        \
    FTGC__VARIANT_ZERO(x);                              \
    x->field_type = FTGC_VARTYPE_VOID;                  \
    }

        

FTGCDEF void
ftgc_variant_init(ftgc_variant_s *ctx)
{
    FTGC__VARIANT_ZERO(ctx);
    ctx->field_type = FTGC_VARTYPE_VOID;
}

FTGCDEF void
ftgc_variant_free(ftgc_variant_s *ctx)
{
    if (ctx->field_type == FTGC_VARTYPE_STRING &&
        ctx->val_str.internal_storage)
    {
        FTGC_FREE(ctx->val_str.internal_storage);
    }

    ftgc_variant_init(ctx);
}

FTGCDEF void
ftgc_variant_set_void_ptr(ftgc_variant_s *ctx, void *ptr)
{
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_VOIDPTR;
    ctx->val_ptr = ptr;
}

FTGCDEF void
ftgc_variant_set_bool(ftgc_variant_s *ctx, int v)
{
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_BOOL;
    ctx->val_bool = (v!=0);
}

FTGCDEF void
ftgc_variant_set_sint32(ftgc_variant_s *ctx, ftgc_sint32 v)
{
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_SINT32;
    ctx->val_sint32 = v;
}

FTGCDEF void
ftgc_variant_set_uint32(ftgc_variant_s *ctx, ftgc_uint32 v)
{
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_UINT32;
    ctx->val_uint32 = v;
}

FTGCDEF void
ftgc_variant_set_float(ftgc_variant_s *ctx, float v)
{
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_FLOAT;
    ctx->val_float3[0] = v;
}

FTGCDEF void
ftgc_variant_set_vec2(ftgc_variant_s *ctx, float *v)
{
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_VEC2;
    ctx->val_float3[0] = v[0];
    ctx->val_float3[1] = v[1];
}

FTGCDEF void
ftgc_variant_set_vec3(ftgc_variant_s *ctx, float *v)
{
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_VEC3;
    ctx->val_float3[0] = v[0];
    ctx->val_float3[1] = v[1];
    ctx->val_float3[2] = v[2];
}

// copy *str to variant, allocating the appropriate memory
// returns false if alloc failed
FTGCDEF int
ftgc_variant_set_string(ftgc_variant_s *ctx, const char *str)
{
    size_t bytes;
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_STRING;

    bytes = strlen(str)+1;
    ctx->val_str.internal_storage = (char*)FTGC_MALLOC(sizeof(char)*bytes);
	if (!ctx->val_str.internal_storage)
	{
		FTGC__ASSERT(ctx->val_str.internal_storage);
		return 0;
	}

    memcpy(ctx->val_str.internal_storage, str, bytes);
	return 1;
}

// store the pointer to p_str internally, not allocating any memory
FTGCDEF void
ftgc_variant_set_string_ptr(ftgc_variant_s *ctx, const char *p_str)
{
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = FTGC_VARTYPE_STRING;

    ctx->val_str.external_storage = p_str;
    ctx->val_str.internal_storage = NULL;
}

FTGCDEF void
ftgc_variant_set_from_variant(ftgc_variant_s *ctx,
                              const ftgc_variant_s *other)
{
    FTGC__ASSERT(ctx != other);
    FTGC__VARIANT_CLEAR(ctx);
    ctx->field_type = other->field_type;

    if (other->field_type == FTGC_VARTYPE_STRING &&
        other->val_str.internal_storage)
    {
        ftgc_variant_set_string(ctx, other->val_str.internal_storage);
    }
    else
    {
        memcpy(ctx, other, sizeof(ftgc_variant_s));
    }

}


FTGCDEF void *
ftgc_variant_get_void_ptr(const ftgc_variant_s *ctx)
{
    FTGC__ASSERT(ctx->field_type == FTGC_VARTYPE_VOIDPTR);
    return ctx->val_ptr;
}

FTGCDEF int
ftgc_variant_get_bool(const ftgc_variant_s *ctx)
{
    FTGC__ASSERT(ctx->field_type == FTGC_VARTYPE_BOOL);
    return ctx->val_bool;
}

FTGCDEF int
ftgc_variant_get_sint32(const ftgc_variant_s *ctx)
{
    FTGC__ASSERT(ctx->field_type == FTGC_VARTYPE_SINT32);
    return ctx->val_sint32;
}

FTGCDEF int
ftgc_variant_get_uint32(const ftgc_variant_s *ctx)
{
    FTGC__ASSERT(ctx->field_type == FTGC_VARTYPE_UINT32);
    return ctx->val_uint32;
}

FTGCDEF float
ftgc_variant_get_float(const ftgc_variant_s *ctx)
{
    FTGC__ASSERT(ctx->field_type == FTGC_VARTYPE_FLOAT);
    return ctx->val_float3[0];
}

FTGCDEF float*
ftgc_variant_get_vec2(const ftgc_variant_s *ctx)
{
    FTGC__ASSERT(ctx->field_type == FTGC_VARTYPE_VEC2);
    return (float*)&ctx->val_float3[0];
}

FTGCDEF float*
ftgc_variant_get_vec3(const ftgc_variant_s *ctx)
{
    FTGC__ASSERT(ctx->field_type == FTGC_VARTYPE_VEC3);
    return (float*)&ctx->val_float3[0];
}

FTGCDEF char *
ftgc_variant_get_string(const ftgc_variant_s *ctx)
{
    FTGC__ASSERT(ctx->field_type == FTGC_VARTYPE_STRING);
    if (ctx->val_str.external_storage)
        return (char*)ctx->val_str.external_storage;
    else
        return (char*)ctx->val_str.internal_storage;
}

// size - the initial size of the pair db; automatically
// reallocated if overflowed.
// 
// hash_size - size of the vertical hash table
//
// size is an upper limit, hash_size is a performance falloff range
FTGCDEF void
ftgc_dict_init(ftgc_dict_s *ctx, int size, int hash_size)
{
    int i;
    FTGC__ASSERT(size >= hash_size);
    if (hash_size < size)
        hash_size = size;

    ctx->dict_size = size;
    ctx->num_pairs = 0;

    // fixme: what is the right initial size for the hashindex vert_size?
    ftgc_hashindex_init(&ctx->hash_index, ctx->dict_size);

    // todo: compress this?
    ctx->pairs.keys = (char*)
        FTGC_MALLOC(sizeof(char) * FTGC_DICT_KEY_BYTES * ctx->dict_size);
    
    ctx->pairs.values = (ftgc_variant_s*)
        FTGC_MALLOC(sizeof(ftgc_variant_s) * ctx->dict_size);
    
    for (i = 0; i < ctx->dict_size; i++) {
        ftgc_variant_init(&ctx->pairs.values[i]);
    }
}

FTGCDEF void
ftgc_dict_free(ftgc_dict_s *ctx)
{
    if (ctx->pairs.keys)
    {
        FTGC_FREE(ctx->pairs.keys);
    }

    if (ctx->pairs.values)
    {
        FTGC_FREE(ctx->pairs.values);
    }

    ftgc_hashindex_free(&ctx->hash_index);
}


static int
ftgc__keycmp(const char *s1, const char *s2)
{
#if FTGC_DICT_CASE_SENSITIVE
    return strcmp(s1, s2);
#else
    /* ascii case folded string compare */
	int result;
	const char *p1;
	const char *p2;
    if ( s1==s2)
        return 0;
    
    p1 = s1;
    p2 = s2;
    result = 0;
    if ( p1 == p2 )
        return result;
    
    while (!result)
    {
        result = tolower(*p1) - tolower(*p2);
        if ( *p1 == '\0' )
            break;
        ++p1;
        ++p2;
    }
    
    return result;
#endif    
}


// Finds a key string that matches in the hashindex
static int
ftgc__dict_find_index_for_key(const ftgc_dict_s *ctx, const char *key)
{
    int i;
    ftgc_hashindex_iter_s iter;    
    int hash = ftgc_hashindex_generate_key_string(&ctx->hash_index,
                                                  key);

    for (i = ftgc_hashindex_iter_get_first(&ctx->hash_index, &iter, hash);
         i != FTGC_HASHINDEX_UNUSED;
         i = ftgc_hashindex_iter_get_next(&iter))
    {
        if (i == FTGC_HASHINDEX_DELETED)
            continue;

        if (ftgc__keycmp(key, &ctx->pairs.keys[i*FTGC_DICT_KEY_BYTES])==0)
            return i;
    }

    return FTGC_HASHINDEX_UNUSED;
}

// Reallocate the key/value pair array to a new, larger size.
// Retains the contents therein.
// Returns 0 on alloc failure.
static int
ftgc__dict_reallocate(ftgc_dict_s *ctx, int new_size)
{
    int i;
    char *new_keys;
    ftgc_variant_s *new_values;
    
    FTGC__ASSERT(ctx->dict_size < new_size);

    new_keys = (char*)FTGC_REALLOC(ctx->pairs.keys,
                                   sizeof(char)*FTGC_DICT_KEY_BYTES*
                                   new_size);
    if (!new_keys)
    {
        FTGC__ASSERT(new_keys);
        return 0;
    }

    new_values = (ftgc_variant_s*)FTGC_REALLOC(ctx->pairs.values,
                                               sizeof(ftgc_variant_s) *
                                               new_size);
    if (!new_values)
    {
        FTGC__ASSERT(new_values);
        return 0;
    }

    // initialize all newly allocated variants while leaving old ones intact
    for (i = ctx->dict_size; i < new_size; i++)
        ftgc_variant_init(&new_values[i]);
                                               
    ctx->pairs.keys = new_keys;
    ctx->pairs.values = new_values;
    ctx->dict_size = new_size;

    return 1;
}

// find an empty slot in the pairs array.
static int
ftgc__dict_find_empty_slot(ftgc_dict_s *ctx)
{
    int i;
    for (i = 0; i < ctx->num_pairs; i++)
    {
        // this only happens if it was being used before and is now deleted.
        if (ctx->pairs.keys[i*FTGC_DICT_KEY_BYTES] == '\0')
            return i;
    }
    
    ctx->num_pairs++;
    return ctx->num_pairs-1;
}

static void
ftgc__dict_set_key(const char *src, char *dst)
{
    int i = 0;    
    const char *p_src = src;
    
    FTGC__ASSERT(*src != '\0');

    while(*p_src && i < FTGC_DICT_KEY_BYTES-1)  {
        *dst = *p_src;
        dst++; p_src++;
        i++;
    }
    
    *dst = '\0';
}

// sets key to value, making *value a string.
// returns false on failure to set string (failure to alloc memory)
FTGCDEF int
ftgc_dict_set_string(ftgc_dict_s *ctx, const char *key, const char *value)
{
    int index, target;
    int hash;
    FTGC__ASSERT(key && value);

    // Check to see if it exists already.
    index = ftgc__dict_find_index_for_key(ctx, key);

    if (index == FTGC_HASHINDEX_UNUSED)
    {
        if (ctx->num_pairs == ctx->dict_size)
        {
            if (ftgc__dict_reallocate(ctx, sizeof(char)*
                                      (ctx->dict_size +
                                       FTGC_DICT_KEY_BYTES)) == 0)
                return 0;
        }

        target = ftgc__dict_find_empty_slot(ctx);
    }
    else
    {
        target = index;
    }

    ftgc__dict_set_key(key, &ctx->pairs.keys[target*FTGC_DICT_KEY_BYTES]);
    ftgc_variant_set_string(&ctx->pairs.values[target], value);  // fixme: return 0 if this fails
    
    // fixme: perf: this is redundant and should be returned from find index
    // for key.
    hash = ftgc_hashindex_generate_key_string(&ctx->hash_index, key);
    
    if (ftgc_hashindex_add_key(&ctx->hash_index, hash, target) == -1)
        return 0;
    
    return 1;
}

FTGCDEF const char*
ftgc_dict_get_string(const ftgc_dict_s *ctx, const char *key,
                     const char *fallback_string)
{
    int index;
    FTGC__ASSERT(key);

    index = ftgc__dict_find_index_for_key(ctx, key);
    if (index != FTGC_HASHINDEX_UNUSED)
        return ftgc_variant_get_string(&ctx->pairs.values[index]);

    return fallback_string;
}

//
// Test suite
//
// To run tests, include ftg_test.h.
// ftgc_decl_suite() should be called somewhere in the declaring C file.
// Then ftgt_run_all_tests(NULL).
//

#ifdef FTGT_TESTS_ENABLED

static int
ftgc__test_setup(void) {
    return 0; /* setup success */
}

static int
ftgc__test_teardown(void) {
    return 0;
}

static int
ftgc__test_array_basic(void)
{
    const unsigned int NUM = 3;
    unsigned int j;
    unsigned int *i = ftgc_array_init(i, NUM);
    
    FTGT_ASSERT(ftgc_array_count(i)==0);
    FTGT_ASSERT(ftgc__array_size(i)==NUM);
    for (j = 0; j < NUM+5; j++)
    {
        ftgc_array_append(i, j);
        FTGT_ASSERT(i[j] == j);
        FTGT_ASSERT(ftgc_array_last(i)==j);
        FTGT_ASSERT(ftgc_array_count(i) == j+1);

        if (j < NUM) {
            FTGT_ASSERT(ftgc__array_size(i) == NUM);
        } else {
            FTGT_ASSERT(ftgc__array_size(i) == j+1);
        }

    }

    ftgc_array_free(i);
    FTGT_ASSERT(i==NULL);

    // test NULL handling (null should be idiomatic
    // for empty)
    size_t count = ftgc_array_count(NULL);
    FTGT_ASSERT(count==0);
    ftgc_array_free(i);
    ftgc_array_append(i, 10);
    FTGT_ASSERT(i != NULL);
    FTGT_ASSERT(ftgc_array_count(i)==1);
    ftgc_array_free(i);
    FTGT_ASSERT(i == NULL);

    return ftgt_test_errorlevel();
}

static int
ftgc__test_array_realloc(void)
{
    int *i = ftgc_array_init(i, 1);
    int j = 0;

    for (j = 0; j < 50; j++)
    {
        ftgc_array_append(i, 0xFF);
    }

    ftgc_array_free(i);
    
    return ftgt_test_errorlevel();    
}

static int
ftgc__test_hashindex_basic(void)
{
    ftgc_hashkey_t key1, key2, key3;
    ftgc_hashindex_s table;
    ftgc_hashindex_iter_s iter;
    int oom;

    // create a table, using strings as keys
    ftgc_hashindex_init(&table, 32);
    
    key1 = ftgc_hashindex_generate_key_string(&table, "one");
    oom = ftgc_hashindex_add_key(&table, key1, 1);
    FTGT_ASSERT(oom != -1);
    
    key2 = ftgc_hashindex_generate_key_string(&table, "two");
    ftgc_hashindex_add_key(&table, key2, 2);
    FTGT_ASSERT(oom != -1);
    
    key3 = ftgc_hashindex_generate_key_string(&table, "three");
    ftgc_hashindex_add_key(&table, key3, 3);
    FTGT_ASSERT(oom != -1);
    
    // Iterate through the array, looking for the index to "one"
    for (key1 = ftgc_hashindex_iter_get_first(&table, &iter, key1);
         key1 != FTGC_HASHINDEX_UNUSED;
         key1 = ftgc_hashindex_iter_get_next(&iter))
    {
        if (key1 == FTGC_HASHINDEX_DELETED)
            continue;
    }

    ftgc_hashindex_free(&table);

    void *ptr = NULL;
    key1 = ftgc_hashindex_generate_key_ptr(&table, ptr);
    FTGT_ASSERT(key1 != 0);

    return ftgt_test_errorlevel();
}

static int
ftgc__test_hashindex_get_first(void)
{
    ftgc_hashindex_s table;
    ftgc_hashkey_t key;
    int value;

    ftgc_hashindex_init(&table, 128);

    key = ftgc_hashindex_generate_key_string(&table, "First");
    ftgc_hashindex_add_key(&table, key, 4096);

    value = ftgc_hashindex_iter_get_first(&table, NULL, key);
    FTGT_ASSERT(value == 4096);

    ftgc_hashindex_free(&table);

    return ftgt_test_errorlevel();
}

static int
ftgc__test_hashindex_remove_first(void)
{
    ftgc_hashindex_s table;
    ftgc_hashkey_t key;
    int value;

    ftgc_hashindex_init(&table, 128);
    
    key = ftgc_hashindex_generate_key_string(&table, "First");
    ftgc_hashindex_add_key(&table, key, 4096);

    value = ftgc_hashindex_iter_get_first(&table, NULL, key);
    FTGT_ASSERT(value == 4096);

    // Now, remove it.
    ftgc_hashindex_remove_first(&table, key);

    // check it.
    value = ftgc_hashindex_iter_get_first(&table, NULL, key);
    FTGT_ASSERT(value == FTGC_HASHINDEX_DELETED);

    ftgc_hashindex_free(&table);
    
    return ftgt_test_errorlevel();
}

static int
ftgc__test_hashindex_get_next(void)
{
    ftgc_hashkey_t key;
    ftgc_hashkey_t collide_key = FTGC_HASHINDEX_UNUSED;
    int collide_value = 0;
    int collision_count = 0;
    ftgc_hashindex_s tiny_table;
    int i;

    int found_collision_value = 0;
    int resolution_steps = 0;
    ftgc_hashindex_iter_s iter;
    
    ftgc_hashindex_init(&tiny_table, 4);

    // insert 32 keys into the tiny table, forcing collisions.
    for (i = 0; i < 32; i++)
    {
        int value = 1000 - i;
        int collision = 0;

        key = ftgc_hashindex_generate_key_int(&tiny_table, value);
        collision = ftgc_hashindex_add_key(&tiny_table, key, value);

        // Count the collisions, but also take one of the values that
        // collided and store it so we can find it below.  The last one
        // that collides is the one we will be testing below.
        if (collision)
        {
            collision_count++;

            collide_key = key;
            collide_value = value;
        }
    }

    // If there was no collision, there was an error.  Together, these confirm
    // that there was a collision.
    FTGT_ASSERT(collision_count > 0);
    FTGT_ASSERT(collide_key != FTGC_HASHINDEX_UNUSED);

    // Look for the collision value
    for (key = ftgc_hashindex_iter_get_first(&tiny_table, &iter, key);
         key != FTGC_HASHINDEX_UNUSED;
         key = ftgc_hashindex_iter_get_next(&iter))
    {
        if (key == FTGC_HASHINDEX_DELETED)
            continue;

        // we know this is a used key, so this would be crazy.
        FTGT_ASSERT(key != FTGC_HASHINDEX_UNUSED);

        if (key == collide_value)
        {
            found_collision_value = 1;
            break;
        }

        resolution_steps++;
    }

    // Confirms that calling get_next() eventually found the collision value.
    FTGT_ASSERT(found_collision_value);
    // Confirms that we had to "dig" for it (get_first() was not enough)
    FTGT_ASSERT(resolution_steps >= 1);

    ftgc_hashindex_free(&tiny_table);
    
    return ftgt_test_errorlevel();
}

FTGCDEF int
ftgc__test_variant_basic(void)
{
    ftgc_variant_s var, other;
    float v[] = {1.0f, 2.0f, 3.0f};
    float *w;
    const char *p_str = "Point to me";
    
    ftgc_variant_init(&var);
    ftgc_variant_init(&other);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_VOID);

    ftgc_variant_set_void_ptr(&var, (void*)4096);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_VOIDPTR);
    FTGT_ASSERT(4096==(size_t)ftgc_variant_get_void_ptr(&var));

    ftgc_variant_set_bool(&var, 2);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_BOOL);
    FTGT_ASSERT(ftgc_variant_get_bool(&var)==1);
    ftgc_variant_set_bool(&var, 0);
    FTGT_ASSERT(ftgc_variant_get_bool(&var)==0);

    ftgc_variant_set_from_variant(&other, &var);

    ftgc_variant_set_sint32(&var, -4096);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_SINT32);
    FTGT_ASSERT(ftgc_variant_get_sint32(&var)==-4096);

    ftgc_variant_set_uint32(&var, 0xFF00);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_UINT32);
    FTGT_ASSERT(ftgc_variant_get_uint32(&var) == 0xFF00);

    ftgc_variant_set_float(&var, 1.0f);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_FLOAT);
    FTGT_ASSERT(ftgc_variant_get_float(&var) == 1.0f);

    ftgc_variant_set_vec2(&var, v);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_VEC2);
    w = ftgc_variant_get_vec2(&var);
    FTGT_ASSERT(v[0]==w[0] && v[1]==w[1]);

    ftgc_variant_set_vec3(&var, v);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_VEC3);
    w = ftgc_variant_get_vec3(&var);
    FTGT_ASSERT(v[0]==w[0] && v[1]==w[1] && v[2]==w[2]);

    ftgc_variant_set_string_ptr(&var, p_str);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_STRING);
    FTGT_ASSERT(strcmp(p_str, ftgc_variant_get_string(&var))==0);
    FTGT_ASSERT(ftgc_variant_get_string(&var) == p_str);

    ftgc_variant_set_string(&var, p_str);
    FTGT_ASSERT(var.field_type == FTGC_VARTYPE_STRING);
    FTGT_ASSERT(strcmp(p_str, ftgc_variant_get_string(&var))==0);
    FTGT_ASSERT(ftgc_variant_get_string(&var) != p_str);    

    FTGT_ASSERT(ftgc_variant_get_bool(&other) == 0);
    ftgc_variant_set_from_variant(&other, &var);
    FTGT_ASSERT(strcmp(ftgc_variant_get_string(&other), p_str)==0);
    
    ftgc_variant_free(&var);
    ftgc_variant_free(&other);
    
    return ftgt_test_errorlevel();
}

FTGCDEF int
ftgc__test_dict_basic(void)
{
    ftgc_dict_s dict;
    const char *returned;
    int result;

    ftgc_dict_init(&dict, 128, 32);
    result = ftgc_dict_set_string(&dict, "mr.key", "mr.value");
    returned = ftgc_dict_get_string(&dict, "mr.key", NULL);
    FTGT_ASSERT(result);
    FTGT_ASSERT(returned && strcmp(ftgc_dict_get_string(&dict, "mr.key", NULL),
                       "mr.value")==0);

    ftgc_dict_free(&dict);
    
    return ftgt_test_errorlevel();
}

FTGCDEF int
ftgc__test_dict_force_overflow(void)
{
    /* force an overflow of the contained hashindex, triggering a realloc. 
       size of 1 means second key is a realloc */
    ftgc_dict_s dict;
    const char *returned;
    int result;
	int i;
	char num_key[16], num_val[16];

    ftgc_dict_init(&dict, 4, 4);

	for (i = 0; i < 64; i++)
    {
		sprintf(num_key, "%d", i);
		sprintf(num_val, "num %d", i);
		result = ftgc_dict_set_string(&dict, num_key, num_val);
		returned = ftgc_dict_get_string(&dict, num_key, NULL);

		FTGT_ASSERT(result);
		FTGT_ASSERT(returned && strcmp(returned, num_val) == 0);
	}

    ftgc_dict_free(&dict);

    return ftgt_test_errorlevel();    
}

FTGCDEF
void ftgc_decl_suite(void)
{
    ftgt_suite_s *suite = ftgt_create_suite(NULL, "ftg_containers",
                                            ftgc__test_setup, ftgc__test_teardown);
    FTGT_ADD_TEST(suite, ftgc__test_array_basic);
    FTGT_ADD_TEST(suite, ftgc__test_array_realloc);
    FTGT_ADD_TEST(suite, ftgc__test_hashindex_basic);
    FTGT_ADD_TEST(suite, ftgc__test_hashindex_get_first);
    FTGT_ADD_TEST(suite, ftgc__test_hashindex_remove_first);
    FTGT_ADD_TEST(suite, ftgc__test_hashindex_get_next);
    FTGT_ADD_TEST(suite, ftgc__test_variant_basic);
    FTGT_ADD_TEST(suite, ftgc__test_dict_basic);
    FTGT_ADD_TEST(suite, ftgc__test_dict_force_overflow);
}

#endif /* FTGT_TESTS_ENABLED */
#endif /* defined(FTG_IMPLEMENT_CONTAINERS) */

