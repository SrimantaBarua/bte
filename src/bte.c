#include "glad/glad.h"
#include <locale.h>

#include "fonts.h"
#include "color.h"
#include "window.h"
#include "render.h"


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
	struct renderer *renderer;

	setlocale(LC_ALL, "");

	window = window_new(BTE_WIDTH, BTE_HEIGHT, BTE_TITLE);
	fonts = fonts_new("monospace", 12);
	renderer = renderer_new(window, fonts, BTE_COLOR_FG, BTE_COLOR_BG);
	window_set_renderer(window, renderer);

	while (!window_should_close(window)) {
		window_get_events(window);
	}

	fonts_free(fonts);
	window_free(window);

	return 0;
}
