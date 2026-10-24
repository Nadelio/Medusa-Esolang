#include <stdio.h>
#include <stdlib.h>
#include "../include/bytecode.h"

bool bc_has_operand(cell instr) {
	if(instr < 0 || instr > 0xFF) return false;
	int type = BC_TYPE(instr);
	return type == VT_LITERAL || type == VT_VARIABLE || type == VT_META;
}

size_t bc_instruction_length(cell instr) {
	return bc_has_operand(instr) ? 2 : 1;
}

static int width_for_value(cell value) {
	if(value >= INT8_MIN && value <= INT8_MAX) return WIDTH_1;
	if(value >= INT16_MIN && value <= INT16_MAX) return WIDTH_2;
	return WIDTH_4;
}

u8* bc_encode(const cell* cells, size_t cell_count, size_t* out_len) {
	// worst case every cell is an instruction followed by a 4 byte operand
	u8* bytes = malloc(cell_count * 5 + 1);
	if(bytes == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate bytecode buffer\n");
		exit(1);
	}

	size_t length = 0;
	for(size_t i = 0; i < cell_count; i++) {
		cell instr = cells[i];
		if(!bc_has_operand(instr) || i + 1 >= cell_count) {
			bytes[length++] = (u8)instr;
			continue;
		}

		cell value = cells[++i];
		int width = width_for_value(value);
		bytes[length++] = (u8)((instr & ~0x18) | (width << 3));
		uint32_t raw = (uint32_t)value;
		for(int b = 0; b < (1 << width); b++) {
			bytes[length++] = (u8)(raw >> (8 * b));
		}
	}

	*out_len = length;
	return bytes;
}

cell* bc_decode(const u8* bytes, size_t byte_len, size_t* out_cell_count, size_t* out_data_mem_size) {
	// a program never decodes to more cells than it has bytes, +1 for a missing EOD
	cell* cells = malloc(sizeof(cell) * (byte_len + 1));
	if(cells == NULL) return NULL;

	size_t count = 0;
	bool found_mem_size = false;
	bool found_eod = false;
	*out_data_mem_size = 0;

	for(size_t i = 0; i < byte_len && !found_eod;) {
		u8 instr = bytes[i++];
		int type = BC_TYPE(instr);

		if(type == VT_EOD) found_eod = true;

		if(!bc_has_operand(instr)) {
			// D and P never carry an operand, so their width bits are always 00 in memory
			cells[count++] = (type == VT_DATA_PTR || type == VT_PROG_PTR) ? (cell)(instr & ~0x18) : (cell)instr;
			continue;
		}

		int width = BC_WIDTH(instr);
		if(width == WIDTH_RESERVED) {
			free(cells);
			return NULL;
		}

		size_t operand_len = (size_t)1 << width;
		if(i + operand_len > byte_len) {
			free(cells);
			return NULL;
		}

		uint32_t raw = 0;
		for(size_t b = 0; b < operand_len; b++) {
			raw |= (uint32_t)bytes[i + b] << (8 * b);
		}
		i += operand_len;

		// sign extend 1 and 2 byte operands
		if(operand_len < 4 && (raw & (1u << (operand_len * 8 - 1)))) {
			raw |= ~0u << (operand_len * 8);
		}

		cell value = (cell)raw;
		cells[count++] = (cell)(instr & ~0x18);
		cells[count++] = value;

		if(type == VT_META && BC_ACCESS(instr) == META_DATA_MEM_SIZE && !found_mem_size) {
			found_mem_size = true;
			*out_data_mem_size = value > 0 ? (size_t)value : 0;
		}
	}

	if(!found_eod) {
		cells[count++] = BC_EOD;
	}

	*out_cell_count = count;
	return cells;
}

static const char* operation_name(int operation) {
	switch(operation) {
		case OP_NOP: return "nop";
		case OP_BEGIN_SCOPE: return "[";
		case OP_END_SCOPE: return "]";
		case OP_DEFINE_SCOPE_ITERATIONS: return "$";
		case OP_BREAK_FROM_SCOPE: return "?";
		case OP_INFINITE_LOOP: return "$$";
		case OP_PRINT: return "@";
		case OP_PRINT_LITERAL: return "@@";
		case OP_DEREFERENCE: return "*";
		case OP_ADD: return "+";
		case OP_SUB: return "-";
		case OP_MUL: return "mul";
		case OP_DIV: return "/";
		case OP_MOD: return "%";
		case OP_EQUAL: return "==";
		case OP_NOT_EQUAL: return "!=";
		case OP_LESS: return "<";
		case OP_LESS_EQUAL: return "<=";
		case OP_GREATER: return ">";
		case OP_GREATER_EQUAL: return ">=";
		case OP_BIT_AND: return "&&";
		case OP_BIT_OR: return "|";
		case OP_BIT_XOR: return "^";
		case OP_BIT_NOT: return "~";
		default: return NULL;
	}
}

static void print_access(int access, const char* target) {
	switch(access) {
		case ACC_PUSH_VALUE: printf("%s ", target); break;
		case ACC_PUSH_ADDRESS: printf("&%s ", target); break;
		case ACC_SET_VALUE: printf("=%s ", target); break;
		case ACC_SET_ADDRESS: printf(":%s ", target); break;
		case ACC_INC_VALUE: printf("%s++ ", target); break;
		case ACC_DEC_VALUE: printf("%s-- ", target); break;
		case ACC_INC_ADDRESS: printf("->%s ", target); break;
		case ACC_DEC_ADDRESS: printf("<-%s ", target); break;
		default: break;
	}
}

void bc_print(const cell* cells, size_t cell_count) {
	for(size_t i = 0; i < cell_count; i++) {
		cell instr = cells[i];
		if(instr < 0 || instr > 0xFF) {
			printf("(%d) ", instr);
			continue;
		}

		char target[32];
		cell operand = bc_has_operand(instr) && i + 1 < cell_count ? cells[i + 1] : 0;
		switch(BC_TYPE(instr)) {
			case VT_OPERATION: {
				const char* name = operation_name(BC_OPERATION(instr));
				if(name != NULL) printf("%s ", name);
				else printf("(%d) ", instr);
				break;
			}
			case VT_LITERAL:
				snprintf(target, sizeof(target), "I%d", operand);
				print_access(BC_ACCESS(instr), target);
				i++;
				break;
			case VT_DATA_PTR:
				print_access(BC_ACCESS(instr), "D");
				break;
			case VT_PROG_PTR:
				print_access(BC_ACCESS(instr), "P");
				break;
			case VT_VARIABLE:
				snprintf(target, sizeof(target), "C%d", operand);
				print_access(BC_ACCESS(instr), target);
				i++;
				break;
			case VT_META:
				if(BC_ACCESS(instr) == META_DATA_MEM_SIZE) printf("#%d", operand);
				else printf("meta(%d) %d", BC_ACCESS(instr), operand);
				i++;
				break;
			case VT_EOS:
				printf(";\n");
				break;
			case VT_EOD:
				printf("\nEOD\n");
				break;
			default:
				break;
		}
	}
	printf("\n");
}
