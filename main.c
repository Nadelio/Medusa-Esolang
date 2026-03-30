#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <string_slice.h>

/// @brief Compares if to strings are equivalent
/// @param str_1 
/// @param str_2
bool streql(const char* str_1, const char* str_2) {
    return strcmp(str_1, str_2) == 0;
}

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
23 : $ (define number of iterations to run scope, following integer represents number of interations)
24 : ^ (break from loop)
25 : ~ (define infinite loop)
26 : # (data space allocation instruction)

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
C0 * 1 // mul
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
C0 == C1 [ -> P ] // if C0 is equal to C1, move the program pointer once to the right
*/

/*
Steps:
1. compile program to array of integers
2. malloc space to store program and to store additional working memory
    - defined by user in the command line args?
    - defined by user in program data using `#<mem_size>`?
3. Initialize program pointer and data pointer
4. Begin incrementing the program pointer and running the instructions
5. Continue until program reaches an end of program data instruction (-1)
*/

/*
Command line arguments:
--help              | -h            : print the help message
--verbose           | -v            : verbose information about parser and runtime states
--export            | -x            : export the binary for the program
--input <filename>  | -f <filename> : use the following file as a program
--output <filename> | -o <filename> : define the name for the exported program (defaults to program.bin)
*/

int main(int argc, char** argv) {

    bool verbose = false;

    bool use_existing_file = false;
    char* name_of_input_file;

    bool export_binary = false;
    char* name_of_output_file;

    for(int i = 1; i < argc; i++) {
        
    }

    return 0;
}