#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "structs.h"
#include "parser.h"

char* interpreter_err_msg;
size_t data_mem_size = 128;

static MapElement* find_or_create_var(Map* map, i32 key) {
	for(size_t i = 0; i < map->size; i++) {
		if(map->pairs[i].key == key) {
			return &map->pairs[i];
		}
	}
	put(map, key, 0, key < 0 ? 0 : key);
	return &map->pairs[map->size - 1];
}

static void push_runtime_value(StackVariable* stack, Variable value) {
	Variable* boxed = malloc(sizeof(Variable));
	if(boxed == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate runtime value\n");
		exit(1);
	}
	*boxed = value;
	push_varstack(stack, boxed);
}

static Variable pop_runtime_value(StackVariable* stack) {
	Variable fallback = { .value = 0, .indent = -1, .address = -1, .is_literal = true };
	Variable* boxed = pop_varstack(stack);
	if(boxed == NULL) {
		return fallback;
	}
	Variable value = *boxed;
	free(boxed);
	return value;
}

static void clear_runtime_stack(StackVariable* stack) {
	while(!is_varstack_empty(stack)) {
		Variable* boxed = pop_varstack(stack);
		free(boxed);
	}
}

static i32 read_data_cell(i32* data_base, i32 address) {
	if(address < 0 || (size_t)address >= data_mem_size) {
		return 0;
	}
	return data_base[address];
}

static void write_data_cell(i32* data_base, i32 address, i32 value) {
	if(address < 0 || (size_t)address >= data_mem_size) {
		return;
	}
	data_base[address] = value;
}

