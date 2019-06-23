#include "glad/glad.h"

#include <stdlib.h>
#include <fontconfig/fontconfig.h>

#include "fonts.h"


// Get font file from fontconfig
static char* get_font_file(const char *font_name) {
	char *ret;
	FcResult result;
	FcChar8 *file;
	FcConfig *config = FcInitLoadConfigAndFonts();
	FcPattern *pat = FcNameParse((const FcChar8*) font_name);
	FcConfigSubstitute(config, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	FcPattern *font = FcFontMatch(config, pat, &result);
	if (font) {
		file = NULL;
		if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) {
			file = NULL;
		}
		if (!(ret = strdup((char*) file))) {
			die_err("strdup()");
		}
		FcPatternDestroy(font);
	}
	FcPatternDestroy(pat);
	FcConfigDestroy(config);
	return ret;
}


// Initialize font-loading subsystem
struct fonts* fonts_new(const char *default_font, unsigned font_sz) {
	char *file;
	struct fonts *fonts;
	FT_Face face;
	unsigned c, glyph_idx, line_ht = 0, line_sp = 0;
	struct glyph *glyph;
	// Allocate fonts
	if (!(fonts = calloc(1, sizeof(struct fonts)))) {
		die_err("calloc()");
	}
	fonts->glyphs = htu32_new();
	// Get font file
	if (!default_font) {
		warn("");
		default_font = "monospace";
	}
	if (!(file = get_font_file(default_font))) {
		die_fmt("Failed to get font file for font: %s", default_font);
	}
	// Initialize Freetype2
	if (FT_Init_FreeType(&fonts->ft_lib)) {
		die("Could not initialize the Freetype2 library");
	}
	if (FT_New_Face(fonts->ft_lib, file, 0, &face)) {
		die_fmt("Could not load Freetype2 face for font: %s", default_font);
	}
	if (FT_Set_Pixel_Sizes(face, 0, font_sz)) {
		die("Could not set pixel size");
	}
	// Push face to list
	fonts->faces = list_new(face);
	// Disable byte alignment restriction
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// Load ASCII glyphs
	for (c = 32; c < 128; c++) {
		glyph_idx = FT_Get_Char_Index(face, c);
		if (FT_Load_Glyph(face, glyph_idx, FT_LOAD_RENDER)) {
			warn_fmt("Could not load glyph for codepoint: %u\n", c);
			continue;
		}
		// Allocate glyph and store character data
		if (!(glyph = malloc(sizeof(struct glyph)))) {
			die_err("malloc()");
		}
		glyph->size.x = face->glyph->bitmap.width;
		glyph->size.y = face->glyph->bitmap.rows;
		glyph->bearing.x = face->glyph->bitmap_left;
		glyph->bearing.y = face->glyph->bitmap_top;
		glyph->advance_x = face->glyph->advance.x;
		// Generate texture atlas
		glGenTextures(1, &glyph->tex);
		glBindTexture(GL_TEXTURE_2D, glyph->tex);
		// Set texture options
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// Update texture data
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, glyph->size.x, glyph->size.y, 0, GL_RED,
				GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
		// Update advance and line height
		if (face->glyph->advance.x > 0 && face->glyph->advance.x > fonts->advance.x) {
			fonts->advance.x = face->glyph->advance.x;
		}
		if (glyph->bearing.y > 0 && glyph->bearing.y > line_ht) {
			line_ht = glyph->bearing.y;
		}
		if (line_sp + glyph->bearing.y < glyph->size.y) {
			line_sp = glyph->size.y - glyph->bearing.y;
		}
		// Store in hash table
		htu32_set(fonts->glyphs, c, glyph);
	}
	// Compute metrics
	fonts->advance.x >>= 6;
	fonts->advance.y = line_ht + line_sp;
	if (fonts->advance.x < 1) {
		fonts->advance.x = 1;
	}
	if (fonts->advance.y < 1) {
		fonts->advance.y = 1;
	}
	fonts->line_height = line_ht;
	// Unbind texture
	glBindTexture(GL_TEXTURE_2D, 0);
	// Return font
	return fonts;
}


// Free a glyph
static void _glyph_free(struct glyph *glyph) {
	if (!glyph) {
		return;
	}
	glDeleteTextures(1, &glyph->tex);
	free(glyph);
}


// Free resources of font-loading subsystem
void fonts_free(struct fonts *fonts) {
	if (!fonts) {
		warn("NULL fonts");
	}
	htu32_free(fonts->glyphs, (free_cb_t) _glyph_free);
	list_free(fonts->faces, (free_cb_t) FT_Done_Face);
	FT_Done_FreeType(fonts->ft_lib);
	free(fonts);
}


// Get glyph for codepoint
const struct glyph* fonts_get_glyph(struct fonts *fonts, uint32_t codepoint) {
	struct glyph *glyph;
	enum htres res;
	if (!fonts) {
		die("NULL fonts");
	}
	glyph = htu32_get(fonts->glyphs, codepoint, &res);
	if (res == HTRES_OK) {
		return glyph;
	}
	// TODO: Handle loading glyph
	return NULL;
}
