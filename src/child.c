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
	if (!child->renderer) {
		return;
	}
	switch (esc->final) {
	case 'A':
		if (esc->nparam == 0) {
			esc->params[0] = 1;
		}
		renderer_move_up(child->renderer, esc->params[0]);
		break;
	case 'B':
		if (esc->nparam == 0) {
			esc->params[0] = 1;
		}
		renderer_move_down(child->renderer, esc->params[0]);
		break;
	case 'C':
		if (esc->nparam == 0) {
			esc->params[0] = 1;
		}
		renderer_move_right(child->renderer, esc->params[0]);
		break;
	case 'D':
		if (esc->nparam == 0) {
			esc->params[0] = 1;
		}
		renderer_move_left(child->renderer, esc->params[0]);
		break;
	case 'H':
		if (esc->nparam == 1) {
			esc->params[1] = 1;
		}
		if (esc->nparam == 0) {
			esc->params[0] = 1;
		}
		renderer_move_yx(child->renderer, esc->params[0], esc->params[1]);
		break;
	case 'J':
		if (esc->nparam == 0) {
			esc->params[0] = 0;
		}
		renderer_clear_screen(child->renderer, esc->params[0]);
		break;
	case 'K':
		if (esc->nparam == 0) {
			esc->params[0] = 0;
		}
		renderer_clear_line(child->renderer, esc->params[0]);
		break;
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
		/*
		if (cp >= 32) {
			printf("codepoint: %u : %c\n", cp, cp);
		} else {
			printf("codepoint: %u\n", cp);
		}
		*/

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

			/*
			if (cp >= 32) {
				printf("codepoint: %u : %c\n", cp, cp);
			} else {
				printf("codepoint: %u\n", cp);
			}
			*/

			param = 0;
			in_num = false;
			memset(&esc, 0, sizeof(struct esc_seq));
			while (1) {
				if ((cp = _next_codepoint(child)) == UINT32_MAX) {
					goto out;
				}

				/*
				if (cp >= 32) {
					printf("codepoint: %u : %c\n", cp, cp);
				} else {
					printf("codepoint: %u\n", cp);
				}
				*/

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
struct child* child_new(const char **argv, const char **envp, struct renderer *r, struct window *w) {
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
}