i32 run(bool verbose, i32* program, size_t program_size) {
	print_debug(verbose, " Beginning program...\n");
	if(data_mem_size == 0) data_mem_size = 128; // if user overwrites data_mem_size with `#0;` forcefully replace it wiht 128

	// reserved memory space for program and data
	print_debug(verbose, " Allocating memory space, size: %zu\n", sizeof(i32) * (program_size + data_mem_size));
	i32* mem_space = (i32*)malloc(sizeof(i32) * (program_size + data_mem_size));
	if(mem_space == NULL) {
		interpreter_err_msg = "Failed to allocate interpreter memory";
		return FAIL;
	}
	
	size_t total_memory_size = program_size + data_mem_size;

	print_debug(verbose, " Initializing variable table, program counter, and data pointer...\n");
	// program counter
	i32* prog_ptr = mem_space;
	// points to somewhere in data
	i32* data_ptr = mem_space + program_size;
	// variable table for interpreter
	Map* vtable = (Map*)malloc(sizeof(Map));

	// evaluation stacks
	if(vtable == NULL) {
		free(mem_space);
		interpreter_err_msg = "Failed to allocate variable table";
		return FAIL;
	}
	vtable->size = 0;
	vtable->pairs = NULL;

	Stacki32* result_stack = build();
	StackVariable* value_stack = build_varstack();
	i32* data_base = data_ptr;
	i32* scope_starts[64] = {0};
	int scope_remaining[64] = {0};
	bool scope_is_infinite[64] = {false};
	int scope_top = -1;

	// initialize memory
	memcpy(prog_ptr, program, sizeof(i32) * program_size);
	memset(data_ptr, 0, sizeof(i32) * data_mem_size);

	// begin running program
	while(*prog_ptr != EOD) {
		switch(*prog_ptr) {
			case EOD:
				clear_runtime_stack(value_stack);
				free(value_stack);
				return SUCCESS;
			case _NULL:
				break;
			case VARIABLE: {
				i32 ident = *(prog_ptr + 1);
				MapElement* slot = find_or_create_var(vtable, ident);
				if(slot->position >= 0 && (size_t)slot->position < data_mem_size) {
					slot->value = read_data_cell(data_base, slot->position);
				}
				push_runtime_value(value_stack, (Variable){ .value = slot->value, .indent = ident, .address = slot->position, .is_literal = false });
				prog_ptr += 1;
				break;
			}
			case DATA_PTR_REF:
				push_runtime_value(value_stack, (Variable){ .value = *data_ptr, .indent = DATA_PTR_REF, .address = (i32)(data_ptr - data_base), .is_literal = false });
				break;
			case PROG_PTR_REF:
				push_runtime_value(value_stack, (Variable){ .value = *prog_ptr, .indent = PROG_PTR_REF, .address = (i32)(prog_ptr - mem_space), .is_literal = false });
				break;
			case INT_LITERAL:
				push_runtime_value(value_stack, (Variable){ .value = *(prog_ptr + 1), .indent = -1, .address = -1, .is_literal = true });
				prog_ptr += 1;
				break;
			case ASSIGN: {
				Variable right = pop_runtime_value(value_stack);
				Variable left = pop_runtime_value(value_stack);
				if(left.indent == DATA_PTR_REF) {
					*data_ptr = right.value;
				} else if(left.indent == PROG_PTR_REF) {
					*prog_ptr = right.value;
				} else if(!left.is_literal) {
					MapElement* slot = find_or_create_var(vtable, left.indent);
					slot->value = right.value;
					slot->position = left.address;
					write_data_cell(data_base, slot->position, right.value);
				}
				break;
			}
			case ADDRESS_ASSIGN: {
				Variable right = pop_runtime_value(value_stack);
				Variable left = pop_runtime_value(value_stack);
				if(left.indent == DATA_PTR_REF) {
					if(right.value >= 0 && (size_t)right.value < data_mem_size) {
						data_ptr = data_base + right.value;
					}
				} else if(left.indent == PROG_PTR_REF) {
					if(right.value >= 0 && (size_t)right.value < program_size) {
						prog_ptr = mem_space + right.value - 1;
					}
				} else if(!left.is_literal) {
					MapElement* slot = find_or_create_var(vtable, left.indent);
					slot->position = right.value;
				}
				break;
			}
			case DECLARE_REF: {
				Variable target = pop_runtime_value(value_stack);
				push_runtime_value(value_stack, (Variable){ .value = target.address, .indent = -1, .address = -1, .is_literal = true });
				break;
			}
			case DEREF: {
				Variable target = pop_runtime_value(value_stack);
				i32 value = read_data_cell(data_base, target.value);
				push_runtime_value(value_stack, (Variable){ .value = value, .indent = -1, .address = -1, .is_literal = true });
				break;
			}
			case MULTI:
			case DIV:
			case PLUS:
			case MINUS:
			case EQUIV:
			case NOT_EQUIV:
			case LESS:
			case GREATER:
			case LESS_EQUAL:
			case GREATER_EQUAL: {
				Variable right = pop_runtime_value(value_stack);
				Variable left = pop_runtime_value(value_stack);
				i32 value = 0;
				switch(*prog_ptr) {
					case MULTI: value = left.value * right.value; break;
					case DIV: value = right.value == 0 ? 0 : left.value / right.value; break;
					case PLUS: value = left.value + right.value; break;
					case MINUS: value = left.value - right.value; break;
					case EQUIV: value = left.value == right.value; break;
					case NOT_EQUIV: value = left.value != right.value; break;
					case LESS: value = left.value < right.value; break;
					case GREATER: value = left.value > right.value; break;
					case LESS_EQUAL: value = left.value <= right.value; break;
					case GREATER_EQUAL: value = left.value >= right.value; break;
					default: break;
				}
				push_runtime_value(value_stack, (Variable){ .value = value, .indent = -1, .address = -1, .is_literal = true });
				break;
			}
			case INC:
			case DEC: {
				Variable target = pop_runtime_value(value_stack);
				i32 delta = *prog_ptr == INC ? 1 : -1;
				if(target.indent == DATA_PTR_REF) {
					*data_ptr += delta;
				} else if(target.indent == PROG_PTR_REF) {
					*prog_ptr += delta;
				} else if(!target.is_literal) {
					MapElement* slot = find_or_create_var(vtable, target.indent);
					slot->value += delta;
					write_data_cell(data_base, slot->position, slot->value);
				}
				break;
			}
			case RIGHT_ARROW:
			case LEFT_ARROW: {
				Variable target = pop_runtime_value(value_stack);
				i32 delta = *prog_ptr == RIGHT_ARROW ? 1 : -1;
				if(target.indent == DATA_PTR_REF) {
					i32 new_address = (i32)(data_ptr - data_base) + delta;
					if(new_address >= 0 && (size_t)new_address < data_mem_size) {
						data_ptr = data_base + new_address;
					}
				} else if(target.indent == PROG_PTR_REF) {
					prog_ptr += delta;
				} else if(!target.is_literal) {
					MapElement* slot = find_or_create_var(vtable, target.indent);
					slot->position += delta;
				}
				break;
			}
			case BEGIN_SCOPE: {
				bool should_enter = true;
				if(!is_varstack_empty(value_stack)) {
					Variable condition = pop_runtime_value(value_stack);
					should_enter = condition.value != 0;
				}
				if(!should_enter) {
					int depth = 1;
					while(depth > 0 && !is_out_of_bounds(prog_ptr + 1, mem_space, mem_space + total_memory_size)) {
						prog_ptr += 1;
						if(*prog_ptr == BEGIN_SCOPE) depth++;
						if(*prog_ptr == END_SCOPE) depth--;
					}
					break;
				}
				if(scope_top + 1 < 64) {
					scope_top++;
					scope_starts[scope_top] = prog_ptr + 1;
					scope_remaining[scope_top] = 0;
					scope_is_infinite[scope_top] = false;
				}
				break;
			}
			case DEFINE_SCOPE_ITERATIONS: {
				Variable count = pop_runtime_value(value_stack);
				if(scope_top >= 0) {
					if(count.value <= 0) {
						scope_top--;
					} else {
						if(scope_remaining[scope_top] == 0) {
							scope_remaining[scope_top] = count.value - 1;
						}
						if(scope_remaining[scope_top] > 0) {
							scope_remaining[scope_top]--;
							prog_ptr = scope_starts[scope_top] - 1;
						}
					}
				}
				break;
			}
			case INFINITE_LOOP:
				if(scope_top >= 0) {
					scope_is_infinite[scope_top] = true;
					prog_ptr = scope_starts[scope_top] - 1;
				}
				break;
			case BREAK_FROM_SCOPE: {
				Variable condition = pop_runtime_value(value_stack);
				if(scope_top >= 0 && condition.value) {
					int depth = 1;
					while(depth > 0 && !is_out_of_bounds(prog_ptr + 1, mem_space, mem_space + total_memory_size)) {
						prog_ptr += 1;
						if(*prog_ptr == BEGIN_SCOPE) depth++;
						if(*prog_ptr == END_SCOPE) depth--;
					}
					scope_top--;
				}
				break;
			}
			case END_SCOPE:
				if(scope_top >= 0) {
					scope_top--;
				}
				break;
			case DEFINE_DATA_MEM_SIZE:
				break;
			case PRINT: {
				Variable target = pop_runtime_value(value_stack);
				i32 value = target.value;
				if(!target.is_literal && target.indent != DATA_PTR_REF && target.indent != PROG_PTR_REF) {
					MapElement* slot = find_or_create_var(vtable, target.indent);
					if(slot->position >= 0 && (size_t)slot->position < data_mem_size) {
						slot->value = read_data_cell(data_base, slot->position);
					}
					value = slot->value;
				}
				printf("%c", value);
				break;
			}
			case PRINT_LITERAL: {
				Variable target = pop_runtime_value(value_stack);
				i32 value = target.value;
				if(!target.is_literal && target.indent != DATA_PTR_REF && target.indent != PROG_PTR_REF) {
					MapElement* slot = find_or_create_var(vtable, target.indent);
					if(slot->position >= 0 && (size_t)slot->position < data_mem_size) {
						slot->value = read_data_cell(data_base, slot->position);
					}
					value = slot->value;
				}
				printf("%d", value);
				break;
			}
			case EOS:
				clear_runtime_stack(value_stack);
				result_stack->top = -1;
				break;
			default:
				break;
		}

		prog_ptr += 1;
		if(is_out_of_bounds(prog_ptr, mem_space, mem_space + total_memory_size)) {
			interpreter_err_msg = "Program Pointer ran out of bounds";
			free(vtable->pairs);
			free(mem_space);
			free(vtable);
			free(result_stack);
			return OUT_OF_BOUNDS;
		}
	}

	// clean up runtime memory space
	clear_runtime_stack(value_stack);
	free(value_stack);
	free(vtable->pairs);
	free(mem_space);
	free(vtable);
	free(result_stack);

	return SUCCESS;
}
