#include <locale.h>

#include "fonts.h"
#include "window.h"


#define BTE_WIDTH  1360
#define BTE_HEIGHT 720
#define BTE_TITLE  "bte"


int main(int argc, const char **argv, const char **envp) {
	setlocale(LC_ALL, "");
	struct window *window = window_new(BTE_WIDTH, BTE_HEIGHT, BTE_TITLE);
	struct fonts *fonts = fonts_new("monospace", 12);
	fonts_free(fonts);
	window_free(window);
	return 0;
}
