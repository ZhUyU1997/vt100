/**@file      gui.c
 * @brief     Simulate the H2 SoC peripherals visually
 * @copyright Richard James Howe (2017)
 * @license   MIT */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/freeglut_ext.h> /* for glutStrokeHeight */

#define VGA_BUFFER_LENGTH          (1 << 13)
#define VGA_WIDTH                  (80)
#define VGA_HEIGHT                 (40)
#define VGA_AREA                   (VGA_WIDTH * VGA_HEIGHT)

#define VGA_CTL_B_BIT              (0)
#define VGA_CTL_G_BIT              (1)
#define VGA_CTL_R_BIT              (2)
#define VGA_CUR_MODE_BIT           (3)
#define VGA_CUR_BLINK_BIT          (4)
#define VGA_CUR_EN_BIT             (5)
#define VGA_EN_BIT                 (6)
#define VGA_SCREEN_SELECT_BIT      (7)

#define VGA_CTL_B                  (1 << VGA_CTL_B_BIT)
#define VGA_CTL_G                  (1 << VGA_CTL_G_BIT)
#define VGA_CTL_R                  (1 << VGA_CTL_R_BIT)
#define VGA_CUR_MODE               (1 << VGA_CUR_MODE_BIT)
#define VGA_CUR_BLINK              (1 << VGA_CUR_BLINK_BIT)
#define VGA_CUR_EN                 (1 << VGA_CUR_EN_BIT)
#define VGA_EN                     (1 << VGA_EN_BIT)
#define VGA_SCREEN_SELECT          (1 << VGA_SCREEN_SELECT_BIT)

#define UART_FIFO_DEPTH            (8)

typedef enum { /**@warning do not change the order or insert elements */
	BLACK,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	MAGENTA,
	CYAN,
	WHITE,
} color_t;

typedef enum {
	TERMINAL_NORMAL_MODE,
	TERMINAL_CSI,
	TERMINAL_COMMAND,
	TERMINAL_NUMBER_1,
	TERMINAL_NUMBER_2,
	TERMINAL_DECTCEM,
	TERMINAL_STATE_END,
} terminal_state_t;

typedef struct {
	unsigned bold:          1;
	unsigned under_score:   1;
	unsigned blink:         1;
	unsigned reverse_video: 1;
	unsigned conceal:       1;
	unsigned foreground_color: 3;
	unsigned background_color: 3;
} vt100_attribute_t;

#define VT100_MAX_SIZE (8192)

typedef struct {
	size_t cursor;
	size_t cursor_saved;
	unsigned n1, n2;
	unsigned height;
	unsigned width;
	unsigned size;
	terminal_state_t state;
	bool blinks;
	bool cursor_on;
	vt100_attribute_t attribute;
	vt100_attribute_t attributes[VT100_MAX_SIZE];
	uint8_t m[VT100_MAX_SIZE];
	uint8_t command_index;
} vt100_t;

void *allocate_or_die(size_t length);
FILE *fopen_or_die(const char *file, const char *mode);


typedef uint8_t fifo_data_t;

typedef struct {
	size_t head;
	size_t tail;
	size_t size;
	fifo_data_t *buffer;
} fifo_t;

/** @warning LOG_FATAL level kills the program */
#define X_MACRO_LOGGING\
	X(LOG_MESSAGE_OFF,  "")\
	X(LOG_FATAL,        "fatal")\
	X(LOG_ERROR,        "error")\
	X(LOG_WARNING,      "warning")\
	X(LOG_NOTE,         "note")\
	X(LOG_DEBUG,        "debug")\
	X(LOG_ALL_MESSAGES, "any")

typedef enum {
#define X(ENUM, NAME) ENUM,
	X_MACRO_LOGGING
#undef X
} log_level_e;

int logger(log_level_e level, const char *func, const unsigned line, const char *fmt, ...);

#define fatal(FMT, ...)   logger(LOG_FATAL,   __func__, __LINE__, FMT, ##__VA_ARGS__)
#define error(FMT, ...)   logger(LOG_ERROR,   __func__, __LINE__, FMT, ##__VA_ARGS__)
#define warning(FMT, ...) logger(LOG_WARNING, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define note(FMT, ...)    logger(LOG_NOTE,    __func__, __LINE__, FMT, ##__VA_ARGS__)
#define debug(FMT, ...)   logger(LOG_DEBUG,   __func__, __LINE__, FMT, ##__VA_ARGS__)
#define BACKSPACE (8)
#define ESCAPE    (27)
#define DELETE    (127)  /* ASCII delete */

void vt100_update(vt100_t *t, uint8_t c);

/* ====================================== Utility Functions ==================================== */

#define PI               (3.1415926535897932384626433832795)
#define MAX(X, Y)        ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y)        ((X) < (Y) ? (X) : (Y))
#define UNUSED(X)        ((void)(X))
#define X_MAX            (100.0)
#define X_MIN            (0.0)
#define Y_MAX            (100.0)
#define Y_MIN            (0.0)
#define LINE_WIDTH       (0.5)
#define CYCLE_MODE_FIXED (false)
#define CYCLE_INITIAL    (100000)
#define CYCLE_INCREMENT  (10000)
#define CYCLE_DECREMENT  (500)
#define CYCLE_MINIMUM    (10000)
#define CYCLE_HYSTERESIS (2.0)
#define TARGET_FPS       (30.0)
#define BACKGROUND_ON    (false)


