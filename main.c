#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

typedef char* string;
typedef int32_t i32;

typedef struct {
	i32 key;
	i32 value;
} MapElement;

typedef struct {
	size_t size;
	MapElement* pairs;
} Map;

void put(Map* map, i32 key, i32 value) {
	map->pairs = realloc(map->pairs, sizeof(MapElement*) * (map->size + 1));
	map->pairs[map->size] = (MapElement){ .key = key, .value = value };
	map->size = map->size + 1;
}

i32 get(Map* map, i32 key) {
	for(int i = 0; i < map->size; i++) {
		if(map->pairs[i].key == key) {
			return map->pairs[i].value;
		}
	}
	printf("[WARNING] `get()` failed to find value referenced by %d.\n  Adding placeholder: { %d : 0 }\n", key, key);
	put(map, key, 0);
	return 0;
}

void set(Map* map, i32 key, i32 value) {
	for(int i = 0; i < map->size; i++) {
		if(map->pairs[i].key == key) {
			map->pairs[i].value = value;
		}
	}
	put(map, key, value);
}

typedef struct {
	i32 value;
	i32 position;
	bool is_literal;
} Variable;

typedef struct {
	Variable* stack[64];
	size_t top;
	size_t size;
} StackVariable;

StackVariable* build_varstack() {
	StackVariable* stack = (StackVariable*)malloc(sizeof(StackVariable));
	stack->top = -1;
	stack->size = 64;
	return stack;
}

void deconstruct(StackVariable* varstack) {
	for(int i = 0; i < varstack->size; i++) {
		free(varstack->stack[i]);
	}
	free(varstack);
}

bool is_varstack_empty(StackVariable* stack) {
	return stack->top == -1;
}

bool is_varstack_full(StackVariable* stack) {
	return stack->top == stack->size;
}

Variable* pop_varstack(StackVariable* stack) {
	if(is_varstack_empty(stack)) {
		printf("[ERROR] Stack underflow.\n");
		return NULL;
	}
	stack->top -= 1;
	return stack->stack[stack->top + 1];
}

Variable* peek_varstack(StackVariable* stack) {
	if(is_varstack_empty(stack)) {
		printf("[ERROR] Stack is empty");
		return NULL;
	}
	return stack->stack[stack->top];
}

void push_varstack(StackVariable* stack, Variable* value) {
	if(is_varstack_full(stack)) {
		printf("[ERROR] Stack overflow.\n");
		return;
	}
	stack->top += 1;
	stack->stack[stack->top] = value;
}

typedef struct {
	i32 stack[64];
	size_t top;
	size_t size;
} Stacki32;

Stacki32* build() {
	Stacki32* stack = (Stacki32*)malloc(sizeof(Stacki32));
	stack->top = -1;
	stack->size = 64;
	return stack;
}

bool is_empty(Stacki32* stack) {
	return stack->top == -1;
}

bool is_full(Stacki32* stack) {
	return stack->top == stack->size;
}

i32 pop(Stacki32* stack) {
	if(is_empty(stack)) {
		printf("[ERROR] Stack underflow.\n");
		return -1;
	}
	stack->top -= 1;
	return stack->stack[stack->top + 1];
}

i32 peek(Stacki32* stack) {
	if(is_empty(stack)) {
		printf("[ERROR] Stack is empty");
		return 0;
	}
	return stack->stack[stack->top];
}

void push(Stacki32* stack, i32 value) {
	if(is_full(stack)) {
		printf("[ERROR] Stack overflow.\n");
		return;
	}
	stack->top += 1;
	stack->stack[stack->top] = value;
}

bool is_out_of_bounds(i32* ptr, i32* lower_bound, i32* upper_bound) {
	if(ptr < lower_bound || ptr > upper_bound) {
		return true;
	}
	return false;
}

