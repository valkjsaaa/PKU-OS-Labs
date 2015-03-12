// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <kern/console.h>

char ansi_fmt_buffer[100];
uint16_t ansi_fmt_ptr = 0;

uint16_t foreground_color = 7;
uint16_t background_color = 0;

/** map ansi color index to cga palette index */
static const uint8_t ansi_to_cga[16] = {
	0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15
};

ansi_state state = NORMAL;

bool 
is_char(int ch)
{
	return (ch>=0x41&&ch<=0x5A)||(ch>=0x61&&ch<=0x7A);
}

bool 
is_digit(int ch)
{
	return (ch>=0x30&&ch<=0x39);
}

int 
parse_int(char* start, char* end){
	int result = 0;
	for (start; start < end; ++start)
	{
		if(is_digit(*start)) {
			result *= 10;
			result += *start-0x30;
		}
	}
	return result;
}

void
apply_ansi_color(int color_idx)
{
	if(color_idx>=30 && color_idx<40) {
		foreground_color = ansi_to_cga[color_idx-30];
	}
	else if(color_idx>=40 && color_idx<50){
		background_color = ansi_to_cga[color_idx-40];
	}
	return;
}

void
apply_ansi_control(char * control_string, char command)
{
	char* back = control_string;
	if(command == 'm') {
		for (back; *back; ++back)
		{
			if(*back == ';'){
				apply_ansi_color(parse_int(control_string, back));
				control_string = back+1;
			}
		}
		if(control_string<back) {
			apply_ansi_color(parse_int(control_string, back));
		}
	}
	return;
}

static void
putch(int ch, int *cnt)
{
	*cnt++;
	cons_ctrl_putc(ch);
	switch(state){
		case NORMAL:
		{
			if( ch == '\x1B' ) {
				state = START;
			}
			else {
				cons_disp_putc(ch);
			}
			break;
		}
		case START:{
			if(ch == '['){
				state = ESCAPING;
			}else{
				cons_disp_putc('\x1B');
				cons_disp_putc(ch);
			}
		}
		case ESCAPING:
		{
			if(is_char(ch)) {
				ansi_fmt_buffer[ansi_fmt_ptr] = '\0';
				apply_ansi_control(ansi_fmt_buffer, ch);
				ansi_fmt_ptr = 0;
				state = NORMAL;
			}else if(ansi_fmt_ptr == 99){
				state = OVERFLOW;
			}else{
				ansi_fmt_buffer[ansi_fmt_ptr] = ch;
				ansi_fmt_ptr++;
				state = ESCAPING;
			}
		}
		case OVERFLOW:
		{
			if(is_char(ch)) {
				state = NORMAL;
			}
		}
	}
}

int
vcprintf(const char *fmt, va_list ap)
{
	int cnt = 0;

	vprintfmt((void*)putch, &cnt, fmt, ap);
	return cnt;
}

int
cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}

