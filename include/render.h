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


struct termbuf {
	struct termchar     *termbox;      // Glyphs buffer
	uvec2_t             dim;           // Dimensions (no. of chars)
	uvec2_t             cursor;        // Current cursor position
	const struct glyph  *cursor_glyph; // Glyph to draw for cursor
	bool                cursor_vis;    // Is cursor supposed to be visible?
	unsigned            toprow;        // Topmost row (prevent memcpy)
};


struct renderer {
	// Terminal screens (double buffering)
	struct termbuf      *draw_buf;     // Buffer to draw
	struct termbuf      *mod_buf;      // Buffer to modify
	pthread_mutex_t     buf_mut;       // Mutex for swapping buffers
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
	const struct color  *palette;      // Standard palette of 16 colors
};


// Create a new renderer
struct renderer *renderer_new(struct window *w, struct fonts *f, const char *fg, const char *bg, uint32_t cursor, const struct color *palette);

// Free renderer resources
void renderer_free(struct renderer *renderer);

// Denote that the renderer should render the current scene. Not guaranteed to be carried out
// immediately
void renderer_render(struct renderer *renderer);

// Do whatever the renderer needs to do
void renderer_update(struct renderer *renderer);

// Add codepoints to renderer. Return number of codepoints added
size_t renderer_add_codepoints(struct renderer *renderer, uint32_t *cps, size_t n_cps);

// Resize renderer to match window (called by window subsystem)
uvec2_t renderer_resize(struct renderer *renderer);


#endif // __BTE_RENDER_H__