static const char *log_levels[] =
{
#define X(ENUM, NAME) [ENUM] = NAME,
	X_MACRO_LOGGING
#undef X
};

static log_level_e log_level = LOG_WARNING;

/* ========================== Preamble: Types, Macros, Globals ============= */

/* ========================== Utilities ==================================== */

int logger(log_level_e level, const char *func,
		const unsigned line, const char *fmt, ...)
{
	int r = 0;
	va_list ap;
       	assert(func);
       	assert(fmt);
	assert(level <= LOG_ALL_MESSAGES);
	if(level <= log_level) {
		fprintf(stderr, "[%s %u] %s: ", func, line, log_levels[level]);
		va_start(ap, fmt);
		r = vfprintf(stderr, fmt, ap);
		va_end(ap);
		fputc('\n', stderr);
		fflush(stderr);
	}
	if(level == LOG_FATAL)
		exit(EXIT_FAILURE);
	return r;
}

static const char *reason(void)
{
	static const char *unknown = "unknown reason";
	const char *r;
	if(errno == 0)
		return unknown;
	r = strerror(errno);
	if(!r)
		return unknown;
	return r;
}

void *allocate_or_die(size_t length)
{
	void *r;
	errno = 0;
	r = calloc(1, length);
	if(!r)
		fatal("allocation of size %zu failed: %s",
				length, reason());
	return r;
}

FILE *fopen_or_die(const char *file, const char *mode)
{
	FILE *f = NULL;
	assert(file);
	assert(mode);
	errno = 0;
	f = fopen(file, mode);
	if(!f)
		fatal("failed to open file '%s' (mode %s): %s",
				file, mode, reason());
	return f;
}

typedef struct {
	double window_height;
	double window_width;
	double window_x_starting_position;
	double window_y_starting_position;
	double window_scale_x;
	double window_scale_y;
	volatile unsigned tick;
	volatile bool     halt_simulation;
	unsigned arena_tick_ms;
	bool use_uart_input;
	bool debug_extra;
	bool step;
	bool debug_mode;
	uint64_t cycle_count;
	uint64_t cycles;
	void *font_scaled;
} world_t;

static world_t world = {
	.window_height               = 800.0,
	.window_width                = 800.0,
	.window_x_starting_position  = 60.0,
	.window_y_starting_position  = 20.0,
	.window_scale_x              = 1.0,
	.window_scale_y              = 1.0,
	.tick                        = 0,
	.halt_simulation             = false,
	.arena_tick_ms               = 30, /**@todo This should be automatically adjusted based on frame rate */
	.use_uart_input              = true,
	.debug_extra                 = false,
	.step                        = false,
	.debug_mode                  = false,
	.cycle_count                 = 0,
	.cycles                      = CYCLE_INITIAL,
	.font_scaled                 = GLUT_STROKE_MONO_ROMAN
};

typedef enum {
	TRIANGLE,
	SQUARE,
	PENTAGON,
	HEXAGON,
	SEPTAGON,
	OCTAGON,
	DECAGON,
	CIRCLE,
	INVALID_SHAPE
} shape_e;

typedef shape_e shape_t;

typedef struct {
	double x;
	double y;
} scale_t;

typedef struct {
	double x, y;
	bool draw_border;
	color_t color_text, color_box;
	double width, height;
} textbox_t;

typedef struct { /**@note it might be worth translating some functions to use points*/
	double x, y;
} point_t;

static void terminal_default_command_sequence(vt100_t *t)
{
	assert(t);
	t->n1 = 1;
	t->n2 = 1;
	t->command_index = 0;
}

static void terminal_at_xy(vt100_t *t, unsigned x, unsigned y, bool limit_not_wrap)
{
	assert(t);
	if(limit_not_wrap) {
		x = MAX(x, 0);
		y = MAX(y, 0);
		x = MIN(x, t->width - 1);
		y = MIN(y, t->height - 1);
	} else {
		x %= t->width;
		y %= t->height;
	}
	t->cursor = (y * t->width) + x;
}

static int terminal_x_current(vt100_t *t)
{
	assert(t);
	return t->cursor % t->width;
}

static int terminal_y_current(vt100_t *t)
{
	assert(t);
	return t->cursor / t->width;
}

static void terminal_at_xy_relative(vt100_t *t, int x, int y, bool limit_not_wrap)
{
	assert(t);
	int x_current = terminal_x_current(t);
	int y_current = terminal_y_current(t);
	terminal_at_xy(t, MAX(x_current + x, 0), MAX(y_current + y, 0), limit_not_wrap);
}

static void terminal_parse_attribute(vt100_attribute_t *a, unsigned v)
{
	switch(v) {
	case 0:
		memset(a, 0, sizeof(*a));
		a->foreground_color = WHITE;
		a->background_color = BLACK;
		return;
	case 1: a->bold          = true; return;
	case 4: a->under_score   = true; return;
	case 5: a->blink         = true; return;
	case 7: a->reverse_video = true; return;
	case 8: a->conceal       = true; return;
	default:
		if(v >= 30 && v <= 37)
			a->foreground_color = v - 30;
		if(v >= 40 && v <= 47)
			a->background_color = v - 40;
	}
}