/*
FULL INSTRUCTION SET:
(meta instructions)
#128 // allocate 128 bytes for data space (only valid at the very beginning of a program)

(variable instructions)
C0 : 12 // set address of variable
C0 = 10 // set value of variable
C1 = &C0 // store the address of a variable

(system pointer instructions)
<- P // move left program ptr
<- D // move left data ptr
-> P // move right p ptr
-> D // move right d ptr
P : 5 // move program ptr
D : 12 // move data ptr
P = 9 // set value at program ptr
D = 12 // set value at data ptr

(arithmetic instructions)
C0 ++ // inc
C0 -- // dec
C0 + 12 // add
C0 - 12 // sub
C0 * 1 // multiplication
C0 / 1 // integer division

(conditional instructions will run the following instruction if true)
C0 == C1 // is equal
C0 > C1 // is greater
C0 >= C1 // is greater or equal
C0 < C1 // is less
C0 <= C1 // is less or equal
C0 != C1 // is not

(scopes and control flow instructions)
[ $10 ... ] // loop 10 times
[ ~ ... ] // loop infinitely
[ ~ ... C0 != C1 ^ ] // break from infinite loop if C0 is not equal to C1
C0 == C1 [ $1 -> P ] // if C0 is equal to C1, move the program pointer once to the right
*/

/*
OP CODES:
-1 : end of program data
0 : null
1 : variable (C), following integer represents the index in the v-table
2 : data pointer reference (D)
3 : program pointer reference (P)
4 : =
5 : :
6 : &
7 : *
8 : /
9 : +
10 : ++
11 : -
12 : --
13 : ==
14 : !=
15 : <
16 : >
17 : <=
18 : >=
19 : -> (move data or program pointer once to the right)
20 : <- (move data or program pointer once to the left)
21 : [ (begin scope)
22 : ] (end scope)
23 : $ (define number of iterations to run scope, following integer represents number of iterations)
24 : ^ (break from loop)
25 : ~ (define infinite loop)
26 : # (data space allocation instruction)
*/

enum TokenValues {
	EOD = -1, // End of program data marker
	_NULL, // NOP and padding
	VARIABLE,
	DATA_PTR_REF,
	PROG_PTR_REF,
	ASSIGN,
	ADDRESS_ASSIGN,
	AMPERSAND,
	ASTRICK,
	DIV,
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
	DEFINE_DATA_MEM_SIZE,
	INT_LITERAL,
	EOS // end of statement
};

enum STATUS {
	FAIL = -1,
	SUCCESS,
	SYNTAX_ERROR,
	OUT_OF_BOUNDS
};
char* interpreter_err_msg;
size_t data_mem_size = 128; // size of allocated memory for interpreter data (in bytes)

/*
Steps:
1. Compile program to array of integers
2. Malloc space to store program and to store additional working memory
		- defined by user in the command line args?
		- defined by user in program data using `#<mem_size>`?
3. Initialize program pointer and data pointer
4. Begin incrementing the program pointer and running the instructions
5. Continue until program reaches an end of program data instruction (-1)
*/

/// Returns pointer to first int in array
i32* parse(FILE* source_file);
/// Returns the program status
i32 run(i32* program, size_t program_size);
/// evaluates the interpreter stacks
i32 evaluate(Stacki32* opstack, StackVariable* varstack);

/*
Command line arguments:
--help							| -h						: print the help message
--verbose						| -v						: verbose information about parser and runtime states
--export						| -x						: export the binary for the program
--input <filename>	| -f <filename> : use the following file as a program
--output <filename> | -o <filename> : define the name for the exported program (defaults to program.bin)
*/

