# Toy Lang - Esolang

Just a simple esolang written in C99 as practice.\
Not much else to it.

Its unique feature is that the program is compiled and loaded into the same memory space as your working data.\
This means that you can modify your program code at runtime!\
Although there it isn't very strongly supported, you just *can* do it.

Anyways, here are the different instructions:
## Meta Instructions
`#128` - Allocate 128 bytes for data space (only valid at the very beginning of a program)

## Variable Instructions
`C0 : 12` - set address of variable\
`C0 = 10` - set value of variable\
`C1 = &C0` - store the address of a variable
`C2 = *C1` - dereference a variable
`<- C0` - decrement address of a variable
`-> C0` - increment address of a variable

## System Pointer Instructions
`<- P` - move left program ptr\
`<- D` - move left data ptr\
`-> P` - move right p ptr\
`-> D` - move right d ptr\
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
`C0 / 1` - integer division

## Conditional Instructions
`C0 == C1` - is equal\
`C0 > C1` - is greater\
`C0 >= C1` - is greater or equal\
`C0 < C1` - is less\
`C0 <= C1` - is less or equal\
`C0 != C1` - is not

## Scopes and Control Flow Instructions
`[ $10 ... ]` - loop 10 times\
`[ ~ ... ]` - loop infinitely\
`[ ~ ... C0 != C1 ^ ]` - break from infinite loop if C0 is not equal to C1\
`C0 == C1 [ $1 -> P ]` - if C0 is equal to C1, move the program pointer once to the right
