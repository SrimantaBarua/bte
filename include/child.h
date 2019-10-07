#ifndef __BTE_CHILD_H__
#define __BTE_CHILD_H__


#include <pthread.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>

#include "color.h"
#include "render.h"
#include "window.h"


struct child {
	// Handles
	pid_t           pid;         // PID of child process
	int             fd;          // File descriptor for talking to child
	pthread_t       tid;         // Thread handle for reading from child
	// Pointers to other subsystems
	struct renderer *renderer;   // Renderer subsystem (not owned)
	struct window   *window;     // Window subsystem (not owned)
};


// Initialize new child
struct child* child_new(const char **argv, const char **envp, struct renderer *r, struct window *w);

// Shutdown child
void child_fini(struct child *child);

// Callback for unicode codepoints (called by window)
void child_char_cb(struct child *child, uint32_t codepoint);

// Callback for resize
void child_resize_cb(struct child *child, uvec2_t dim);

// Callback for other keypresses (called by window)
void child_key_cb(struct child *child, int key, int mods);


#endif // __BTE_CHILD_H__
