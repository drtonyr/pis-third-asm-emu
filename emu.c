// gcc -O2 -o emu emu.c -lm
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "os.h"

// address, instruction and data size
#define NBIT 16
#define MSIZE (1 << NBIT)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// note we store everything little endian, so the high order bits are last
typedef struct {
  unsigned int Rz  : 3;
  unsigned int Ry  : 3;
  unsigned int Rx  : 3;
  unsigned int op  : 4;  
  unsigned int cnd : 3;
} instType;

const char *scnd[] = {"", "cin", "eq0", "ne0", "lt0", "le0", "ge0", "gt0"};
const char *sop[] = {"&", "|", "^", ">>", "+", "+c", "-", "-c"};

double timeInit, timeDiff;
int ninst = 0;

double dtime(void) {
  // #include <sys/timeb.h>
  // struct timeb time;
  // ftime(&time);
  // return(time.time + 0.001 * time.millitm);
  return clock() / CLOCKS_PER_SEC;
}

void OScall(unsigned short *m) {
  switch(m[0]) {

  case OSexit:
    timeDiff = dtime() - timeInit;
    if(timeDiff > 0.01)
      fprintf(stderr, "ninst: %d\ttime: %f\t inst/s: %f\n", ninst, timeDiff, ninst / timeDiff);
    exit(m[1]);
    break;

  case OSgetw:
    m[2] = fread(&m[1], sizeof(short), 1, stdin);
    break;

  case OSputw:
    m[2] = fwrite(&m[1], sizeof(short), 1, stdout);
    break;

  case OSgetc:
    m[1] = fgetc(stdin);
    break;

  case OSputc:
    fputc(m[1], stdout);
    break;

  case OSf2put: {
    unsigned int f = (m[1] << 16) | m[2];
    fprintf(stdout, "%f", *((float*) &f));
    } break;

  case OSf3put: {
    double f = (((long long) m[1] << 16) | m[2]) * pow(2, ((short) m[3] >> 1) - 31);
    if(m[3] & 1)
      f = -f;
    fprintf(stdout, "%g", f);
    } break;

  // something has gone wrong
  default:
    fprintf(stderr, "ERROR, undefined OScall: %d\n", m[0]);
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {
  unsigned short r[8]; // registers
  int carry;  // flag for carry
  int eq0;    // flag for equal-zero
  int lt0;    // flag for less-than-zero (top bit set)
  const int z = 5; // zero register
  const int p = 6; // program counter register
  const int i = 7; // identity and instruction register
  const char *reg = "ABCDEZPI";
  int debug = 0;
  
  unsigned short m[MSIZE]; // main memory
  
  if(argc != 2 && argc != 3) {
    fprintf(stderr, "usage: emu [-debug] mem-map\n");
    exit(EXIT_FAILURE);
  }

  if(argc != 2)
    debug = 1;
  
  setbuf(stdout, NULL);
  
  // read file into memory
  {
    size_t nshort = 0;
    FILE* stream = fopen(argv[argc - 1], "r");
    
    if(stream == NULL) {
      fprintf(stderr, "emu: failed to open: %s\n", argv[1]);
      exit(EXIT_FAILURE);
    }

    // read file
    int nread;
    while((nread = fread(m + nshort, sizeof(short), MIN(BUFSIZ, MSIZE - nshort), stream)) != 0)
      nshort += nread;
    
    fclose(stream);
  }

  // emulate the processor
  r[i] = 1;
  r[p] = 0;
  r[z] = 0;
  // loop forever until we read OSexit which calls exit()
  timeInit = dtime();
  while(1) {
    // fetch a new instruction
    instType inst = *((instType*) &m[r[p]++]);
    int exec = 0, useCarry = 0;

    if(debug) {
      if(inst.op & (1 << 3))
	if(inst.op & (1 << 2))
	  printf("%3s *%c = %c, %c %c= %c\t", scnd[inst.cnd], reg[inst.Rz], reg[inst.Rx], reg[inst.Rz], (inst.op & (1 << 0)) ? '+' : '-', reg[inst.Ry]);
	else
	  printf("%3s %c = *%c, %c %c= %c\t", scnd[inst.cnd], reg[inst.Rx], reg[inst.Rz], reg[inst.Rz], (inst.op & (1 << 0)) ? '+' : '-', reg[inst.Ry]);
       else
	printf("%3s %c = %c %s %c\t\t", scnd[inst.cnd], reg[inst.Rz], reg[inst.Rx], sop[inst.op], reg[inst.Ry]);
    }
    
    switch(inst.cnd) {
    case 0:
      exec = 1; // always execute ignoring carry
      break;
    case 1:
      exec = 1; // cin - always execute using carry
      useCarry = 1;
      break;
    case 2:
      exec = eq0;  // eq0
      break;
    case 3:
      exec = !eq0; // ne0
      break;
    case 4:
      exec = lt0;  // lt0
      break;
    case 5:
      exec = lt0 || eq0; // le0
      break;
    case 6:
      exec = !lt0;         // ge0 
      break;
    case 7:
      exec = !lt0 && !eq0; // gt0
      break;
    }

    if(exec) {
      if(inst.op & (1 << 3)) {
	// have a stack operation
	if(inst.op & (1 << 2)) {
	  // write/push: *Rz = Rx, Rz +-= Ry
	  m[(unsigned short) r[inst.Rz]] = r[inst.Rx];

	  // trap for OScall
	  if(r[inst.Rz] == 0)
	    OScall(m);   // I know it's 16 bits, UTF-16 comes later
	} else {
	  r[inst.Rx] = m[(unsigned short) r[inst.Rz]];
	}
      } else {
	// have a full ALU operation
	// these set flags so compute and store result, then set flags
	int res; // storage for result
	
	switch(inst.op) {
	case 0: // AND
	  res = r[inst.Rx] & r[inst.Ry];
	  break;
	case 1: // OR
	  res = r[inst.Rx] | r[inst.Ry];
	  break;
	case 2: // XOR
	  res = r[inst.Rx] ^ r[inst.Ry];
	  break;
	case 3: // SR
	  res = (r[inst.Rx] & 0xffff) >> 1;
	  if(useCarry)
	    res |= (carry << 15);
	  carry = r[inst.Rx] & 1;  // carry is lowest bit
	  break;
	case 4: // ADD
	  res = r[inst.Rx] + r[inst.Ry];
	  if(useCarry)
	    res += carry;
	  carry = (res & (1 << 16)) != 0;
	  break;
	case 5:
	  exit(1);
	  break;
	case 6: // SUB
	  if(useCarry)
	    res = r[inst.Rx] + (~ r[inst.Ry]) + 1 - carry;
	  else
	    res = r[inst.Rx] + (~ r[inst.Ry]) + 1;
	  carry = (res & (1 << 16)) != 0;
	  break;
	case 7:
	  exit(1);
	  break;
	}

	// set the conditional flags for the next instruction
	if(inst.cnd == 0 || inst.cnd == 1) {
	  eq0 = (res == 0);
	  lt0 = ((res & (1 << 15)) != 0);
	}

	// write back result
	if(inst.Rz != z)
	  r[inst.Rz] = res;
      }
    }

    // do inc/dec
    if(inst.Rz != z && inst.op & (1 << 3)) {
      // fails on inst: 1  9 6 7 3   P = *D++
      if((exec && (inst.Rx != p || inst.Rz != p)) || (!exec && inst.Rz == p)) {
	if(inst.op & (1 << 0))
	  r[inst.Rz] += r[inst.Ry];
	else {
	  r[inst.Rz] -= r[inst.Ry];
	}
      }
    }

    // if debug then print state
    if(debug) {
      int n;

      // print flags
      printf("Z%dN%dC%d ", eq0, lt0, carry);
	
      // print registers (there is no good format, sometimes you want
      // signed, unsigned or hex)
      for(n = 0; n < 5; n++)
	printf("% 6d", r[n] & 0xffff);
      printf(" %d %4x %d\n", r[5], r[6], r[7]);
    }
    ninst++;
  }
  
  // and exit happy
  exit(EXIT_SUCCESS);
}
