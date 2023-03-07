#define _POSIX_C_SOURCE 200112L
#include <math.h>
#include <time.h>
#include <cairo/cairo.h>
#include <librsvg/rsvg.h>
#include <wordexp.h>
#include "waywrap/waywrap.h"

struct color {
	double a;
	double r;
	double g;
	double b;
};

struct keyhold {
	long pressed_time;
	long counter;
	xkb_keysym_t key;
	struct keyhold *next;
};

void on_draw(struct surface_state *state, unsigned char *data);
void do_cairo(struct surface_state *state, cairo_t *ctx);
void do_cairo_text(cairo_t *cr, const char *str, int origin_x, int origin_y);
struct color hex2rgb(unsigned int val);
void on_keyboard(uint32_t, xkb_keysym_t, const char *); int run_cmd(const char *);
struct keyhold *keyhold_new(xkb_keysym_t);
struct keyhold *keyhold_add(struct keyhold *, xkb_keysym_t);
struct keyhold *keyhold_remove(struct keyhold *, xkb_keysym_t);
struct keyhold *keyhold_check(struct keyhold *, xkb_keysym_t);
long millis();

bool running = true;
char input_str[1024];
const char *placeholder_str = "Run a command...";
RsvgHandle *svg_handle;
struct color clr_main, clr_text, clr_placeholder;
struct keyhold *keyhold_root = NULL;

int main(int argc, char *argv[]) {
	clr_main = hex2rgb(0xAB8449);
	clr_placeholder = hex2rgb(0x999999);
	clr_text = hex2rgb(0x313131);

	strcpy(input_str, "");
	svg_handle = rsvg_handle_new_from_file("/usr/local/share/kallos/data/cmd.svg", NULL);
	rsvg_handle_set_dpi(svg_handle, 72.0);

   	struct client_state *state = client_state_new();
	state->on_keyboard = on_keyboard;
	struct surface_state *a;
    a = surface_state_new(state, "Kallos Runner", 748, 78);
	a->on_draw = on_draw;

	// TODO if width is bigger than display, then make it smaller!
	xdg_toplevel_set_max_size(a->xdg_toplevel, 748, 78);
	xdg_toplevel_set_min_size(a->xdg_toplevel, 748, 78);
	xdg_toplevel_set_app_id(a->xdg_toplevel, "kallos-runner");
	//xdg_toplevel_set_resize(a->xdg_toplevel, 400, 70);
	//xdg_surface_set_window_geometry(a->xdg_surface, 0, 0, 400, 70);
	//wl_surface_commit(a->wl_surface);

	zxdg_toplevel_decoration_v1_set_mode(a->decos, 1);

	long last = millis();
    while (state->root_surface != NULL && 
		wl_display_dispatch(state->wl_display) &&
		running == true) {

		long milli = millis();
		long delta = milli - last;
		struct keyhold *check = keyhold_check(keyhold_root, XKB_KEY_BackSpace);
		if (check != NULL) {
			long x = milli - check->pressed_time;
			if (x > state->key_repeat_delay) {
				check->counter += delta;
				if (check->counter >= state->key_repeat_rate/1000) {
					check->counter = 0;
					int len = strlen(input_str);
					input_str[len-1] = '\0';
				}
			}
		}
	};
	client_state_destroy(state);	

	if (strlen(input_str) > 0) {
		printf("running command! %s\n", input_str);
		run_cmd(input_str);
	}
    return 0;
}

void on_draw(struct surface_state *state, unsigned char *data) {
	cairo_surface_t *csurf = cairo_image_surface_create_for_data(data,
		CAIRO_FORMAT_ARGB32, state->width, state->height, state->width * 4);
	cairo_t *cairo_ctx = cairo_create(csurf);
	//cairo_scale(ctx, scale, scale);
	do_cairo(state, cairo_ctx);
	cairo_surface_flush(csurf);
	cairo_destroy(cairo_ctx);
	cairo_surface_destroy(csurf);
}

