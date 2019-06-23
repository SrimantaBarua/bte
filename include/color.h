#ifndef __BTE_COLOR_H__
#define __BTE_COLOR_H__


#include "util.h"


// RGBA color
struct color {
	uint8_t r, g, b, a;
};

// Parse color from HTML specification (e.g. #224433)
bool color_parse(struct color *color_dest, const char *html_src);

// Fill vector with normalized (0.0 - 1.0) color values
void color_normalize(const struct color *color_src, vec4_t *vec_dest);


#endif // __BTE_COLOR_H__
