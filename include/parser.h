#ifndef MEDUSA_PARSER_H
#define MEDUSA_PARSER_H

#include <stdio.h>
#include "structs.h"
#include "debug.h"

enum TokenValues {
	EOD = -1,
	_NULL,
	INT_LITERAL,
    IDENT,
	VARIABLE,
	DATA_PTR_REF,
	PROG_PTR_REF,
	VALUE_ASSIGN,
	ADDRESS_ASSIGN,
	READ_ADDRESS,
	DEREFERENCE,
	MULTI,
	DIV,
	MODULO,
	BIT_AND,
	BIT_OR,
	BIT_XOR,
	BIT_NOT,
	PLUS,
	INC,
	MINUS,
	DEC,
	EQUIV,
	NOT_EQUIV,
	LESS,
	GREATER,
	LESS_EQUAL,
	GREATER_EQUAL,
	RIGHT_ARROW,
	LEFT_ARROW,
	BEGIN_SCOPE,
	END_SCOPE,
	DEFINE_SCOPE_ITERATIONS,
	BREAK_FROM_SCOPE,
	INFINITE_LOOP,
    L_PAREN,
    R_PAREN,
    PRINT,
    PRINT_LITERAL,
	DEFINE_DATA_MEM_SIZE,
	EOS
};

enum STATUS {
	FAIL = -1,
	SUCCESS,
	SYNTAX_ERROR,
	OUT_OF_BOUNDS
};

extern char* interpreter_err_msg;
extern size_t data_mem_size;

// compiles src code into bytecode, `out_len` receives the byte count
u8* parse(bool verbose, FILE* source_file, size_t* out_len);
int run(bool verbose, const u8* bytecode, size_t bytecode_len);

#endif
