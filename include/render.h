#ifndef __BTE_RENDER_H__
#define __BTE_RENDER_H__


#include <pthread.h>
#include <inttypes.h>

#include "util.h"
#include "fonts.h"
#include "window.h"


enum renderer_clear_type {
	RENDERER_CLEAR_TO_END = 0,
	RENDERER_CLEAR_FROM_BEG = 1,
	RENDERER_CLEAR_ALL = 2,
};


struct termchar {
	const struct glyph *glyph;  // Glyph to draw
	vec4_t             fgcol;   // Foreground color
	vec4_t             bgcol;   // Background color
	bool               to_draw; // Is this to be rendered?
};


struct renderer {
	// Terminal box
	struct termchar     *termbox;      // Glyphs buffer
	uvec2_t             dim;           // Dimensions (no. of chars)
	uvec2_t             cursor;        // Current cursor position
	const struct glyph  *cursor_glyph; // Glyph to draw for cursor
	bool                cursor_vis;    // Is cursor supposed to be visible?
	unsigned            toprow;        // Topmost row (prevent memcpy)
	// Pointers to other systems
	struct window       *window;       // Pointer to window (not owned)
	struct fonts        *fonts;        // Pointer to fonts subsystem (not owned)
	// OpenGL stuff
	GLuint              VAO_text;
	GLuint              VBO_text;
	GLuint              text_shader;   // Shader program for text
	GLuint              VAO_bg;
	GLuint              VBO_bg;
	GLuint              bg_shader;     // Shader program for background
	// Misc
	vec4_t              fgcol;         // Normalized foreground color
	vec4_t              bgcol;         // Normalized background color
	vec4_t              default_fgcol; // Normalized default foreground color
	vec4_t              default_bgcol; // Normalized default background color
	bool                req_render;    // Has an updated render been requested?
	pthread_mutex_t     mut;           // Mutex for general access to renderer
	pthread_mutex_t     size_mut;      // Mutex for resizing
};


// Create a new renderer
struct renderer *renderer_new(struct window *w, struct fonts *f, const char *fg, const char *bg, uint32_t cursor);

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

// Set renderer foreground color
void renderer_set_fgcol(struct renderer *renderer, const struct color *color);

// Set renderer background color
void renderer_set_bgcol(struct renderer *renderer, const struct color *color);

// Reset renderer foreground color
void renderer_reset_fgcol(struct renderer *renderer);

// Reset renderer background color
void renderer_reset_bgcol(struct renderer *renderer);


#endif // __BTE_RENDER_H__