static const vt100_attribute_t vt100_default_attribute = {
	.foreground_color = WHITE,
	.background_color = BLACK,
};

static void terminal_attribute_block_set(vt100_t *t, size_t size, const vt100_attribute_t const *a)
{
	assert(t);
	assert(a);
	for(size_t i = 0; i < size; i++)
		memcpy(&t->attributes[i], a, sizeof(*a));
}

static int terminal_escape_sequences(vt100_t *t, uint8_t c)
{
	assert(t);
	assert(t->state != TERMINAL_NORMAL_MODE);
	switch(t->state) {
	case TERMINAL_CSI:
		if(c == '[')
			t->state = TERMINAL_COMMAND;
		else
			goto fail;
		break;
	case TERMINAL_COMMAND:
		switch(c) {
		case 's':
			t->cursor_saved = t->cursor;
			goto success;
		case 'n':
			t->cursor = t->cursor_saved;
			goto success;
		case '?':
			terminal_default_command_sequence(t);
			t->state = TERMINAL_DECTCEM;
			break;
		case ';':
			terminal_default_command_sequence(t);
			t->state = TERMINAL_NUMBER_2;
			break;
		default:
			if(isdigit(c)) {
				terminal_default_command_sequence(t);
				t->command_index++;
				t->n1 = c - '0';
				t->state = TERMINAL_NUMBER_1;
			} else {
				goto fail;
			}
		}
		break;
	case TERMINAL_NUMBER_1:
		if(isdigit(c)) {
			if(t->command_index > 3)
				goto fail;
			t->n1 = (t->n1 * (t->command_index ? 10 : 0)) + (c - '0');
			t->command_index++;
			break;
		}

		switch(c) {
		case 'A': terminal_at_xy_relative(t,  0,     -t->n1, true); goto success;/* relative cursor up */
		case 'B': terminal_at_xy_relative(t,  0,      t->n1, true); goto success;/* relative cursor down */
		case 'C': terminal_at_xy_relative(t,  t->n1,  0,     true); goto success;/* relative cursor forward */
		case 'D': terminal_at_xy_relative(t, -t->n1,  0,     true); goto success;/* relative cursor back */
		case 'E': terminal_at_xy(t, 0,  t->n1, false); goto success; /* relative cursor down, beginning of line */
		case 'F': terminal_at_xy(t, 0, -t->n1, false); goto success; /* relative cursor up, beginning of line */
		case 'G': terminal_at_xy(t, t->n1, terminal_y_current(t), true); goto success; /* move the cursor to column n */
		case 'm': /* set attribute, CSI number m */
			terminal_parse_attribute(&t->attribute, t->n1);
			t->attributes[t->cursor] = t->attribute;
			goto success;
		case 'i': /* AUX Port On == 5, AUX Port Off == 4 */
			if(t->n1 == 5 || t->n1 == 4)
				goto success;
			goto fail;
		case 'n': /* Device Status Report */
			/** @note This should transmit to the H2 system the
			 * following "ESC[n;mR", where n is the row and m is the column,
			 * we're not going to do this, although fifo_push() on
			 * uart_rx_fifo could be called to do this */
			if(t->n1 == 6)
				goto success;
			goto fail;
		case 'J': /* reset */
			switch(t->n1) {
			case 3:
			case 2: t->cursor = 0; /* with cursor */
			case 1:
				if(t->command_index) {
					memset(t->m, ' ', t->size);
					terminal_attribute_block_set(t, t->size, &vt100_default_attribute);
					goto success;
				} /* fall through if number not supplied */
			case 0:
				memset(t->m, ' ', t->cursor);
				terminal_attribute_block_set(t, t->cursor, &vt100_default_attribute);
				goto success;
			}
			goto fail;
		case ';':
			t->command_index = 0;
			t->state = TERMINAL_NUMBER_2;
			break;
		default:
			goto fail;
		}
		break;
	case TERMINAL_NUMBER_2:
		if(isdigit(c)) {
			if(t->command_index > 3)
				goto fail;
			t->n2 = (t->n2 * (t->command_index ? 10 : 0)) + (c - '0');
			t->command_index++;
		} else {
			switch(c) {
			case 'm':
				terminal_parse_attribute(&t->attribute, t->n1);
				terminal_parse_attribute(&t->attribute, t->n2);
				t->attributes[t->cursor] = t->attribute;
				goto success;
			case 'H':
			case 'f':
				terminal_at_xy(t, t->n2, t->n1, true);
				goto success;
			}
			goto fail;
		}
		break;
	case TERMINAL_DECTCEM:
		if(isdigit(c)) {
			if(t->command_index > 1)
				goto fail;
			t->n1 = (t->n1 * (t->command_index ? 10 : 0)) + (c - '0');
			t->command_index++;
			break;
		}

		if(t->n1 != 25)
			goto fail;
		switch(c) {
		case 'l': t->cursor_on = false; goto success;
		case 'h': t->cursor_on = true;  goto success;
		default:
			goto fail;
		}
	case TERMINAL_STATE_END:
		t->state = TERMINAL_NORMAL_MODE;
		break;
	default:
		fatal("invalid terminal state: %u", (unsigned)t->state);
	}

	return 0;
success:
	t->state = TERMINAL_NORMAL_MODE;
	return 0;
fail:
	t->state = TERMINAL_NORMAL_MODE;
	return -1;
}

