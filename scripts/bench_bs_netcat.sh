remoteport=12345
MAXBS=$((2**17))
bs=8

while [ $bs -le $MAXBS ]
do
  dd if=/dev/zero bs=$bs count=1 | nc -N titanic $remoteport ;
  bs=$(( $bs * 2 )) # increase block size
done