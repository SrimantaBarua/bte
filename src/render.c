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


// Create a new terminal buffer
static struct termbuf* _termbuf_new(uvec2_t dim, const struct glyph *cursor_glyph) {
	struct termbuf *ret;
	if (!(ret = calloc(1, sizeof(struct termbuf)))) {
		die_err("calloc()");
	}
	if (!(ret->termbox = calloc(dim.x * (dim.y + 1), sizeof(struct termchar)))) {
		die_err("calloc()");
	}
	ret->dim = dim;
	ret->cursor_glyph = cursor_glyph;
	ret->cursor.x = ret->cursor.y = 0;
	ret->cursor_vis = true;
	ret->toprow = 0;
	return ret;
}


// Free a terminal buffer
static void _termbuf_free(struct termbuf *tb) {
	free(tb->termbox);
	free(tb);
}


// Create a new renderer
struct renderer *renderer_new(struct window *w, struct fonts *f, const char *fg, const char *bg, uint32_t cursor, const struct color *palette) {
	struct renderer *r;
	struct color fgc, bgc;
	uvec2_t dim;
	const struct glyph *cursor_glyph;

	if (!w) {
		die("NULL window");
	}
	if (!f) {
		die("NULL fonts");
	}

	if (!(r = malloc(sizeof(struct renderer)))) {
		die_err("malloc()");
	}
	// Get colors
	if (!color_parse(&fgc, fg)) {
		die_fmt("Unable to parse foreground color: %s", fg);
	}
	color_normalize(&fgc, &r->default_fgcol);
	if (!color_parse(&bgc, bg)) {
		die_fmt("Unable to parse foreground color: %s", bg);
	}
	color_normalize(&bgc, &r->default_bgcol);
	r->fgcol = r->default_fgcol;
	r->bgcol = r->default_bgcol;
	// Allocate draw and modify buffers
	dim.x = w->dim.x / f->advance.x;
	dim.y = w->dim.y / f->advance.y;
	cursor_glyph = fonts_get_glyph(f, cursor);
	r->draw_buf = _termbuf_new(dim, cursor_glyph);
	r->mod_buf = _termbuf_new(dim, cursor_glyph);
	// Set pointers
	r->window = w;
	r->fonts = f;
	r->palette = palette;
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
	// Initialize mutexes
	pthread_mutex_init(&r->buf_mut, NULL);
	return r;
}


// Free renderer resources
void renderer_free(struct renderer *renderer) {
	if (!renderer) {
		warn("NULL renderer");
		return;
	}
	pthread_mutex_destroy(&renderer->buf_mut);
	glDeleteBuffers(1, &renderer->VBO_bg);
	glDeleteVertexArrays(1, &renderer->VAO_bg);
	glDeleteBuffers(1, &renderer->VBO_text);
	glDeleteVertexArrays(1, &renderer->VAO_text);
	glDeleteProgram(renderer->text_shader);
	glDeleteProgram(renderer->bg_shader);
	_termbuf_free(renderer->draw_buf);
	_termbuf_free(renderer->mod_buf);
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
	const vec4_t *bgcol;
	const uvec2_t *advance, *dim;
	unsigned i, j, toprow;
	float projmat[16];
	GLfloat vertices[6][2] = { 0 };
	GLfloat xpos = 0.0f, ypos = 0.0f;
	const struct termchar *tchar;

	toprow = r->draw_buf->toprow;
	advance = &r->fonts->advance;
	dim = &r->draw_buf->dim;
	for (i = 0; i < 16; i++) {
		projmat[i] = r->window->projmat[i];
	}
	ypos = r->window->dim.y;

	glUseProgram(r->bg_shader);
	loc_proj_mat = glGetUniformLocation(r->bg_shader, "projection");
	loc_bg_color = glGetUniformLocation(r->bg_shader, "bg_color");
	glUniformMatrix4fv(loc_proj_mat, 1, GL_FALSE, projmat);
	glBindVertexArray(r->VAO_bg);

	for (i = toprow; i != (toprow + dim->y) % (dim->y + 1); i = (i + 1) % (dim->y + 1)) {
		for (j = 0; j < dim->x; j++) {
			tchar = &r->draw_buf->termbox[i * dim->x + j];
			if (!tchar->to_draw) {
				continue;
			}
			bgcol = &tchar->bgcol;
			glUniform4f(loc_bg_color, bgcol->x, bgcol->y, bgcol->z, bgcol->w);
			// Set vertices
			vertices[0][0] = xpos;
			vertices[0][1] = ypos - advance->y;
			vertices[1][0] = xpos;
			vertices[1][1] = ypos;
			vertices[2][0] = xpos + advance->x;
			vertices[2][1] = ypos;
			vertices[3][0] = xpos;
			vertices[3][1] = ypos - advance->y;
			vertices[4][0] = xpos + advance->x;
			vertices[4][1] = ypos;
			vertices[5][0] = xpos + advance->x;
			vertices[5][1] = ypos - advance->y;
			// Update VBO
			glBindBuffer(GL_ARRAY_BUFFER, r->VBO_bg);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			// Render quad
			glDrawArrays(GL_TRIANGLES, 0, 6);
			// Advance coords
			xpos += advance->x;
		}
		xpos = 0;
		ypos -= advance->y;
	}

	glBindVertexArray(0);
}


