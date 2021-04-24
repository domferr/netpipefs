#
# Runs a benchmark which increases block size. Count will always be 1
#

netpipe=./tmp/cons/bench
ibs=131072  # read blocksize
curribs=1024

# netpipe used to sync reader and writer
syncpipe=./tmp/cons/sync

# read max block size
MAXBS=$(< ./tmp/cons/maxbs)

while [ $curribs -le $MAXBS ]
do
  # read
  dd if=$netpipe of=/dev/null bs=$ibs 2>&1 | grep -a copied

  # notify the writer that reading is done
  echo "1" > $syncpipe

  # increase block size
  curribs=$(( $curribs * 2 ))
done
