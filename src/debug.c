#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include "../include/debug.h"

void print_debug(bool is_debug, const char* message, ...) {
	if(!is_debug) return;
	va_list args;
	va_start(args, message);
	printf("[DEBUG]");
	vprintf(message, args);
	printf("\n");
	va_end(args);
}

void print_system(const char* message, ...) {
	va_list args;
	va_start(args, message);
	printf("[SYSTEM]");
	vprintf(message, args);
	printf("\n");
	va_end(args);
}

void print_error(const char* message, ...) {
	va_list args;
	va_start(args, message);
	printf("[ERROR]");
	vprintf(message, args);
	printf("\n");
	va_end(args);
}

void print_warning(const char* message, ...) {
    va_list args;
	va_start(args, message);
	printf("[WARNING]");
	vprintf(message, args);
	printf("\n");
	va_end(args);
}
