#define _XOPEN_SOURCE 600
#include <fcntl.h>
#include <wchar.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <GLFW/glfw3.h>

#include "util.h"
#include "child.h"


#if 0

#define print_codepoint(cp) do { \
	if ((cp) >= 32) { \
		printf("codepoint: %u : %c\n", (cp), (cp)); \
	} else { \
		printf("codepoint: %u\n", (cp)); \
	} \
} while (0)

#else

#define print_codepoint(cp)

#endif


// An escape sequence
struct esc_seq {
	unsigned nparam;     // Number of parameters
	unsigned params[32]; // Parameters
	char     final;      // Final character
	char     private;    // Character denoting private escape sequence
};


static void _set_child_term_size(int fd, unsigned width, unsigned height) {
	struct winsize ws = { 0 };
	ws.ws_col = width;
	ws.ws_row = height;
	if (ioctl(fd, TIOCSWINSZ, &ws) < 0) {
		die_err("ioctl(TIOCSWINSZ)");
	}
}


static void _get_fds(int *master, int *slave) {
	char *slave_name;
	if ((*master = posix_openpt(O_RDWR | O_NOCTTY)) < 0) {
		die_err("posix_openpt()");
	}
	if (grantpt(*master) < 0) {
		die_err("grantpt()");
	}
	if (unlockpt(*master) < 0) {
		die_err("unlockpt()");
	}
	if (!(slave_name = ptsname(*master))) {
		die_err("ptsname()");
	}
	if ((*slave = open(slave_name, O_RDWR | O_NOCTTY)) < 0) {
		die_err("open()");
	}
}


static void _spawn_child(struct child *child, const char **argv, const char **envp) {
	pid_t pid;
	int fd_master, fd_slave;
	// Get FD pair
	_get_fds(&fd_master, &fd_slave);
	// Set child terminal size
	_set_child_term_size(fd_master, child->renderer->dim.x, child->renderer->dim.y);
	// Fork child
	if ((pid = fork()) < 0) {
		die_err("fork()");
	}
	if (pid == 0) {
		close(fd_master);
		setsid();
		if (ioctl(fd_slave, TIOCSCTTY, NULL) < 0) {
			die_err("ioctl(TIOCSCTTY)");
		}
		dup2(fd_slave, 0);
		dup2(fd_slave, 1);
		dup2(fd_slave, 2);
		close(fd_slave);
		execve(argv[0], (char * const *) argv, (char * const *) envp);
		exit(0);
	}
	close(fd_slave);
	child->pid = pid;
	child->fd = fd_master;
}


static int _fill_buf(struct child *child) {
	ssize_t ret;
	if (child->bufidx >= child->buflen) {
		child->bufidx = child->buflen = 0;
	} else {
		memcpy(child->buf, &child->buf[child->bufidx], child->buflen - child->bufidx);
		child->buflen -= child->bufidx;
		child->bufidx = 0;
	}
	if ((ret = read(child->fd, &child->buf[child->buflen], BUFSIZ - child->buflen)) <= 0) {
		return -1;
	}
	child->buflen += ret;
	return 0;
}


static uint32_t _next_codepoint(struct child *child) {
	mbstate_t ps;
	// TODO: Check size of wchar_t
	wchar_t wc;
	size_t ret, i;
	memset(&ps, 0, sizeof(ps));
	while (1) {
		ret = mbrtowc(&wc, (char*) &child->buf[child->bufidx], child->buflen - child->bufidx, &ps);
		if (ret == (size_t) -2) {
			// Incomplete multibyte character
			// Render what we have first
			if (child->renderer) {
				renderer_render(child->renderer);
			}
			// Fill buffer
			if (_fill_buf(child) < 0) {
				return UINT32_MAX;
			}
			continue;
		}
		if (ret == (size_t) -1) {
			// Invalid multibyte sequence
			memset(&ps, 0, sizeof(ps));
			warn_fmt("Could not process byte in multibyte string: %u\n", child->buf[child->bufidx]);
			child->bufidx++;
			continue;
		}
		if (ret == 0) {
			die("Unexpected ret == 0");
		}
		child->bufidx += ret;
		return (uint32_t) wc;
	}
}


