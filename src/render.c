#include "glad/glad.h"

#include <stdlib.h>

#include "util.h"
#include "color.h"
#include "render.h"


const char *vtxtsrc = "";
const char *ftxtsrc = "";


// Vertex shader
const char *vsrc =
"#version 330 core\n"
"layout (location = 0) in vec4 vertex;\n"
"out vec2 tex_coords;\n"
"uniform mat4 projection;\n" // <vec2 pos, vec2 tex>
"void main() {\n"
"  gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
"  tex_coords = vertex.zw;\n"
"}";


// Fragment shader
const char *fsrc =
"#version 330 core\n"
"in vec2 tex_coords;\n"
"out vec4 color;\n"
"uniform sampler2D text;\n"
"uniform vec3 text_color;\n"
"void main() {\n"
"  vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, tex_coords).r);\n"
"  color = vec4(text_color, 1.0) * sampled;\n"
"}";



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
struct renderer *renderer_new(struct window *w, struct fonts *f, const char *fg, const char *bg) {
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
	color_normalize(&fgc, &r->fgcol);
	// Get background color
	if (!color_parse(&bgc, bg)) {
		die_fmt("Unable to parse foreground color: %s", bg);
	}
	color_normalize(&bgc, &r->bgcol);
	// Fill dimensions
	r->dim.x = w->dim.x / f->advance.x;
	r->dim.y = w->dim.y / f->advance.y;
	// Allocate terminal box
	if (!(r->termbox = calloc(r->dim.x * r->dim.y, sizeof(struct glyph*)))) {
		die_err("calloc()");
	}
	// Initialize cursor
	r->cursor.x = 0;
	r->cursor.y = 0;
	// Set pointers
	r->window = w;
	r->fonts = f;
	// Compile and link shader
	r->text_shader = _load_shaders(vsrc, fsrc);
	// Create and initialize VAO and VBO
	glGenVertexArrays(1, &r->VAO);
	glGenBuffers(1, &r->VBO);
	glBindVertexArray(r->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, r->VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	// Clear window
	glClearColor(r->bgcol.x, r->bgcol.y, r->bgcol.z, r->bgcol.w);
	glClear(GL_COLOR_BUFFER_BIT);
	window_refresh(r->window);
	return r;
}


// Free renderer resources
void renderer_free(struct renderer *renderer) {
	if (!renderer) {
		warn("NULL renderer");
		return;
	}
	glDeleteBuffers(1, &renderer->VBO);
	glDeleteVertexArrays(1, &renderer->VAO);
	glDeleteProgram(renderer->text_shader);
	free(renderer->termbox);
	free(renderer);
}


// Render current contents
void renderer_render(const struct renderer *r) {
	const struct glyph *glyph;
	GLfloat xpos, ypos, width, height;
	GLfloat vertices[6][4] = {
		{ 0.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f },
	};
	GLuint loc_text_color, loc_proj_mat;
	unsigned i, j;

	if (!r) {
		die("NULL renderer");
	}

	// Clear window
	glClearColor(r->bgcol.x, r->bgcol.y, r->bgcol.z, r->bgcol.w);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(r->text_shader);
	loc_text_color = glGetUniformLocation(r->text_shader, "text_color");
	loc_proj_mat = glGetUniformLocation(r->text_shader, "projection");
	glUniform3f(loc_text_color, r->fgcol.x, r->fgcol.y, r->fgcol.z);
	glUniformMatrix4fv(loc_proj_mat, 1, GL_FALSE,  r->window->projmat);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(r->VAO);

	for (i = 0; i < r->dim.y; i++) {
		for (j = 0; j < r->dim.x; j++) {
			if (!(glyph = r->termbox[i * r->dim.x + j])) {
				continue;
			}
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
			glBindBuffer(GL_ARRAY_BUFFER, r->VBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			// Render quad
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);

	// Swap buffers
	if (r->window) {
		window_refresh(r->window);
	}
}


// Add character
void renderer_add_codepoint(struct renderer *r, uint32_t cp) {
	const  struct glyph *glyph;

	if (!r) {
		die("NULL renderer");
	}
	if (cp > 0x10ffff || (cp >= 0xd800 && cp < 0xe000)) {
		die_fmt("Invalid Unicode codepoint: %u\n", cp);
	}

	switch (cp) {
	case '\r':
		r->cursor.x = 0;
		return;
	case '\n':
		r->cursor.y++;
		break;
	default:
		if (!(glyph = fonts_get_glyph(r->fonts, cp))) {
			warn_fmt("Could not get glyph for codepoint: %u", cp);
		} else {
			r->termbox[r->cursor.y * r->dim.x + r->cursor.x] = glyph;
		}
	}

	// Is this default behaviour?
	r->cursor.x++;
	if (r->cursor.x >= r->dim.x) {
		r->cursor.x = 0;
		r->cursor.y++;
	}
	if (r->cursor.y >= r->dim.y) {
		// Scroll 1 line up
		memcpy(r->termbox, &r->termbox[r->dim.x], r->dim.x * (r->dim.y - 1) * sizeof(struct glyph*));
		memset(&r->termbox[r->dim.x * (r->dim.y - 1)], 0, r->dim.y * sizeof(struct glyph*));
		r->cursor.y = r->dim.y - 1;
	}
}


// Move cursor up by n
void renderer_move_up(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (r->cursor.y < n) {
		r->cursor.y = 0;
	} else {
		r->cursor.y -= n;
	}
}


// Move cursor down by n
void renderer_move_down(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (r->cursor.y + n >= r->dim.y) {
		r->cursor.y = r->dim.y - 1;
	} else {
		r->cursor.y += n;
	}
}


// Move cursor right by n
void renderer_move_right(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (r->cursor.x + n >= r->dim.x) {
		r->cursor.x = r->dim.x - 1;
	} else {
		r->cursor.x += n;
	}
}


// Move cursor left by n
void renderer_move_left(struct renderer *r, unsigned n) {
	if (!r) {
		die("NULL renderer");
	}
	if (r->cursor.x < n) {
		r->cursor.x = 0;
	} else {
		r->cursor.x -= n;
	}
}


// Resize renderer to match window (called by window subsystem)
void renderer_resize(struct renderer *r) {
	if (!r) {
		die("NULL renderer");
	}
	// Fill dimensions
	r->dim.x = r->window->dim.x / r->fonts->advance.x;
	r->dim.y = r->window->dim.y / r->fonts->advance.y;
	// Realloc terminal box
	if (!(r->termbox = realloc(r->termbox, r->dim.x * r->dim.y * sizeof(struct glyph*)))) {
		die_err("realloc()");
	}
	// Move cursor to 0
	r->cursor.x = 0;
	r->cursor.y = 0;
	// TODO: Copy data?
}
