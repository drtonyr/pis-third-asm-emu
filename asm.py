#!/usr/bin/python3

# first pass assembles everything but does not resolve labels
# second pass just adds in the label vales
#
# must throw an error on all errors with a useful error message (file, lineno, line)
#
## currently used
# lab:
# var[n]:
# var[]: 1 2 3 4
# var[n]: 1 2 3 4
# :const
#
## required extensions
# cnd Z = *Y
# cnd Y = *Z
# cnd Y = *Z++
# cnd *Z++ = Y
# cnd X = {-1,0,1,2}   - use Z - I to I + I
# cnd X = expr         - use X = *P++ then :const
# blank line
# error
#
# local variables are a nice-to-have, i.e. for loops within functions

import sys
import re

cnd = {'' : '000', 'cin': '001', 'eq0': '010', 'ne0': '011',
         'lt0':  '100', 'le0': '101', 'ge0': '110', 'gt0': '111'}

cndex = '(' + '|'.join(cnd.keys()) + ')'

ops = {'&': '000', '|': '001', '^': '010', '>>': '011',
       '+': '100', '+c': '101', '-': '110', '-c': '111'}

opsex = '(&|\||\^|>>|\+|-)'

reg = {'A': '000', 'B': '001', 'C': '010', 'D': '011',
       'E': '100', 'Z': '101', 'P': '110', 'I': '111'}

regex = '(['+''.join(reg.keys())+'01])'

map0Z = {"0": "Z", "1": "I"}
def map01(reg):
  if reg in map0Z:
    return map0Z[reg]
  else:
    return reg
  
# four char for hex address, one space, 16 chars for binary instruction
blank = ' ' * 21


# cn Rz = Rx op Ry
def opReg(match, sym, code, undef):
  cn = match.group(1)
  Rz = map01(match.group(2))
  Rx = map01(match.group(3))
  op = match.group(4)
  Ry = map01(match.group(5))
  code.append("%04x %s0%s%s%s%s\t%s" % (sym["addr"], cnd[cn], ops[op], reg[Rx], reg[Ry], reg[Rz], match.group(0)))
  sym["addr"] += 1


# cn *Rz = Rx, Rz +-= Ry 
def opPushStride(match, sym, code, undef):
  cn = match.group(1)
  Rz = map01(match.group(2))
  Rx = map01(match.group(3))
  if map01(match.group(4)) != Rz:
    sys.exit("asm: Parse failed, must modify pointer: %s" % match.group(0))
  op = match.group(5)
  Ry = map01(match.group(6))
  code.append("%04x %s110%d%s%s%s\t%s" % (sym["addr"], cnd[cn], op == '+', reg[Rx], reg[Ry], reg[Rz], match.group(0)))
  sym["addr"] += 1
  

# cn Rx = *Rz, Rz +-= Ry
def opPopStride(match, sym, code, undef):
  cn = match.group(1)
  Rx = map01(match.group(2))
  Rz = map01(match.group(3))
  if map01(match.group(4)) != Rz:
    sys.exit("Parse failed, must modify pointer: %s" % match.group(0))
  op = match.group(5)
  Ry = map01(match.group(6))
  code.append("%04x %s100%d%s%s%s\t%s" % (sym["addr"], cnd[cn], op == '+', reg[Rx], reg[Ry], reg[Rz], match.group(0)))
  sym["addr"] += 1


# cn *Rz = Rx  |  cn *Rz++ = Rx  |  cn *Rz-- = Rx
def opPush(match, sym, code, undef):
  op = match.group(3)
  if op == "":
    op = "++"
    Ry = "Z"
  else:
    Ry = "I"
  parseLine("%s *%s = %s, %s %c= %s  # %s" % (match.group(1), match.group(2),
            match.group(4), match.group(2), op[0], Ry, match.group(0)))


# cn Rz = expr
def opConst(match, sym, code, undef):
  print("small:%s:%s:%s" % (match.group(1), match.group(2), match.group(3)))


# cn Rx = *Rz  |  Rx = *Rz++  |  Rx = *Rz-- 
def opPop(match, sym, code, undef):
  op = match.group(4)
  if op == "":
    op = "++"
    Ry = "Z"
  else:
    Ry = "I"
  parseLine("%s %s = *%s, %s %c= %s  # %s" % (match.group(1), match.group(2),
            match.group(3), match.group(3), op[0], Ry, match.group(0)))


