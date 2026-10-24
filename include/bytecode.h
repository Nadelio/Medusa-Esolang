#ifndef MEDUSA_BYTECODE_H
#define MEDUSA_BYTECODE_H

#include <stddef.h>
#include <stdbool.h>
#include "structs.h"

// Instruction byte layout:
//   typed:   [TTT][WW][AAA]  value type, operand width, access
//   untyped: [000][OOOOO]    operation

enum ValueType {
	VT_OPERATION = 0,
	VT_LITERAL   = 1,
	VT_DATA_PTR  = 2,
	VT_PROG_PTR  = 3,
	VT_VARIABLE  = 4,
	VT_META      = 5,
	VT_EOS       = 6,
	VT_EOD       = 7
};

enum Access {
	ACC_PUSH_VALUE   = 0,
	ACC_PUSH_ADDRESS = 1,
	ACC_SET_VALUE    = 2,
	ACC_SET_ADDRESS  = 3,
	ACC_INC_VALUE    = 4,
	ACC_DEC_VALUE    = 5,
	ACC_INC_ADDRESS  = 6,
	ACC_DEC_ADDRESS  = 7
};

enum MetaAccess {
	META_DATA_MEM_SIZE = 0
};

enum OperandWidth {
	WIDTH_1 = 0,
	WIDTH_2 = 1,
	WIDTH_4 = 2,
	WIDTH_RESERVED = 3
};

enum Operation {
	OP_NOP                     = 0x00,
	OP_BEGIN_SCOPE             = 0x01,
	OP_END_SCOPE               = 0x02,
	OP_DEFINE_SCOPE_ITERATIONS = 0x03,
	OP_BREAK_FROM_SCOPE        = 0x04,
	OP_INFINITE_LOOP           = 0x05,
	OP_PRINT                   = 0x06,
	OP_PRINT_LITERAL           = 0x07,
	OP_DEREFERENCE             = 0x08,
	OP_ADD                     = 0x09,
	OP_SUB                     = 0x0A,
	OP_MUL                     = 0x0B,
	OP_DIV                     = 0x0C,
	OP_MOD                     = 0x0D,
	OP_EQUAL                   = 0x0E,
	OP_NOT_EQUAL               = 0x0F,
	OP_LESS                    = 0x10,
	OP_LESS_EQUAL              = 0x11,
	OP_GREATER                 = 0x12,
	OP_GREATER_EQUAL           = 0x13,
	OP_BIT_AND                 = 0x14,
	OP_BIT_OR                  = 0x15,
	OP_BIT_XOR                 = 0x16,
	OP_BIT_NOT                 = 0x17
};

#define BC_EOS ((cell)(VT_EOS << 5))
#define BC_EOD ((cell)(VT_EOD << 5))

#define BC_TYPED(type, access) ((cell)(((type) << 5) | (access)))
#define BC_TYPE(instr)      ((int)(((instr) >> 5) & 0x7))
#define BC_WIDTH(instr)     ((int)(((instr) >> 3) & 0x3))
#define BC_ACCESS(instr)    ((int)((instr) & 0x7))
#define BC_OPERATION(instr) ((int)((instr) & 0x1F))

// true if the instruction is followed by an operand (one cell in memory, 1/2/4 bytes on disk)
bool bc_has_operand(cell instr);
// number of memory cells the instruction occupies, including its operand
size_t bc_instruction_length(cell instr);

// encodes canonical memory cells into the on-disk byte format, returns the byte count
u8* bc_encode(const cell* cells, size_t cell_count, size_t* out_len);
// decodes the on-disk byte format into memory cells, returns NULL on malformed input
cell* bc_decode(const u8* bytes, size_t byte_len, size_t* out_cell_count, size_t* out_data_mem_size);
// prints a human readable listing of memory cells
void bc_print(const cell* cells, size_t cell_count);

#endif
