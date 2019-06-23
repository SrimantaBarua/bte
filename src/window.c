#include "glad/glad.h"

#include <stdlib.h>
#include <inttypes.h>

#include "window.h"


// Guaranteed cleanup of GLFW
static void glfw_cleanup() {
	glfwTerminate();
}


// Callback for resize
static void _glfw_fb_resize_cb(GLFWwindow *window, int width, int height) {
}


// Callback for keypresses
static void _glfw_key_cb(GLFWwindow *window, int key, int scancode, int action, int mods) {
	// TODO
}


// Callback for unicode codepoints
static void _glfw_char_cb(GLFWwindow *window, uint32_t codepoint) {
	// TODO
}


// Create a new window, and initialize OpenGL context
struct window* window_new(unsigned width, unsigned height, const char *title) {
	struct window *window;
	if (!(window = malloc(sizeof(struct window)))) {
		die_err("malloc()");
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
	glViewport(0, 0, width, height);
	// Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Return window
	return window;
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
		glfwWaitEvents();
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
