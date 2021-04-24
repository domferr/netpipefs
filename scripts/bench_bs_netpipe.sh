#
# Runs a benchmark which increases block size until 4gb. Count will always be 1
#
BENCH_READ_FILE=bench_read.txt    # benchmark file where to print read results
> $BENCH_READ_FILE

BENCH_WRITE_FILE=bench_write.txt  # benchmark file where to print write results
> $BENCH_WRITE_FILE

COUNT=1   # how many times should write/read
bs=1024   # read/write blocksize
MAXBS=$((2**30))   # max block size

if [ $# -lt 2 ]; then
  echo "error: missing number of writers or number of readers" >&2
  printf "usage: %s <readers> <writers>\n" $0
  exit 1
fi

readers=$1 # number of readers
re='^[0-9]+$'
if ! [[ $readers =~ $re ]] ; then
 echo "error: invalid number of readers" >&2
 printf "usage: %s <readers> <writers>\n" $0
 exit 1
fi

writers=$2 # number of writers
re='^[0-9]+$'
if ! [[ $writers =~ $re ]] ; then
 echo "error: invalid number of writers" >&2
 printf "usage: %s <readers> <writers>\n" $0
 exit 1
fi

namedpipe=/tmp/bench

trap "rm -f $namedpipe" EXIT

if [[ ! -p $namedpipe ]]; then
    mkfifo $namedpipe
fi

while [ $bs -le $MAXBS ]
do

  #printf "\rRunning benchmark with %d readers and %d writers, bs=%d, count=%d" $readers $writers $bs $COUNT

  # run all the writers
#  for (( i = 0; i < writers; i++ )); do
#      dd if=/dev/zero of=$namedpipe bs=$(($bs/$writers)) count=$COUNT 2>&1 | grep copied >> $BENCH_WRITE_FILE &
#  done
#
#  # run all the readers
#  for (( i = 0; i < readers; i++ )); do
#      dd if=$namedpipe of=/dev/null bs=$(($bs/$readers)) count=$COUNT 2>&1 | grep -a copied >> $BENCH_READ_FILE &
#  done
#
#  # wait all the processes to complete
#  wait

  ./bin/benchmark ./tmp/prod/bench ./tmp/cons/bench $bs $writers $readers

  # increase block size
  bs=$(( $bs * 2 ))
done

printf "\n"
