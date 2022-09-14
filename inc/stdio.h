#ifndef JOS_INC_STDIO_H
#define JOS_INC_STDIO_H

#include <inc/stdarg.h>

#ifndef NULL
#define NULL	((void *) 0)
#endif /* !NULL */

// colors
enum COLOR{
	Black = 0x0,	
	Blue,
	Green,		
	Cyan,		
	Red,		
	Magenta,		
	Brown,		
	LightGray = 0x7,
			
	DarkGray,
	LightBlue,
	LightGreen,
	LightCyan,
	LightRed,
	Pink,
	Yellow,
	White = 0xf,
};

// lib/console.c
void	cputchar(int c);
int	getchar(void);
int	iscons(int fd);
int cga_set_color(int foreground_color_, int background_color_);
void serial_set_color(int fgc, int bgc);
void serial_default_color();

// lib/printfmt.c
void	printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void	vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list);
int	snprintf(char *str, int size, const char *fmt, ...);
int	vsnprintf(char *str, int size, const char *fmt, va_list);

// lib/printf.c
int	cprintf(const char *fmt, ...);
int	vcprintf(const char *fmt, va_list);
int set_color(enum COLOR foreground_color, enum COLOR background_color);
void back_to_default_color();

// lib/fprintf.c
int	printf(const char *fmt, ...);
int	fprintf(int fd, const char *fmt, ...);
int	vfprintf(int fd, const char *fmt, va_list);

// lib/readline.c
char*	readline(const char *prompt);

#endif /* !JOS_INC_STDIO_H */
