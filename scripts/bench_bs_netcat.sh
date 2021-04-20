remoteport=12345
MAXBS=512
bs=8
while [ $bs -lt $MAXBS ]
do
  dd if=/dev/zero bs=$bs count=1000000 | nc -N titanic $remoteport ;
  bs=$(( $bs * 2 )) # increase block size
done