#include <stdarg.h>
#include <stdio.h>

#include "roccat-vulcan.h"

void rv_print_buffer(unsigned char *buffer, int len) {
	int i;
	// This is always classed as verbose
	if (rv_verbose) {
		for (i = 0; i < len; i++) printf("%02hhx ", buffer[i]);
		printf("\n");
	}
}

void rv_printf(int verbose, const char *format, ...) {
	va_list args;
	if (!verbose || (verbose && rv_verbose)) {
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
}
