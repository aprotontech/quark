
CODE_ROOT=$(cd `dirname $0`; pwd)
cd $CODE_ROOT

cp $CODE_ROOT/third/*.tar.gz $CODE_ROOT/framework

cd $CODE_ROOT/framework

if [ ! -d openssl ]; then
    tar zxf openssl.tar.gz
    rm -f openssl.tar.gz
fi

if [ ! -d opus ]; then
    tar zxf opus.tar.gz
    rm -f opus.tar.gz
fi

if [ ! -d paho ]; then
    tar zxf paho.tar.gz
    rm -f paho.tar.gz
fi

if [ ! -d speex ]; then
    tar zxf speex.tar.gz
    rm -f speex.tar.gz
fi
