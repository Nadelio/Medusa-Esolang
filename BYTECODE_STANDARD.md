# Medusa Bytecode Standard

## Parser Tokens
| Identifier                | Notes |
| ------------------------- | ----- |
| `EOD`                     | This marks the end of the program data in memory |
| `_NULL`                   | Can be used for padding, or as a NOP |
| `INT_LITERAL`             | A 32-bit signed integer literal |
| `IDENT`                   | Label name, compiles to an integer literal holding the label's address |
| `VARIABLE`                |
| `DATA_PTR_REF`            |
| `PROG_PTR_REF`            |
| `VALUE_ASSIGN`            | Assign/reassign value of a variable |
| `ADDRESS_ASSIGN`          | Assign/reassign address of a variable |
| `READ_ADDRESS`            | Used to store the address of a variable instead of the value |
| `DEREFERENCE`             | In Medusa, this is the `*` symbol, which is shared with the `MULTI` opcode |
| `MULTI`                   | In Medusa, this is the `*` symbol, which is shared with the `DEREF` opcode |
| `DIV`                     | Integer division, Medusa doesn't support floats natively |
| `MODULO`                  | Remainder, `%` |
| `BIT_AND`                 | Bitwise and, `&&` |
| `BIT_OR`                  | Bitwise or, `\|` |
| `BIT_XOR`                 | Bitwise xor, `^` |
| `BIT_NOT`                 | Bitwise not, `~` (prefix) |
| `PLUS`                    | Addition |
| `INC`                     | Increment |
| `MINUS`                   | Subtraction |
| `DEC`                     | Decrement |
| `EQUIV`                   | Checks for equivalence between two values/variables |
| `NOT_EQUIV`               | Checks if two values/variables are NOT equivalent |
| `LESS`                    |
| `GREATER`                 |
| `LESS_EQUAL`              |
| `GREATER_EQUAL`           |
| `MOVE_RIGHT`              | Increments a variable's address |
| `MOVE_LEFT`               | Decrements a variable's address |
| `BEGIN_SCOPE`             | Marks the beginning of a scope |
| `END_SCOPE`               | Marks the end of a scope |
| `DEFINE_SCOPE_ITERATIONS` | `$` |
| `BREAK_FROM_SCOPE`        | `?` |
| `INFINITE_LOOP`           | `$$` |
| `L_PAREN`                 | Only used by the parser for grouping, never emitted into bytecode |
| `R_PAREN`                 | Only used by the parser for grouping, never emitted into bytecode |
| `PRINT`                   | `@`, prints a value as an ASCII character |
| `PRINT_LITERAL`           | `@@`, prints a value as a number |
| `DEFINE_DATA_MEM_SIZE`    | This is a meta instruction that defines how many cells to allocate for the interpreter's data section (if this is excluded, default to 128 cells) |
| `EOS`                     | End of statement, clears the runtime stack |

## Memory Model
Memory is an array of 32-bit signed *cells*. The program is loaded at address `0`, and the data section starts immediately after it.
Every address in Medusa (labels, `&C0`, `D : 12`, `P : 5`, etc.) counts cells, not bytes.

When a program is loaded, every instruction takes one cell, and an instruction with an operand takes one extra cell holding the full 32-bit operand.
This means `C0 = 1000000;` takes the same number of cells as `C0 = 1;`, which keeps label and self-modifying code offsets easy to calculate.

## Binary Format
The binary format (what `--export` writes) is a stream of bytes. Each instruction byte is split up into a 3-bit *value type* and 5 bits that depend on the type:
```
typed:   [TTT][WW][AAA]   value type, operand width, access
untyped: [000][OOOOO]     operation
```