static void _process_esc(struct child *child, struct esc_seq *esc) {
	unsigned i;
	if (!child->renderer) {
		return;
	}
	printf("Escape: \\x1b[");
	if (esc->private) {
		printf("%c", esc->private);
	}
	for (i = 0; i + 1 < esc->nparam; i++) {
		printf("%u;", esc->params[i]);
	}
	if (i < esc->nparam) {
		printf("%u", esc->params[i]);
	}
	printf("%c\n", esc->final);
	if (esc->private == 0) {
		switch (esc->final) {
		// Cursor position
		case 'A':
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			renderer_move_up(child->renderer, esc->params[0]);
			return;
		case 'B':
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			renderer_move_down(child->renderer, esc->params[0]);
			return;
		case 'C':
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			renderer_move_right(child->renderer, esc->params[0]);
			return;
		case 'D':
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			renderer_move_left(child->renderer, esc->params[0]);
			return;
		case 'H':
			if (esc->nparam == 1) {
				esc->params[1] = 1;
			}
			if (esc->nparam == 0) {
				esc->params[0] = 1;
			}
			renderer_move_yx(child->renderer, esc->params[0], esc->params[1]);
			return;
		// Clear line/screen
		case 'J':
			if (esc->nparam == 0) {
				esc->params[0] = 0;
			}
			renderer_clear_screen(child->renderer, esc->params[0]);
			return;
		case 'K':
			if (esc->nparam == 0) {
				esc->params[0] = 0;
			}
			renderer_clear_line(child->renderer, esc->params[0]);
			return;
		// Attributes
		case 'm':
			// TODO: Handle attribs other than color (bold, underline etc)
			i = 0;
			while (i < esc->nparam) {
				switch (esc->params[i]) {
				case 0:
					renderer_reset_fgcol(child->renderer);
					renderer_reset_bgcol(child->renderer);
					break;
				case 30:
					renderer_set_fgcol(child->renderer, &child->palette[0]);
					break;
				case 31:
					renderer_set_fgcol(child->renderer, &child->palette[1]);
					break;
				case 32:
					renderer_set_fgcol(child->renderer, &child->palette[2]);
					break;
				case 33:
					renderer_set_fgcol(child->renderer, &child->palette[3]);
					break;
				case 34:
					renderer_set_fgcol(child->renderer, &child->palette[4]);
					break;
				case 35:
					renderer_set_fgcol(child->renderer, &child->palette[5]);
					break;
				case 36:
					renderer_set_fgcol(child->renderer, &child->palette[6]);
					break;
				case 37:
					renderer_set_fgcol(child->renderer, &child->palette[7]);
					break;
				case 39:
					renderer_reset_fgcol(child->renderer);
					break;
				case 40:
					renderer_set_bgcol(child->renderer, &child->palette[0]);
					break;
				case 41:
					renderer_set_bgcol(child->renderer, &child->palette[1]);
					break;
				case 42:
					renderer_set_bgcol(child->renderer, &child->palette[2]);
					break;
				case 43:
					renderer_set_bgcol(child->renderer, &child->palette[3]);
					break;
				case 44:
					renderer_set_bgcol(child->renderer, &child->palette[4]);
					break;
				case 45:
					renderer_set_bgcol(child->renderer, &child->palette[5]);
					break;
				case 46:
					renderer_set_bgcol(child->renderer, &child->palette[6]);
					break;
				case 47:
					renderer_set_bgcol(child->renderer, &child->palette[7]);
					break;
				case 49:
					renderer_reset_bgcol(child->renderer);
					break;
				case 90:
					renderer_set_fgcol(child->renderer, &child->palette[8]);
					break;
				case 91:
					renderer_set_fgcol(child->renderer, &child->palette[9]);
					break;
				case 92:
					renderer_set_fgcol(child->renderer, &child->palette[10]);
					break;
				case 93:
					renderer_set_fgcol(child->renderer, &child->palette[11]);
					break;
				case 94:
					renderer_set_fgcol(child->renderer, &child->palette[12]);
					break;
				case 95:
					renderer_set_fgcol(child->renderer, &child->palette[13]);
					break;
				case 96:
					renderer_set_fgcol(child->renderer, &child->palette[14]);
					break;
				case 97:
					renderer_set_fgcol(child->renderer, &child->palette[15]);
					break;
				case 100:
					renderer_set_bgcol(child->renderer, &child->palette[8]);
					break;
				case 101:
					renderer_set_bgcol(child->renderer, &child->palette[9]);
					break;
				case 102:
					renderer_set_bgcol(child->renderer, &child->palette[10]);
					break;
				case 103:
					renderer_set_bgcol(child->renderer, &child->palette[11]);
					break;
				case 104:
					renderer_set_bgcol(child->renderer, &child->palette[12]);
					break;
				case 105:
					renderer_set_bgcol(child->renderer, &child->palette[13]);
					break;
				case 106:
					renderer_set_bgcol(child->renderer, &child->palette[14]);
					break;
				case 107:
					renderer_set_bgcol(child->renderer, &child->palette[15]);
					break;
				}
				i++;
			}
			return;
		}
	}
}


