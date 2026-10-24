#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/structs.h"
#include "../include/parser.h"
#include "../include/bytecode.h"

#define VALUE_STACK_SIZE 64
#define SCOPE_STACK_SIZE 64

char* interpreter_err_msg;
size_t data_mem_size = 128;

typedef struct {
	cell* mem;
	size_t total_size;
	size_t program_size;
	cell stack[VALUE_STACK_SIZE];
	int stack_top;
} Machine;

static MapElement* find_or_create_var(Map* map, cell index, const Machine* vm) {
	for(size_t i = 0; i < map->size; i++) {
		if(map->pairs[i].key == index) {
			return &map->pairs[i];
		}
	}

	// variables start immediately after the program data
	ptrdiff_t default_position = (ptrdiff_t)vm->program_size + (ptrdiff_t)index;
	if(index < 0 || (size_t)default_position >= vm->total_size) {
		default_position = (ptrdiff_t)vm->program_size;
	}

	put(map, index, 0, default_position);
	return &map->pairs[map->size - 1];
}

static void push_value(Machine* vm, cell value) {
	if(vm->stack_top + 1 >= VALUE_STACK_SIZE) {
		print_error(" Stack overflow.\n");
		return;
	}
	vm->stack[++vm->stack_top] = value;
}

static cell pop_value(Machine* vm) {
	if(vm->stack_top < 0) {
		print_error(" Stack underflow.\n");
		return 0;
	}
	return vm->stack[vm->stack_top--];
}

static bool in_bounds(const Machine* vm, ptrdiff_t address) {
	return address >= 0 && (size_t)address < vm->total_size;
}

static cell read_cell(const Machine* vm, ptrdiff_t address) {
	return in_bounds(vm, address) ? vm->mem[address] : 0;
}

static void write_cell(Machine* vm, ptrdiff_t address, cell value) {
	if(in_bounds(vm, address)) {
		vm->mem[address] = value;
	}
}

static cell wrap_add(cell left, cell right) { return (cell)((uint32_t)left + (uint32_t)right); }
static cell wrap_sub(cell left, cell right) { return (cell)((uint32_t)left - (uint32_t)right); }
static cell wrap_mul(cell left, cell right) { return (cell)((uint32_t)left * (uint32_t)right); }

static cell binary_operation(int operation, cell left, cell right) {
	switch(operation) {
		case OP_ADD: return wrap_add(left, right);
		case OP_SUB: return wrap_sub(left, right);
		case OP_MUL: return wrap_mul(left, right);
		case OP_DIV:
			if(right == 0) return 0;
			if(right == -1) return wrap_sub(0, left);
			return left / right;
		case OP_MOD:
			if(right == 0 || right == -1) return 0;
			return left % right;
		case OP_EQUAL: return left == right;
		case OP_NOT_EQUAL: return left != right;
		case OP_LESS: return left < right;
		case OP_LESS_EQUAL: return left <= right;
		case OP_GREATER: return left > right;
		case OP_GREATER_EQUAL: return left >= right;
		case OP_BIT_AND: return left & right;
		case OP_BIT_OR: return left | right;
		case OP_BIT_XOR: return left ^ right;
		default: return 0;
	}
}

// returns the address just after the `]` that closes the scope containing `from`
static size_t skip_scope(const Machine* vm, size_t from) {
	int depth = 1;
	size_t address = from;
	while(depth > 0 && address < vm->total_size) {
		cell instr = vm->mem[address];
		if(instr == OP_BEGIN_SCOPE) depth++;
		if(instr == OP_END_SCOPE) depth--;
		address += bc_instruction_length(instr);
	}
	return address;
}

// applies a typed access to a target whose address is `*address`, pointers clamp their address to memory
static void access_target(Machine* vm, int access, ptrdiff_t* address, bool clamp_address) {
	switch(access) {
		case ACC_PUSH_VALUE: push_value(vm, read_cell(vm, *address)); break;
		case ACC_PUSH_ADDRESS: push_value(vm, (cell)*address); break;
		case ACC_SET_VALUE: write_cell(vm, *address, pop_value(vm)); break;
		case ACC_SET_ADDRESS: {
			cell new_address = pop_value(vm);
			if(!clamp_address || in_bounds(vm, new_address)) *address = new_address;
			break;
		}
		case ACC_INC_VALUE: write_cell(vm, *address, wrap_add(read_cell(vm, *address), 1)); break;
		case ACC_DEC_VALUE: write_cell(vm, *address, wrap_sub(read_cell(vm, *address), 1)); break;
		case ACC_INC_ADDRESS:
			if(!clamp_address || in_bounds(vm, *address + 1)) *address += 1;
			break;
		case ACC_DEC_ADDRESS:
			if(!clamp_address || in_bounds(vm, *address - 1)) *address -= 1;
			break;
		default: break;
	}
}

