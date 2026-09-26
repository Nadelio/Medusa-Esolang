#include <stdio.h>
#include <stdlib.h>
#include "../include/structs.h"
#include "../include/debug.h"

void put(Map* map, i32 key, i32 value, i32 position) {
	map->pairs = realloc(map->pairs, sizeof(MapElement) * (map->size + 1));
	map->pairs[map->size] = (MapElement){ .key = key, .value = value, .position = position };
	map->size = map->size + 1;
}

MapElement* get(Map* map, i32 key) {
	for(size_t i = 0; i < map->size; i++) {
		if(map->pairs[i].key == key) {
			return &map->pairs[i];
		}
	}
	print_warning(" `get()` failed to find value referenced by %d.\n  Adding placeholder: { %d : (0, 0) }\n", key, key);
	put(map, key, 0, 0);
	return 0;
}

void set(Map* map, i32 indent, i32 value, i32 address) {
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

Stacki32* build(void) {
	Stacki32* stack = (Stacki32*)malloc(sizeof(Stacki32));
	stack->top = -1;
	stack->size = 64;
	return stack;
}

bool is_empty(Stacki32* stack) {
	return stack->top == -1;
}

bool is_full(Stacki32* stack) {
	return (size_t)(stack->top + 1) >= stack->size;
}

i32 pop(Stacki32* stack) {
	if(is_empty(stack)) {
		print_error(" Stack underflow.\n");
		return -1;
	}
	stack->top -= 1;
	return stack->stack[stack->top + 1];
}

i32 peek(Stacki32* stack) {
	if(is_empty(stack)) {
		print_error(" Stack is empty");
		return 0;
	}
	return stack->stack[stack->top];
}

void push(Stacki32* stack, i32 value) {
	if(is_full(stack)) {
		print_error(" Stack overflow.\n");
		return;
	}
	stack->top += 1;
	stack->stack[stack->top] = value;
}

bool is_out_of_bounds(i32* ptr, i32* lower_bound, i32* upper_bound) {
	if(ptr < lower_bound || ptr >= upper_bound) {
		return true;
	}
	return false;
}