i32 main(int argc, char** argv) {
	const char* help_message = "--help       | -h      : print the help message\n--verbose      | -v      : verbose information about parser and runtime states\n--export       | -x       : export the binary for the program\n--input <filename> | -f <filename> : use the following file as a program\n--output <filename> | -o <filename> : define the name for the exported program (defaults to program.bin)\n";

	bool verbose = false;

	bool use_existing_file = false;
	char* name_of_input_file = NULL;

	bool export_binary = false;
	char* name_of_output_file = NULL;

	// process command line arguments
	for(size_t i = 1; i < argc; i++) {
		char* curr_arg = argv[i];
		
		if(strcmp(curr_arg, "--verbose") == 0 || strcmp(curr_arg, "-v") == 0) verbose = true;

		if(strcmp(curr_arg, "--help") == 0 || strcmp(curr_arg, "-h") == 0) printf("%s", help_message);
		
		if(strcmp(curr_arg, "--export") == 0 || strcmp(curr_arg, "-x") == 0) export_binary = true;
		
		if(strcmp(curr_arg, "--input") == 0 || strcmp(curr_arg, "-f") == 0) {
			if(argc > i + 1) {
				use_existing_file = true;
				name_of_input_file = malloc(strlen(argv[i + 1]) + 1);
				if(name_of_input_file == NULL) {
					fprintf(stderr, "[ERROR] Failed to allocate memory for input filename\n");
					return 1;
				}
				strcpy(name_of_input_file, argv[i + 1]);
				i++;
			} else {
				printf("Missing a file name for input flag.");
				return 1;
			}
		}
		
		if(strcmp(curr_arg, "--output") == 0 || strcmp(curr_arg, "-o") == 0) {
			if(argc > i + 1) {
				export_binary = true;
				name_of_output_file = malloc(strlen(argv[i + 1]) + 1);
				if(name_of_output_file == NULL) {
					fprintf(stderr, "[ERROR] Failed to allocate memory for output filename\n");
					return 1;
				}
				strcpy(name_of_output_file, argv[i + 1]);
				i++;
			} else {
				printf("Missing a file name for output flag.");
				return 1;
			}
		}
	}

	if(argc == 1) {
		printf("No flags passed in.");
	} else {
		printf("Flags:\n  Verbose? %d\n  Input file? %d Name: %s\n  Export binary? %d Name: %s\n", 
		       verbose, 
		       use_existing_file, 
		       name_of_input_file ? name_of_input_file : "(none)",
		       export_binary, 
		       name_of_output_file ? name_of_output_file : "(none)");
	}

	if (use_existing_file) {
		printf("Using existing file...\n");

		FILE* source_file;
		errno_t err = fopen_s(&source_file, name_of_input_file, "r");
		if(err != 0) {
			fprintf(stderr, "[ERROR] Cannot open file %s.\n\tError: %s\n", name_of_input_file, strerror(err));
			return 1;
		}
		
		printf("Parsing %s...\n", name_of_input_file);

		i32* compiled_program = parse(source_file);
		
		size_t program_len = 0;
		while(compiled_program[program_len] != EOD) {
			program_len++;
		}
		program_len++;

		printf("Compiled program (modified slightly for readability):\n");
		for(size_t i = 0; i < program_len; i++) {
			if(compiled_program[i] == EOS) {
				printf(";\n");
			} else if(compiled_program[i] == EOD) {
				printf("\nEOD\n");
			} else {
				printf("%d ", compiled_program[i]);
			}
		}
		printf("\n");

		// export program to file
		if(export_binary) {
			printf("Exporting compiled program to %s...\n", name_of_output_file);

			if(name_of_output_file == NULL) name_of_output_file = "a.bin";

			FILE* exported_file;
			errno_t export_errors = fopen_s(&exported_file, name_of_output_file, "w");
			if(export_errors != 0) {
				fprintf(stderr, "[ERROR] Cannot export file %s.\n\tError: %s\n", name_of_output_file, strerror(export_errors));
				return 1;
			}
		
			fwrite(compiled_program, sizeof(i32), program_len, exported_file);
			fclose(exported_file);
			printf("Successfully exported program to %s.\n", name_of_output_file);
		}

		i32 status = run(compiled_program, program_len);

		if(name_of_input_file) free(name_of_input_file);
		if(name_of_output_file) free(name_of_output_file);
		free(compiled_program);

		// run program
		return 0;
	}

	return 0;
}