| Binary | Value Type | Operand | Notes |
|--------|------------|---------|-------|
| `[000][OOOOO]` | Operation | none | Instructions that only work on the stack, see [Operations](#operations) |
| `[001][WW][AAA]` | Integer literal | value | Only *push value* is valid |
| `[010][00][AAA]` | Data pointer (`D`) | none | `D` is a global, so it doesn't need an operand |
| `[011][00][AAA]` | Program pointer (`P`) | none | `P` is a global, so it doesn't need an operand |
| `[100][WW][AAA]` | Variable | variable index | e.g. `C12` has the index `12` |
| `[101][WW][AAA]` | Meta | value | See [Meta](#meta) |
| `[110][00000]` | End of statement (`EOS`) | none | Clears the runtime stack |
| `[111][00000]` | End of data (`EOD`) | none | Marks the end of the program |

### Operand Width
Literals, variables, and meta instructions are followed by an operand. The `WW` bits say how many bytes the operand takes.
Operands are signed little-endian two's complement, and 1 and 2 byte operands are sign-extended to 32 bits when loaded.
The compiler always picks the smallest width that fits the value.

| `WW` | Operand Size | Range |
|------|--------------|-------|
| `00` | 1 byte | `-128` to `127` |
| `01` | 2 bytes | `-32768` to `32767` |
| `10` | 4 bytes | `-2147483648` to `2147483647` |
| `11` | Reserved | Rejected by the loader (room for 8 byte values later) |

`D` and `P` have no operand, so their width bits are always `00`.
Once loaded into memory, the operand always occupies exactly one cell, and the width bits are cleared to `00`. Self-modifying code only ever sees `WW = 00`.

### Access
The lowest 3 bits of a typed instruction say what to do with the target.
Only the *target* of an instruction is typed, so operators like `+` never need to know what produced their operands.

| `AAA` | Access | Stack Effect | Source Example |
|-------|--------|--------------|----------------|
| `000` | Push value | `-> value` | `C0`, `D`, `5` |
| `001` | Push address | `-> address` | `&C0`, `&D` |
| `010` | Set value | `value ->` | `C0 = ...` |
| `011` | Set address | `address ->` | `C0 : ...`, `P : label` |
| `100` | Increment value | none | `C0++` |
| `101` | Decrement value | none | `C0--` |
| `110` | Increment address | none | `-> C0` |
| `111` | Decrement address | none | `<- C0` |

`P` always refers to the instruction currently being executed. So `P = 9` overwrites the current instruction, `P : 5` jumps to address `5`, and `-> P` skips the next cell.

### Meta
| Binary | Meta Instruction | Notes |
|--------|------------------|-------|
| `[101][WW][000]` | `DEFINE_DATA_MEM_SIZE` | The operand is the size of the data section in cells, a NOP at runtime |

### Operations
Operations pop their operands off the stack, and binary operations pop the right-hand side first.

| Binary | Operation | Stack Effect | Notes |
|--------|-----------|--------------|-------|
| `[000][00000]` | `NOP` | none | |
| `[000][00001]` | `BEGIN_SCOPE` | `[condition] ->` | Pops a condition if the stack isn't empty, skips the scope if it is `0` |
| `[000][00010]` | `END_SCOPE` | none | |
| `[000][00011]` | `DEFINE_SCOPE_ITERATIONS` | `count ->` | |
| `[000][00100]` | `BREAK_FROM_SCOPE` | `condition ->` | |
| `[000][00101]` | `INFINITE_LOOP` | none | |
| `[000][00110]` | `PRINT` | `value ->` | Prints the lowest byte as an ASCII character |
| `[000][00111]` | `PRINT_LITERAL` | `value ->` | Prints the value as a number |
| `[000][01000]` | `DEREFERENCE` | `address -> value` | |
| `[000][01001]` | `ADD` | `left right -> result` | Wraps on overflow |
| `[000][01010]` | `SUB` | `left right -> result` | Wraps on overflow |
| `[000][01011]` | `MUL` | `left right -> result` | Wraps on overflow |
| `[000][01100]` | `DIV` | `left right -> result` | Dividing by `0` gives `0` |
| `[000][01101]` | `MOD` | `left right -> result` | Modulo by `0` gives `0` |
| `[000][01110]` | `EQUAL` | `left right -> 0/1` | |
| `[000][01111]` | `NOT_EQUAL` | `left right -> 0/1` | |
| `[000][10000]` | `LESS` | `left right -> 0/1` | |
| `[000][10001]` | `LESS_EQUAL` | `left right -> 0/1` | |
| `[000][10010]` | `GREATER` | `left right -> 0/1` | |
| `[000][10011]` | `GREATER_EQUAL` | `left right -> 0/1` | |
| `[000][10100]` | `BIT_AND` | `left right -> result` | |
| `[000][10101]` | `BIT_OR` | `left right -> result` | |
| `[000][10110]` | `BIT_XOR` | `left right -> result` | |
| `[000][10111]` | `BIT_NOT` | `value -> result` | |

`[000][11000]` to `[000][11111]` are unused. Any unused, invalid, or out of byte range cell is treated as a `NOP`.

## Compiling Expressions
Expressions are compiled in three stages:
1. **Shunting yard**: converts infix to postfix. Parentheses only exist in this stage, they are used to order the postfix output and are then thrown away. Unmatched parentheses are a compile error.
2. **Target resolution**: the operand of `=`, `:`, `&`, `++`, `--`, `->`, and `<-` is folded into a single typed instruction. The operand must be a variable, `D`, or `P`, anything else (e.g. `5 = 3` or `&(C0 + 1)`) is a compile error.
3. **Emission**: tokens are written out as cells, then encoded into bytes.

Operator precedence, from loosest to tightest:
| Precedence | Operators | Associativity |
|------------|-----------|---------------|
| 1 | `=` `:` `?` | right (`=`, `:`) |
| 2 | `==` `!=` `<` `<=` `>` `>=` | left |
| 3 | `\|` | left |
| 4 | `^` | left |
| 5 | `&&` | left |
| 6 | `+` `-` | left |
| 7 | `*` `/` `%` | left |
| 8 | prefix `*` `&` `~` `->` `<-` `$`, postfix `++` `--` | |

For example:
```c
C0 = (C1 + 2) * C2;
```
| Stage | Output |
|-------|--------|
| Postfix | `C0 C1 2 + C2 * =` |
| Targets resolved | `C1 2 + C2 * =C0` |
| Bytes | `[100][00][000] 01`, `[001][00][000] 02`, `[000][01001]`, `[100][00][000] 02`, `[000][01011]`, `[100][00][010] 00`, `[110][00000]` |

Note that `$(C1 <= 0)` compiles to `C1 0 <= $`, while `$C1 <= 0` is a compile error: `$` binds tighter than `<=`, so it becomes `C1 $ 0 <=` and `<=` is missing an operand.
