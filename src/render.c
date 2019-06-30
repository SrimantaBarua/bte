#include "glad/glad.h"

#include <stdlib.h>

#include "util.h"
#include "color.h"
#include "render.h"


// Vertex shader for text
const char *vtxtsrc =
"#version 330 core\n"
"layout (location = 0) in vec4 vertex;\n"
"out vec2 tex_coords;\n"
"uniform mat4 projection;\n" // <vec2 pos, vec2 tex>
"void main() {\n"
"  gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
"  tex_coords = vertex.zw;\n"
"}";


// Fragment shader for text
const char *ftxtsrc =
"#version 330 core\n"
"in vec2 tex_coords;\n"
"out vec4 color;\n"
"uniform sampler2D text;\n"
"uniform vec3 text_color;\n"
"void main() {\n"
"  vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, tex_coords).r);\n"
"  color = vec4(text_color, 1.0) * sampled;\n"
"}";


// Vertex shader for background
const char *vbgsrc =
"#version 330 core\n"
"layout (location = 0) in vec2 vertex;\n"
"uniform mat4 projection;\n" // <vec2 pos, vec2 tex>
"void main() {\n"
"  gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
"}";


// Fragment shader for background
const char *fbgsrc =
"#version 330 core\n"
"out vec4 color;\n"
"uniform vec4 bg_color;\n"
"void main() {\n"
"  color = bg_color;\n"
"}";




#define BTE_TABSZ 8


// Compile and link vertex and fragment shaders
static GLuint _load_shaders(const char *vsrc, const char *fsrc) {
	GLuint vsh, fsh, prog;
	int success;
	char info_log[512];
	// Vertex shader
	vsh = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vsh, 1, &vsrc, NULL);
	glCompileShader(vsh);
	glGetShaderiv(vsh, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vsh, sizeof(info_log), NULL, info_log);
		die_fmt("Failed to compile vertex shader: %s\n", info_log);
	}
	// Fragment shader
	fsh = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fsh, 1, &fsrc, NULL);
	glCompileShader(fsh);
	glGetShaderiv(fsh, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fsh, sizeof(info_log), NULL, info_log);
		die_fmt("Failed to compile fragment shader: %s\n", info_log);
	}
	// Program
	prog = glCreateProgram();
	glAttachShader(prog, vsh);
	glAttachShader(prog, fsh);
	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(prog, sizeof(info_log), NULL, info_log);
		die_fmt("Failed to link program shader: %s\n", info_log);
	}
	// Delete individual shaders
	glDeleteShader(vsh);
	glDeleteShader(fsh);
	return prog;
}


// Create a new renderer
struct renderer *renderer_new(struct window *w, struct fonts *f, const char *fg, const char *bg, uint32_t cursor) {
	struct renderer *r;
	struct color fgc, bgc;

	if (!w) {
		die("NULL window");
	}
	if (!f) {
		die("NULL fonts");
	}

	if (!(r = malloc(sizeof(struct renderer)))) {
		die_err("malloc()");
	}
	// Get foreground color
	if (!color_parse(&fgc, fg)) {
		die_fmt("Unable to parse foreground color: %s", fg);
	}
	color_normalize(&fgc, &r->default_fgcol);
	// Get background color
	if (!color_parse(&bgc, bg)) {
		die_fmt("Unable to parse foreground color: %s", bg);
	}
	r->fgcol = r->default_fgcol;
	color_normalize(&bgc, &r->default_bgcol);
	// Fill dimensions
	r->dim.x = w->dim.x / f->advance.x;
	r->dim.y = w->dim.y / f->advance.y;
	// Allocate terminal box
	if (!(r->termbox = calloc(r->dim.x * (r->dim.y + 1), sizeof(struct termchar)))) {
		die_err("calloc()");
	}
	r->bgcol = r->default_bgcol;
	// Initialize cursor
	r->cursor.x = 0;
	r->cursor.y = 0;
	// Initialize top row
	r->toprow = 0;
	// Set pointers
	r->window = w;
	r->fonts = f;
	// Compile and link shaders
	r->text_shader = _load_shaders(vtxtsrc, ftxtsrc);
	r->bg_shader = _load_shaders(vbgsrc, fbgsrc);
	// Create and initialize VAO and VBO for text
	glGenVertexArrays(1, &r->VAO_text);
	glGenBuffers(1, &r->VBO_text);
	glBindVertexArray(r->VAO_text);
	glBindBuffer(GL_ARRAY_BUFFER, r->VBO_text);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	// Create and initialize VAO and VBO for background
	glGenVertexArrays(1, &r->VAO_bg);
	glGenBuffers(1, &r->VBO_bg);
	glBindVertexArray(r->VAO_bg);
	glBindBuffer(GL_ARRAY_BUFFER, r->VBO_bg);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 2, NULL, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	// Clear window
	glClearColor(r->bgcol.x, r->bgcol.y, r->bgcol.z, r->bgcol.w);
	glClear(GL_COLOR_BUFFER_BIT);
	window_refresh(r->window);
	// Get cursor glyph and set cursor visible
	if (r->fonts) {
		r->cursor_glyph = fonts_get_glyph(r->fonts, cursor);
	}
	r->cursor_vis = true;
	// Initialize mutexes
	pthread_mutex_init(&r->mut, NULL);
	pthread_mutex_init(&r->size_mut, NULL);
	return r;
}