i32* parse(FILE* source_file) {
	char *source_code;
	size_t code_len = 0;
	int c;

	size_t buf_size = 1024;
	source_code = malloc(sizeof(char) * buf_size);

	while((c = fgetc(source_file)) != EOF) {
		if(code_len >= buf_size - 1) {
			buf_size *= 2;
			source_code = realloc(source_code, buf_size);
			if(source_code == NULL) {
				fprintf(stderr, "[ERROR] Failed to reallocate buffer\n");
				exit(1);
			}
		}
		source_code[code_len++] = c;
	}

	source_code[code_len] = '\0'; // null terminate
	
	printf("Source code:\n%s\n", source_code);

	i32* compiled_code = (int*)malloc(sizeof(i32) * (code_len * 2));
	if(compiled_code == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate memory for compiled code\n");
		free(source_code);
		exit(1);
	}

	size_t write_index = 0;
	i32 integer_buf_size;
	char* buf;
	int var_index;

	for(size_t i = 0; i < code_len; i++) {
		char curr_tok = source_code[i];
		switch(curr_tok) {
			case 'C':
				compiled_code[write_index++] = VARIABLE;
				integer_buf_size = 10; // the MAX_I32 value has 10 digits
				buf = (char*)malloc(sizeof(char) * (integer_buf_size + 1));
				if(buf == NULL) {
					fprintf(stderr, "[ERROR] Failed to allocate parsing buffer\n");
					free(source_code);
					free(compiled_code);
					exit(1);
				}
				size_t j;
				for(j = 1; j < integer_buf_size && (i + j) < code_len; j++) {
					if(isdigit(source_code[i + j])) {
						buf[j-1] = source_code[i + j];
					} else {
						break;
					}
				}
				buf[j-1] = '\0'; // null terminate
				i += j - 1; // consume all the integer characters

				if(strlen(buf) == 0) {
					fprintf(stderr, "[ERROR] Variable reference or declaration does not have an variable index following the `C` symbol.\n");
					free(buf);
					free(source_code);
					free(compiled_code);
					exit(1);
				}

				var_index = atoi(buf);
				compiled_code[write_index++] = var_index;

				free(buf);
				break;
			case 'D':
				compiled_code[write_index] = DATA_PTR_REF;
				write_index++;
				break;
			case 'P':
				compiled_code[write_index] = PROG_PTR_REF;
				write_index++;
				break;
			case 'I':
				compiled_code[write_index++] = INT_LITERAL;

				integer_buf_size = 10; // the MAX_I32 value has 10 digits
				buf = (char*)malloc(sizeof(char) * (integer_buf_size + 1));
				if(buf == NULL) {
					fprintf(stderr, "[ERROR] Failed to allocate parsing buffer\n");
					free(source_code);
					free(compiled_code);
					exit(1);
				}
				for(j = 1; j < integer_buf_size && (i + j) < code_len; j++) {
					if(isdigit(source_code[i + j])) {
						buf[j-1] = source_code[i + j];
					} else {
						break;
					}
				}
				buf[j-1] = '\0'; // null terminate
				i += j - 1; // consume all the integer characters

				if(strlen(buf) == 0) {
					fprintf(stderr, "[ERROR] Expected an integer following `I` symbol.\n");
					free(buf);
					free(source_code);
					free(compiled_code);
					exit(1);
				}

				var_index = atoi(buf);
				compiled_code[write_index++] = var_index;

				free(buf);
				break;
			case '=':
				if(i + 1 < code_len && source_code[i + 1] == '=') {
					compiled_code[write_index++] = EQUIV;
					i++; // skip =
				} else {
					compiled_code[write_index++] = ASSIGN;
				}
				break;
			case ':':
				compiled_code[write_index] = ADDRESS_ASSIGN;
				write_index++;
				break;
			case '&':
				compiled_code[write_index] = AMPERSAND;
				write_index++;
				break;
			case '*':
				compiled_code[write_index] = ASTRICK;
				write_index++;
				break;
			case '/':
				compiled_code[write_index++] = DIV;
				break;
			case '+':
				if(i + 1 < code_len && source_code[i + 1] == '+') {
					compiled_code[write_index++] = INC;
					i++; // skip +
				} else {
					compiled_code[write_index++] = PLUS;
				}
				break;
			case '-':
				if(i + 1 < code_len && source_code[i + 1] == '-') {
					compiled_code[write_index++] = DEC;
					i++; // skip -
				} else if(i + 1 < code_len && source_code[i + 1] == '>'){
					compiled_code[write_index++] = RIGHT_ARROW;
					i++; // skip >
				} else {
					compiled_code[write_index++] = MINUS;
				}
				break;
			case '!':
				if(i + 1 < code_len && source_code[i + 1] == '=') {
					compiled_code[write_index++] = NOT_EQUIV;
					i++; // skip =
				} else {
					fprintf(stderr, "[ERROR] Expected `=` following `!` symbol.\n");
					free(source_code);
					free(compiled_code);
					exit(1);
				}
				break;
			case '<':
				if(i + 1 < code_len && source_code[i + 1] == '=') {
					compiled_code[write_index++] = LESS_EQUAL;
					i++; // skip =
				} else if(i + 1 < code_len && source_code[i + 1] == '-') {
					compiled_code[write_index++] = LEFT_ARROW;
					i++; // skip -
				} else {
					compiled_code[write_index++] = LESS;
				}
				break;
			case '>':
				if(i + 1 < code_len && source_code[i + 1] == '=') {
					compiled_code[write_index++] = GREATER_EQUAL;
					i++; // skip =
				} else {
					compiled_code[write_index++] = GREATER;
				}
				break;
			case '[':
				compiled_code[write_index] = BEGIN_SCOPE;
				write_index++;
				break;
			case ']':
				compiled_code[write_index] = END_SCOPE;
				write_index++;
				break;
			case '$':
				compiled_code[write_index++] = DEFINE_SCOPE_ITERATIONS;
				integer_buf_size = 10; // the MAX_I32 value has 10 digits
				buf = (char*)malloc(sizeof(char) * (integer_buf_size + 1));
				if(buf == NULL) {
					fprintf(stderr, "[ERROR] Failed to allocate parsing buffer\n");
					free(source_code);
					free(compiled_code);
					exit(1);
				}
				for(j = 1; j < integer_buf_size && (i + j) < code_len; j++) {
					if(isdigit(source_code[i + j])) {
						buf[j-1] = source_code[i + j];
					} else {
						break;
					}
				}
				buf[j-1] = '\0'; // null terminate
				i += j - 1; // consume all the integer characters

				if(strlen(buf) == 0) {
					fprintf(stderr, "[ERROR] Expected an integer following `$` symbol.\n");
					free(buf);
					free(source_code);
					free(compiled_code);
					exit(1);
				}

				var_index = atoi(buf);
				compiled_code[write_index++] = var_index;

				free(buf);
				break;
			case '~':
				compiled_code[write_index] = INFINITE_LOOP;
				write_index++;
				break;
			case '^':
				compiled_code[write_index] = BREAK_FROM_SCOPE;
				write_index++;
				break;
			case '#':
				compiled_code[write_index++] = DEFINE_DATA_MEM_SIZE;
				integer_buf_size = 10; // the MAX_I32 value has 10 digits
				buf = (char*)malloc(sizeof(char) * (integer_buf_size + 1));
				if(buf == NULL) {
					fprintf(stderr, "[ERROR] Failed to allocate parsing buffer\n");
					free(source_code);
					free(compiled_code);
					exit(1);
				}
				for(j = 1; j < integer_buf_size && (i + j) < code_len; j++) {
					if(isdigit(source_code[i + j])) {
						buf[j-1] = source_code[i + j];
					} else {
						break;
					}
				}
				buf[j-1] = '\0'; // null terminate
				i += j - 1; // consume all the integer characters

				if(strlen(buf) == 0) {
					fprintf(stderr, "[ERROR] Expected an integer following `#` symbol.\n");
					free(buf);
					free(source_code);
					free(compiled_code);
					exit(1);
				}

				var_index = atoi(buf);
				compiled_code[write_index++] = var_index;
				data_mem_size = var_index;

				free(buf);
				break;
			case ';':
				compiled_code[write_index] = EOS;
				write_index++;
				break;
			default: break;
		}
	}

	if(write_index >= code_len * 2) {
		fprintf(stderr, "[ERROR] Write index exceeded allocated buffer size\n");
		free(source_code);
		free(compiled_code);
		exit(1);
	}
	
	compiled_code[write_index] = EOD;

	fclose(source_file);
	free(source_code);

	return compiled_code;
}