// Render current contents
static void _do_render(struct renderer *r) {
	const struct glyph *glyph;
	GLuint loc_text_color, loc_proj_mat;
	const vec4_t *fgcol, *bgcol;
	uvec2_t cursor;
	const uvec2_t *dim;
	unsigned i, j, toprow, y;
	float projmat[16];
	const struct termchar *tchar;
	struct termbuf *tmp_termbuf;
	bool draw_cursor = r->draw_buf->cursor_vis;

	toprow = r->draw_buf->toprow;
	cursor.x = r->draw_buf->cursor.x;
	cursor.y = (toprow + r->draw_buf->cursor.y) % (r->draw_buf->dim.y + 1);
	for (i = 0; i < 16; i++) {
		projmat[i] = r->window->projmat[i];
	}
	dim = &r->draw_buf->dim;

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

	for (i = toprow; i != (toprow + dim->y) % (dim->y + 1); i = (i + 1) % (dim->y + 1), y++) {
		for (j = 0; j < dim->x; j++) {
			tchar = &r->draw_buf->termbox[i * dim->x + j];

			bgcol = &tchar->bgcol;
			fgcol = &tchar->fgcol;

			if (i == cursor.y && j == cursor.x && draw_cursor && r->draw_buf->cursor_glyph) {
				glUniform3f(loc_text_color, r->default_fgcol.x, r->default_fgcol.y,
						r->default_fgcol.z);
				_render_glyph(r, y, j, r->draw_buf->cursor_glyph);
				if (!tchar->to_draw) {
					continue;
				}
				if (!(glyph = tchar->glyph)) {
					continue;
				}
				glUniform3f(loc_text_color, r->default_bgcol.x, r->default_bgcol.y,
						r->default_bgcol.z);
				_render_glyph(r, y, j, glyph);
			} else {
				if (!tchar->to_draw) {
					continue;
				}
				if (!(glyph = tchar->glyph)) {
					continue;
				}
				glUniform3f(loc_text_color, fgcol->x, fgcol->y, fgcol->z);
				_render_glyph(r, y, j, glyph);
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);

	// Swap buffers
	if (r->window) {
		window_refresh(r->window);
		pthread_mutex_lock(&r->buf_mut);
		tmp_termbuf = r->draw_buf;
		r->draw_buf = r->mod_buf;
		r->mod_buf = r->draw_buf;
		pthread_mutex_unlock(&r->buf_mut);
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
		_do_render(r);
	}
}


// Move cursor up by n
static void _move_up(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (n == 0) {
		n = 1;
	}
	if (r->mod_buf->cursor.y < n) {
		r->mod_buf->cursor.y = 0;
	} else {
		r->mod_buf->cursor.y -= n;
	}
}


// Move cursor down by n
static void _move_down(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (n == 0) {
		n = 1;
	}
	if (r->mod_buf->cursor.y + n >= r->mod_buf->dim.y) {
		r->mod_buf->cursor.y = r->mod_buf->dim.y - 1;
	} else {
		r->mod_buf->cursor.y += n;
	}
}


// Move cursor right by n
static void _move_right(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (n == 0) {
		n = 1;
	}
	if (r->mod_buf->cursor.x + n >= r->mod_buf->dim.x) {
		r->mod_buf->cursor.x = r->mod_buf->dim.x - 1;
	} else {
		r->mod_buf->cursor.x += n;
	}
}


// Move cursor left by n
static void _move_left(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (n == 0) {
		n = 1;
	}
	if (r->mod_buf->cursor.x < n) {
		r->mod_buf->cursor.x = 0;
	} else {
		r->mod_buf->cursor.x -= n;
	}
}


// Move cursor to (x, y)
static void _move_yx(struct renderer *r, unsigned y, unsigned x) {
	if (!r) {
		die("NULL renderer");
	}
	if (y == 0) {
		r->mod_buf->cursor.y = 0;
	} else if (y > r->mod_buf->dim.y) {
		r->mod_buf->cursor.y = r->mod_buf->dim.y - 1;
	} else {
		r->mod_buf->cursor.y = y - 1;
	}
	if (x == 0) {
		r->mod_buf->cursor.x = 0;
	} else if (x > r->mod_buf->dim.x) {
		r->mod_buf->cursor.x = r->mod_buf->dim.x - 1;
	} else {
		r->mod_buf->cursor.x = x - 1;
	}
}


// Clear screen
static void _clear_screen(struct renderer *r, enum renderer_clear_type type) {
	unsigned i, tr, cy, dy, ydiff, charsz;
	const uvec2_t *dim;
	const size_t chsz = sizeof(struct termchar);
	struct termchar *termbox;
	if (!r) {
		die("NULL renderer");
	}
	termbox = r->mod_buf->termbox;
	dim = &r->mod_buf->dim;
	tr = r->mod_buf->toprow;
	cy = (tr + r->mod_buf->cursor.y) % (dim->y + 1);
	dy = (tr + dim->y) % (dim->y + 1);
	i = cy * dim->x + r->mod_buf->cursor.x;
	// NOTE: dy CANNOT be equal to cy
	// NOTE: dy CANNOT be equal to tr
	switch (type) {
	case RENDERER_CLEAR_TO_END:
		if (dy < cy) {
			memset(&termbox[i], 0, ((dim->y + 1) * dim->x - i) * chsz);
			memset(&termbox[0], 0, dy * dim->x * chsz);
		} else {
			memset(&termbox[i], 0, (dy * dim->x - i) * chsz);
		}
		break;
	case RENDERER_CLEAR_FROM_BEG:
		if (tr <= cy) {
			memset(&termbox[tr * dim->x], 0, i - (tr * dim->x) * chsz);
		} else {
			memset(&termbox[tr * dim->x], 0, (dim->y + 1 - tr) * dim->x * chsz);
			memset(&termbox[0], 0, i * chsz);
		}
		//memset(r->termbox, 0, i * sizeof(struct termchar));
		break;
	case RENDERER_CLEAR_ALL:
		if (tr < dy) {
			memset(&termbox[tr * dim->x], 0, dim->x * dim->y * chsz);
		} else {
			ydiff = dim->y + 1 - tr;
			memset(&termbox[tr * dim->x], 0, dim->x * ydiff * chsz);
			memset(&termbox[0], 0, dim->x * dy * chsz);
		}
		break;
	}
}


// Clear line
static void _clear_line(struct renderer *r, enum renderer_clear_type type) {
	unsigned i, j, k, y;
	if (!r) {
		die("NULL renderer");
	}
	y = (r->mod_buf->toprow + r->mod_buf->cursor.y) % (r->mod_buf->dim.y + 1);
	i = y * r->mod_buf->dim.x;
	j = y * r->mod_buf->dim.x + r->mod_buf->cursor.x;
	k = (y + 1) * r->mod_buf->dim.x;
	switch (type) {
	case RENDERER_CLEAR_TO_END:
		memset(&r->mod_buf->termbox[j], 0, (k - j) * sizeof(struct termchar));
		break;
	case RENDERER_CLEAR_FROM_BEG:
		memset(&r->mod_buf->termbox[i], 0, (j - i) * sizeof(struct termchar));
		break;
	case RENDERER_CLEAR_ALL:
		memset(&r->mod_buf->termbox[i], 0, (k - i) * sizeof(struct termchar));
	}
}


// Set renderer foreground color
static void _set_fgcol(struct renderer *r, const struct color *color) {
	if (!r) {
		die("NULL renderer");
	}
	if (!color) {
		die("NULL color");
	}
	color_normalize(color, &r->fgcol);
}


// Set renderer background color
static void _set_bgcol(struct renderer *r, const struct color *color) {
	if (!r) {
		die("NULL renderer");
	}
	if (!color) {
		die("NULL color");
	}
	color_normalize(color, &r->bgcol);
}


// Reset renderer foreground color
static void _reset_fgcol(struct renderer *r) {
	if (!r) {
		die("NULL renderer");
	}
	r->fgcol = r->default_fgcol;
}


// Reset renderer background color
static void _reset_bgcol(struct renderer *r) {
	if (!r) {
		die("NULL renderer");
	}
	r->bgcol = r->default_bgcol;
}


// An escape sequence
struct esc_seq {
	unsigned nparam;     // Number of parameters
	unsigned params[32]; // Parameters
	char     final;      // Final character
	char     private;    // Character denoting private escape sequence
};


static void _process_esc(struct renderer *r, struct esc_seq *esc) {
	unsigned i;
	printf("Escape: \\x1b[");
	if (esc->private) {
		printf("%c", esc->private);
	}
	for (i = 0; i + 1 < esc->nparam; i++) {
		printf("%u;", esc->params[i]);
	}
	if (i < esc->nparam) {
		printf("%u", esc->params[i]);
	}
	printf("%c\n", esc->final);
	if (esc->private == 0) {
		switch (esc->final) {
		// Cursor position
		case 'A':
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			_move_up(r, esc->params[0]);
			return;
		case 'B':
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			_move_down(r, esc->params[0]);
			return;
		case 'C':
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			_move_right(r, esc->params[0]);
			return;
		case 'D':
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			_move_left(r, esc->params[0]);
			return;
		case 'H':
			if (esc->nparam == 1) {
				esc->params[1] = 1;
			}
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			_move_yx(r, esc->params[0], esc->params[1]);
			return;
		// Clear line/screen
		case 'J':
			if (esc->nparam == 0) {
				esc->params[0] = 0;
			}
			_clear_screen(r, esc->params[0]);
			return;
		case 'K':
			if (esc->nparam == 0) {
				esc->params[0] = 0;
			}
			_clear_line(r, esc->params[0]);
			return;
		// Attributes
		case 'm':
			// TODO: Handle attribs other than color (bold, underline etc)
			i = 0;
			while (i < esc->nparam) {
				switch (esc->params[i]) {
				case 0:
					_reset_fgcol(r);
					_reset_bgcol(r);
					break;
				case 30:
					_set_fgcol(r, &r->palette[0]);
					break;
				case 31:
					_set_fgcol(r, &r->palette[1]);
					break;
				case 32:
					_set_fgcol(r, &r->palette[2]);
					break;
				case 33:
					_set_fgcol(r, &r->palette[3]);
					break;
				case 34:
					_set_fgcol(r, &r->palette[4]);
					break;
				case 35:
					_set_fgcol(r, &r->palette[5]);
					break;
				case 36:
					_set_fgcol(r, &r->palette[6]);
					break;
				case 37:
					_set_fgcol(r, &r->palette[7]);
					break;
				case 39:
					_reset_fgcol(r);
					break;
				case 40:
					_set_bgcol(r, &r->palette[0]);
					break;
				case 41:
					_set_bgcol(r, &r->palette[1]);
					break;
				case 42:
					_set_bgcol(r, &r->palette[2]);
					break;
				case 43:
					_set_bgcol(r, &r->palette[3]);
					break;
				case 44:
					_set_bgcol(r, &r->palette[4]);
					break;
				case 45:
					_set_bgcol(r, &r->palette[5]);
					break;
				case 46:
					_set_bgcol(r, &r->palette[6]);
					break;
				case 47:
					_set_bgcol(r, &r->palette[7]);
					break;
				case 49:
					_reset_bgcol(r);
					break;
				case 90:
					_set_fgcol(r, &r->palette[8]);
					break;
				case 91:
					_set_fgcol(r, &r->palette[9]);
					break;
				case 92:
					_set_fgcol(r, &r->palette[10]);
					break;
				case 93:
					_set_fgcol(r, &r->palette[11]);
					break;
				case 94:
					_set_fgcol(r, &r->palette[12]);
					break;
				case 95:
					_set_fgcol(r, &r->palette[13]);
					break;
				case 96:
					_set_fgcol(r, &r->palette[14]);
					break;
				case 97:
					_set_fgcol(r, &r->palette[15]);
					break;
				case 100:
					_set_bgcol(r, &r->palette[8]);
					break;
				case 101:
					_set_bgcol(r, &r->palette[9]);
					break;
				case 102:
					_set_bgcol(r, &r->palette[10]);
					break;
				case 103:
					_set_bgcol(r, &r->palette[11]);
					break;
				case 104:
					_set_bgcol(r, &r->palette[12]);
					break;
				case 105:
					_set_bgcol(r, &r->palette[13]);
					break;
				case 106:
					_set_bgcol(r, &r->palette[14]);
					break;
				case 107:
					_set_bgcol(r, &r->palette[15]);
					break;
				}
				i++;
			}
			return;
		}
	}
}


// Add codepoints to renderer. Return number of codepoints added
size_t renderer_add_codepoints(struct renderer *r, uint32_t *cps, size_t n_cps) {
	const struct glyph *glyph;
	struct esc_seq esc = { 0 };
	unsigned y, param;
	bool in_num = false;
	size_t i, j, lines;
	struct termbuf *m;

	if (!r) {
		die("NULL renderer");
	}

	lines = 0;

	pthread_mutex_lock(&r->buf_mut);

	m = r->mod_buf;

	i = 0;
	while (i < n_cps) {
		if (cps[i] > 0x10ffff || (cps[i] >= 0xd800 && cps[i] < 0xe000)) {
			die_fmt("Invalid Unicode codepoint: %u\n", cps[i]);
		}
		if (cps[i] == 27) {
			// Escape
			if ((j = i + 1) >= n_cps) {
				break;
			}
			if (cps[j] != '[') {
				i++;
				continue;
			}
			param = 0;
			in_num = false;
			memset(&esc, 0, sizeof(struct esc_seq));
			if (++j >= n_cps) {
				goto out;
			}
			if (cps[j] == '?') {
				esc.private = '?';
				j++;
			}
			while (1) {
				if (j >= n_cps) {
					goto out;
				}
				if (cps[j] == ';') {
					if (esc.nparam < sizeof(esc.params) / sizeof(esc.params[0])) {
						esc.params[esc.nparam++] = param;
					}
					param = 0;
					in_num = false;
					j++;
					continue;
				}
				if (cps[j] >= '0' && cps[j] <= '9') {
					in_num = true;
					param = param * 10 + (cps[j] - '0');
					j++;
					continue;
				}
				if (esc.nparam < sizeof(esc.params) / sizeof(esc.params[0])) {
					esc.params[esc.nparam++] = param;
				}
				esc.final = cps[j];
				i = j + 1;
				break;
			}
			_process_esc(r, &esc);
			continue;
		}
		switch (cps[i]) {
		case '\a':
			// Ignore
			break;
		case '\b':
			if (m->cursor.x > 0) {
				m->cursor.x--;
			}
			break;
		case '\t':
			do {
				m->cursor.x++;
			} while (m->cursor.x % BTE_TABSZ != 0);
			break;
		case '\r':
			m->cursor.x = 0;
			break;
		case '\n':
			y = (m->toprow + m->cursor.y) % (m->dim.y + 1);
			m->cursor.y++;
			lines++;
			break;
		default:
			if (!(glyph = fonts_get_glyph(r->fonts, cps[i]))) {
				warn_fmt("Could not get glyph for codepoint: %u", cps[i]);
			} else {
				y = (m->toprow + m->cursor.y) % (m->dim.y + 1);
				m->termbox[y * m->dim.x + m->cursor.x].glyph = glyph;
				m->termbox[y * m->dim.x + m->cursor.x].to_draw = true;
				m->termbox[y * m->dim.x + m->cursor.x].fgcol = r->fgcol;
				m->termbox[y * m->dim.x + m->cursor.x].bgcol = r->bgcol;
			}
			m->cursor.x++;
		}

		// Is this default behaviour?
		if (m->cursor.x >= m->dim.x) {
			m->cursor.x = 0;
			m->cursor.y++;
			lines++;
		}
		if (m->cursor.y >= m->dim.y) {
			// Scroll 1 line up
			m->toprow = (m->toprow + 1) % (m->dim.y + 1);
			y = (m->toprow + m->dim.y - 1) % (m->dim.y + 1);
			m->cursor.y = m->dim.y - 1;
			memset(&m->termbox[m->dim.x * y], 0, m->dim.x * sizeof(struct termchar));
		}
		i++;
	}

out:
	if (lines > 0) {
		// Clear out last line
		y = (m->toprow + m->cursor.y) % (m->dim.y + 1);
		memset(&m->termbox[m->dim.x * y + m->cursor.x], 0, (m->dim.x - m->cursor.x) * sizeof(struct termchar));
	}

	pthread_mutex_unlock(&r->buf_mut);

	return i;
}


// Resize renderer to match window (called by window subsystem)
uvec2_t renderer_resize(struct renderer *r) {
	struct termchar *tmp;
	struct termbuf *m;
	uvec2_t ret;
	if (!r) {
		die("NULL renderer");
	}
	pthread_mutex_lock(&r->buf_mut);
	m = r->mod_buf;
	// Fill dimensions
	ret.x = m->dim.x = r->window->dim.x / r->fonts->advance.x;
	ret.y = m->dim.y = r->window->dim.y / r->fonts->advance.y;
	// Realloc terminal box
	if (!(tmp = realloc(m->termbox, m->dim.x * (m->dim.y + 1) * sizeof(struct termchar)))) {
		die_err("realloc()");
	}
	m->termbox = tmp;
	// Move cursor to 0
	m->cursor.x = 0;
	m->cursor.y = 0;
	// FIXME: Reset?
	m->toprow = 0;
	memset(m->termbox, 0, m->dim.x * (m->dim.y + 1) * sizeof(struct termchar));
	pthread_mutex_unlock(&r->buf_mut);
	// TODO: Copy data?
	// Render
	renderer_render(r);
	return ret;
}
