# benchmark file where to print write results
BENCH_WRITE_FILE=bench-write_incr_bs.txt
# benchmark file where to print read results
BENCH_READ_FILE=bench-read_incr_bs.txt
# write blocksize
OBS=1024
# how many times should write
COUNT=1
# read blocksize
IBS=1024
ITER=22 # until 2gb

# empty out both files
> $BENCH_WRITE_FILE
> $BENCH_READ_FILE

i=1
while [ $i -le $ITER ]
do
  printf "\rRunning benchmark: bs=%d count=%d %d%c" $OBS $COUNT $i '%'
  dd if=/dev/zero of=./tmp/prod/bench bs=$OBS count=$COUNT 2>&1 | grep copied >> $BENCH_WRITE_FILE &
  dd if=./tmp/cons/bench bs=$IBS 2>&1 | grep -a copied >> $BENCH_READ_FILE
  wait
  i=$(( $i + 1 ))
  OBS=$(( $OBS + 1024 ))
  IBS=$OBS
done

printf "\n"