int run(bool verbose, const u8* bytecode, size_t bytecode_len) {
	print_debug(verbose, " Decoding bytecode...\n");
	size_t program_size = 0;
	size_t requested_data_size = 0;
	cell* program = bc_decode(bytecode, bytecode_len, &program_size, &requested_data_size);
	if(program == NULL) {
		interpreter_err_msg = "Malformed bytecode";
		return FAIL;
	}
	// a missing or zero `#` -> 128 cells
	data_mem_size = requested_data_size > 0 ? requested_data_size : 128;

	print_debug(verbose, " Beginning program...\n");
	print_debug(verbose, " Allocating memory space, size: %zu cells\n", program_size + data_mem_size);

	Machine vm = { .program_size = program_size, .total_size = program_size + data_mem_size, .stack_top = -1 };
	vm.mem = calloc(vm.total_size, sizeof(cell));
	if(vm.mem == NULL) {
		free(program);
		interpreter_err_msg = "Failed to allocate interpreter memory";
		return FAIL;
	}
	memcpy(vm.mem, program, sizeof(cell) * program_size);
	free(program);

	print_debug(verbose, " Initializing variable table, program counter, and data pointer...\n");
	ptrdiff_t prog_ptr = 0;
	// starts at the data section, addresses start at 0
	ptrdiff_t data_ptr = (ptrdiff_t)program_size;
	Map vtable = { .size = 0, .pairs = NULL };

	size_t scope_starts[SCOPE_STACK_SIZE] = {0};
	cell scope_remaining[SCOPE_STACK_SIZE] = {0};
	bool scope_is_infinite[SCOPE_STACK_SIZE] = {false};
	int scope_top = -1;

	int status = SUCCESS;
	while(true) {
		if(!in_bounds(&vm, prog_ptr)) {
			interpreter_err_msg = "Program Pointer ran out of bounds";
			status = OUT_OF_BOUNDS;
			break;
		}

		cell instr = vm.mem[prog_ptr];
		if(instr == BC_EOD) break;

		// anything outside of a byte (Ex: written by self modifying code) is a NOP
		if(instr < 0 || instr > 0xFF) {
			prog_ptr += 1;
			continue;
		}

		ptrdiff_t next = prog_ptr + (ptrdiff_t)bc_instruction_length(instr);
		cell operand = bc_has_operand(instr) ? read_cell(&vm, prog_ptr + 1) : 0;
		int access = BC_ACCESS(instr);

		switch(BC_TYPE(instr)) {
			case VT_LITERAL:
				if(access == ACC_PUSH_VALUE) push_value(&vm, operand);
				break;
			case VT_VARIABLE: {
				MapElement* slot = find_or_create_var(&vtable, operand, &vm);
				access_target(&vm, access, &slot->position, false);
				break;
			}
			case VT_DATA_PTR:
				access_target(&vm, access, &data_ptr, true);
				break;
			case VT_PROG_PTR: {
				// `P` refers to the instruction currently being executed
				ptrdiff_t target = prog_ptr;
				if(access == ACC_SET_ADDRESS) {
					cell jump = pop_value(&vm);
					if(jump >= 0 && (size_t)jump < program_size) next = jump;
					break;
				}
				access_target(&vm, access, &target, false);
				if(access == ACC_INC_ADDRESS || access == ACC_DEC_ADDRESS) {
					next += target - prog_ptr;
				}
				break;
			}
			case VT_META:
			case VT_EOD:
				break;
			case VT_EOS:
				vm.stack_top = -1;
				break;
			case VT_OPERATION: {
				int operation = BC_OPERATION(instr);
				switch(operation) {
					case OP_NOP:
						break;
					case OP_BEGIN_SCOPE: {
						bool should_enter = true;
						if(vm.stack_top >= 0) {
							should_enter = pop_value(&vm) != 0;
						}
						if(!should_enter) {
							next = (ptrdiff_t)skip_scope(&vm, (size_t)next);
							break;
						}
						if(scope_top + 1 < SCOPE_STACK_SIZE) {
							scope_top++;
							scope_starts[scope_top] = (size_t)next;
							scope_remaining[scope_top] = 0;
							scope_is_infinite[scope_top] = false;
						}
						break;
					}
					case OP_DEFINE_SCOPE_ITERATIONS: {
						cell count = pop_value(&vm);
						if(scope_top >= 0) {
							if(count <= 0) {
								scope_top--;
							} else {
								if(scope_remaining[scope_top] == 0) {
									scope_remaining[scope_top] = count - 1;
								}
								if(scope_remaining[scope_top] > 0) {
									scope_remaining[scope_top]--;
									next = (ptrdiff_t)scope_starts[scope_top];
								}
							}
						}
						break;
					}
					case OP_INFINITE_LOOP:
						if(scope_top >= 0) {
							scope_is_infinite[scope_top] = true;
						}
						break;
					case OP_BREAK_FROM_SCOPE: {
						cell condition = pop_value(&vm);
						if(scope_top >= 0 && condition) {
							next = (ptrdiff_t)skip_scope(&vm, (size_t)next);
							scope_top--;
						}
						break;
					}
					case OP_END_SCOPE:
						if(scope_top >= 0) {
							if(scope_is_infinite[scope_top]) {
								next = (ptrdiff_t)scope_starts[scope_top];
							} else {
								scope_top--;
							}
						}
						break;
					case OP_PRINT:
						putchar((unsigned char)pop_value(&vm));
						break;
					case OP_PRINT_LITERAL:
						printf("%d", pop_value(&vm));
						break;
					case OP_DEREFERENCE:
						push_value(&vm, read_cell(&vm, pop_value(&vm)));
						break;
					case OP_BIT_NOT:
						push_value(&vm, ~pop_value(&vm));
						break;
					default: {
						if(operation < OP_ADD || operation > OP_BIT_XOR) break;
						cell right = pop_value(&vm);
						cell left = pop_value(&vm);
						push_value(&vm, binary_operation(operation, left, right));
						break;
					}
				}
				break;
			}
			default:
				break;
		}

		prog_ptr = next;
	}

	// clean up runtime memory space
	free(vtable.pairs);
	free(vm.mem);

	return status;
}