// interpret the given program, allocate memory equal to program_size + data_mem_size (defined by the program or is 128 bytes)
i32 run(i32* program, size_t program_size) {
	printf("[DEBUG] Beginning program...\n");
	if(data_mem_size == 0) data_mem_size = 128; // if user overwrites data_mem_size with `#0;` forcefully replace it with 128 

	printf("[DEBUG] Allocating memory space, size: %llu\n", sizeof(i32) * (program_size + data_mem_size));
	// reserved memory space for program and data
	i32* mem_space = (i32*)malloc(sizeof(i32) * (program_size + data_mem_size));
	size_t total_memory_size = program_size + data_mem_size;

	printf("[DEBUG] Initializing variable table, program counter, and data pointer...\n");
	// program counter
	i32* prog_ptr = mem_space;
	// points to somewhere in data
	i32* data_ptr = mem_space + program_size;
	// variable table for interpreter
	Map* vtable = (Map*)malloc(sizeof(Map));

	// evaluation stacks
	StackVariable* varstack = build_varstack();
	Stacki32* opstack = build();
	// NOTE: both stacks are [i32] b/c both operands, variables, and literal values are all i32

	// initialize memory
	memcpy(prog_ptr, program, program_size);
	memset(data_ptr, 0, data_mem_size);
	
	// begin running program
	while(*prog_ptr != EOD) {
		switch(*prog_ptr) {
			case EOD:
				return SUCCESS;
			case _NULL:
				// do nothing, equivalent to NOP
				break;
			case VARIABLE:
				i32 var_index = *(prog_ptr + 1);
				i32 var_value = get(vtable, var_index);
				Variable* v = (Variable*)malloc(sizeof(Variable));
				*v = (Variable){.value = var_value, .position = var_index, .is_literal = false};
				push_varstack(varstack, v);
				prog_ptr += 1;
				break;
			case DATA_PTR_REF:
				i32 data_position = (i32)(data_ptr - mem_space); // get the index in memory that the data pointer is at
				i32 data_value = *data_ptr; // get the value at the data pointer
				Variable* d = (Variable*)malloc(sizeof(Variable));
				*d = (Variable){ .value = data_value, .position = data_position, .is_literal = false};
				push_varstack(varstack, d);
				break;
			case PROG_PTR_REF:
				i32 prog_position = (i32)(prog_ptr - mem_space);
				i32 prog_value = *prog_ptr;
				Variable* p = (Variable*)malloc(sizeof(Variable));
				*p = (Variable){ .value = prog_value, .position = prog_position, .is_literal = false};
				push_varstack(varstack, p);
				break;
			case ASSIGN:
				push(opstack, ASSIGN);
				break;
			case ADDRESS_ASSIGN:
				push(opstack, ADDRESS_ASSIGN);
				break;
			case AMPERSAND:
				push(opstack, AMPERSAND);
				break;
			case ASTRICK:
				push(opstack, ASTRICK);
				break;
			case DIV:
				push(opstack, DIV);
				break;
			case PLUS:
				push(opstack, PLUS);
				break;
			case INC:
				push(opstack, INC);
				break;
			case MINUS:
				push(opstack, MINUS);
				break;
			case DEC:
				push(opstack, DEC);
				break;
			case EQUIV:
				push(opstack, EQUIV);
				break;
			case NOT_EQUIV:
				push(opstack, NOT_EQUIV);
				break;
			case LESS:
				push(opstack, LESS);
				break;
			case GREATER:
				push(opstack, GREATER);
				break;
			case LESS_EQUAL:
				push(opstack, LESS_EQUAL);
				break;
			case GREATER_EQUAL:
				push(opstack, GREATER_EQUAL);
				break;
			case RIGHT_ARROW:
				push(opstack, RIGHT_ARROW);
				break;
			case LEFT_ARROW:
				push(opstack, LEFT_ARROW);
				break;
			case BEGIN_SCOPE:
			case END_SCOPE:
			case DEFINE_SCOPE_ITERATIONS:
			case BREAK_FROM_SCOPE:
			case INFINITE_LOOP:
			case DEFINE_DATA_MEM_SIZE:
			case INT_LITERAL:
				i32 lit_index = (i32)(prog_ptr - mem_space);
				i32 lit_value = *prog_ptr;
				Variable* lit = (Variable*)malloc(sizeof(Variable));
				*lit = (Variable){.position = lit_index, .value = lit_value, .is_literal = true};
				push_varstack(varstack, lit);
				break;
			case EOS: // end of statement
				i32 result = evaluate(opstack, varstack);
				if(result < 0) { /* some error occurred, handle errors */ }
				break;
			default: break;
		}

		prog_ptr += 1;
		if(is_out_of_bounds(prog_ptr, mem_space, mem_space + total_memory_size)) {
			interpreter_err_msg = "Program Pointer ran out of bounds";
			return OUT_OF_BOUNDS;
		}
	}

	// clean up runtime memory space
	free(mem_space);
	free(vtable);
	deconstruct(varstack); // since varstack contains pointers to all the Variables that were allocated, we need to deallocate those variables as well
	free(opstack);

	return FAIL;
}

i32 evaluate(Stacki32* opstack, StackVariable* varstack) {
	// implemented evaluation of stacks
	// make sure to free all the used Variable pointers that are no longer needed at the end of this function
	return -100; // not implemented yet
}
