#include "glad/glad.h"
#include <time.h>
#include <unistd.h>
#include <locale.h>

#include "fonts.h"
#include "color.h"
#include "child.h"
#include "window.h"
#include "render.h"


#define CURSOR_BLOCK 9608

#define BTE_FONT     "monospace"
#define BTE_FONTSZ   13
#define BTE_WIDTH    1360
#define BTE_HEIGHT   720
#define BTE_TITLE    "bte"
#define BTE_SHELL    "/bin/sh"
#define BTE_TERM     "xterm-color"
#define BTE_FPS      60
#define BTE_CURSOR   CURSOR_BLOCK

#define TDIFF_NSEC (1000000000UL / BTE_FPS)

#define BTE_COLOR_FG "#d5c4a1"
#define BTE_COLOR_BG "#282828"

static const char *BTE_COLOR_PALETTE[16] = {
	"#282828", // color0
	"#fb4934", // color1
	"#b8bb26", // color2
	"#fabd2f", // color3
	"#83a598", // color4
	"#d3869b", // color5
	"#8ec07c", // color6
	"#d5c4a1", // color7
	"#665c54", // color8
	"#fe8019", // color9
	"#3c3836", // color10
	"#504945", // color11
	"#bdae93", // color12
	"#ebdbb2", // color13
	"#d65d0e", // color14
	"#d5c4a1", // color15
};

static struct color parsed_palette[16] = { 0 };


static struct child* _spawn_child(const char **envp, struct window *w, struct renderer *r) {
	size_t i, n_env, term_i = SIZE_MAX, shell_i = SIZE_MAX;
	const char **new_env, *new_argv[] = { BTE_SHELL, NULL };
	char buf[128];
	struct child *child;

	for (i = 0; i < 16; i++) {
		if (!color_parse(&parsed_palette[i], BTE_COLOR_PALETTE[i])) {
			die_fmt("Could not parse color: %s", BTE_COLOR_PALETTE[i]);
		}
	}

	for (n_env = 0; envp[n_env]; n_env++);
	if (!(new_env = calloc(n_env + 1, sizeof(char*)))) {
		die_err("calloc()");
	}
	for (i = 0; envp[i]; i++) {
		if (!strncmp(envp[i], "TERM=", 5)) {
			snprintf(buf, sizeof(buf), "TERM=" BTE_TERM);
			if (!(new_env[i] = strdup(buf))) {
				die_err("strdup()");
			}
			term_i = i;
		} else if (!strncmp(envp[i], "SHELL=", 6)) {
			snprintf(buf, sizeof(buf), "SHELL=" BTE_SHELL);
			if (!(new_env[i] = strdup(buf))) {
				die_err("strdup()");
			}
			shell_i = i;
		} else {
			new_env[i] = envp[i];
		}
	}

	child = child_new(new_argv, new_env, r, w, parsed_palette);

	if (term_i != SIZE_MAX) {
		free((void*) new_env[term_i]);
	}
	if (shell_i != SIZE_MAX) {
		free((void*) new_env[shell_i]);
	}
	free(new_env);

	return child;
}


int main(int argc, const char **argv, const char **envp) {
	struct window * window;
	struct fonts *fonts;
	struct renderer *renderer;
	struct child *child;
	struct timespec last, cur;
	uint64_t tdiff;

	setlocale(LC_ALL, "");

	window = window_new(BTE_WIDTH, BTE_HEIGHT, BTE_TITLE);
	fonts = fonts_new(BTE_FONT, BTE_FONTSZ);
	renderer = renderer_new(window, fonts, BTE_COLOR_FG, BTE_COLOR_BG, BTE_CURSOR);
	window_set_renderer(window, renderer);
	child = _spawn_child(envp, window, renderer);
	window_set_child(window, child);

	clock_gettime(CLOCK_MONOTONIC, &last);
	while (!window_should_close(window)) {
		window_get_events(window);
		renderer_update(renderer);

		clock_gettime(CLOCK_MONOTONIC, &cur);
		if (cur.tv_nsec < last.tv_nsec) {
			tdiff = (cur.tv_sec - last.tv_sec - 1) * 1000000000 \
				+ (1000000000 + cur.tv_nsec - last.tv_nsec);
		} else {
			tdiff = (cur.tv_sec - last.tv_sec) * 1000000000 \
				+ (cur.tv_nsec - last.tv_nsec);
		}
		if (tdiff / 1000 < TDIFF_NSEC / 1000) {
			usleep((TDIFF_NSEC - tdiff) / 1000);
		}
		last = cur;
	}

	window_set_renderer(window, NULL);
	renderer_free(renderer);
	fonts_free(fonts);
	window_free(window);

	return 0;
}
