#include <locale.h>

#include "window.h"


#define BTE_WIDTH  1360
#define BTE_HEIGHT 720
#define BTE_TITLE  "bte"


int main(int argc, const char **argv, const char **envp) {
	setlocale(LC_ALL, "");
	struct window *window = window_new(BTE_WIDTH, BTE_HEIGHT, BTE_TITLE);
	window_free(window);
	return 0;
}
