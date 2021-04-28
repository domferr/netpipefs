#
# Runs a benchmark which increases block size until 4gb. Count will always be 1
#

if [ $# -lt 1 ]; then
  echo "error: missing max block size" >&2
  printf "usage: %s <max_block_size>\n" $0
  exit 1
fi

maxbs=$1 # max block size
obs=$2 # starting write blocksize

# file to be sent
if=./data.bin
if [ -p $if ]; then
    > $if > $if # empty out if it already exists
fi
# generate file with maxbs bytes
echo "$if created"
dd if=/dev/zero of=$if bs=$maxbs count=1 2>&1 | grep copied

# send max block
echo $maxbs > ./tmp/prod/maxbs

start_time="$(date -u +%s.%N)"

# write
dd if=$if of=./tmp/prod/bench bs=$obs 2>&1 | grep copied

# wait for the reader to complete
nc -l 8787 > /dev/null

end_time="$(date -u +%s.%N)"

elapsed="$(bc <<<"$end_time-$start_time")"
gigabitpersec="$(bc <<<"scale=4; $maxbs * 8 / $elapsed / $((2**30))")"
megabytepersec="$(bc <<<"scale=4; $maxbs / $((2**20)) / $elapsed")"
printf "bs=%d, %ss, %s Gbit/s, %s MB/s\n" $obs $elapsed $gigabitpersec $megabytepersec


rm $if