// Free renderer resources
void renderer_free(struct renderer *renderer) {
	if (!renderer) {
		warn("NULL renderer");
		return;
	}
	pthread_mutex_destroy(&renderer->mut);
	pthread_mutex_destroy(&renderer->size_mut);
	glDeleteBuffers(1, &renderer->VBO_bg);
	glDeleteVertexArrays(1, &renderer->VAO_bg);
	glDeleteBuffers(1, &renderer->VBO_text);
	glDeleteVertexArrays(1, &renderer->VAO_text);
	glDeleteProgram(renderer->text_shader);
	glDeleteProgram(renderer->bg_shader);
	free(renderer->termbox);
	free(renderer);
}


// Render glyph
static void _render_glyph(struct renderer *r, unsigned i, unsigned j, const struct glyph *glyph) {
	GLfloat vertices[6][4] = {
		{ 0.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f },
	};
	GLfloat xpos, ypos, width, height;
	// Load texture
	glBindTexture(GL_TEXTURE_2D, glyph->tex);
	// Calculate dimensions
	xpos = (j * r->fonts->advance.x + glyph->bearing.x);
	ypos = i * r->fonts->advance.y + r->fonts->line_height \
	       + glyph->size.y - glyph->bearing.y;
	ypos = r->window->dim.y - ypos;
	width = glyph->size.x;
	height = glyph->size.y;
	// Update vertices
	vertices[0][0] = xpos;
	vertices[0][1] = ypos + height;
	vertices[1][0] = xpos;
	vertices[1][1] = ypos;
	vertices[2][0] = xpos + width;
	vertices[2][1] = ypos;
	vertices[3][0] = xpos;
	vertices[3][1] = ypos + height;
	vertices[4][0] = xpos + width;
	vertices[4][1] = ypos;
	vertices[5][0] = xpos + width;
	vertices[5][1] = ypos + height;
	// Update VBO
	glBindBuffer(GL_ARRAY_BUFFER, r->VBO_text);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	// Render quad
	glDrawArrays(GL_TRIANGLES, 0, 6);
}


// Draw background for each location
static void _render_bg(struct renderer *r) {
	GLuint loc_bg_color, loc_proj_mat;
	vec4_t bgcol;
	uvec2_t advance, dim;
	unsigned i, j, toprow;
	float projmat[16];
	GLfloat vertices[6][2] = { 0 };
	GLfloat xpos = 0.0f, ypos = 0.0f;

	pthread_mutex_lock(&r->mut);
	toprow = r->toprow;
	advance = r->fonts->advance;
	dim = r->dim;
	for (i = 0; i < 16; i++) {
		projmat[i] = r->window->projmat[i];
	}
	pthread_mutex_unlock(&r->mut);

	glUseProgram(r->bg_shader);
	loc_proj_mat = glGetUniformLocation(r->bg_shader, "projection");
	loc_bg_color = glGetUniformLocation(r->bg_shader, "bg_color");
	glUniformMatrix4fv(loc_proj_mat, 1, GL_FALSE, projmat);
	glBindVertexArray(r->VAO_bg);

	for (i = toprow; i != (toprow + dim.y) % (dim.y + 1); i = (i + 1) % (dim.y + 1)) {
		for (j = 0; j < dim.x; j++) {
			if (!r->termbox[i * dim.x + j].to_draw) {
				continue;
			}
			bgcol = r->termbox[i * dim.x + j].bgcol;
			glUniform4f(loc_bg_color, bgcol.x, bgcol.y, bgcol.z, bgcol.w);
			// Set vertices
			vertices[0][0] = xpos;
			vertices[0][1] = ypos + advance.y;
			vertices[1][0] = xpos;
			vertices[1][1] = ypos;
			vertices[2][0] = xpos + advance.x;
			vertices[2][1] = ypos;
			vertices[3][0] = xpos;
			vertices[3][1] = ypos + advance.y;
			vertices[4][0] = xpos + advance.x;
			vertices[4][1] = ypos;
			vertices[5][0] = xpos + advance.x;
			vertices[5][1] = ypos + advance.y;
			// Update VBO
			glBindBuffer(GL_ARRAY_BUFFER, r->VBO_bg);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			// Render quad
			glDrawArrays(GL_TRIANGLES, 0, 6);
			// Advance coords
			xpos += advance.x;
			ypos += advance.y;
		}
	}

	glBindVertexArray(0);
}


