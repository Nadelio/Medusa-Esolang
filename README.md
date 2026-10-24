# Medusa - Esolang

Just a simple esolang written in C99 as practice.\
Not much else to it.

Its unique feature is that the program is compiled and loaded into the same memory space as your working data.\
This means that you can modify your program code at runtime!\
Although it isn't very strongly supported, you just *can* do it.

The parser compiles from infix to prefix using shunting yard.\
Then the interpreter is a stack machine.

<p align="center">
  <img src="assets/hello_world.png" />
</p>
<p align="center">
  <img src="assets/modify.png" />
</p>
<p align="center">
  <img src="assets/syntax_demo.png" />
</p>

## Meta Instructions
`#128` - Allocate 128 cells for data space (only valid at the very beginning of a program)

## Variable Instructions
`C0 : 12` - set address of variable\
`C0 = 10` - set value of variable\
`C1 = &C0` - store the address of a variable\
`C2 = *C1` - dereference a variable\
`<- C0` - decrement address of a variable\
`-> C0` - increment address of a variable

## System Pointer Instructions
`<- P` - move program ptr left \
`<- D` - move data ptr left\
`-> P` - move program ptr right\
`-> D` - move data ptr right\
`P : 5` - move program ptr\
`D : 12` - move data ptr\
`P = 9` - set value at program ptr\
`D = 12` - set value at data ptr

## Arithmetic Instructions
`C0 ++` - inc\
`C0 --` - dec\
`C0 + 12` - add\
`C0 - 12` - sub\
`C0 * 1` - multiplication\
`C0 / 1` - integer division\
`C0 % 3` - modulo (remainder)

## Bitwise Instructions
`C0 && C1` - bitwise and\
`C0 | C1` - bitwise or\
`C0 ^ C1` - bitwise xor\
`~C0` - bitwise not (flips every bit)

> [!note]
> Bitwise operators bind tighter than comparisons (`C0 && 1 == 1` means `(C0 && 1) == 1`), but looser than `+` and `-`. From loosest to tightest: `|`, `^`, `&&`.

## Conditional Instructions
`C0 == C1` - is equal\
`C0 > C1` - is greater\
`C0 >= C1` - is greater or equal\
`C0 < C1` - is less\
`C0 <= C1` - is less or equal\
`C0 != C1` - is not

## Scopes and Control Flow Instructions
`[ $10 ... ]` - loop 10 times\
`[ $$ ... ]` - loop infinitely\
`[ $$ ... C0 != C1 ? ]` - break from infinite loop if C0 is not equal to C1\
`C0 == C1 [ $1 -> P ]` - if C0 is equal to C1, move the program pointer once to the right\
`FOO:` - create label\
`P : FOO` - jump to label

## Miscellaneous Instructions
`@ ...` - print the following integer as an ascii character\
`@@ ...` - print the following integer\
`// ...` - comments (these are stripped from the final compiled program, obviously)\
`'a'` - a single character (syntax sugar, compiles to an `I...` instruction)\
`@"..."` - prints a string (syntax sugar, compiles to repeated `@I...` instructions)\
`D = "..."` - writes a string to memory starting at where the data pointer is currently (syntax sugar, compiles to repeated `D = I...; -> D;` instructions)

> [!note]
> - `@D` prints the value currently under the data pointer.
> - `*D` dereferences the address stored in the current data cell.
> - Values, variables, and memory cells are 32-bit signed integers, and every address counts cells.
> - Integers compile to TWO cells in memory, the `I` instruction and then the integer (1, 2, or 4 bytes in the exported binary, see [BYTECODE_STANDARD.md](BYTECODE_STANDARD.md)).
> - The interpreter is a stack machine.
> - The compiler/parser converts infix expressions to postfix.
> - Variables are automatically created immediately after the program data ends in memory
> - Addresses begin at the start of program data

## Turing Completeness

One Instruction Set Computer
```c
process:
    // initialize variables
    C0 = *D;
    -> D;
    C1 = *D;
    -> D;
    C2 = *D;
    <- D; <- D; // reset data ptr position

    // SUBLEQZ
    C1 = C1 - C0;
    [ $(C1 <= 0) D : C2 ]

    // loop
    P : process;
```