void vt100_update(vt100_t *t, uint8_t c)
{
	assert(t);
	assert(t->size <= VT100_MAX_SIZE);
	assert((t->width * t->height) <= VT100_MAX_SIZE);

	if(t->state != TERMINAL_NORMAL_MODE) {
		if(terminal_escape_sequences(t, c)) {
			t->state = TERMINAL_NORMAL_MODE;
			/*warning("invalid ANSI command sequence");*/
		}
	} else {
		switch(c) {
		case ESCAPE:
			t->state = TERMINAL_CSI;
			break;
		case '\t':
			t->cursor += 8;
			t->cursor &= ~0x7;
			break;
		case '\r':
		case '\n':
			t->cursor += t->width;
			t->cursor = (t->cursor / t->width) * t->width;
			break;
		case DELETE:
		case BACKSPACE:
			terminal_at_xy_relative(t, -1, 0, true);
			t->m[t->cursor] = ' ';
			break;
		default:
			assert(t->cursor < t->size);
			t->m[t->cursor] = c;
			memcpy(&t->attributes[t->cursor], &t->attribute, sizeof(t->attribute));
			t->cursor++;
		}
		if(t->cursor >= t->size) {
			terminal_attribute_block_set(t, t->size, &vt100_default_attribute);
			memset(t->m, ' ', t->size);
		}
		t->cursor %= t->size;
	}
}

/**@bug not quite correct, arena_tick_ms is what we request, not want the arena
 * tick actually is */
static double seconds_to_ticks(const world_t *world, double s)
{
	assert(world);
	return s * (1000. / (double)world->arena_tick_ms);
}

static double rad2deg(double rad)
{
	return (rad / (2.0 * PI)) * 360.0;
}

static void set_color(color_t color, bool light)
{
	double ON = light ? 0.8 : 0.4;
	static const double OFF = 0.0;
	switch(color) {      /* RED  GRN  BLU */
	case WHITE:   glColor3f( ON,  ON,  ON);   break;
	case RED:     glColor3f( ON, OFF, OFF);   break;
	case YELLOW:  glColor3f( ON,  ON, OFF);   break;
	case GREEN:   glColor3f(OFF,  ON, OFF);   break;
	case CYAN:    glColor3f(OFF,  ON,  ON);   break;
	case BLUE:    glColor3f(OFF, OFF,  ON);   break;
	case MAGENTA: glColor3f( ON, OFF,  ON);   break;
	case BLACK:   glColor3f(OFF, OFF, OFF);   break;
	default:      fatal("invalid color '%d'", color);
	}
}

/* see: https://www.opengl.org/discussion_boards/showthread.php/160784-Drawing-Circles-in-OpenGL */
static void _draw_regular_polygon(
		double x, double y,
		double orientation,
		double radius, double sides,
		bool lines, double thickness,
		color_t color)
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
		glLoadIdentity();
		glTranslatef(x, y, 0.0);
		glRotated(rad2deg(orientation), 0, 0, 1);
		set_color(color, true);
		if(lines) {
			glLineWidth(thickness);
			glBegin(GL_LINE_LOOP);
		} else {
			glBegin(GL_POLYGON);
		}
			for(double i = 0; i < 2.0 * PI; i += PI / sides)
				glVertex3d(cos(i) * radius, sin(i) * radius, 0.0);
		glEnd();
	glPopMatrix();
}

static void _draw_rectangle(double x, double y, double width, double height, bool lines, double thickness, color_t color)
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
		glLoadIdentity();
		glRasterPos2d(x, y);
		set_color(color, true);
		if(lines) {
			glLineWidth(thickness);
			glBegin(GL_LINE_LOOP);
		} else {
			glBegin(GL_POLYGON);
		}
		glVertex3d(x,       y,        0);
		glVertex3d(x+width, y,        0);
		glVertex3d(x+width, y+height, 0);
		glVertex3d(x,       y+height, 0);
		glEnd();
	glPopMatrix();
}

static void draw_rectangle_filled(double x, double y, double width, double height, color_t color)
{
	return _draw_rectangle(x, y, width, height, false, 0, color);
}

static void draw_rectangle_line(double x, double y, double width, double height, double thickness, color_t color)
{
	return _draw_rectangle(x, y, width, height, true, thickness, color);
}

static double shape_to_sides(shape_t shape)
{
	static const double sides[] =
	{
		[TRIANGLE] = 1.5,
		[SQUARE]   = 2,
		[PENTAGON] = 2.5,
		[HEXAGON]  = 3,
		[SEPTAGON] = 3.5,
		[OCTAGON]  = 4,
		[DECAGON]  = 5,
		[CIRCLE]   = 24
	};
	if(shape >= INVALID_SHAPE)
		fatal("invalid shape '%d'", shape);
	return sides[shape % INVALID_SHAPE];
}

