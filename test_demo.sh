
#BIN=./bin/rc_demo
BIN=./bin/sdktest

rm -f $BIN
rm -f core.*
make -j10

ulimit -c unlimited
$BIN $1