static void* _reader_thread(void *arg) {
	uint32_t cp;
	struct child *child = (struct child*) arg;
	struct esc_seq esc = { 0 };
	bool in_num = false;
	unsigned param = 0;

	while (1) {
		if ((cp = _next_codepoint(child)) == UINT32_MAX) {
			break;
		}
		print_codepoint(cp);

		if (!child->renderer) {
			continue;
		}

		// Beginning of escape sequence?
		if (cp == 27) {
			if ((cp = _next_codepoint(child)) == UINT32_MAX) {
				break;
			}
			if (cp != '[') {
				continue;
			}

			print_codepoint(cp);

			param = 0;
			in_num = false;
			memset(&esc, 0, sizeof(struct esc_seq));
			while (1) {
				if ((cp = _next_codepoint(child)) == UINT32_MAX) {
					goto out;
				}

				print_codepoint(cp);

				if (cp == '?') {
					if (in_num) {
						if (esc.nparam < sizeof(esc.params) / sizeof(esc.params[0])) {
							esc.params[esc.nparam++] = param;
						}
					}
					esc.private = '?';
					continue;
				}
				if (cp == ';') {
					if (esc.nparam < sizeof(esc.params) / sizeof(esc.params[0])) {
						esc.params[esc.nparam++] = param;
					}
					param = 0;
					in_num = false;
					continue;
				}
				if (cp >= '0' && cp <= '9') {
					in_num = true;
					param = param * 10 + (cp - '0');
					continue;
				}
				if (esc.nparam < sizeof(esc.params) / sizeof(esc.params[0])) {
					esc.params[esc.nparam++] = param;
				}
				esc.final = cp;
				break;
			}
			_process_esc(child, &esc);
			renderer_render(child->renderer);
			continue;
		}

		if (cp == '\a') {
			continue;
		}
		renderer_add_codepoint(child->renderer, cp);
		renderer_render(child->renderer);
	}

out:
	if (child->window) {
		window_set_should_close(child->window);
	}

	return NULL;
}


// Initialize new child
struct child* child_new(const char **argv, const char **envp, struct renderer *r, struct window *w, const struct color *palette) {
	struct child *child;
	if (!argv || !*argv) {
		die("NULL argv");
	}
	if (!r) {
		die("NULL renderer");
	}
	if (!w) {
		die("NULL window");
	}
	// Allocate child
	if (!(child = calloc(1, sizeof(struct child)))) {
		die_err("calloc()");
	}
	// Set pointers
	child->renderer = r;
	child->window = w;
	child->palette = palette;
	// Get FD and spawn chid
	_spawn_child(child, argv, envp);
	// Allocate read buffer
	if (!(child->buf = malloc(BUFSIZ))) {
		die_err("malloc()");
	}
	// Start reader thread
	if (pthread_create(&child->tid, NULL, _reader_thread, (void*) child)) {
		die_err("pthread_create()");
	}
	return child;
}


// Shutdown child
void child_fini(struct child *child) {
	if (!child) {
		warn("NULL child");
		return;
	}
	kill(child->pid, SIGKILL);
	waitpid(child->pid, NULL, 0);
	pthread_join(child->tid, NULL);
	free(child->buf);
	close(child->fd);
	free(child);
}


// Write codepoints to child
static void _write_cps_to_child(struct child *child, wchar_t *cps) {
	char tmp[128];
	size_t tmplen = 0, i = 0;
	ssize_t ret;
	// Write codepoint to multibyte string
	// TODO: Check size of wchar_t
	if ((tmplen = wcstombs(tmp, cps, sizeof(tmp))) == (size_t) -1) {
		return;
	}
	// Write
	while (i < tmplen) {
		if ((ret = write(child->fd, tmp + i, tmplen - i)) < 0) {
			die_err("write()");
		}
		i += ret;
	}
}


