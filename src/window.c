#include "glad/glad.h"

#include <stdlib.h>
#include <inttypes.h>

#include "window.h"


// Update projection matrix for window
static void _update_projmat(struct window *window) {
	memset(window->projmat, 0, sizeof(window->projmat));
	window->projmat[0] =  2.0 / window->dim.x;
	window->projmat[5] = 2.0 / window->dim.y;
	window->projmat[10] = -1.0f;
	window->projmat[12] = -1.0f;
	window->projmat[13] = -1.0f;
	window->projmat[15] = 1.0f;
}


// Guaranteed cleanup of GLFW
static void glfw_cleanup() {
	glfwTerminate();
}


// Callback for resize
static void _glfw_fb_resize_cb(GLFWwindow *window, int width, int height) {
	struct window *w;
	if (width <= 0 || height <= 0) {
		return;
	}
	w = (struct window*) glfwGetWindowUserPointer(window);
	w->dim.x = width;
	w->dim.y = height;
	glViewport(0, 0, width, height);
	_update_projmat(w);
	if (w->renderer) {
		renderer_resize(w->renderer);
	}
}


// Callback for keypresses
static void _glfw_key_cb(GLFWwindow *window, int key, int scancode, int action, int mods) {
	// TODO
	struct window *w = (struct window*) glfwGetWindowUserPointer(window);
	if (action == GLFW_RELEASE) {
		return;
	}
	if (w->child) {
		child_key_cb(w->child, key, mods);
	}
}


// Callback for unicode codepoints
static void _glfw_char_cb(GLFWwindow *window, uint32_t codepoint) {
	// TODO
	struct window *w = (struct window*) glfwGetWindowUserPointer(window);
	if (w->child) {
		child_char_cb(w->child, codepoint);
	}

}


// Create a new window, and initialize OpenGL context
struct window* window_new(unsigned width, unsigned height, const char *title) {
	struct window *window;
	if (!(window = calloc(1, sizeof(struct window)))) {
		die_err("calloc()");
	}
	if (!(window->title = strdup(title))) {
		die_err("strdup()");
	}
	window->dim.x = width;
	window->dim.y = height;
	// Initialize GLFW
	glfwInit();
	atexit(glfw_cleanup);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	// Create window
	if (!(window->window = glfwCreateWindow(width, height, title, NULL, NULL))) {
		die("Failedto create GLFW window");
	}
	glfwMakeContextCurrent(window->window);
	glfwSetWindowUserPointer(window->window, window);
	glfwSetFramebufferSizeCallback(window->window, _glfw_fb_resize_cb);
	glfwSetInputMode(window->window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
	glfwSetKeyCallback(window->window, _glfw_key_cb);
	glfwSetCharCallback(window->window, _glfw_char_cb);
	// Initialize cursor to be text ibeam
	if (!(window->cursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR))) {
		die("Failed to create GLFW cursor");
	}
	glfwSetCursor(window->window, window->cursor);
	// Initialize GLAD
	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		die("Failed to initialize GLAD");
	}
	// Clear out spurious errors
	while (glGetError() != GL_NO_ERROR);
	// Setup viewport
	glViewport(0, 0, width, height);
	gl_check_error();
	// Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Update projection matrix
	_update_projmat(window);
	// Return window
	return window;
}


// Set renderer pointer for window
void window_set_renderer(struct window *window, struct renderer *renderer) {
	if (!window) {
		die("NULL window");
	}
	window->renderer = renderer;
}


// Set child pointer for window
void window_set_child(struct window *window, struct child *child) {
	if (!window) {
		die("NULL window");
	}
	window->child = child;
}


// Free window resources
void window_free(struct window *window) {
	if (!window) {
		warn("NULL window");
		return;
	}
	glfwDestroyCursor(window->cursor);
	glfwDestroyWindow(window->window);
	free(window->title);
	free(window);
}


// Check whether window should close
bool window_should_close(const struct window *window) {
	if (!window) {
		die("NULL window");
	}
	return window->should_close;
}


// Set that window should close
void window_set_should_close(struct window *window) {
	if (!window) {
		die("NULL window");
	}
	window->should_close = true;
}


// Wait for events on the window and process callbacks
void window_get_events(struct window *window) {
	if (!window) {
		die("NULL window");
	}
	if (!window->should_close) {
		glfwPollEvents();
		if (glfwWindowShouldClose(window->window)) {
			window->should_close = true;
		}
	}
}


// Refresh window
void window_refresh(struct window *window) {
	if (!window) {
		die("NULL window");
	}
	if (!window->should_close) {
		glfwSwapBuffers(window->window);
	}
}