void do_cairo(struct surface_state *state, cairo_t *ctx) {
	int w = state->width;
	int h = state->height;

	cairo_set_source_rgb(ctx, 1, 1, 1);
	cairo_rectangle(ctx, 4, 4, w - 8, h - 8);
	cairo_fill(ctx);

	cairo_set_line_join(ctx, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width(ctx, 4);
	cairo_set_source_rgb(ctx, clr_main.r, clr_main.g, clr_main.b);
	cairo_rectangle(ctx, 2, 2, w-4, h-4);
	cairo_stroke(ctx);

	// Draw an svg image
	RsvgRectangle vp = {
		.x = 30, .y = 23,
		.width = 32, .height = 32,
	};
	rsvg_handle_render_document(svg_handle, ctx, &vp, NULL);

	if (strlen(input_str) <= 0) {
		cairo_set_source_rgb(ctx, clr_placeholder.r, clr_placeholder.g, 
			clr_placeholder.b);
		do_cairo_text(ctx, placeholder_str, 84, 46);
	} else {
		cairo_set_source_rgb(ctx, clr_text.r, clr_text.g, clr_text.b);
		do_cairo_text(ctx, input_str, 84, 46); //"Hello world! How are we party people, good?");
	}
}

void do_cairo_text(cairo_t *cr, const char *str, int origin_x, int origin_y) {
	cairo_font_extents_t fe;
	cairo_text_extents_t te;
	char letter[2];
	int x = 0;
	int len = strlen(str);

	cairo_set_font_size(cr, 20);	
	cairo_font_extents(cr, &fe);
	cairo_move_to(cr, 0, 0);
	for (int i=0; i < len; i++) {
	    *letter = '\0';
	    strncat(letter, str + i, 1);
	
	    cairo_text_extents(cr, letter, &te);
	    cairo_move_to(cr, origin_x + x, origin_y);
		x += te.x_advance;
	    cairo_show_text(cr, letter);
	}
}

struct color hex2rgb(unsigned int val) {
	struct color c;
	c.r = ((val >> 16) & 0xFF) / 255.0;
	c.g = ((val >> 8) & 0xFF) / 255.0;
	c.b = ((val) & 0xFF) / 255.0;
	return c;
}

bool is_valid_char(const char *str) {
	return strlen(str) > 0;
}

void on_keyboard(uint32_t state, xkb_keysym_t sym, const char *utf8) {
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		keyhold_root = keyhold_add(keyhold_root, sym);
		if (sym == XKB_KEY_BackSpace) {
			int len = strlen(input_str);
			input_str[len-1] = '\0';
		} else if (sym == XKB_KEY_Escape) {
			running = false;
			input_str[0] = '\0';
			printf("quit application!\n");
		} else if (sym == XKB_KEY_Return) {
			printf("submit command!\n");
			running = false;
		} else if (is_valid_char(utf8)) {
			//printf("char '%s'\n", utf8);
			strcat(input_str, utf8);
		}
	} else {
		keyhold_root = keyhold_remove(keyhold_root, sym);
	}
}

int run_cmd(const char *str) {
	wordexp_t w;
	switch (wordexp(str, &w, WRDE_NOCMD)) {
		case 0: 
			break;
		case WRDE_NOSPACE:
		case WRDE_CMDSUB:
		case WRDE_BADCHAR:
		default:
			return -1;
	}
	if (w.we_wordc < 1) {
		return -1;
	}
	const char *bin = w.we_wordv[0];
	if (!bin || !*bin) {
		return -1;
	}
	if (strchr(bin, '/'))
		execv(bin, w.we_wordv);
	else
		execvp(bin, w.we_wordv);
	return 0;
}

struct keyhold *keyhold_new(xkb_keysym_t key) {
	struct keyhold *k = malloc(sizeof(struct keyhold));
	k->key = key;
	k->pressed_time = millis();
	k->counter = 0;
	k->next = NULL;
	return k;
}

struct keyhold *keyhold_add(struct keyhold *root, xkb_keysym_t key) {
	struct keyhold *tmp, *n;
	n = keyhold_new(key);
	if (root == NULL)
		return n;
	tmp = root;
	// Find the last node in linked list.
	while (tmp != NULL) {
		// Don't add the key because it already exists.
		if (tmp->key == key) {
			free(n);
			return root;
		}
		if (tmp->next == NULL)
			break;
		tmp = tmp->next;
	}
	tmp->next = n;
	return root;
}

struct keyhold *keyhold_remove(struct keyhold *root, xkb_keysym_t key) {
	struct keyhold *tmp, *parent;
	tmp = root;
	parent = NULL;
	while (tmp != NULL) {
		if (tmp->key == key) {
			if (parent != NULL) {
				parent->next = tmp->next;
				root = parent;
			} else {
				root = tmp->next;
			}
			free(tmp);
			break;
		}
		parent = tmp;
		tmp = tmp->next;
	}
	return root;
}

struct keyhold *keyhold_check(struct keyhold *n, xkb_keysym_t key) {
	while (n != NULL) {
		if (n->key == key)
			break;
		n = n->next;
	}
	return n;
}

long millis() {
	struct timespec _t;
	clock_gettime(CLOCK_MONOTONIC, &_t);
	return _t.tv_sec*1000 + lround(_t.tv_nsec/1e6);
}
