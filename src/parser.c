#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../include/parser.h"
#include "../include/bytecode.h"

typedef struct {
	int type;
	cell value;
	bool has_value;
	// for operands, how the instruction accesses them (one of `enum Access`)
	int access;
	char text[64];
} ParsedToken;

typedef struct {
	char name[64];
	cell address;
} LabelEntry;

typedef struct {
	LabelEntry* items;
	size_t count;
	size_t capacity;
} LabelTable;

typedef struct {
	char name[64];
	size_t patch_index;
} PendingLabelEntry;

typedef struct {
	PendingLabelEntry* items;
	size_t count;
	size_t capacity;
} PendingLabelTable;

typedef struct {
	ParsedToken* items;
	size_t count;
	size_t capacity;
} TokenList;

static bool is_identifier_character(char ch) {
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
}

static bool is_whitespace_character(char ch) {
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static void append_source_char(char** buffer, size_t* length, size_t* capacity, char ch) {
	if(*length + 2 >= *capacity) {
		size_t new_capacity = *capacity == 0 ? 64 : *capacity;
		while(*length + 2 >= new_capacity) {
			new_capacity *= 2;
		}

		char* resized = realloc(*buffer, sizeof(char) * new_capacity);
		if(resized == NULL) {
			fprintf(stderr, "[ERROR] Failed to grow expanded source buffer\n");
			exit(1);
		}

		*buffer = resized;
		*capacity = new_capacity;
	}

	(*buffer)[(*length)++] = ch;
	(*buffer)[*length] = '\0';
}

static void append_source_text(char** buffer, size_t* length, size_t* capacity, const char* text) {
	for(size_t i = 0; text[i] != '\0'; i++) {
		append_source_char(buffer, length, capacity, text[i]);
	}
}

static void append_source_int(char** buffer, size_t* length, size_t* capacity, int value) {
	char number[32];
	snprintf(number, sizeof(number), "%d", value);
	append_source_text(buffer, length, capacity, number);
}

static bool read_escaped_character(const char* source, size_t code_len, size_t* index, char* out_char) {
	if(*index >= code_len) return false;

	char ch = source[(*index)++];
	if(ch != '\\') {
		*out_char = ch;
		return true;
	}

	if(*index >= code_len) return false;

	char escape = source[(*index)++];
	switch(escape) {
		case 'n': *out_char = '\n'; break;
		case 'r': *out_char = '\r'; break;
		case 't': *out_char = '\t'; break;
		case '0': *out_char = '\0'; break;
		case '\\': *out_char = '\\'; break;
		case '\'': *out_char = '\''; break;
		case '"': *out_char = '"'; break;
		default: *out_char = escape; break;
	}

	return true;
}

static char* expand_syntax_sugar(const char* source, size_t code_len, size_t* expanded_len) {
	size_t capacity = code_len > 0 ? (code_len * 4) + 64 : 64;
	char* expanded = malloc(sizeof(char) * capacity);
	if(expanded == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate expanded source buffer\n");
		exit(1);
	}

	size_t length = 0;
	expanded[0] = '\0';

	for(size_t i = 0; i < code_len;) {
		if(source[i] == '/' && i + 1 < code_len && source[i + 1] == '/') {
			while(i < code_len) {
				char ch = source[i++];
				append_source_char(&expanded, &length, &capacity, ch);
				if(ch == '\n') break;
			}
			continue;
		}

		if(source[i] == 'D'
			&& (i == 0 || !is_identifier_character(source[i - 1]))
			&& (i + 1 >= code_len || !is_identifier_character(source[i + 1]))) {
			size_t lookahead = i + 1;
			while(lookahead < code_len && is_whitespace_character(source[lookahead])) {
				lookahead++;
			}

			if(lookahead < code_len && source[lookahead] == '=') {
				lookahead++;
				while(lookahead < code_len && is_whitespace_character(source[lookahead])) {
					lookahead++;
				}

				if(lookahead < code_len && source[lookahead] == '"') {
					lookahead++;
					while(lookahead < code_len && source[lookahead] != '"') {
						char decoded = 0;
						if(!read_escaped_character(source, code_len, &lookahead, &decoded)) {
							break;
						}
						append_source_text(&expanded, &length, &capacity, "D = ");
						append_source_int(&expanded, &length, &capacity, (unsigned char)decoded);
						append_source_text(&expanded, &length, &capacity, "; -> D; ");
					}

					if(lookahead < code_len && source[lookahead] == '"') {
						size_t next_index = lookahead + 1;
						while(next_index < code_len && is_whitespace_character(source[next_index])) {
							next_index++;
						}
						i = (next_index < code_len && source[next_index] == ';') ? next_index + 1 : lookahead + 1;
						continue;
					}
				}
			}
		}

		if(source[i] == '@') {
			size_t lookahead = i + 1;
			while(lookahead < code_len && is_whitespace_character(source[lookahead])) {
				lookahead++;
			}

			if(lookahead < code_len && source[lookahead] == '"') {
				lookahead++;
				while(lookahead < code_len && source[lookahead] != '"') {
					char decoded = 0;
					if(!read_escaped_character(source, code_len, &lookahead, &decoded)) {
						break;
					}
					append_source_char(&expanded, &length, &capacity, '@');
					append_source_int(&expanded, &length, &capacity, (unsigned char)decoded);
					append_source_text(&expanded, &length, &capacity, "; ");
				}

				if(lookahead < code_len && source[lookahead] == '"') {
					i = lookahead + 1;
					continue;
				}
			}
		}

		if(source[i] == '\'') {
			size_t lookahead = i + 1;
			char decoded = 0;
			if(read_escaped_character(source, code_len, &lookahead, &decoded)
				&& lookahead < code_len && source[lookahead] == '\'') {
				append_source_int(&expanded, &length, &capacity, (unsigned char)decoded);
				i = lookahead + 1;
				continue;
			}
		}

		append_source_char(&expanded, &length, &capacity, source[i]);
		i++;
	}

	*expanded_len = length;
	return expanded;
}

static void reserve_label_table(LabelTable* table, size_t additional) {
	size_t required = table->count + additional;
	if(required <= table->capacity) return;

	size_t new_capacity = table->capacity == 0 ? 8 : table->capacity;
	while(new_capacity < required) {
		new_capacity *= 2;
	}

	LabelEntry* resized = realloc(table->items, sizeof(LabelEntry) * new_capacity);
	if(resized == NULL) {
		fprintf(stderr, "[ERROR] Failed to grow label table\n");
		exit(1);
	}

	table->items = resized;
	table->capacity = new_capacity;
}

static void reserve_pending_labels(PendingLabelTable* table, size_t additional) {
	size_t required = table->count + additional;
	if(required <= table->capacity) return;

	size_t new_capacity = table->capacity == 0 ? 8 : table->capacity;
	while(new_capacity < required) {
		new_capacity *= 2;
	}

	PendingLabelEntry* resized = realloc(table->items, sizeof(PendingLabelEntry) * new_capacity);
	if(resized == NULL) {
		fprintf(stderr, "[ERROR] Failed to grow pending label table\n");
		exit(1);
	}

	table->items = resized;
	table->capacity = new_capacity;
}

static bool find_label_address(const LabelTable* table, const char* name, cell* out_address) {
	for(size_t i = 0; i < table->count; i++) {
		if(strcmp(table->items[i].name, name) == 0) {
			*out_address = table->items[i].address;
			return true;
		}
	}
	return false;
}

static void set_label_address(LabelTable* table, const char* name, cell address) {
	for(size_t i = 0; i < table->count; i++) {
		if(strcmp(table->items[i].name, name) == 0) {
			table->items[i].address = address;
			return;
		}
	}

	reserve_label_table(table, 1);
	memset(&table->items[table->count], 0, sizeof(LabelEntry));
	strncpy_s(table->items[table->count].name, sizeof(table->items[table->count].name), name, _TRUNCATE);
	table->items[table->count].address = address;
	table->count++;
}

static void add_pending_label(PendingLabelTable* table, const char* name, size_t patch_index) {
	reserve_pending_labels(table, 1);
	memset(&table->items[table->count], 0, sizeof(PendingLabelEntry));
	strncpy_s(table->items[table->count].name, sizeof(table->items[table->count].name), name, _TRUNCATE);
	table->items[table->count].patch_index = patch_index;
	table->count++;
}

static void resolve_pending_labels(const LabelTable* labels, PendingLabelTable* pending, cell* compiled_code) {
	for(size_t i = 0; i < pending->count; i++) {
		cell address = 0;
		if(!find_label_address(labels, pending->items[i].name, &address)) {
			fprintf(stderr, "[ERROR] Undefined label: %s\n", pending->items[i].name);
			exit(1);
		}
		compiled_code[pending->items[i].patch_index] = address;
	}
}

static void reserve_token_list(TokenList* list, size_t additional) {
	size_t required = list->count + additional;
	if(required <= list->capacity) return;

	size_t new_capacity = list->capacity == 0 ? 16 : list->capacity;
	while(new_capacity < required) {
		new_capacity *= 2;
	}

	ParsedToken* resized = realloc(list->items, sizeof(ParsedToken) * new_capacity);
	if(resized == NULL) {
		fprintf(stderr, "[ERROR] Failed to grow token buffer\n");
		exit(1);
	}

	list->items = resized;
	list->capacity = new_capacity;
}

static void push_token(TokenList* list, ParsedToken token) {
	reserve_token_list(list, 1);
	list->items[list->count++] = token;
}

static ParsedToken pop_token(TokenList* list) {
	if(list->count == 0) {
		return (ParsedToken){ .type = _NULL };
	}
	return list->items[--list->count];
}

static ParsedToken peek_token(TokenList* list) {
	if(list->count == 0) {
		return (ParsedToken){ .type = _NULL };
	}
	return list->items[list->count - 1];
}

typedef struct {
	cell* items;
	size_t count;
	size_t capacity;
} CellBuffer;

static void emit_cell(CellBuffer* code, cell value) {
	if(code->count + 1 > code->capacity) {
		size_t new_capacity = code->capacity == 0 ? 64 : code->capacity * 2;
		cell* resized = realloc(code->items, sizeof(cell) * new_capacity);
		if(resized == NULL) {
			fprintf(stderr, "[ERROR] Failed to grow compiled code buffer\n");
			exit(1);
		}
		code->items = resized;
		code->capacity = new_capacity;
	}
	code->items[code->count++] = value;
}

static const char* token_symbol(int type) {
	switch(type) {
		case VALUE_ASSIGN: return "=";
		case ADDRESS_ASSIGN: return ":";
		case READ_ADDRESS: return "&";
		case DEREFERENCE: return "*";
		case MULTI: return "*";
		case DIV: return "/";
		case MODULO: return "%";
		case BIT_AND: return "&&";
		case BIT_OR: return "|";
		case BIT_XOR: return "^";
		case BIT_NOT: return "~";
		case PLUS: return "+";
		case MINUS: return "-";
		case INC: return "++";
		case DEC: return "--";
		case EQUIV: return "==";
		case NOT_EQUIV: return "!=";
		case LESS: return "<";
		case GREATER: return ">";
		case LESS_EQUAL: return "<=";
		case GREATER_EQUAL: return ">=";
		case RIGHT_ARROW: return "->";
		case LEFT_ARROW: return "<-";
		case DEFINE_SCOPE_ITERATIONS: return "$";
		case BREAK_FROM_SCOPE: return "?";
		case INFINITE_LOOP: return "$$";
		case PRINT: return "@";
		case PRINT_LITERAL: return "@@";
		case DEFINE_DATA_MEM_SIZE: return "#";
		default: return "?";
	}
}

static void compile_error(const char* message, int token_type) {
	fprintf(stderr, "[ERROR] %s `%s`\n", message, token_symbol(token_type));
	exit(1);
}

static bool is_operand_token(int type) {
	switch(type) {
		case VARIABLE:
		case DATA_PTR_REF:
		case PROG_PTR_REF:
		case INT_LITERAL:
		case IDENT:
			return true;
		default:
			return false;
	}
}

// operands that can be written to, so they can be the target of `=`, `:`, `&`, `++`, `--`, `->`, and `<-`
static bool is_target_token(int type) {
	return type == VARIABLE || type == DATA_PTR_REF || type == PROG_PTR_REF;
}

static bool is_binary_operator(int type) {
	switch(type) {
		case VALUE_ASSIGN:
		case ADDRESS_ASSIGN:
		case MULTI:
		case DIV:
		case MODULO:
		case BIT_AND:
		case BIT_OR:
		case BIT_XOR:
		case PLUS:
		case MINUS:
		case EQUIV:
		case NOT_EQUIV:
		case LESS:
		case GREATER:
		case LESS_EQUAL:
		case GREATER_EQUAL:
			return true;
		default:
			return false;
	}
}

static bool is_prefix_operator(int type) {
	switch(type) {
		case DEREFERENCE:
		case READ_ADDRESS:
		case BIT_NOT:
		case RIGHT_ARROW:
		case LEFT_ARROW:
		case DEFINE_SCOPE_ITERATIONS:
		case PRINT:
		case PRINT_LITERAL:
		case BREAK_FROM_SCOPE:
			return true;
		default:
			return false;
	}
}

static bool is_postfix_operator(int type) {
	return type == INC || type == DEC;
}

// lowest to highest: assignment, comparison, |, ^, &&, + -, * / %, prefix operators
// bitwise operators bind tighter than comparisons, so `C0 && 1 == 1` means `(C0 && 1) == 1`
static int operator_precedence(int type) {
	switch(type) {
		case VALUE_ASSIGN:
		case ADDRESS_ASSIGN:
		case BREAK_FROM_SCOPE:
			return 1;
		case EQUIV:
		case NOT_EQUIV:
		case LESS:
		case GREATER:
		case LESS_EQUAL:
		case GREATER_EQUAL:
			return 2;
		case BIT_OR:
			return 3;
		case BIT_XOR:
			return 4;
		case BIT_AND:
			return 5;
		case PLUS:
		case MINUS:
			return 6;
		case MULTI:
		case DIV:
		case MODULO:
			return 7;
		case DEREFERENCE:
		case READ_ADDRESS:
		case BIT_NOT:
		case RIGHT_ARROW:
		case LEFT_ARROW:
		case DEFINE_SCOPE_ITERATIONS:
			return 8;
		default:
			return 0;
	}
}

static bool is_right_associative(int type) {
	return type == VALUE_ASSIGN || type == ADDRESS_ASSIGN;
}

static void consume_label_definition(TokenList* expression, LabelTable* labels, cell address) {
	while(expression->count >= 2 && expression->items[0].type == IDENT && expression->items[1].type == ADDRESS_ASSIGN) {
		set_label_address(labels, expression->items[0].text, address);
		if(expression->count == 2) {
			expression->count = 0;
			return;
		}
		memmove(expression->items, expression->items + 2, sizeof(ParsedToken) * (expression->count - 2));
		expression->count -= 2;
	}
}

// shunting yard, infix -> postfix, parens only exist in this stage.
static void to_postfix(const TokenList* expression, TokenList* postfix) {
	TokenList operators = {0};
	bool expect_operand = true;

	for(size_t i = 0; i < expression->count; i++) {
		ParsedToken token = expression->items[i];

		if(token.type == L_PAREN) {
			push_token(&operators, token);
			expect_operand = true;
			continue;
		}

		if(token.type == R_PAREN) {
			while(operators.count > 0 && peek_token(&operators).type != L_PAREN) {
				push_token(postfix, pop_token(&operators));
			}
			if(operators.count == 0) {
				fprintf(stderr, "[ERROR] Unmatched `)`\n");
				exit(1);
			}
			pop_token(&operators);
			expect_operand = false;
			continue;
		}

		if(token.type == MULTI && expect_operand) {
			token.type = DEREFERENCE;
		}

		// `~` takes no operands, so it goes straight to the output like an operand would
		if(is_operand_token(token.type) || is_postfix_operator(token.type) || token.type == INFINITE_LOOP) {
			// an operand right after a complete value starts a new sub-statement (e.g. the `D` in `$(C0) D : C1`),
			// so everything waiting for the previous value has to be emitted first
			if(is_operand_token(token.type) && !expect_operand) {
				while(operators.count > 0 && peek_token(&operators).type != L_PAREN) {
					push_token(postfix, pop_token(&operators));
				}
			}
			push_token(postfix, token);
			if(token.type != INFINITE_LOOP) expect_operand = false;
			continue;
		}

		if(token.type == DEFINE_DATA_MEM_SIZE) {
			compile_error("Must be the only thing in its statement (e.g. `#128;`):", token.type);
		}

		if(!is_binary_operator(token.type) && !is_prefix_operator(token.type)) {
			continue;
		}

		// a prefix operator that directly follows another operator (e.g. the `*` in `&*C0`) has no operand
		// yet, so it must not pop the operator before it, otherwise that operator ends up before its operand
		bool is_nested_prefix = is_prefix_operator(token.type) && expect_operand;
		while(!is_nested_prefix && operators.count > 0 && peek_token(&operators).type != L_PAREN) {
			ParsedToken top = peek_token(&operators);
			int top_precedence = operator_precedence(top.type);
			int current_precedence = operator_precedence(token.type);
			if(top_precedence > current_precedence || (top_precedence == current_precedence && !is_right_associative(token.type))) {
				push_token(postfix, pop_token(&operators));
			} else {
				break;
			}
		}

		push_token(&operators, token);
		expect_operand = true;
	}

	while(operators.count > 0) {
		ParsedToken token = pop_token(&operators);
		if(token.type == L_PAREN) {
			fprintf(stderr, "[ERROR] Unmatched `(`\n");
			exit(1);
		}
		push_token(postfix, token);
	}

	free(operators.items);
}

// a run of postfix tokens that produces one value on the stack
typedef struct {
	size_t start;
	// true if the fragment is a single variable, `D`, or `P`
	bool is_target;
} Fragment;

static Fragment pop_fragment(Fragment* fragments, size_t* depth, int operator_type) {
	if(*depth == 0) {
		compile_error("Missing operand for", operator_type);
	}
	return fragments[--(*depth)];
}

static void require_target(Fragment fragment, int operator_type) {
	if(!fragment.is_target) {
		compile_error("Expected a variable, `D`, or `P` as the target of", operator_type);
	}
}

static int mutation_access(int type) {
	switch(type) {
		case INC: return ACC_INC_VALUE;
		case DEC: return ACC_DEC_VALUE;
		case RIGHT_ARROW: return ACC_INC_ADDRESS;
		case LEFT_ARROW: return ACC_DEC_ADDRESS;
		default: return ACC_PUSH_VALUE;
	}
}

// fold the target of `=`, `:`, `&`, `++`, `--`, `->`, and `<-` into a single typed instruction.
// Ex: `C0 = C1 + 2` is `C0 C1 2 + =` in postfix and becomes `C1 2 + =C0`
static void resolve_targets(const TokenList* postfix, TokenList* out) {
	Fragment* fragments = malloc(sizeof(Fragment) * (postfix->count + 1));
	if(fragments == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate fragment stack\n");
		exit(1);
	}
	size_t depth = 0;

	for(size_t i = 0; i < postfix->count; i++) {
		ParsedToken token = postfix->items[i];
		token.access = ACC_PUSH_VALUE;

		if(is_operand_token(token.type)) {
			push_token(out, token);
			fragments[depth++] = (Fragment){ .start = out->count - 1, .is_target = is_target_token(token.type) };
			continue;
		}

		switch(token.type) {
			case READ_ADDRESS: {
				Fragment target = pop_fragment(fragments, &depth, token.type);
				require_target(target, token.type);
				out->items[target.start].access = ACC_PUSH_ADDRESS;
				fragments[depth++] = (Fragment){ .start = target.start, .is_target = false };
				break;
			}
			case INC:
			case DEC:
			case RIGHT_ARROW:
			case LEFT_ARROW: {
				Fragment target = pop_fragment(fragments, &depth, token.type);
				require_target(target, token.type);
				out->items[target.start].access = mutation_access(token.type);
				break;
			}
			case VALUE_ASSIGN:
			case ADDRESS_ASSIGN: {
				pop_fragment(fragments, &depth, token.type);
				Fragment target = pop_fragment(fragments, &depth, token.type);
				require_target(target, token.type);
				ParsedToken store = out->items[target.start];
				store.access = token.type == VALUE_ASSIGN ? ACC_SET_VALUE : ACC_SET_ADDRESS;
				// the target is always directly before the value, move it after the value as a store
				memmove(out->items + target.start, out->items + target.start + 1, sizeof(ParsedToken) * (out->count - target.start - 1));
				out->count--;
				push_token(out, store);
				break;
			}
			case INFINITE_LOOP:
				push_token(out, token);
				break;
			case DEREFERENCE:
			case BIT_NOT: {
				Fragment address = pop_fragment(fragments, &depth, token.type);
				push_token(out, token);
				fragments[depth++] = (Fragment){ .start = address.start, .is_target = false };
				break;
			}
			case DEFINE_SCOPE_ITERATIONS:
			case BREAK_FROM_SCOPE:
			case PRINT:
			case PRINT_LITERAL:
				pop_fragment(fragments, &depth, token.type);
				push_token(out, token);
				break;
			default: {
				pop_fragment(fragments, &depth, token.type);
				Fragment left = pop_fragment(fragments, &depth, token.type);
				push_token(out, token);
				fragments[depth++] = (Fragment){ .start = left.start, .is_target = false };
				break;
			}
		}
	}

	free(fragments);
}

static int operation_for_token(int type) {
	switch(type) {
		case DEFINE_SCOPE_ITERATIONS: return OP_DEFINE_SCOPE_ITERATIONS;
		case BREAK_FROM_SCOPE: return OP_BREAK_FROM_SCOPE;
		case INFINITE_LOOP: return OP_INFINITE_LOOP;
		case PRINT: return OP_PRINT;
		case PRINT_LITERAL: return OP_PRINT_LITERAL;
		case DEREFERENCE: return OP_DEREFERENCE;
		case PLUS: return OP_ADD;
		case MINUS: return OP_SUB;
		case MULTI: return OP_MUL;
		case DIV: return OP_DIV;
		case MODULO: return OP_MOD;
		case BIT_AND: return OP_BIT_AND;
		case BIT_OR: return OP_BIT_OR;
		case BIT_XOR: return OP_BIT_XOR;
		case BIT_NOT: return OP_BIT_NOT;
		case EQUIV: return OP_EQUAL;
		case NOT_EQUIV: return OP_NOT_EQUAL;
		case LESS: return OP_LESS;
		case LESS_EQUAL: return OP_LESS_EQUAL;
		case GREATER: return OP_GREATER;
		case GREATER_EQUAL: return OP_GREATER_EQUAL;
		default: return -1;
	}
}

// tokens -> memory cells
static void emit_tokens(const TokenList* tokens, LabelTable* labels, PendingLabelTable* pending, CellBuffer* code) {
	for(size_t i = 0; i < tokens->count; i++) {
		ParsedToken token = tokens->items[i];
		switch(token.type) {
			case INT_LITERAL:
				emit_cell(code, BC_TYPED(VT_LITERAL, ACC_PUSH_VALUE));
				emit_cell(code, token.value);
				break;
			case IDENT: {
				// labels compile to integer literals holding the label's address
				emit_cell(code, BC_TYPED(VT_LITERAL, ACC_PUSH_VALUE));
				size_t patch_index = code->count;
				emit_cell(code, 0);
				cell address = 0;
				if(find_label_address(labels, token.text, &address)) {
					code->items[patch_index] = address;
				} else {
					add_pending_label(pending, token.text, patch_index);
				}
				break;
			}
			case VARIABLE:
				emit_cell(code, BC_TYPED(VT_VARIABLE, token.access));
				emit_cell(code, token.value);
				break;
			case DATA_PTR_REF:
				emit_cell(code, BC_TYPED(VT_DATA_PTR, token.access));
				break;
			case PROG_PTR_REF:
				emit_cell(code, BC_TYPED(VT_PROG_PTR, token.access));
				break;
			default: {
				int operation = operation_for_token(token.type);
				if(operation >= 0) {
					emit_cell(code, (cell)operation);
				}
				break;
			}
		}
	}
}

static void flush_expression(TokenList* expression, LabelTable* labels, PendingLabelTable* pending, CellBuffer* code) {
	if(expression->count == 0) return;

	if(expression->items[0].type == DEFINE_DATA_MEM_SIZE) {
		if(expression->count != 2 || expression->items[1].type != INT_LITERAL) {
			compile_error("Expected a single integer literal after", DEFINE_DATA_MEM_SIZE);
		}
		cell size = expression->items[1].value;
		data_mem_size = size > 0 ? (size_t)size : 128;
		emit_cell(code, BC_TYPED(VT_META, META_DATA_MEM_SIZE));
		emit_cell(code, (cell)data_mem_size);
		expression->count = 0;
		return;
	}

	TokenList postfix = {0};
	TokenList resolved = {0};
	to_postfix(expression, &postfix);
	resolve_targets(&postfix, &resolved);
	emit_tokens(&resolved, labels, pending, code);

	free(postfix.items);
	free(resolved.items);
	expression->count = 0;
}

static cell read_number(const char* source, size_t code_len, size_t* index) {
	int64_t value = 0;
	while(*index < code_len && source[*index] >= '0' && source[*index] <= '9') {
		if(value <= INT32_MAX) {
			value = (value * 10) + (source[*index] - '0');
		}
		(*index)++;
	}
	if(value > INT32_MAX) {
		print_warning(" Integer literal is larger than 32 bits, clamping to %d\n", INT32_MAX);
		value = INT32_MAX;
	}
	return (cell)value;
}

static bool next_token_from_source(const char* source, size_t code_len, size_t* index, ParsedToken* token) {
	while(*index < code_len) {
		char ch = source[*index];
		if(ch == '/' && *index + 1 < code_len && source[*index + 1] == '/') {
			while(*index < code_len && source[*index] != '\n') {
				(*index)++;
			}
			continue;
		}
		if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
			(*index)++;
			continue;
		}
		break;
	}

	if(*index >= code_len) return false;

	char curr = source[*index];
	char next = *index + 1 < code_len ? source[*index + 1] : '\0';

	if(curr == 'C' && next >= '0' && next <= '9') {
		(*index)++;
		cell value = read_number(source, code_len, index);
		*token = (ParsedToken){ .type = VARIABLE, .value = value, .has_value = true, .text = {0} };
		return true;
	}

	if((curr >= '0' && curr <= '9') || (curr == '-' && next >= '0' && next <= '9')) {
		cell sign = 1;
		if(curr == '-') {
			sign = -1;
			(*index)++;
		}
		cell value = read_number(source, code_len, index);
		*token = (ParsedToken){ .type = INT_LITERAL, .value = value * sign, .has_value = true, .text = {0} };
		return true;
	}

	if(curr == 'D' && !((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z') || (next >= '0' && next <= '9') || next == '_')) {
		(*index)++;
		*token = (ParsedToken){ .type = DATA_PTR_REF, .value = 0, .has_value = false, .text = {0} };
		return true;
	}

	if(curr == 'P' && !((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z') || (next >= '0' && next <= '9') || next == '_')) {
		(*index)++;
		*token = (ParsedToken){ .type = PROG_PTR_REF, .value = 0, .has_value = false, .text = {0} };
		return true;
	}

	if((curr >= 'A' && curr <= 'Z') || (curr >= 'a' && curr <= 'z') || curr == '_') {
		char name[64] = {0};
		size_t name_len = 0;
		while(*index < code_len && ((source[*index] >= 'A' && source[*index] <= 'Z') || (source[*index] >= 'a' && source[*index] <= 'z') || (source[*index] >= '0' && source[*index] <= '9') || source[*index] == '_')) {
			if(name_len + 1 < sizeof(name)) {
				name[name_len++] = source[*index];
			}
			(*index)++;
		}
		name[name_len] = '\0';
		*token = (ParsedToken){ .type = IDENT, .value = 0, .has_value = false, .text = {0} };
		strncpy_s(token->text, sizeof(token->text), name, _TRUNCATE);
		return true;
	}

	if(curr == '+' && next == '+') {
		*index += 2;
		*token = (ParsedToken){ .type = INC, .value = 0, .has_value = false };
		return true;
	}
	if(curr == '-' && next == '-') {
		*index += 2;
		*token = (ParsedToken){ .type = DEC, .value = 0, .has_value = false };
		return true;
	}
	if(curr == '-' && next == '>') {
		*index += 2;
		*token = (ParsedToken){ .type = RIGHT_ARROW, .value = 0, .has_value = false };
		return true;
	}
	if(curr == '<' && next == '-') {
		*index += 2;
		*token = (ParsedToken){ .type = LEFT_ARROW, .value = 0, .has_value = false };
		return true;
	}
	if(curr == '=' && next == '=') {
		*index += 2;
		*token = (ParsedToken){ .type = EQUIV, .value = 0, .has_value = false };
		return true;
	}
	if(curr == '!' && next == '=') {
		*index += 2;
		*token = (ParsedToken){ .type = NOT_EQUIV, .value = 0, .has_value = false };
		return true;
	}
	if(curr == '<' && next == '=') {
		*index += 2;
		*token = (ParsedToken){ .type = LESS_EQUAL, .value = 0, .has_value = false };
		return true;
	}
	if(curr == '>' && next == '=') {
		*index += 2;
		*token = (ParsedToken){ .type = GREATER_EQUAL, .value = 0, .has_value = false };
		return true;
	}
    if(curr == '@' && next == '@') {
        *index += 2;
        *token = (ParsedToken){ .type = PRINT_LITERAL, .value = 0, .has_value = false };
        return true;
    }
	if(curr == '&' && next == '&') {
		*index += 2;
		*token = (ParsedToken){ .type = BIT_AND, .value = 0, .has_value = false };
		return true;
	}
	if(curr == '$' && next == '$') {
		*index += 2;
		*token = (ParsedToken){ .type = INFINITE_LOOP, .value = 0, .has_value = false };
		return true;
	}

	switch(curr) {
		case '=':
			(*index)++;
			*token = (ParsedToken){ .type = VALUE_ASSIGN, .value = 0, .has_value = false };
			return true;
		case ':':
			(*index)++;
			*token = (ParsedToken){ .type = ADDRESS_ASSIGN, .value = 0, .has_value = false };
			return true;
		case '+':
			(*index)++;
			*token = (ParsedToken){ .type = PLUS, .value = 0, .has_value = false };
			return true;
		case '-':
			(*index)++;
			*token = (ParsedToken){ .type = MINUS, .value = 0, .has_value = false };
			return true;
		case '*':
			(*index)++;
			*token = (ParsedToken){ .type = MULTI, .value = 0, .has_value = false };
			return true;
		case '/':
			(*index)++;
			*token = (ParsedToken){ .type = DIV, .value = 0, .has_value = false };
			return true;
		case '<':
			(*index)++;
			*token = (ParsedToken){ .type = LESS, .value = 0, .has_value = false };
			return true;
		case '>':
			(*index)++;
			*token = (ParsedToken){ .type = GREATER, .value = 0, .has_value = false };
			return true;
		case '&':
			(*index)++;
			*token = (ParsedToken){ .type = READ_ADDRESS, .value = 0, .has_value = false };
			return true;
		case '#':
			(*index)++;
			*token = (ParsedToken){ .type = DEFINE_DATA_MEM_SIZE, .value = 0, .has_value = false };
			return true;
		case ';':
			(*index)++;
			*token = (ParsedToken){ .type = EOS, .value = 0, .has_value = false };
			return true;
		case '[':
			(*index)++;
			*token = (ParsedToken){ .type = BEGIN_SCOPE, .value = 0, .has_value = false };
			return true;
		case ']':
			(*index)++;
			*token = (ParsedToken){ .type = END_SCOPE, .value = 0, .has_value = false };
			return true;
		case '$':
			(*index)++;
			*token = (ParsedToken){ .type = DEFINE_SCOPE_ITERATIONS, .value = 0, .has_value = false };
			return true;
		case '?':
			(*index)++;
			*token = (ParsedToken){ .type = BREAK_FROM_SCOPE, .value = 0, .has_value = false };
			return true;
		case '^':
			(*index)++;
			*token = (ParsedToken){ .type = BIT_XOR, .value = 0, .has_value = false };
			return true;
		case '~':
			(*index)++;
			*token = (ParsedToken){ .type = BIT_NOT, .value = 0, .has_value = false };
			return true;
		case '|':
			(*index)++;
			*token = (ParsedToken){ .type = BIT_OR, .value = 0, .has_value = false };
			return true;
		case '(':
			(*index)++;
			*token = (ParsedToken){ .type = L_PAREN, .value = 0, .has_value = false };
			return true;
		case ')':
			(*index)++;
			*token = (ParsedToken){ .type = R_PAREN, .value = 0, .has_value = false };
			return true;
        case '@':
            (*index)++;
            *token = (ParsedToken){ .type = PRINT, .value = 0, .has_value = false };
            return true;
		case '%':
			(*index)++;
			*token = (ParsedToken){ .type = MODULO, .value = 0, .has_value = false };
			return true;
		default:
			print_warning(" Skipping unknown token '%c' at index %zu\n", curr, *index);
			(*index)++;
			return next_token_from_source(source, code_len, index, token);
	}
}

u8* parse(bool verbose, FILE* source_file, size_t* out_len) {
	char* source_code;
	size_t code_len = 0;
	int current_char;

	size_t buf_size = 1024;
	source_code = malloc(sizeof(char) * buf_size);
	if(source_code == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate source buffer\n");
		exit(1);
	}

	while((current_char = fgetc(source_file)) != EOF) {
		if(code_len >= buf_size - 1) {
			buf_size *= 2;
			char* resized = realloc(source_code, sizeof(char) * buf_size);
			if(resized == NULL) {
				fprintf(stderr, "[ERROR] Failed to reallocate buffer\n");
				free(source_code);
				exit(1);
			}
			source_code = resized;
		}
		source_code[code_len++] = (char)current_char;
	}

	source_code[code_len] = '\0'; // null terminate

	size_t expanded_len = 0;
	char* expanded_source = expand_syntax_sugar(source_code, code_len, &expanded_len);
	free(source_code);
	source_code = expanded_source;
	code_len = expanded_len;

	print_debug(verbose, " Source code:\n%s\n", source_code);

	CellBuffer code = {0};
	TokenList expression = {0};
	LabelTable labels = {0};
	PendingLabelTable pending = {0};
	ParsedToken token;
	size_t index = 0;

	// begin parsing
	while(next_token_from_source(source_code, code_len, &index, &token)) {
		if(token.type == EOS) {
			consume_label_definition(&expression, &labels, (cell)code.count);
			if(expression.count > 0) {
				flush_expression(&expression, &labels, &pending, &code);
				emit_cell(&code, BC_EOS);
			}
			continue;
		}

		if(token.type == BEGIN_SCOPE || token.type == END_SCOPE) {
			consume_label_definition(&expression, &labels, (cell)code.count);
			flush_expression(&expression, &labels, &pending, &code);
			emit_cell(&code, token.type == BEGIN_SCOPE ? OP_BEGIN_SCOPE : OP_END_SCOPE);
			continue;
		}

		push_token(&expression, token);
	}

	consume_label_definition(&expression, &labels, (cell)code.count);
	flush_expression(&expression, &labels, &pending, &code);
	emit_cell(&code, BC_EOD);
	resolve_pending_labels(&labels, &pending, code.items);

	u8* bytecode = bc_encode(code.items, code.count, out_len);

	free(code.items);
	free(labels.items);
	free(pending.items);
	free(expression.items);
	fclose(source_file);
	free(source_code);

	return bytecode;
}
