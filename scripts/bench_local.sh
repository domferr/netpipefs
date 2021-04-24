#
#
#
if [ $# -lt 3 ]; then
  echo "error: missing pipe name" >&2
  printf "usage: %s <writer_pipe_name> <reader_pipe_name> <data_size>\n" $0
  exit 1
fi
writer_pipe_name=$1
reader_pipe_name=$2
maxbs=$3
ibs=131072  # read blocksize
obs=1024    # write blocksize

# file to be sent
if=./data.bin
if [ -p $if ]; then
    > $if > $if # empty out if it already exists
fi
# generate file with maxbs bytes
dd if=/dev/zero of=$if bs=$maxbs count=1 2> /dev/null

while [ $obs -le $maxbs ]
do
  echo "Block size=$obs bytes"

  # write
  dd if=$if of=$writer_pipe_name bs=$obs 2>&1 | grep copied | (read result; echo "[Write] $result") &
  # read
  dd if=$reader_pipe_name of=/dev/null bs=$ibs 2>&1 | grep -a copied | (read result; echo "[Read ] $result")

  # increase block size
  obs=$(( $obs * 2 ))
done

rm $if
