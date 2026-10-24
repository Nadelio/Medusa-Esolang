#include <stdio.h>
#include <stdlib.h>
#include "../include/structs.h"
#include "../include/debug.h"

void put(Map* map, cell key, cell value, ptrdiff_t position) {
	map->pairs = realloc(map->pairs, sizeof(MapElement) * (map->size + 1));
	map->pairs[map->size] = (MapElement){ .key = key, .value = value, .position = position };
	map->size = map->size + 1;
}

MapElement* get(Map* map, cell key) {
	for(size_t i = 0; i < map->size; i++) {
		if(map->pairs[i].key == key) {
			return &map->pairs[i];
		}
	}
	print_warning(" `get()` failed to find value referenced by %d.\n  Adding placeholder: { %d : (0, 0) }\n", key, key);
	put(map, key, 0, 0);
	return 0;
}

void set(Map* map, cell indent, cell value, ptrdiff_t address) {
	for(size_t i = 0; i < map->size; i++) {
		if(map->pairs[i].key == indent) {
			map->pairs[i].value = value;
			map->pairs[i].position = address;
			return;
		}
	}
	put(map, indent, value, address);
}

StackVariable* build_varstack(void) {
	StackVariable* stack = (StackVariable*)malloc(sizeof(StackVariable));
	stack->top = -1;
	stack->size = 64;
	return stack;
}

void deconstruct(StackVariable* varstack) {
	for(size_t i = 0; i < varstack->size; i++) {
		free(varstack->stack[i]);
	}
	free(varstack);
}

bool is_varstack_empty(StackVariable* stack) {
	return stack->top == -1;
}

bool is_varstack_full(StackVariable* stack) {
	return (size_t)(stack->top + 1) >= stack->size;
}

Variable* pop_varstack(StackVariable* stack) {
	if(is_varstack_empty(stack)) {
		print_error(" Stack underflow.\n");
		return NULL;
	}
	stack->top -= 1;
	return stack->stack[stack->top + 1];
}

Variable* peek_varstack(StackVariable* stack) {
	if(is_varstack_empty(stack)) {
		print_error(" Stack is empty");
		return NULL;
	}
	return stack->stack[stack->top];
}

void push_varstack(StackVariable* stack, Variable* value) {
	if(is_varstack_full(stack)) {
		print_error(" Stack overflow.\n");
		return;
	}
	stack->top += 1;
	stack->stack[stack->top] = value;
}

Stacki8* build(void) {
	Stacki8* stack = (Stacki8*)malloc(sizeof(Stacki8));
	stack->top = -1;
	stack->size = 64;
	return stack;
}

bool is_empty(Stacki8* stack) {
	return stack->top == -1;
}

bool is_full(Stacki8* stack) {
	return (size_t)(stack->top + 1) >= stack->size;
}

i8 pop(Stacki8* stack) {
	if(is_empty(stack)) {
		print_error(" Stack underflow.\n");
		return -1;
	}
	stack->top -= 1;
	return stack->stack[stack->top + 1];
}

i8 peek(Stacki8* stack) {
	if(is_empty(stack)) {
		print_error(" Stack is empty");
		return 0;
	}
	return stack->stack[stack->top];
}

void push(Stacki8* stack, i8 value) {
	if(is_full(stack)) {
		print_error(" Stack overflow.\n");
		return;
	}
	stack->top += 1;
	stack->stack[stack->top] = value;
}

bool is_out_of_bounds(i8* ptr, i8* lower_bound, i8* upper_bound) {
	if(ptr < lower_bound || ptr >= upper_bound) {
		return true;
	}
	return false;
}
