#include "glad/glad.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

#include "util.h"


// -------- OPENGL ---------------


// Check GL errors and die on error
void _gl_check_error(const char *file, const char *func, int line) {
	GLenum errcode;
	const char *msg = "unknown";
	while ((errcode = glGetError()) != GL_NO_ERROR) {
		switch (errcode) {
		case GL_INVALID_ENUM:
			msg = "INVALID_ENUM";
			break;
		case GL_INVALID_VALUE:
			msg = "INVALID_VALUE";
			break;
		case GL_INVALID_OPERATION:
			msg = "INVALID_OPERATION";
			break;
		case GL_OUT_OF_MEMORY:
			msg = "OUT_OF_MEMORY";
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			msg = "INVALID_FRAMEBUFFER_OPERATION";
			break;
		}
		fprintf(stderr, "OpenGL error: %s:%s:%d: code: %d: %s\n", file, func, line,
			errcode, msg);
		exit(1);
	}
}


// -------- HASH TABLE ----------------


#define HTU32_BLKSZ 128


static uint32_t _hash_u32(uint32_t x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x);
	return x;
}


// Bucket in hash table
struct htu32_bkt {
	void     *v; // Value
	uint32_t k;  // Actual key
	bool     p;  // Is entry present?
};


// Hash table with u32 keys
struct htu32 {
	struct htu32_bkt *b;  // Buckets
	size_t           sz;  // Number of entries
	size_t           cap; // Allocated capacity
};


// Initialize a new hash table
struct htu32* htu32_new() {
	struct htu32 *ht;
	if (!(ht = calloc(1, sizeof(struct htu32)))) {
		die_err("calloc()");
	}
	return ht;
}


// Free hash table. If free_cb is provided, use it to free items
void htu32_free(struct htu32 *ht, free_cb_t free_cb) {
	size_t i;
	if (!ht) {
		warn("NULL ht");
		return;
	}
	if (free_cb) {
		for (i = 0; i < ht->cap; i++) {
			if (ht->b[i].p) {
				free_cb(ht->b[i].v);
			}
		}
	}
	free(ht->b);
	free(ht);
}


// Insert key-value pair into hash table
enum htres htu32_set(struct htu32 *ht, uint32_t k, void *v) {
	struct htu32_bkt *tmp;
	uint32_t i, j;
	if (!ht) {
		die("NULL ht");
	}
	if (ht->sz >= ht->cap / 2) {
		// Resize and rehash
		if (!(tmp = calloc(ht->cap + HTU32_BLKSZ, sizeof(struct htu32)))) {
			die_err("calloc()");
		}
		for (i = 0; i < ht->cap; i++) {
			if (ht->b[i].p) {
				j = _hash_u32(ht->b[i].k) % (ht->cap + HTU32_BLKSZ);
				for ( ; tmp[j].p; j = (j + 1) % (ht->cap + HTU32_BLKSZ));
				tmp[j] = ht->b[i];
			}
		}
		free(ht->b);
		ht->b = tmp;
		ht->cap += HTU32_BLKSZ;
	}
	for (i = _hash_u32(k) % ht->cap; ht->b[i].p; i = (i + 1) % ht->cap) {
		if (ht->b[i].k == k) {
			return HTRES_EXISTS;
		}
	}
	ht->b[i].k = k;
	ht->b[i].v = v;
	ht->b[i].p = true;
	ht->sz++;
	return HTRES_OK;
}


// Get value for key. If res is not NULL, set res to result
void* htu32_get(struct htu32 *ht, uint32_t k, enum htres *res) {
	uint32_t i;
	if (!ht) {
		die("NULL ht");
	}
	if (ht->sz == 0) {
		if (res) {
			*res = HTRES_EMPTY;
		}
		return NULL;
	}
	for (i = _hash_u32(k) % ht->cap; ht->b[i].p; i = (i + 1) % ht->cap) {
		if (ht->b[i].k == k) {
			if (res) {
				*res = HTRES_OK;
			}
			return ht->b[i].v;
		}
	}
	if (res) {
		*res = HTRES_NOKEY;
	}
	return NULL;
}


// Get value for key, and remove it from table. If res is not NULL, set res to result
void* htu32_pop(struct htu32 *ht, uint32_t k, enum htres *res) {
	uint32_t i;
	if (!ht) {
		die("NULL ht");
	}
	if (ht->sz == 0) {
		if (res) {
			*res = HTRES_EMPTY;
		}
		return NULL;
	}
	for (i = _hash_u32(k) % ht->cap; ht->b[i].p; i = (i + 1) % ht->cap) {
		if (ht->b[i].k == k) {
			if (res) {
				*res = HTRES_OK;
			}
			ht->b[i].p = false;
			ht->sz--;
			return ht->b[i].v;
		}
	}
	if (res) {
		*res = HTRES_NOKEY;
	}
	return NULL;
}


// -------- LINKED LIST ----------------


// Initialize new list node
struct list *list_new(void *val) {
	struct list *list;
	if (!(list = malloc(sizeof(struct list)))) {
		die_err("malloc()");
	}
	list->val = val;
	list->next = NULL;
	return list;
}


// Free list. If free_cb is not NULL, use it to free items
void list_free(struct list *list, free_cb_t free_cb) {
	if (!list) {
		return;
	}
	list_free(list->next, free_cb);
	if (free_cb) {
		free_cb(list->val);
	}
	free(list);
}


// Push to front of list
struct list *list_push_front(struct list *list, void *val) {
	struct list *node = list_new(val);
	node->next = list;
	return node;
}


// Set end of list to node
static void _list_set_end(struct list *list, struct list *node) {
	if (list->next) {
		_list_set_end(list->next, node);
	} else {
		list->next = node;
	}
}


// Push to end of list
struct list *list_push_end(struct list *list, void *val) {
	struct list *node = list_new(val);
	if (!list) {
		return node;
	}
	_list_set_end(list, node);
	return list;
}


// Pop from front of list
void* list_pop_front(struct list **list) {
	struct list *tmp;
	void *val;
	if (!list) {
		die("NULL list");
	}
	tmp = *list;
	if (!tmp) {
		return NULL;
	}
	val = tmp->val;
	*list = tmp->next;
	free(tmp);
	return val;
}


// Return value of end of list
static void* _list_get_end(struct list **node) {
	struct list *tmp = *node;
	void *val;
	if (tmp->next) {
		return _list_get_end(&tmp->next);
	}
	val = tmp->val;
	free(*node);
	*node = NULL;
	return val;
}


// Pop from end of list
void* list_pop_end(struct list **list) {
	struct list *tmp;
	void *val;
	if (!list) {
		die("NULL list");
	}
	if (!*list) {
		return NULL;
	}
	return _list_get_end(list);
}
