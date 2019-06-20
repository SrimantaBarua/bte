#ifndef __BTE_WINDOW_H__
#define __BTE_WINDOW_H__


#include <GLFW/glfw3.h>

#include "util.h"


// Store information about a window
struct window {
	GLFWwindow *window; // GLFW window
	GLFWcursor *cursor; // GLFW cursor
	uvec2_t    dim;     // Window dimensions
	char       *title;  // Window title
};

// Create a new window, and initialize OpenGL context
struct window* window_new(unsigned width, unsigned height, const char *title);

// Free window resources
void window_free(struct window *window);


#endif // __BTE_WINDOW_H__
