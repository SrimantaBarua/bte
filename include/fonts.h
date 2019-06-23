#ifndef __BTE_FONTS_H__
#define __BTE_FONTS_H__


#include <ft2build.h>
#include FT_FREETYPE_H

#include "util.h"


// Information about a glyph
struct glyph {
	ivec2_t  bearing;   // Bearing
	uvec2_t  size;      // Bitmap size
	unsigned tex;       // Glyph texture
	int      advance_x; // Advance to next character
};


// Font loading subsystem
struct fonts {
	struct htu32 *glyphs;       // Hash table mapping codeoint to glyph
	uvec2_t      advance;       // Advance to the next glyph
	unsigned     line_height;   // Distance from top of glyphs to base
	// Freetype
	FT_Library   ft_lib;        // Handle to Freetype2 library
	struct list  *faces;        // List of faces
};

// Initialize font-loading subsystem
struct fonts* fonts_new(const char *default_font, unsigned font_sz);

// Free resources of font-loading subsystem
void fonts_free(struct fonts *fonts);

// Get glyph for codepoint
const struct glyph* fonts_get_glyph(struct fonts *fonts, uint32_t codepoint);


#endif // __BTE_FONTS_H__
