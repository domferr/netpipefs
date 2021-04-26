#
# Runs a benchmark which increases block size. Count will always be 1
#

netpipe=./tmp/cons/bench
ibs=131072  # read blocksize
curribs=1024

# read max block size
MAXBS=$(< ./tmp/cons/maxbs)

#while [ $curribs -le $MAXBS ]
#do
  # read
  dd if=$netpipe of=/dev/null bs=$1 2>&1 | grep -a copied

  # notify the writer that reading is done
  echo 1 | nc -N localhost 8787

  # increase block size
  curribs=$(( $curribs * 2 ))
#done
