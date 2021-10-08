# A Pathetic Instruction Set, a cut down Forth (third) and an emulator (emu)

I've wanted to build a very simple educational computer for decades, this is the archive of part of those dreams (there exists a more complete but somewhat messier archive [here](https://sites.google.com/site/drtonyrobinson/) and my current plans are at [The Blinking Computer](https://blinkingcomputer.org))

I planned on a 16 bit computer: instructions, memory, busses, everything was 16 bits wide.  My architecture had eight 'registers' and instructions were one of only three forms:

Form  | Example
----- | -------------
ALU   | A = B + C
READ  | A = *B, B +-= C
WRITE | *A = B, A +-= C

There were only five general purpose registers, two were constants and one was the program counters:

Register | Usage
-------- | -----
A 	     | general purpose
B	       | general purpose
C	       | general purpose ( Call stack)
D 	     | general purpose ( Data stack)
E 	     | general purpose ( Execution pointer)
Z	       | read Zero, write ignored
P	       | Program counter
I 	     | read 1 ( Identity), write Instruction
 
All instructions were conditional on three status bits:
 
mnemonic | bits | condition
-------- | ---- | ---------
nop      | 000  | never execute
&nbsp;	 | 001  | always execute
eq0      | 010  | last result == 0
ne0      | 011  | last result != 0
lt0      | 100  | last result < 0
le0      | 101  | last result <= 0
 
Finally, here are the eight ALU operations (note - no multiply!):
 
example       | descrition
------------- | ----------
A = B & C     | bitwise AND
A = B \| C    | bitwise OR
A = B ^ C	    | bitwise Exclusive OR
A = B >> 1	  | arithmetic shift right
A = B + C	    | ADD without carry
A = B + C + c	| ADD with carry
A = B - C	    | SUB without carry
A = B - C - c	| SUB with carry
 
And that's it.  Notably there is no control flow, you just write to the P register and the next instuction will be fetched from there.   There wasn't even a plan to build hardware to increment the program counter, it was just going to force execution of P = P + I between every instruction.

Somewhat suprisingly, this is a very nice sweetspot for compact and efficient Forth like envoronment.  Here are some of the fun things you can do efficiently:

example       | descrition
------------- | ----------
Z = A op B    | set the flags and discards the result by writing to Z
A = B         | a synonym for A = B \| Z, i.e. OR with zero is the identity operation
A = 1         | a synonym for A = I as I always reads as 1
A = *B        | LOAD, a synonym for A = *B, B += Z
*A = B        | STORE
A = *B++      | a synonym for *A = B, A += I i.e. POP with descending full stack
A = *P++      | load immediate.   P is incremented whether this instruction is run or not.
P = A         | JUMP to A
P = P + A     | branch relative by A
P = *P        | JUMP immediate


To call a subroutine is only two cycles (3 words) providing we can store the return address in E:

    E = P + I
    P = *P, P += I   # JUMP immediate
    :subroutineAddress

To return from a subroutine is only one cycle:

    P = E + I  # P is now the instruction after the address

If the subroutine calls another, or has any other use for the E register then it must push it onto the stack.

This gets us to the point at which we canwrite a Forth like langauge.

# Third

The processor needed a language that was higher level than assembler but was simple enough to understand in it's entirety (including the compiler) and yet with minimal overhead over assembly language.   I'm writing that language right now, it is called Third as it's a plausible prequel to Forth.   For a great introduction to this language see https://skilldrick.github.io/easyforth/.

The main advantage of Third and Forth is that code can be written as a list of subroutine addresses that are called functions (Forth calls them words).  Specifically, we use a direct threaded interpreter.   This is compact and efficient, all that has to be done to exit one function and start executing the next is to call the code:

    P = *E++  # execute the NEXT code
 
Functions may be written in assembler or Third.   If they are written in Third then an initial few lines of assembler are needed to start the Third interpreter on the upcoming list of function addresses.

    *C = E--      # docolon: Push execution pointer onto call stack
     E = P + I    # docolon: Execution pointer to start of labels
     P = *E++     # docolon: Execute function1
    :function1
    :function2
    :return

The last function address is that of return (Forth calls this exit).

The main differences between Forth and Third are:


Terminology:

Functions/subroutines are called functions (Forth calls these words)
The stacks are the data stack and the call stack (Forth calls these the stack and the return stack)
Functions that operate on multiwords are prefixed by the size, e.g. 4dup duplicates 4 words on the stack (e.g. a 64 bit number)
Functions consume their arguments by default (unless it's clear they don't like pick) unless they start with the letter q
Functions that start with the letter q query the stack and do not consume, e.g. q4signword leaves the 4 word on the stack and adds the sign word (executes pick(4))
Third may be called from assembler very efficiently:

    E = P + I          # docolon: Execution pointer to start of labels
    P = *E, E += I  # docolon: Execute first function
    :function1
    :function2
    :quit
    E = A + B         # whatever code you want to follow

Where quit is simply:

    quit:
    P = E

Of course,  you still have to save E on the call stack at some point (*C = E, C -= I) and retrieve it later if this code was called from Third and you want to return there.

# Tool chain

A typical run looks like this:

    ./third2asm.py sys.3rd myCode.3rd post.3rd | ./asm.py | ./txt2bin > tmp.bin
    ./emu tmp.bin

A third file, (extension .3rd) can contain both the third language and assembler.   The main system file is sys.3rd after which you can add your own code and it's followed by post.3rd

asm.py compiles the assembler to a text format that contains the address, the binary value of that address and the original code.  This format is very useful for seeing what the memory contents really mean.

txt2bin.c strips all the comments away and leaves just the binary form ready for execution.
 
emu.c runs the binary form.

The code contains unit tests written in comments, utest.sh runs all the unit tests.

# Some comments on the code

## Random numbers

The code contains a novel and very efficient implementation

## Floating point
This processor plnne on a 'f3' format for floating point which used one 16-bit word for the significand and sign and two 16-bit words for the fractional part.   The sign is stored as the lowest bit, which makes it easy to shift off leaving a 16 bit signed number for the significand.   In floating point arithmetic often the sign is dealt with first, leaving positive numbers (sign zero), and this means the significands can be added/subtracted without any shifting.   The fractional part always has the top bit set, this means that the normal 32 bit integer routines can be used.

