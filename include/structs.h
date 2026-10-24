#ifndef MEDUSA_STRUCTS_H
#define MEDUSA_STRUCTS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef char* string;
typedef int8_t i8;
typedef uint8_t u8;
// one memory cell, every address refers to a cell
typedef int32_t cell;

typedef struct {
	cell key;
	cell value;
	ptrdiff_t position;
} MapElement;

typedef struct {
	size_t size;
	MapElement* pairs;
} Map;

void put(Map* map, cell key, cell value, ptrdiff_t position);
MapElement* get(Map* map, cell key);
void set(Map* map, cell indent, cell value, ptrdiff_t address);

typedef struct {
	i8 value;
	i8 indent;
	ptrdiff_t address;
	bool is_literal;
} Variable;

typedef struct {
	Variable* stack[64];
	int top;
	size_t size;
} StackVariable;

StackVariable* build_varstack(void);
void deconstruct(StackVariable* varstack);
bool is_varstack_empty(StackVariable* stack);
bool is_varstack_full(StackVariable* stack);
Variable* pop_varstack(StackVariable* stack);
Variable* peek_varstack(StackVariable* stack);
void push_varstack(StackVariable* stack, Variable* value);

typedef struct {
	i8 stack[64];
	int top;
	size_t size;
} Stacki8;

Stacki8* build(void);
bool is_empty(Stacki8* stack);
bool is_full(Stacki8* stack);
i8 pop(Stacki8* stack);
i8 peek(Stacki8* stack);
void push(Stacki8* stack, i8 value);

bool is_out_of_bounds(i8* ptr, i8* lower_bound, i8* upper_bound);

#endif
