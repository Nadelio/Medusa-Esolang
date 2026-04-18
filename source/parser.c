#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../include/parser.h"

typedef struct {
	i32 type;
	i32 value;
	bool has_value;
	char text[64];
} ParsedToken;

typedef struct {
	char name[64];
	i32 address;
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

static void append_source_int(char** buffer, size_t* length, size_t* capacity, i32 value) {
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

		if(source[i] == '%') {
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
					append_source_char(&expanded, &length, &capacity, '%');
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

static i32 find_label_address(const LabelTable* table, const char* name) {
	for(size_t i = 0; i < table->count; i++) {
		if(strcmp(table->items[i].name, name) == 0) {
			return table->items[i].address;
		}
	}
	return -1;
}

static void set_label_address(LabelTable* table, const char* name, i32 address) {
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

static void resolve_pending_labels(const LabelTable* labels, PendingLabelTable* pending, i32* compiled_code) {
	for(size_t i = 0; i < pending->count; i++) {
		i32 address = find_label_address(labels, pending->items[i].name);
		if(address < 0) {
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

static void push_token(TokenList* list, i32 type, i32 value, bool has_value) {
	reserve_token_list(list, 1);
	list->items[list->count++] = (ParsedToken){ .type = type, .value = value, .has_value = has_value, .text = {0} };
}

static void push_named_token(TokenList* list, i32 type, const char* text) {
	reserve_token_list(list, 1);
	ParsedToken token = { .type = type, .value = 0, .has_value = false, .text = {0} };
	strncpy_s(token.text, sizeof(token.text), text, _TRUNCATE);
	list->items[list->count++] = token;
}

static ParsedToken pop_token(TokenList* list) {
	if(list->count == 0) {
		return (ParsedToken){ .type = _NULL, .value = 0, .has_value = false, .text = {0} };
	}
	return list->items[--list->count];
}

static ParsedToken peek_token(TokenList* list) {
	if(list->count == 0) {
		return (ParsedToken){ .type = _NULL, .value = 0, .has_value = false, .text = {0} };
	}
	return list->items[list->count - 1];
}

static void ensure_compiled_capacity(i32** compiled_code, size_t* capacity, size_t write_index, size_t additional) {
	if(write_index + additional < *capacity) return;

	size_t new_capacity = *capacity == 0 ? 64 : *capacity;
	while(write_index + additional >= new_capacity) {
		new_capacity *= 2;
	}

	i32* resized = realloc(*compiled_code, sizeof(i32) * new_capacity);
	if(resized == NULL) {
		fprintf(stderr, "[ERROR] Failed to grow compiled code buffer\n");
		exit(1);
	}

	*compiled_code = resized;
	*capacity = new_capacity;
}

static void emit_compiled_value(i32** compiled_code, size_t* capacity, size_t* write_index, i32 value) {
	ensure_compiled_capacity(compiled_code, capacity, *write_index, 1);
	(*compiled_code)[(*write_index)++] = value;
}

static void emit_compiled_token(i32** compiled_code, size_t* capacity, size_t* write_index, ParsedToken token) {
	emit_compiled_value(compiled_code, capacity, write_index, token.type);
	if(token.has_value) {
		emit_compiled_value(compiled_code, capacity, write_index, token.value);
	}
}

static bool is_operand_token(i32 type) {
	switch(type) {
		case VARIABLE:
		case DATA_PTR_REF:
		case PROG_PTR_REF:
		case INT_LITERAL:
			return true;
		default:
			return false;
	}
}

static bool is_binary_operator(i32 type) {
	switch(type) {
		case ASSIGN:
		case ADDRESS_ASSIGN:
		case MULTI:
		case DIV:
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

static bool is_prefix_operator(i32 type) {
	switch(type) {
		case DEREF:
		case DECLARE_REF:
		case RIGHT_ARROW:
		case LEFT_ARROW:
		case DEFINE_DATA_MEM_SIZE:
		case DEFINE_SCOPE_ITERATIONS:
		case INFINITE_LOOP:
        case PRINT:
        case PRINT_LITERAL:
		case BREAK_FROM_SCOPE:
			return true;
		default:
			return false;
	}
}

static bool is_postfix_operator(i32 type) {
	return type == INC || type == DEC;
}

static int operator_precedence(i32 type) {
	switch(type) {
		case ASSIGN:
		case ADDRESS_ASSIGN:
			return 1;
		case EQUIV:
		case NOT_EQUIV:
		case LESS:
		case GREATER:
		case LESS_EQUAL:
		case GREATER_EQUAL:
			return 2;
		case PLUS:
		case MINUS:
			return 3;
		case MULTI:
		case DIV:
			return 4;
		case DEREF:
		case DECLARE_REF:
		case RIGHT_ARROW:
		case LEFT_ARROW:
		case DEFINE_DATA_MEM_SIZE:
		case DEFINE_SCOPE_ITERATIONS:
		case INFINITE_LOOP:
		case BREAK_FROM_SCOPE:
			return 5;
		default:
			return 0;
	}
}

static bool is_right_associative(i32 type) {
	return type == ASSIGN || type == ADDRESS_ASSIGN;
}

static void consume_label_definition(TokenList* expression, LabelTable* labels, size_t write_index) {
	while(expression->count >= 2 && expression->items[0].type == IDENT && expression->items[1].type == ADDRESS_ASSIGN) {
		set_label_address(labels, expression->items[0].text, (i32)write_index);
		if(expression->count == 2) {
			expression->count = 0;
			return;
		}
		memmove(expression->items, expression->items + 2, sizeof(ParsedToken) * (expression->count - 2));
		expression->count -= 2;
	}
}

static void flush_expression(TokenList* expression, LabelTable* labels, PendingLabelTable* pending, i32** compiled_code, size_t* compiled_capacity, size_t* write_index) {
	if(expression->count == 0) return;

	if(expression->count >= 2 && expression->items[0].type == DEFINE_DATA_MEM_SIZE && expression->items[1].type == INT_LITERAL) {
		data_mem_size = expression->items[1].value > 0 ? (size_t)expression->items[1].value : 128;
	}

	TokenList operators = {0};
	bool expect_operand = true;

	for(size_t i = 0; i < expression->count; i++) {
		ParsedToken token = expression->items[i];

		if(token.type == L_PAREN) {
			push_token(&operators, token.type, token.value, token.has_value);
			expect_operand = true;
			continue;
		}

		if(token.type == R_PAREN) {
			while(operators.count > 0 && peek_token(&operators).type != L_PAREN) {
				emit_compiled_token(compiled_code, compiled_capacity, write_index, pop_token(&operators));
			}
			if(operators.count > 0 && peek_token(&operators).type == L_PAREN) {
				pop_token(&operators);
			}
			expect_operand = false;
			continue;
		}

		if(token.type == MULTI && expect_operand) {
			token.type = DEREF;
		}

		if(token.type == IDENT) {
			emit_compiled_value(compiled_code, compiled_capacity, write_index, INT_LITERAL);
			size_t patch_index = *write_index;
			emit_compiled_value(compiled_code, compiled_capacity, write_index, 0);
			i32 address = find_label_address(labels, token.text);
			if(address >= 0) {
				(*compiled_code)[patch_index] = address;
			} else {
				add_pending_label(pending, token.text, patch_index);
			}
			expect_operand = false;
			continue;
		}

		if(is_operand_token(token.type)) {
			emit_compiled_token(compiled_code, compiled_capacity, write_index, token);
			expect_operand = false;
			continue;
		}

		if(is_postfix_operator(token.type)) {
			emit_compiled_value(compiled_code, compiled_capacity, write_index, token.type);
			expect_operand = false;
			continue;
		}

		if(!is_binary_operator(token.type) && !is_prefix_operator(token.type)) {
			continue;
		}

		while(operators.count > 0 && peek_token(&operators).type != L_PAREN) {
			ParsedToken top = peek_token(&operators);
			int top_precedence = operator_precedence(top.type);
			int current_precedence = operator_precedence(token.type);
			if(top_precedence > current_precedence || (top_precedence == current_precedence && !is_right_associative(token.type))) {
				emit_compiled_token(compiled_code, compiled_capacity, write_index, pop_token(&operators));
			} else {
				break;
			}
		}

		push_token(&operators, token.type, token.value, token.has_value);
		expect_operand = true;
	}

	while(operators.count > 0) {
		ParsedToken token = pop_token(&operators);
		if(token.type != L_PAREN && token.type != R_PAREN) {
			emit_compiled_token(compiled_code, compiled_capacity, write_index, token);
		}
	}

	free(operators.items);
	expression->count = 0;
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
		i32 value = 0;
		while(*index < code_len && source[*index] >= '0' && source[*index] <= '9') {
			value = (value * 10) + (source[*index] - '0');
			(*index)++;
		}
		*token = (ParsedToken){ .type = VARIABLE, .value = value, .has_value = true, .text = {0} };
		return true;
	}

	if((curr >= '0' && curr <= '9') || (curr == '-' && next >= '0' && next <= '9')) {
		i32 sign = 1;
		if(curr == '-') {
			sign = -1;
			(*index)++;
		}
		i32 value = 0;
		while(*index < code_len && source[*index] >= '0' && source[*index] <= '9') {
			value = (value * 10) + (source[*index] - '0');
			(*index)++;
		}
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
    if(curr == '%' && next == '%') {
        *index += 2;
        *token = (ParsedToken){ .type = PRINT_LITERAL, .value = 0, .has_value = false };
        return true;
    }

	switch(curr) {
		case '=':
			(*index)++;
			*token = (ParsedToken){ .type = ASSIGN, .value = 0, .has_value = false };
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
			*token = (ParsedToken){ .type = DECLARE_REF, .value = 0, .has_value = false };
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
		case '^':
			(*index)++;
			*token = (ParsedToken){ .type = BREAK_FROM_SCOPE, .value = 0, .has_value = false };
			return true;
		case '~':
			(*index)++;
			*token = (ParsedToken){ .type = INFINITE_LOOP, .value = 0, .has_value = false };
			return true;
		case '(':
			(*index)++;
			*token = (ParsedToken){ .type = R_PAREN, .value = 0, .has_value = false };
			return true;
		case ')':
			(*index)++;
			*token = (ParsedToken){ .type = L_PAREN, .value = 0, .has_value = false };
			return true;
        case '%':
            (*index)++;
            *token = (ParsedToken){ .type = PRINT, .value = 0, .has_value = false };
            return true;
		default:
			print_warning(" Skipping unknown token '%c' at index %zu\n", curr, *index);
			(*index)++;
			return next_token_from_source(source, code_len, index, token);
	}
}

i32* parse(bool verbose, FILE* source_file) {
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

	size_t compiled_capacity = code_len > 0 ? code_len * 4 : 64;
	i32* compiled_code = malloc(sizeof(i32) * compiled_capacity);
	if(compiled_code == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate memory for compiled code\n");
		free(source_code);
		exit(1);
	}

	size_t write_index = 0;
	TokenList expression = {0};
	LabelTable labels = {0};
	PendingLabelTable pending = {0};
	ParsedToken token;
	size_t index = 0;

	// begin parsing
	while(next_token_from_source(source_code, code_len, &index, &token)) {
		if(token.type == EOS) {
			consume_label_definition(&expression, &labels, write_index);
			if(expression.count > 0) {
				flush_expression(&expression, &labels, &pending, &compiled_code, &compiled_capacity, &write_index);
				emit_compiled_value(&compiled_code, &compiled_capacity, &write_index, EOS);
			}
			continue;
		}

		if(token.type == BEGIN_SCOPE || token.type == END_SCOPE) {
			consume_label_definition(&expression, &labels, write_index);
			flush_expression(&expression, &labels, &pending, &compiled_code, &compiled_capacity, &write_index);
			emit_compiled_value(&compiled_code, &compiled_capacity, &write_index, token.type);
			continue;
		}

		if(token.type == IDENT) {
			push_named_token(&expression, token.type, token.text);
		} else {
			push_token(&expression, token.type, token.value, token.has_value);
		}
	}

	consume_label_definition(&expression, &labels, write_index);
	flush_expression(&expression, &labels, &pending, &compiled_code, &compiled_capacity, &write_index);
	emit_compiled_value(&compiled_code, &compiled_capacity, &write_index, EOD);
	resolve_pending_labels(&labels, &pending, compiled_code);

	free(labels.items);
	free(pending.items);
	free(expression.items);
	fclose(source_file);
	free(source_code);

	return compiled_code;
}

