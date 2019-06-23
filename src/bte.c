#include "glad/glad.h"
#include <locale.h>

#include "fonts.h"
#include "color.h"
#include "window.h"


#define BTE_WIDTH    1360
#define BTE_HEIGHT   720
#define BTE_TITLE    "bte"
#define BTE_COLOR_FG "#ffffff"
#define BTE_COLOR_BG "#222222"

const char *BTE_COLOR_PALETTE[256] = {
	[0] = "#22222222",
	[15] = "#ffffffff",
};


int main(int argc, const char **argv, const char **envp) {
	struct window * window;
	struct fonts *fonts;
	struct color color_fg, color_bg;
	vec4_t bg_normalized;

	setlocale(LC_ALL, "");

	if (!color_parse(&color_fg, BTE_COLOR_FG)) {
		die_fmt("Could not parse foreground color: %s", BTE_COLOR_FG);
	}
	if (!color_parse(&color_bg, BTE_COLOR_BG)) {
		die_fmt("Could not parse background color: %s", BTE_COLOR_BG);
	}
	color_normalize(&color_bg, &bg_normalized);

	window = window_new(BTE_WIDTH, BTE_HEIGHT, BTE_TITLE);
	fonts = fonts_new("monospace", 12);

	while (!window_should_close(window)) {
		window_get_events(window);
		glClearColor(bg_normalized.x, bg_normalized.y, bg_normalized.z, bg_normalized.w);
		glClear(GL_COLOR_BUFFER_BIT);
		window_refresh(window);
	}

	fonts_free(fonts);
	window_free(window);

	return 0;
}
