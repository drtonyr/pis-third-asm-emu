// gcc -Wall -o emu emu.c
#include <stdio.h>
#include <stdlib.h>

// address, instruction and data size
#define NTXT 21
#define OFFS 5
#define NBIT 16

int main(int argc, char *argv[]) {
  char txt[NTXT];   // hex address and binary instruction
  char *ctxt = txt + OFFS;
  unsigned short mem[1 << NBIT];
  int maxaddr = 0;
  
  while(fread(txt, sizeof(*txt), NTXT, stdin) == NTXT) {
    int addr;
    
    // check to see if this is an address instruction pair
    if(sscanf(txt, "%x", &addr) == 1) {
      int i, code = 0;

      // convert instruction to binary
      for(i = 0; i < NBIT && code != -1; i++) {
	if(ctxt[i] != '0' && ctxt[i] != '1')
	  code = -1;
	else if(ctxt[i] == '1')
	  code += 1 << (NBIT - 1 - i);
      }

      if(code != -1) {

	if(addr > maxaddr)
	  maxaddr = addr;
    
	mem[addr] = code;
      }
    }
    
    // skip to next line
    while(fgetc(stdin) != '\n');
  }

  // now write it all out (well, as far as maxaddr
  fwrite(mem, sizeof(*mem), maxaddr + 1, stdout);
  
  // and exit happy
  exit(EXIT_SUCCESS);
}