/* static void draw_regular_polygon_filled(double x, double y, double orientation, double radius, shape_t shape, color_t color)
{
	double sides = shape_to_sides(shape);
	_draw_regular_polygon(x, y, orientation, radius, sides, false, 0, color);
} */

static void draw_regular_polygon_line(double x, double y, double orientation, double radius, shape_t shape, double thickness, color_t color)
{
	double sides = shape_to_sides(shape);
	_draw_regular_polygon(x, y, orientation, radius, sides, true, thickness, color);
}

static void draw_char(uint8_t c)
{
	c = c >= 32 && c <= 127 ? c : '?';
	glutStrokeCharacter(world.font_scaled, c);
}

/* see: https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_01
 *      https://stackoverflow.com/questions/538661/how-do-i-draw-text-with-glut-opengl-in-c
 *      https://stackoverflow.com/questions/20866508/using-glut-to-simply-print-text */
static int draw_block(const uint8_t *msg, size_t len)
{
	assert(msg);
	for(size_t i = 0; i < len; i++)
		draw_char(msg[i]);
	return len;
}

static int draw_string(const char *msg)
{
	assert(msg);
	return draw_block((uint8_t*)msg, strlen(msg));
}

static scale_t font_attributes(void)
{
	static bool initialized = false;
	static scale_t scale = { 0., 0.};
	if(initialized)
		return scale;
	scale.y = glutStrokeHeight(world.font_scaled);
	scale.x = glutStrokeWidth(world.font_scaled, 'M');
	initialized = true;
	return scale;
}

static void draw_vt100_char(double x, double y, double scale_x, double scale_y, double orientation, uint8_t c, vt100_attribute_t *attr, bool blink)
{
	/*scale_t scale = font_attributes();
	double char_width  = scale.x / X_MAX;
       	double char_height = scale.y / Y_MAX;*/

	if(blink && attr->blink)
		return;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
		glLoadIdentity();
		glTranslatef(x, y, 0.0);
		glScaled(scale_x, scale_y, 1.0);
		glRotated(rad2deg(orientation), 0, 0, 1);
		set_color(attr->foreground_color, attr->bold);
		draw_char(attr->conceal ? '*' : c);
		glEnd();
	glPopMatrix();
	if(BACKGROUND_ON)
		draw_rectangle_filled(x, y, 1.20, 1.55, attr->background_color);
}

static int draw_vt100_block(double x, double y, double scale_x, double scale_y, double orientation, const uint8_t *msg, size_t len, vt100_attribute_t *attr, bool blink)
{
	scale_t scale = font_attributes();
	double char_width = (scale.x / X_MAX)*1.1;
	for(size_t i = 0; i < len; i++)
		draw_vt100_char(x+char_width*i, y, scale_x, scale_y, orientation, msg[i], &attr[i], blink);
	return len;
}

static int draw_block_scaled(double x, double y, double scale_x, double scale_y, double orientation, const uint8_t *msg, size_t len, color_t color)
{
	assert(msg);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
		glLoadIdentity();
		glTranslatef(x, y, 0.0);
		glScaled(scale_x, scale_y, 1.0);
		glRotated(rad2deg(orientation), 0, 0, 1);
		set_color(color, true);
		for(size_t i = 0; i < len; i++) {
			uint8_t c = msg[i];
			c = c >= 32 && c <= 127 ? c : '?';
			glutStrokeCharacter(world.font_scaled, c);
		}
		glEnd();
	glPopMatrix();
	return len;
}

static int draw_string_scaled(double x, double y, double scale_x, double scale_y, double orientation, const char *msg, color_t color)
{
	assert(msg);
	return draw_block_scaled(x, y, scale_x, scale_y, orientation, (uint8_t*)msg, strlen(msg), color);
}

static int vdraw_text(color_t color, double x, double y, const char *fmt, va_list ap)
{
	char f;
	int r = 0;
	assert(fmt);
	static const double scale_x = 0.011;
	static const double scale_y = 0.011;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	set_color(color, true);
	glTranslatef(x, y, 0);
	glScaled(scale_x, scale_y, 1.0);
	while(*fmt) {
		if('%' != (f = *fmt++)) {
			glutStrokeCharacter(world.font_scaled, f);
			r++;
			continue;
		}
		switch(f = *fmt++) {
		case 'c':
		{
			char x[2] = {0, 0};
			x[0] = va_arg(ap, int);
			r += draw_string(x);
			break;
		}
		case 's':
		{
			char *s = va_arg(ap, char*);
			r += draw_string(s);
			break;
		}
		case 'x':
		{
			unsigned d = va_arg(ap, unsigned);
			char s[64] = {0};
			sprintf(s, "%04x", d);
			r += draw_string(s);
			break;
		}
		case 'u':
		case 'd':
		{
			int d = va_arg(ap, int);
			char s[64] = {0};
			sprintf(s, f == 'u' ? "%u": "%d", d);
			r += draw_string(s);
			break;
		}
		case 'f':
		{
			double f = va_arg(ap, double);
			char s[512] = {0};
			sprintf(s, "%.2f", f);
			r += draw_string(s);
			break;
		}
		case 0:
		default:
			fatal("invalid format specifier '%c'", f);
		}

	}
	glPopMatrix();
	return r;
}

