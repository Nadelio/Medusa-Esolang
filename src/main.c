#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../include/structs.h"
#include "../include/parser.h"

static void print_compiled_program(const i32* program, size_t program_len);

i32 main(int argc, char** argv) {
	const char* help_message = "--help              | -h      : print the help message\n--verbose           | -v      : verbose information about parser and runtime states\n--export            | -x       : export the binary for the program\n--input <filename>  | -f <filename> : use the following file as a program\n--output <filename> | -o <filename> : define the name for the exported program (defaults to program.bin)\n";

	bool verbose = false;

	bool use_existing_file = false;
	char* name_of_input_file = NULL;

	bool export_binary = false;
	char* name_of_output_file = NULL;

	// process command line arguments
	for(int i = 1; i < argc; i++) {
		char* curr_arg = argv[i];
		
		if(strcmp(curr_arg, "--verbose") == 0 || strcmp(curr_arg, "-v") == 0) verbose = true;

		if(strcmp(curr_arg, "--help") == 0 || strcmp(curr_arg, "-h") == 0) printf("%s", help_message);
		
		if(strcmp(curr_arg, "--export") == 0 || strcmp(curr_arg, "-x") == 0) export_binary = true;
		
		if(strcmp(curr_arg, "--input") == 0 || strcmp(curr_arg, "-f") == 0) {
			if(argc > i + 1) {
				use_existing_file = true;
				size_t input_len = strlen(argv[i + 1]) + 1;
				name_of_input_file = malloc(input_len);
				if(name_of_input_file == NULL) {
					print_error(" Failed to allocate memory for input filename\n");
					return 1;
				}
				strcpy_s(name_of_input_file, input_len, argv[i + 1]);
				i++;
			} else {
				print_error(" Missing a file name for input flag.");
				return 1;
			}
		}
		
		if(strcmp(curr_arg, "--output") == 0 || strcmp(curr_arg, "-o") == 0) {
			if(argc > i + 1) {
				export_binary = true;
				size_t output_len = strlen(argv[i + 1]) + 1;
				name_of_output_file = malloc(output_len);
				if(name_of_output_file == NULL) {
					print_error(" Failed to allocate memory for output filename\n");
					return 1;
				}
				strcpy_s(name_of_output_file, output_len, argv[i + 1]);
				i++;
			} else {
				print_error(" Missing a file name for output flag.");
				return 1;
			}
		}
	}

	if(argc == 1) {
		print_debug(verbose, " No flags passed in.");
		printf("%s", help_message);
	} else {
		print_debug(verbose, " Flags:\n  Verbose? %s\n  Input file? %s\n    Name: %s\n  Export binary? %s\n    Name: %s\n", 
		       verbose ? "true" : "false", 
		       use_existing_file ? "true" : "false", 
		       name_of_input_file ? name_of_input_file : "(none)",
		       export_binary ? "true" : "false", 
		       name_of_output_file ? name_of_output_file : "(none)");
	}

	if (use_existing_file) {
		print_debug(verbose, " Using existing file...\n");

		FILE* source_file;
		errno_t err = fopen_s(&source_file, name_of_input_file, "r");
		if(err != 0) {
			char err_buf[128];
			strerror_s(err_buf, sizeof(err_buf), err);
			print_error(" Cannot open file %s.\n\tError: %s\n", name_of_input_file, err_buf);
			return 1;
		}
		
		print_debug(verbose, " Parsing %s...\n", name_of_input_file);

		i32* compiled_program = parse(verbose, source_file);
		
		size_t program_len = 0;
		while(compiled_program[program_len] != EOD) {
			program_len++;
		}
		program_len++;

		print_debug(verbose, " Compiled program (modified slightly for readability):\n");
		if(verbose) {
			print_compiled_program(compiled_program, program_len);
		}

		// export program to file
		if(export_binary) {
			print_debug(verbose, " Exporting compiled program to %s...\n", name_of_output_file);

			if(name_of_output_file == NULL) name_of_output_file = "a.bin";

			FILE* exported_file;
			errno_t export_errors = fopen_s(&exported_file, name_of_output_file, "w");
			if(export_errors != 0) {
				char err_buf[128];
				strerror_s(err_buf, sizeof(err_buf), export_errors);
				print_error(" Cannot export file %s.\n\tError: %s\n", name_of_output_file, err_buf);
				return 1;
			}
		
			fwrite(compiled_program, sizeof(i32), program_len, exported_file);
			fclose(exported_file);
			print_debug(verbose, " Successfully exported program to %s.\n", name_of_output_file);
		}

		i32 status = run(verbose, compiled_program, program_len);
		print_debug(verbose, " Runtime status: %d\n", status);

		if(name_of_input_file) free(name_of_input_file);
		if(name_of_output_file) free(name_of_output_file);
		free(compiled_program);

		// run program
		return 0;
	}

	return 0;
}

static void print_compiled_program(const i32* program, size_t program_len) {
	for(size_t i = 0; i < program_len; i++) {
		switch(program[i]) {
			case EOS:
				printf(";\n");
				break;
			case EOD:
				printf("\nEOD\n");
				break;
			case VARIABLE:
				if(i + 1 < program_len) {
					printf("C%d ", program[++i]);
				} else {
					printf("C? ");
				}
				break;
			case PROG_PTR_REF:
				printf("P ");
				break;
			case DATA_PTR_REF:
				printf("D ");
				break;
			case INT_LITERAL:
				if(i + 1 < program_len) {
					printf("I%d ", program[++i]);
				} else {
					printf("I? ");
				}
				break;
			case ASSIGN: printf("= "); break;
			case ADDRESS_ASSIGN: printf(": "); break;
			case DECLARE_REF: printf("& "); break;
			case DEREF: printf("* "); break;
			case MULTI: printf("mul "); break;
			case DIV: printf("/ "); break;
			case PLUS: printf("+ "); break;
			case INC: printf("++ "); break;
			case MINUS: printf("- "); break;
			case DEC: printf("-- "); break;
			case EQUIV: printf("== "); break;
			case NOT_EQUIV: printf("!= "); break;
			case LESS: printf("< "); break;
			case GREATER: printf("> "); break;
			case LESS_EQUAL: printf("<= "); break;
			case GREATER_EQUAL: printf(">= "); break;
			case RIGHT_ARROW: printf("-> "); break;
			case LEFT_ARROW: printf("<- "); break;
			case BEGIN_SCOPE: printf("[ "); break;
			case END_SCOPE: printf("] "); break;
			case DEFINE_SCOPE_ITERATIONS: printf("$ "); break;
			case BREAK_FROM_SCOPE: printf("^ "); break;
			case INFINITE_LOOP: printf("~ "); break;
			case PRINT: printf("%% "); break;
			case PRINT_LITERAL: printf("%%%% "); break;
			case DEFINE_DATA_MEM_SIZE: printf("# "); break;
			default:
				printf("%d ", program[i]);
				break;
		}
	}
	printf("\n");
}
