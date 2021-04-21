#
# Runs a benchmark which increases block size until 4gb. Count will always be 1
#

BENCH_WRITE_FILE=bench_write.txt  # benchmark file where to print write results
> $BENCH_WRITE_FILE

COUNT=1   # how many times should write
obs=1024  # write blocksize
MAXBS=$((2**17))   # max block size

if [ $# -eq 0 ]; then
  echo "error: missing number of writers" >&2
  printf "usage: %s <writers>\n" $0
  exit 1
fi

writers=$1 # number of writers
re='^[0-9]+$'
if ! [[ $writers =~ $re ]] ; then
 echo "error: invalid number of writers" >&2
 printf "usage: %s <writers>\n" $0
 exit 1
fi

# send max block
echo $MAXBS > ./tmp/prod/maxbs

while [ $obs -le $MAXBS ]
do

  printf "\rRunning benchmark with %d writers, bs=%d, count=%d: " $writers $obs $COUNT

  start_time="$(date -u +%s.%N)"

  # run all the writers
  for (( i = 0; i < writers; i++ )); do
      dd if=/dev/zero of=./tmp/prod/bench bs=$(($obs/$writers)) count=$COUNT 2>&1 | grep copied >> $BENCH_WRITE_FILE &
  done

  # wait for all the readers to complete
  cat ./tmp/prod/syncpipe > /dev/null

  end_time="$(date -u +%s.%N)"

  elapsed="$(bc <<<"$end_time-$start_time")"
  gigabitpersec="$(bc <<<"scale=4; $obs * 8 / $elapsed / $((2**30))")"
  megabytepersec="$(bc <<<"scale=4; $obs / $((2**20)) / $elapsed")"
  printf "%ss, %s Gbit/s, %s MB/s\n" $elapsed $gigabitpersec $megabytepersec

  # increase block size
  obs=$(( $obs * 2 ))
done

printf "\n"
