#ifndef __BTE_WINDOW_H__
#define __BTE_WINDOW_H__


#include <GLFW/glfw3.h>
#include <stdbool.h>

#include "util.h"
#include "render.h"


// Store information about a window
struct window {
	GLFWwindow      *window;      // GLFW window
	GLFWcursor      *cursor;      // GLFW cursor
	uvec2_t         dim;          // Window dimensions
	char            *title;       // Window title
	bool            should_close; // Whether window should close
	float           projmat[16];  // Projection matrix
	struct renderer *renderer;    // Pointer to renderer (not owned);
};

// Create a new window, and initialize OpenGL context
struct window* window_new(unsigned width, unsigned height, const char *title);

// Set renderer pointer for window
void window_set_renderer(struct window *window, struct renderer *renderer);

// Check whether window should close
bool window_should_close(const struct window *window);

// Set that window should close
void window_set_should_close(struct window *window);

// Wait for events on the window and process callbacks
void window_get_events(struct window *window);

// Refresh window
void window_refresh(struct window *window);

// Free window resources
void window_free(struct window *window);


#endif // __BTE_WINDOW_H__
