#
# Runs a benchmark which increases block size. Count will always be 1
#
BENCH_READ_FILE=bench_read.txt # benchmark file where to print read results
> $BENCH_READ_FILE

COUNT=1   # how many times should read
ibs=1024  # read blocksize

if [ $# -eq 0 ]; then
  echo "error: missing number of readers" >&2
  printf "usage: %s <readers>\n" $0
  exit 1
fi

readers=$1 # number of readers
re='^[0-9]+$'
if ! [[ $readers =~ $re ]] ; then
 echo "error: invalid number of readers" >&2
 printf "usage: %s <readers>\n" $0
 exit 1
fi

# read max block size
MAXBS=$(< ./tmp/cons/maxbs)

while [ $ibs -le $MAXBS ]
do

  printf "\rRunning benchmark with %d readers, bs=%d, count=%d" $readers $ibs $COUNT

  # run all the readers
  for (( i = 0; i < readers; i++ )); do
      dd if=./tmp/cons/bench of=/dev/null bs=$(($ibs/$readers)) count=$COUNT 2>&1 | grep -a copied >> $BENCH_READ_FILE &
  done

  # wait all the readers to complete
  wait

  # notify the writers that all the readers have finished reading
  echo "1" > ./tmp/cons/syncpipe

  # increase block size
  ibs=$(( $ibs * 2 ))
done

printf "\n"