// Callback for unicode codepoints (called by window)
void child_char_cb(struct child *child, uint32_t codepoint) {
	// TODO: Check size of wchar_t
	wchar_t cps[2] = { 0 };
	cps[0] = codepoint;
	_write_cps_to_child(child, cps);
}


#define _write_1_cp_to_child(c0) \
	do { cps[0] = (c0); _write_cps_to_child(child, cps); } while (0)

#define _write_2_cp_to_child(c0, c1) \
	do { cps[0] = (c0); cps[1] = (c1); _write_cps_to_child(child, cps); } while (0)

#define _write_3_cp_to_child(c0, c1, c2) \
	do { cps[0] = (c0); cps[1] = (c1); cps[2] = (c2); _write_cps_to_child(child, cps); } while (0)


// Callback for other keypresses (called by window)
void child_key_cb(struct child *child, int key, int mods) {
	// TODO: Check size of wchar_t
	wchar_t cps[4] = { 0 };
	if (mods & GLFW_MOD_CONTROL) {
		switch (key) {
		case GLFW_KEY_A: _write_1_cp_to_child(1); break;
		case GLFW_KEY_B: _write_1_cp_to_child(2); break;
		case GLFW_KEY_C: _write_1_cp_to_child(3); break;
		case GLFW_KEY_D: _write_1_cp_to_child(4); break;
		case GLFW_KEY_E: _write_1_cp_to_child(5); break;
		case GLFW_KEY_F: _write_1_cp_to_child(6); break;
		case GLFW_KEY_G: _write_1_cp_to_child(7); break;
		case GLFW_KEY_H: _write_1_cp_to_child(8); break;
		case GLFW_KEY_I: _write_1_cp_to_child(9); break;
		case GLFW_KEY_J: _write_1_cp_to_child(10); break;
		case GLFW_KEY_K: _write_1_cp_to_child(11); break;
		case GLFW_KEY_L: _write_1_cp_to_child(12); break;
		case GLFW_KEY_M: _write_1_cp_to_child(13); break;
		case GLFW_KEY_N: _write_1_cp_to_child(14); break;
		case GLFW_KEY_O: _write_1_cp_to_child(15); break;
		case GLFW_KEY_P: _write_1_cp_to_child(16); break;
		case GLFW_KEY_Q: _write_1_cp_to_child(17); break;
		case GLFW_KEY_R: _write_1_cp_to_child(18); break;
		case GLFW_KEY_S: _write_1_cp_to_child(19); break;
		case GLFW_KEY_T: _write_1_cp_to_child(20); break;
		case GLFW_KEY_U: _write_1_cp_to_child(21); break;
		case GLFW_KEY_V: _write_1_cp_to_child(22); break;
		case GLFW_KEY_W: _write_1_cp_to_child(23); break;
		case GLFW_KEY_X: _write_1_cp_to_child(24); break;
		case GLFW_KEY_Y: _write_1_cp_to_child(25); break;
		case GLFW_KEY_Z: _write_1_cp_to_child(26); break;
		}
	}
	switch (key) {
	case GLFW_KEY_UP:        _write_3_cp_to_child(27, '[', 'A'); break;
	case GLFW_KEY_DOWN:      _write_3_cp_to_child(27, '[', 'B'); break;
	case GLFW_KEY_RIGHT:     _write_3_cp_to_child(27, '[', 'C'); break;
	case GLFW_KEY_LEFT:      _write_3_cp_to_child(27, '[', 'D'); break;
	case GLFW_KEY_TAB:       _write_1_cp_to_child('\t'); break;
	case GLFW_KEY_BACKSPACE: _write_1_cp_to_child('\b'); break;
	case GLFW_KEY_ESCAPE:    _write_1_cp_to_child(27); break;
	case GLFW_KEY_ENTER:     _write_1_cp_to_child('\n');  break;
	}
}


// Callback for resize
void child_resize_cb(struct child *child) {
	if (!child) {
		die("NULL child");
	}
	// Set child terminal size
	_set_child_term_size(child->fd, child->renderer->dim.x, child->renderer->dim.y);
	// Signal child
	kill(child->pid, SIGWINCH);
}