// Render current contents
static void _do_render(struct renderer *r, bool draw_cursor) {
	const struct glyph *glyph;
	GLuint loc_text_color, loc_proj_mat;
	vec4_t fgcol, bgcol;
	uvec2_t cursor, dim;
	unsigned i, j, toprow, y;
	float projmat[16];
	struct termchar tchar;

	pthread_mutex_lock(&r->mut);
	toprow = r->toprow;
	cursor = r->cursor;
	cursor.y = (toprow + r->cursor.y) % (r->dim.y + 1);
	for (i = 0; i < 16; i++) {
		projmat[i] = r->window->projmat[i];
	}
	dim = r->dim;
	pthread_mutex_unlock(&r->mut);

	// Clear window
	glClearColor(r->default_bgcol.x, r->default_bgcol.y, r->default_bgcol.z, r->default_bgcol.w);
	glClear(GL_COLOR_BUFFER_BIT);

	// Render background
	_render_bg(r);

	// Render foreground
	glUseProgram(r->text_shader);
	loc_proj_mat = glGetUniformLocation(r->text_shader, "projection");
	loc_text_color = glGetUniformLocation(r->text_shader, "text_color");
	glUniformMatrix4fv(loc_proj_mat, 1, GL_FALSE, projmat);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(r->VAO_text);

	y = 0;

	for (i = toprow; i != (toprow + dim.y) % (dim.y + 1); i = (i + 1) % (dim.y + 1), y++) {
		for (j = 0; j < dim.x; j++) {

			pthread_mutex_lock(&r->mut);
			tchar = r->termbox[i * dim.x + j];
			pthread_mutex_unlock(&r->mut);

			bgcol = tchar.bgcol;
			fgcol = tchar.fgcol;

			if (i == cursor.y && j == cursor.x && draw_cursor && r->cursor_glyph) {
				glUniform3f(loc_text_color, r->default_fgcol.x, r->default_fgcol.y,
						r->default_fgcol.z);
				_render_glyph(r, y, j, r->cursor_glyph);
				if (!tchar.to_draw) {
					continue;
				}
				if (!(glyph = tchar.glyph)) {
					continue;
				}
				glUniform3f(loc_text_color, r->default_bgcol.x, r->default_bgcol.y,
						r->default_bgcol.z);
				_render_glyph(r, y, j, glyph);
			} else {
				if (!tchar.to_draw) {
					continue;
				}
				if (!(glyph = tchar.glyph)) {
					continue;
				}
				glUniform3f(loc_text_color, fgcol.x, fgcol.y, fgcol.z);
				_render_glyph(r, y, j, glyph);
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);

	// Swap buffers
	if (r->window) {
		window_refresh(r->window);
	}
}


// Denote that renderer should render contents
void renderer_render(struct renderer *r) {
	if (!r) {
		die("NULL renderer");
	}
	r->req_render = true;
}


// Do whatever the renderer needs to do
void renderer_update(struct renderer *r) {
	if (!r) {
		die("NULL renderer");
	}
	if (__sync_bool_compare_and_swap(&r->req_render, true, false)) {
		_do_render(r, r->cursor_vis);
	}
}


// Add character
void renderer_add_codepoint(struct renderer *r, uint32_t cp) {
	const struct glyph *glyph;
	unsigned toprow, y;
	uvec2_t cursor, dim;
	vec4_t fgcol, bgcol;

	if (!r) {
		die("NULL renderer");
	}
	if (cp > 0x10ffff || (cp >= 0xd800 && cp < 0xe000)) {
		die_fmt("Invalid Unicode codepoint: %u\n", cp);
	}

	pthread_mutex_lock(&r->size_mut);
	pthread_mutex_lock(&r->mut);

	toprow = r->toprow;
	cursor = r->cursor;
	dim = r->dim;
	fgcol = r->fgcol;
	bgcol = r->bgcol;

	switch (cp) {
	case '\b':
		if (cursor.x > 0) {
			cursor.x--;
		}
		break;
	case '\t':
		do {
			cursor.x++;
		} while (cursor.x % BTE_TABSZ != 0);
		break;
	case '\r':
		cursor.x = 0;
		break;
	case '\n':
		cursor.y++;
		break;
	default:
		if (!(glyph = fonts_get_glyph(r->fonts, cp))) {
			warn_fmt("Could not get glyph for codepoint: %u", cp);
		} else {
			y = (toprow + cursor.y) % (dim.y + 1);
			r->termbox[y * dim.x + cursor.x].glyph = glyph;
			r->termbox[y * dim.x + cursor.x].to_draw = true;
			r->termbox[y * dim.x + cursor.x].fgcol = fgcol;
			r->termbox[y * dim.x + cursor.x].bgcol = bgcol;
		}
		cursor.x++;
	}

	// Is this default behaviour?
	if (cursor.x >= dim.x) {
		cursor.x = 0;
		cursor.y++;
	}
	if (cursor.y >= dim.y) {
		// Scroll 1 line up
		toprow = (toprow + 1) % (dim.y + 1);
		y = (toprow + dim.y - 1) % (dim.y + 1);
		memset(&r->termbox[dim.x * y], 0, dim.x * sizeof(struct termchar));
		cursor.y = dim.y - 1;
	}
	// Restore cursor and toprow
	r->toprow = toprow;
	r->cursor = cursor;

	pthread_mutex_unlock(&r->mut);
	pthread_mutex_unlock(&r->size_mut);
}


// Move cursor up by n
void renderer_move_up(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (n == 0) {
		n = 1;
	}
	pthread_mutex_lock(&r->mut);
	if (r->cursor.y < n) {
		r->cursor.y = 0;
	} else {
		r->cursor.y -= n;
	}
	pthread_mutex_unlock(&r->mut);
}


// Move cursor down by n
void renderer_move_down(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (n == 0) {
		n = 1;
	}
	pthread_mutex_lock(&r->size_mut);
	pthread_mutex_lock(&r->mut);
	if (r->cursor.y + n >= r->dim.y) {
		r->cursor.y = r->dim.y - 1;
	} else {
		r->cursor.y += n;
	}
	pthread_mutex_unlock(&r->mut);
	pthread_mutex_unlock(&r->size_mut);
}


// Move cursor right by n
void renderer_move_right(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (n == 0) {
		n = 1;
	}
	pthread_mutex_lock(&r->size_mut);
	pthread_mutex_lock(&r->mut);
	if (r->cursor.x + n >= r->dim.x) {
		r->cursor.x = r->dim.x - 1;
	} else {
		r->cursor.x += n;
	}
	pthread_mutex_unlock(&r->mut);
	pthread_mutex_unlock(&r->size_mut);
}


// Move cursor left by n
void renderer_move_left(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (n == 0) {
		n = 1;
	}
	pthread_mutex_lock(&r->mut);
	if (r->cursor.x < n) {
		r->cursor.x = 0;
	} else {
		r->cursor.x -= n;
	}
	pthread_mutex_unlock(&r->mut);
}


// Resize renderer to match window (called by window subsystem)
void renderer_resize(struct renderer *r) {
	struct termchar *tmp;
	if (!r) {
		die("NULL renderer");
	}
	pthread_mutex_lock(&r->size_mut);
	pthread_mutex_lock(&r->mut);
	// Fill dimensions
	r->dim.x = r->window->dim.x / r->fonts->advance.x;
	r->dim.y = r->window->dim.y / r->fonts->advance.y;
	// Realloc terminal box
	if (!(tmp = realloc(r->termbox, r->dim.x * (r->dim.y + 1) * sizeof(struct termchar)))) {
		die_err("realloc()");
	}
	r->termbox = tmp;
	// Move cursor to 0
	r->cursor.x = 0;
	r->cursor.y = 0;
	// FIXME: Reset?
	r->toprow = 0;
	memset(r->termbox, 0, r->dim.x * (r->dim.y + 1) * sizeof(struct termchar));
	pthread_mutex_unlock(&r->mut);
	pthread_mutex_unlock(&r->size_mut);
	// TODO: Copy data?
	// Render
	renderer_render(r);
}


// Move cursor to (x, y)
void renderer_move_yx(struct renderer *r, unsigned y, unsigned x) {
	if (!r) {
		die("NULL renderer");
	}
	pthread_mutex_lock(&r->size_mut);
	pthread_mutex_lock(&r->mut);
	if (y == 0) {
		r->cursor.y = 0;
	} else if (y > r->dim.y) {
		r->cursor.y = r->dim.y - 1;
	} else {
		r->cursor.y = y - 1;
	}
	if (x == 0) {
		r->cursor.x = 0;
	} else if (x > r->dim.x) {
		r->cursor.x = r->dim.x - 1;
	} else {
		r->cursor.x = x - 1;
	}
	pthread_mutex_unlock(&r->mut);
	pthread_mutex_unlock(&r->size_mut);
}


// Clear screen
void renderer_clear_screen(struct renderer *r, enum renderer_clear_type type) {
	unsigned i, tr, cy, dy, ydiff;
	uvec2_t dim;
	if (!r) {
		die("NULL renderer");
	}
	pthread_mutex_lock(&r->size_mut);
	pthread_mutex_lock(&r->mut);
	dim = r->dim;
	tr = r->toprow;
	cy = (tr + r->cursor.y) % (dim.y + 1);
	dy = (tr + dim.y) % (dim.y + 1);
	i = cy * dim.x + r->cursor.x;
	// NOTE: dy CANNOT be equal to cy
	// NOTE: dy CANNOT be equal to tr
	switch (type) {
	case RENDERER_CLEAR_TO_END:
		if (dy < cy) {
			memset(&r->termbox[i], 0, ((dim.y + 1) * dim.x - i) * sizeof(struct termchar));
			memset(&r->termbox[0], 0, dy * dim.x * sizeof(struct termchar));
		} else {
			memset(&r->termbox[i], 0, (dy * dim.x - i) * sizeof(struct termchar));
		}
		break;
	case RENDERER_CLEAR_FROM_BEG:
		if (tr <= cy) {
			memset(&r->termbox[tr * dim.x], 0, i - (tr * dim.x) * sizeof(struct termchar));
		} else {
			memset(&r->termbox[tr * dim.x], 0, (dim.y + 1 - tr) * dim.x * sizeof(struct termchar));
			memset(&r->termbox[0], 0, i * sizeof(struct termchar));
		}
		//memset(r->termbox, 0, i * sizeof(struct termchar));
		break;
	case RENDERER_CLEAR_ALL:
		if (tr < dy) {
			memset(&r->termbox[tr * dim.x], 0, dim.x * dim.y * sizeof(struct termchar));
		} else {
			ydiff = dim.y + 1 - tr;
			memset(&r->termbox[tr * dim.x], 0, dim.x * ydiff * sizeof(struct termchar));
			memset(&r->termbox[0], 0, dim.x * dy * sizeof(struct termchar));
		}
		break;
	}
	pthread_mutex_unlock(&r->mut);
	pthread_mutex_unlock(&r->size_mut);
}


// Clear line
void renderer_clear_line(struct renderer *r, enum renderer_clear_type type) {
	unsigned i, j, k, y;
	if (!r) {
		die("NULL renderer");
	}
	pthread_mutex_lock(&r->size_mut);
	pthread_mutex_lock(&r->mut);
	y = (r->toprow + r->cursor.y) % (r->dim.y + 1);
	i = y * r->dim.x;
	j = y * r->dim.x + r->cursor.x;
	k = (y + 1) * r->dim.x;
	switch (type) {
	case RENDERER_CLEAR_TO_END:
		memset(&r->termbox[j], 0, (k - j) * sizeof(struct termchar));
		break;
	case RENDERER_CLEAR_FROM_BEG:
		memset(&r->termbox[i], 0, (j - i) * sizeof(struct termchar));
		break;
	case RENDERER_CLEAR_ALL:
		memset(&r->termbox[i], 0, (k - i) * sizeof(struct termchar));
	}
	pthread_mutex_unlock(&r->mut);
	pthread_mutex_unlock(&r->size_mut);
}


// Set renderer foreground color
void renderer_set_fgcol(struct renderer *r, const struct color *color) {
	if (!r) {
		die("NULL renderer");
	}
	if (!color) {
		die("NULL color");
	}
	color_normalize(color, &r->fgcol);
}


// Set renderer background color
void renderer_set_bgcol(struct renderer *r, const struct color *color) {
	if (!r) {
		die("NULL renderer");
	}
	if (!color) {
		die("NULL color");
	}
	color_normalize(color, &r->bgcol);
}


// Reset renderer foreground color
void renderer_reset_fgcol(struct renderer *r) {
	if (!r) {
		die("NULL renderer");
	}
	r->fgcol = r->default_fgcol;
}


// Reset renderer background color
void renderer_reset_bgcol(struct renderer *r) {
	if (!r) {
		die("NULL renderer");
	}
	r->bgcol = r->default_bgcol;
}
