#
# Runs a benchmark which increases block size until 4gb. Count will always be 1
#

if [ $# -lt 1 ]; then
  echo "error: missing max block size" >&2
  printf "usage: %s <max_block_size>\n" $0
  exit 1
fi

maxbs=$1 # max block size
obs=1024 # starting write blocksize

# file to be sent
if=./data.bin
if [ -p $if ]; then
    > $if > $if # empty out if it already exists
fi
# generate file with maxbs bytes
dd if=/dev/zero of=$if bs=$maxbs count=1 2> /dev/null

# send max block
echo $maxbs > ./tmp/prod/maxbs
# netpipe used to sync reader and writer
syncpipe=./tmp/prod/sync

while [ $obs -le $maxbs ]
do
  start_time="$(date -u +%s.%N)"

  # write
  dd if=$if of=./tmp/prod/bench bs=$obs 2>&1 | grep copied > /dev/null

  # wait for the reader to complete
  cat $syncpipe > /dev/null

  end_time="$(date -u +%s.%N)"

  elapsed="$(bc <<<"$end_time-$start_time")"
  gigabitpersec="$(bc <<<"scale=4; $obs * 8 / $elapsed / $((2**30))")"
  megabytepersec="$(bc <<<"scale=4; $obs / $((2**20)) / $elapsed")"
  printf "bs=%d, %ss, %s Gbit/s, %s MB/s\n" $obs $elapsed $gigabitpersec $megabytepersec

  # increase block size
  obs=$(( $obs * 2 ))
done

rm $if