static void fill_textbox(textbox_t *t, const char *fmt, ...)
{
	double r;
	va_list ap;
	assert(t);
	assert(fmt);

	scale_t scale = font_attributes();
	double char_width = scale.x / X_MAX;
	double char_height = scale.y / Y_MAX;
	assert(t && fmt);
	va_start(ap, fmt);
	r = vdraw_text(t->color_text, t->x, t->y - t->height, fmt, ap);
	r *= char_width * 1.11;
	r += 1;
	va_end(ap);
	t->width = MAX(t->width, r);
	t->height += (char_height); /*correct?*/
}

/*static void draw_textbox(textbox_t *t)
{
	assert(t);
	scale_t scale = font_attributes();
	double char_height = scale.y / Y_MAX;
	if(!(t->draw_border))
		return;
	draw_rectangle_line(t->x - LINE_WIDTH, t->y - t->height + char_height - 1, t->width, t->height + 1, LINE_WIDTH, t->color_box);
}*/

static fifo_t *fifo_new(size_t size)
{
	assert(size >= 2); /* It does not make sense to have a FIFO less than this size */
	fifo_data_t *buffer = allocate_or_die(size * sizeof(buffer[0]));
	fifo_t *fifo = allocate_or_die(sizeof(fifo_t));

	fifo->buffer = buffer;
	fifo->head   = 0;
	fifo->tail   = 0;
	fifo->size   = size;

	return fifo;
}

static void fifo_free(fifo_t *fifo)
{
	if(!fifo)
		return;
	free(fifo->buffer);
	free(fifo);
}

static bool fifo_is_full(fifo_t * fifo)
{
	assert(fifo);
	return (fifo->head == (fifo->size - 1) && fifo->tail == 0)
	    || (fifo->head == (fifo->tail - 1));
}

static bool fifo_is_empty(fifo_t * fifo)
{
	assert(fifo);
	return fifo->head == fifo->tail;
}

static size_t fifo_count(fifo_t * fifo)
{
	assert(fifo);
	if (fifo_is_empty(fifo))
		return 0;
	else if (fifo_is_full(fifo))
		return fifo->size;
	else if (fifo->head < fifo->tail)
		return fifo->head + (fifo->size - fifo->tail);
	else
		return fifo->head - fifo->tail;
}

static size_t fifo_push(fifo_t * fifo, fifo_data_t data)
{
	assert(fifo);

	if (fifo_is_full(fifo))
		return 0;

	fifo->buffer[fifo->head] = data;

	fifo->head++;
	if (fifo->head == fifo->size)
		fifo->head = 0;

	return 1;
}

static size_t fifo_pop(fifo_t * fifo, fifo_data_t * data)
{
	assert(fifo);
	assert(data);

	if (fifo_is_empty(fifo))
		return 0;

	*data = fifo->buffer[fifo->tail];

	fifo->tail++;
	if (fifo->tail == fifo->size)
		fifo->tail = 0;

	return 1;
}


/* ====================================== Utility Functions ==================================== */

/* ====================================== Simulator Objects ==================================== */


#define TERMINAL_WIDTH       (80)
#define TERMINAL_HEIGHT      (10)
#define TERMINAL_SIZE        (TERMINAL_WIDTH*TERMINAL_HEIGHT)

typedef struct {
	unsigned width;
	unsigned height;
	GLuint name;
	uint8_t *image;
} vt100_background_texture_t;

typedef struct {
	uint64_t blink_count;
	double x;
	double y;
	bool blink_on;
	color_t color;
	vt100_t vt100;
	vt100_background_texture_t *texture;
} terminal_t;

static void texture_background(terminal_t *t)
{
	assert(t);
	vt100_background_texture_t *v = t->texture;
	vt100_t *vt = &t->vt100;
	unsigned i, j;
	uint8_t *img = v->image;
	const unsigned h = v->height;
	const unsigned w = v->width;

	for (i = 0; i < h; i++) {
		uint8_t *row = &img[i*4];
		unsigned ii = ((h - i - 1)*vt->height) / h;
		for (j = 0; j < w; j++) {
			uint8_t *column = &row[j*h*4];
			const unsigned jj = (vt->width*j) / w;
			const unsigned idx = jj+(ii*vt->width);
			column[0] = 255 * (vt->attributes[idx].background_color & 1);
			column[1] = 255 * (vt->attributes[idx].background_color & 2);
			column[2] = 255 * (vt->attributes[idx].background_color & 4);
			column[3] = 255;
		}
	}
}

