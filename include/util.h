#ifndef __BTE_UTIL_H__
#define __BTE_UTIL_H__


#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>


// -------- OPENGL UTILITY ----------------


// Check GL errors and die on error
void _gl_check_error(const char *file, const char *func, int line);


// -------- TYPES ----------------


typedef void (*free_cb_t) (void*);


typedef struct { unsigned x, y; } uvec2_t;
typedef struct { int x, y; } ivec2_t;
typedef struct { float x, y; } vec2_t;


typedef struct { unsigned x, y, z; } uvec3_t;
typedef struct { int x, y, z; } ivec3_t;
typedef struct { float x, y, z; } vec3_t;


typedef struct { unsigned x, y, z, w; } uvec4_t;
typedef struct { int x, y, z, w; } ivec4_t;
typedef struct { float x, y, z, w; } vec4_t;


// -------- MACROS ----------------

#define warn(msg) fprintf(stderr, "WARN: %s:%s:%d: " msg "\n", __FILE__, __func__, __LINE__)

#define warn_fmt(fmt, ...) \
	fprintf(stderr, "WARN: %s:%s:%d: " fmt "\n", __FILE__, __func__, __LINE__, __VA_ARGS__)

#define warn_err(msg) \
	fprintf(stderr, "WARN: %s:%s:%d: " msg ": %s\n", __FILE__, __func__, __LINE__, strerror(errno))


#define die(msg) do { \
	fprintf(stderr, "ERR: %s:%s:%d: " msg "\n", __FILE__, __func__, __LINE__); \
	exit(1); \
} while(0)

#define die_fmt(fmt, ...) do { \
	fprintf(stderr, "ERR: %s:%s:%d: " fmt "\n", __FILE__, __func__, __LINE__, __VA_ARGS__); \
	exit(1); \
} while(0)

#define die_err(msg) do { \
	fprintf(stderr, "ERR: %s:%s:%d: " msg ": %s\n", __FILE__, __func__, __LINE__, strerror(errno)); \
	exit(1); \
} while(0)

// Macro for checking GL errors
#define gl_check_error() _gl_check_error(__FILE__, __func__, __LINE__)


// -------- HASH TABLE ----------------


// Result of hash table operations
enum htres {
	HTRES_OK,      // All okay
	HTRES_EXISTS,  // Key already exists (for set())
	HTRES_NOKEY,   // Key not found (for get(), pop())
	HTRES_EMPTY    // Hash table is empty
};

// Hash table with u32 keys
struct htu32;

// Initialize a new hash table
struct htu32* htu32_new();

// Free hash table. If free_cb is provided, use it to free items
void htu32_free(struct htu32 *ht, free_cb_t free_cb);

// Insert key-value pair into hash table
enum htres htu32_set(struct htu32 *ht, uint32_t k, void *v);

// Get value for key. If res is not NULL, set res to result
void* htu32_get(struct htu32 *ht, uint32_t k, enum htres *res);

// Get value for key, and remove it from table. If res is not NULL, set res to result
void* htu32_pop(struct htu32 *ht, uint32_t k, enum htres *res);


// -------- LINKED LIST ----------------


// Linked list
struct list {
	void        *val;
	struct list *next;
};

// Initialize new list node
struct list *list_new(void *val);

// Free list. If free_cb is not NULL, use it to free items
void list_free(struct list *list, free_cb_t free_cb);

// Push to front of list
struct list *list_push_front(struct list *list, void *val);

// Push to end of list
struct list *list_push_end(struct list *list, void *val);

// Pop from front of list
void* list_pop_front(struct list **list);

// Pop from end of list
void* list_pop_end(struct list **list);

// Iterate over list
#define list_foreach(list, node, val) \
	for ((node) = (list); (node) && ((val) = (node)->val); (node) = (node)->next)


#endif // __BTE_UTIL_H__
