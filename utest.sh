#!/bin/bash -ef

egrep '^#(in|out) ' sys.3rd | while read -r in ; do
  read -r out

  # check that everything is in order
  if ! echo "$in" | egrep --silent '^#in ' ; then
    echo "expected #in in: $in"
  fi
  if ! echo "$out" | egrep --silent '^#out ' ; then
    echo "expected #out in: $out"
  fi

  # echo $in

  res=$(echo "$in" | sed 's/^#in //' | ./third2asm.py sys.3rd /dev/stdin post.3rd | ./asm.py | ./txt2bin > tmp.bin && ./emu tmp.bin | perl -0777 -pe 's/\s+/ /gs; s/\s*$//gs')
  ans=$(echo "$out" | sed 's/^#out //')
  
  if [ "$res" != "$ans" ] ; then
    echo input: $in
    echo "expected:" $ans
    # echo "$ans" | od -a 
    echo "found:   " $res
    # echo "$res" | od -a 
  fi
done

exit