/* See <http://www.glprogramming.com/red/chapter09.html> */
static void draw_texture(terminal_t *t, bool update)
{
	vt100_background_texture_t *v = t->texture;
	if(!v)
		return;

	scale_t scale = font_attributes();
	double char_width  = scale.x / X_MAX;
       	double char_height = scale.y / Y_MAX;
	double x = t->x;
	double y = t->y - (char_height * (t->vt100.height-1.0));
	double width  = char_width  * t->vt100.width * 1.10;
	double height = char_height * t->vt100.height;

	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	if(update) {
		glClearColor (0.0, 0.0, 0.0, 0.0);
		glShadeModel(GL_FLAT);
		glEnable(GL_DEPTH_TEST);

		texture_background(t);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glGenTextures(1, &v->name);
		glBindTexture(GL_TEXTURE_2D, v->name);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, v->width, v->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, v->image);
	}

	glBindTexture(GL_TEXTURE_2D, v->name);
	glMatrixMode(GL_MODELVIEW);
	glBegin(GL_QUADS);
		glTexCoord2f(1.0, 0.0); glVertex3f(x,       y+height, 0.0);
		glTexCoord2f(1.0, 1.0); glVertex3f(x+width, y+height, 0.0);
		glTexCoord2f(0.0, 1.0); glVertex3f(x+width, y,        0.0);
		glTexCoord2f(0.0, 0.0); glVertex3f(x,       y,        0.0);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void draw_terminal(const world_t *world, terminal_t *t, char *name)
{
	assert(world);
	assert(t);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	static const double scale_x = 0.011;
	static const double scale_y = 0.011;
	vt100_t *v = &t->vt100;
	double now = world->tick - t->blink_count;
	scale_t scale = font_attributes();
	double char_width  = scale.x / X_MAX;
       	double char_height = scale.y / Y_MAX;
	size_t cursor_x = v->cursor % v->width;
	size_t cursor_y = v->cursor / v->width;

	if(now > seconds_to_ticks(world, 1.0)) {
		t->blink_on = !(t->blink_on);
		t->blink_count = world->tick;
	}

	/**@note the cursor is deliberately in a different position compared to draw_vga(), due to how the VGA cursor behaves in hardware */
	if((!(v->blinks) || t->blink_on) && v->cursor_on) /* fudge factor of 1.10? */
		draw_rectangle_filled(t->x + (char_width * 1.10 * (cursor_x)) , t->y - (char_height * cursor_y), char_width, char_height, WHITE);


	for(size_t i = 0; i < t->vt100.height; i++)
		draw_vt100_block(t->x, t->y - ((double)i * char_height), scale_x, scale_y, 0, v->m + (i*v->width), v->width, v->attributes + (i*v->width), t->blink_on);
	draw_string_scaled(t->x, t->y - (v->height * char_height), scale_x, scale_y, 0, name, t->color);

	/* fudge factor = 1/((1/scale_x)/X_MAX) ??? */

	glPopMatrix();

	draw_rectangle_line(t->x, t->y - (char_height * (v->height-1.0)), char_width * v->width * 1.10, char_height * v->height, LINE_WIDTH, t->color);
}

/* ====================================== Simulator Objects ==================================== */

/* ====================================== Simulator Instances ================================== */


#define VGA_TEXTURE_WIDTH  (256)
#define VGA_TEXTURE_HEIGHT (256)
static uint8_t vga_background_image[VGA_TEXTURE_WIDTH*VGA_TEXTURE_HEIGHT*4];

static vt100_background_texture_t vga_background_texture = {
	.width  = VGA_TEXTURE_WIDTH,
	.height = VGA_TEXTURE_HEIGHT,
	.name   = 0,
	.image  = (uint8_t*)vga_background_image
};

static terminal_t vga_terminal = {
	.blink_count = 0,
	.x           = X_MIN + 2.0,
	.y           = Y_MAX - 8.0,
	.color       = GREEN,  /* WHITE */
	.blink_on    = false,

	.vt100  = {
		.width        = VGA_WIDTH,
		.height       = VGA_HEIGHT,
		.size         = VGA_WIDTH * VGA_HEIGHT,
		.cursor       = 0,
		.cursor_saved = 0,
		.state        = TERMINAL_NORMAL_MODE,
		.cursor_on    = true,
		.blinks       = false,
		.n1           = 1,
		.n2           = 1,
		.m            = { 0 },
		.attribute    = { 0 },
		.attributes   = { { 0 } },
	},
	.texture = &vga_background_texture
};


static fifo_t *uart_rx_fifo = NULL;
static fifo_t *uart_tx_fifo = NULL;

/* ====================================== Simulator Instances ================================== */

/* ====================================== Main Loop ============================================ */

/*static double fps(void)
{
	static unsigned frame = 0, timebase = 0;
	static double fps = 0;
	int time = glutGet(GLUT_ELAPSED_TIME);
	frame++;
	if(time - timebase > 1000) {
		fps = frame*1000.0/(time-timebase);
		timebase = time;
		frame = 0;
	}
	return fps;
}*/

static void keyboard_handler(unsigned char key, int x, int y)
{
	UNUSED(x);
	UNUSED(y);
	assert(uart_tx_fifo);
	if(key == ESCAPE) {
		world.halt_simulation = true;
	} else {
		vt100_update(&vga_terminal.vt100, key);
		//fifo_push(uart_rx_fifo, key);
	}
}

