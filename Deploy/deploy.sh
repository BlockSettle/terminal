#!/bin/sh

binpath="../build_terminal/Release/bin"
binary=$binpath/blocksettle
scriptpath="../DealerScripts"
libprotobuf="${DEV_3RD_ROOT}/release/Protobuf/lib"

if [ ! -x $binary ]; then
    echo "Release terminal binary $binary doesn't exist!"
    exit
fi

mkdir -p Ubuntu/usr/bin
mkdir -p Ubuntu/usr/share/blocksettle
mkdir -p Ubuntu/lib/x86_64-linux-gnu

cp -f $binary Ubuntu/usr/bin
cp -f $binpath/blocksettle_signer Ubuntu/usr/bin
cp -f $scriptpath/DealerAutoQuote.qml Ubuntu/usr/share/blocksettle
cp -fP $libprotobuf/libprotobuf.so* Ubuntu/lib/x86_64-linux-gnu
cp -f $libprotobuf/libprotobuf.la Ubuntu/lib/x86_64-linux-gnu

dpkg -b Ubuntu bsterminal.deb
echo "deb package generated"

rm -f Ubuntu/usr/bin/blocksettle
rm -f Ubuntu/usr/share/blocksettle/DealerAutoQuote.qml
rm -f Ubuntu/lib/x86_64-linux-gnu/*
