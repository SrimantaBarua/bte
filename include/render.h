#ifndef __BTE_RENDER_H__
#define __BTE_RENDER_H__


#include <inttypes.h>

#include "util.h"
#include "fonts.h"
#include "window.h"


enum renderer_clear_type {
	RENDERER_CLEAR_TO_END = 0,
	RENDERER_CLEAR_FROM_BEG = 1,
	RENDERER_CLEAR_ALL = 2,
};


struct renderer {
	// Terminal box
	const struct glyph  **termbox;   // Glyphs buffer
	uvec2_t             dim;         // Dimensions (no. of chars)
	uvec2_t             cursor;      // Current cursor position
	// Pointers to other systems
	struct window       *window;     // Pointer to window (not owned)
	struct fonts        *fonts;      // Pointer to fonts subsystem (not owned)
	// OpenGL stuff
	GLuint              VAO;
	GLuint              VBO;
	GLuint              text_shader; // Shader program for text
	// Misc
	vec4_t              fgcol;       // Normalized default text color
	vec4_t              bgcol;       // Normalized background color
	bool                req_render;  // Has an updated render been requested?
};


// Create a new renderer
struct renderer *renderer_new(struct window *w, struct fonts *f, const char *fg, const char *bg);

// Free renderer resources
void renderer_free(struct renderer *renderer);

// Denote that the renderer should render the current scene. Not guaranteed to be carried out
// immediately
void renderer_render(struct renderer *renderer);

// Do whatever the renderer needs to do
void renderer_update(struct renderer *renderer);

// Add character
void renderer_add_codepoint(struct renderer *renderer, uint32_t codepoint);

// Move cursor up by n
void renderer_move_up(struct renderer *renderer, unsigned n);

// Move cursor down by n
void renderer_move_down(struct renderer *renderer, unsigned n);

// Move cursor right by n
void renderer_move_right(struct renderer *renderer, unsigned n);

// Move cursor left by n
void renderer_move_left(struct renderer *renderer, unsigned n);

// Move cursor to (x, y)
void renderer_move_yx(struct renderer *renderer, unsigned y, unsigned x);

// Clear screen
void renderer_clear_screen(struct renderer *renderer, enum renderer_clear_type type);

// Clear line
void renderer_clear_line(struct renderer *renderer, enum renderer_clear_type type);

// Resize renderer to match window (called by window subsystem)
void renderer_resize(struct renderer *renderer);


#endif // __BTE_RENDER_H__