def opAssign(match, sym, code, undef):
  parseLine("%s %s = %s | 0  # %s" % (match.group(1), match.group(2),
            match.group(3), match.group(0)))


def opImmed(match, sym, code, undef):
  parseLine("%s %s = *P, P += I # %s" % (match.group(1), match.group(2), match.group(0)))
  undef.append((len(code), sym["addr"], match.group(3), ":%s  # %s" % (match.group(3), match.group(0))))
  code.append('FIXED IN SECOND PASS')
  sym['addr'] += 1


def opArray(match, sym, code, undef):
  try:
    const = eval(match.group(2), sym)
  except:
    sys.exit("asm: failed parse \"%s\" in line: %s" % (match.group(2), match.group(0)))
  # got it
  sym[match.group(1)] = sym['addr']
  code.append("%s\t%s := 0x%x\t # %s" % (blank, match.group(1), sym["addr"], match.group(0)))
  sym['addr'] += const


def opDefLab(match, sym, code, undef):
  sym[match.group(1)] = sym["addr"]


def opDefLabVal(match, sym, code, undef):
  try:
    const = eval(match.group(2), sym)
  except:
    sys.exit("asm: failed parse \"%s\" in line: %s" % (match.group(2), match.group(0)))
  # got it
  sym[match.group(1)] = const
  code.append("%s\t%s" % (blank, match.group(0)))


def opUseLab(match, sym, code, undef):
  # push index, label, line onto a list to fix up in second pass
  undef.append((len(code), sym["addr"], match.group(1), match.group(0)))
  code.append('FIXED IN SECOND PASS')
  sym['addr'] += 1


def opEmpty(match, sym, code, undef):
  code.append("%s\t%s" % (blank, match.group(0)))

  
def opError(match, sym, code, undef):
  sys.exit("asm: failed to parse: %s" % (match.group(0)))

  
gram = [
  (r'%s\s*%s\s*=\s*%s\s*%s\s*%s' % (cndex, regex, regex, opsex, regex), opReg),
  (r'%s\s*\*%s\s*=\s*%s\s*,\s*%s\s*([+-])=\s*%s' % (cndex, regex, regex, regex, regex), opPushStride),
  (r'%s\s*%s\s*=\s*\*%s\s*,\s*%s\s*([+-])=\s*%s' % (cndex, regex, regex, regex, regex), opPopStride),
  (r'%s\s*\*%s(|\+\+|--)\s*=\s*%s' % (cndex, regex, regex), opPush),
  (r'%s\s*%s\s*=\s*\*%s(|\+\+|--)' % (cndex, regex, regex), opPop),
  (r'%s\s*%s\s*=\s*%s'             % (cndex, regex, regex), opAssign),
  (r'%s\s*%s\s*=\s*(.*)'           % (cndex, regex), opImmed),
  (r'([^\s]+)\[([^\s]+)\]:\s*([^#]*)', opArray),
  (r'([^\s]+):', opDefLab),
  (r'([^\s]+)\s*:=\s*([^#]*)', opDefLabVal),
  (r':([^#]*)', opUseLab),
  (r'\s*', opEmpty),
  (r'.*', opError)
]

cgram = []
# compile all grammar rules along with header/footer
for g in gram:
  cgram.append([re.compile('\s*' + g[0] + '\s*(?:#.*)?$'), g[1]])
  
# global symbols and values
sym = {"addr": 0}

# add in symbols from the OS name space
f = open('os.h', 'r')
for line in f:
  key = line.split()[1]
  val = line.split()[2]
  sym[key] = int(val)
f.close()
  
# code is where we assemble the code to
code = []
undef = []

def parseLine(line):
  for rule in cgram:
    match = rule[0].match(line)
    if match:
      rule[1](match, sym, code, undef)
      break
  
# first pass: apply all rules
for line in sys.stdin:
  parseLine(line.strip())

# second pass: fix up all the undefs      
for (posn, addr, label, line) in undef:
  try:
    if label in sym:
      const = sym[label]
    else:
      const = eval(label, sym)
  except:
    sys.exit("asm: failed to evaluate \"%s\" in line: %s" % (label, line))
  code[posn] = '%04x %s\t%s' % (addr, format(const & 0b1111111111111111, '016b'), line)
    
# print out the whole code
for line in code:
  print(line)
