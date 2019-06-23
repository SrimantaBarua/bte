#include "color.h"


// Parse 1 hex char into uint8_t.  Return 0xFF (impossible with 1 char) on invalid
static uint8_t parse_hex_char(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return 0xff;
}


// Parse 2 hex chars (hexpair) into uint8_t. Return false on error
static bool parse_hexpair(const char *s, uint8_t *val) {
	uint8_t r1, r2;
	if ((r1 = parse_hex_char(s[0])) == 0xff) {
		return false;
	}
	if ((r2 = parse_hex_char(s[1])) == 0xff) {
		return false;
	}
	*val = (r1 << 4) | r2;
	return true;
}


// Parse hex color definition ("#rrggbbaa") into color
bool color_parse(struct color *color, const char *s) {
	uint8_t c1, c2, c3, c4;
	if (!color || !s) {
		die("NULL src or dest");
	}
	if (*s != '#') {
		return false;
	}
	memset(color, 0, sizeof(struct color));
	if (!parse_hexpair(&s[1], &c1)) {
		return false;
	}
	if (!parse_hexpair(&s[3], &c2)) {
		return false;
	}
	if (!parse_hexpair(&s[5], &c3)) {
		return false;
	}
	color->r = c1;
	color->g = c2;
	color->b = c3;
	color->a = 0xff;
	if (parse_hexpair(&s[7], &c4)) {
		color->a = c4;
	}
	return true;
}


// Fill vector with normalized (0.0 - 1.0) color values
void color_normalize(const struct color *color, vec4_t *vec) {
	if (!color|| !vec) {
		die("NULL src or dest");
	}
	vec->x = color->r / 256.0;
	vec->y = color->g / 256.0;
	vec->z = color->b / 256.0;
	vec->w = color->a / 256.0;
}
