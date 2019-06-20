#ifndef __BTE_UTIL_H__
#define __BTE_UTIL_H__


#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>


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


#endif // __BTE_UTIL_H__
