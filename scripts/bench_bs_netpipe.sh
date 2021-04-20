#
# Runs a benchmark which increases block size until 4gb. Count will always be 1
#

BENCH_WRITE_FILE=bench-write.txt  # benchmark file where to print write results
BENCH_READ_FILE=bench-read.txt    # benchmark file where to print read results
OBS=1024  # write blocksize
COUNT=1   # how many times should write
IBS=$OBS  # read blocksize
ITER=22   # number of iterations (23: until 4gb)

# empty out both files
> $BENCH_WRITE_FILE
> $BENCH_READ_FILE

i=0
while [ $i -lt $ITER ]
do
  printf "\rRunning benchmark: bs=%d count=%d %d%c" $OBS $COUNT $((($i*100)/$ITER)) '%'

  dd if=/dev/zero of=./tmp/prod/bench bs=$OBS count=$COUNT 2>&1 | grep copied >> $BENCH_WRITE_FILE &
  # sleep 1
  dd if=./tmp/cons/bench bs=$IBS 2>&1 | grep -a copied >> $BENCH_READ_FILE
  wait

  i=$(( $i + 1 ))
  OBS=$(( $OBS * 2 )) # increase block size
  IBS=$OBS
done

printf "\n"
