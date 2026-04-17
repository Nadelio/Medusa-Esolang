#ifndef MEDUSA_DEBUG_H
#define MEDUSA_DEBUG_H

#include <stdbool.h>

void print_debug(bool is_debug, const char* message, ...);
void print_system(const char* message, ...);
void print_error(const char* message, ...);
void print_warning(const char* message, ...);

#endif