static void keyboard_special_handler(int key, int x, int y)
{
	UNUSED(x);
	UNUSED(y);
	vt100_update(&vga_terminal.vt100, key);
	switch(key) {
	case GLUT_KEY_UP:    
	case GLUT_KEY_LEFT:  
	case GLUT_KEY_RIGHT: 
	case GLUT_KEY_DOWN:  
	case GLUT_KEY_F1:   
	case GLUT_KEY_F2:  
	case GLUT_KEY_F3: 
	case GLUT_KEY_F4:  
	case GLUT_KEY_F5:  
	case GLUT_KEY_F6:  
	case GLUT_KEY_F7:  
	case GLUT_KEY_F8:  
	case GLUT_KEY_F9:  
	case GLUT_KEY_F10: 
	case GLUT_KEY_F11: 
	case GLUT_KEY_F12: 
	default:
		break;
	}
}

static void keyboard_special_up_handler(int key, int x, int y)
{
	UNUSED(x);
	UNUSED(y);
	switch(key) {
	case GLUT_KEY_UP:   
	case GLUT_KEY_LEFT: 
	case GLUT_KEY_RIGHT:
	case GLUT_KEY_DOWN: 
	default:
		break;
	}
}

typedef struct {
	double x;
	double y;
} coordinate_t;

static void resize_window(int w, int h)
{
	double window_x_min, window_x_max, window_y_min, window_y_max;
	double scale, center;
	world.window_width  = w;
	world.window_height = h;

	glViewport(0, 0, w, h);

	w = (w == 0) ? 1 : w;
	h = (h == 0) ? 1 : h;
	if ((X_MAX - X_MIN) / w < (Y_MAX - Y_MIN) / h) {
		scale = ((Y_MAX - Y_MIN) / h) / ((X_MAX - X_MIN) / w);
		center = (X_MAX + X_MIN) / 2;
		window_x_min = center - (center - X_MIN) * scale;
		window_x_max = center + (X_MAX - center) * scale;
		world.window_scale_x = scale;
		window_y_min = Y_MIN;
		window_y_max = Y_MAX;
	} else {
		scale = ((X_MAX - X_MIN) / w) / ((Y_MAX - Y_MIN) / h);
		center = (Y_MAX + Y_MIN) / 2;
		window_y_min = center - (center - Y_MIN) * scale;
		window_y_max = center + (Y_MAX - center) * scale;
		world.window_scale_y = scale;
		window_x_min = X_MIN;
		window_x_max = X_MAX;
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(window_x_min, window_x_max, window_y_min, window_y_max, -1, 1);
}

static void mouse_handler(int button, int state, int x, int y)
{
	UNUSED(button);
	UNUSED(state);
	UNUSED(x);
	UNUSED(y);
}

static void timer_callback(int value)
{
	world.tick++;
	glutTimerFunc(world.arena_tick_ms, timer_callback, value);
}

static void draw_scene(void)
{
	static uint64_t next = 0;
	static uint64_t count = 0;
	//double f = fps();
	if(world.halt_simulation)
		exit(EXIT_SUCCESS);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(next != world.tick) {
		next = world.tick;
		count++;
		/*for(;!fifo_is_empty(uart_tx_fifo);) {
			uint8_t c = 0;
			fifo_pop(uart_tx_fifo, &c);
			vt100_update(&uart_terminal.vt100, c);
		}*/
	}
	draw_terminal(&world, &vga_terminal, "VT100");
	draw_texture(&vga_terminal,  !(count % 2));

	glFlush();
	glutSwapBuffers();
	glutPostRedisplay();
}

static void initialize_rendering(char *arg_0)
{
	char *glut_argv[] = { arg_0, NULL };
	int glut_argc = 0;
	memset(vga_terminal.vt100.m, ' ', vga_terminal.vt100.size);
	glutInit(&glut_argc, glut_argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
	glutInitWindowPosition(world.window_x_starting_position, world.window_y_starting_position);
	glutInitWindowSize(world.window_width, world.window_height);
	glutCreateWindow("VT100 Terminal Emulator");
	glShadeModel(GL_FLAT);
	glEnable(GL_DEPTH_TEST);
	glutKeyboardFunc(keyboard_handler);
	glutSpecialFunc(keyboard_special_handler);
	glutSpecialUpFunc(keyboard_special_up_handler);
	glutMouseFunc(mouse_handler);
	glutReshapeFunc(resize_window);
	glutDisplayFunc(draw_scene);
	glutTimerFunc(world.arena_tick_ms, timer_callback, 0);
}

static void vt100_initialize(vt100_t *v)
{
	assert(v);
	memset(&v->attribute, 0, sizeof(v->attribute));
	v->attribute.foreground_color = WHITE;
	v->attribute.background_color = BLACK;
	for(size_t i = 0; i < v->size; i++)
		v->attributes[i] = v->attribute;
}

static void finalize(void)
{
	fifo_free(uart_tx_fifo);
	fifo_free(uart_rx_fifo);
}

int main(int argc, char **argv)
{
	assert(Y_MAX > 0. && Y_MIN < Y_MAX && Y_MIN >= 0.);
	assert(X_MAX > 0. && X_MIN < X_MAX && X_MIN >= 0.);

	log_level = LOG_NOTE;

	uart_rx_fifo = fifo_new(UART_FIFO_DEPTH);
	uart_tx_fifo = fifo_new(UART_FIFO_DEPTH * 100); /** @note x100 to speed things up */

	vt100_initialize(&vga_terminal.vt100);

	atexit(finalize);
	initialize_rendering(argv[0]);
	glutMainLoop();

	return 0;
}


