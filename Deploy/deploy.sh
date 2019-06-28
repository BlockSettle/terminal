#!/bin/sh

binpath="../build_terminal/Release/bin"
binary=$binpath/blocksettle
scriptpath="../Scripts"

libprotobuf="${DEV_3RD_ROOT}/release/Protobuf/lib"
if [ "${DEV_3RD_ROOT}X" = "X" ]; then
   libprotobuf="../../3rd/release/Protobuf/lib"
fi

if [ ! -x $binary ]; then
    echo "Release terminal binary $binary doesn't exist!"
    exit
fi

if [ ! -d $libprotobuf ]; then
   echo "Protobuf library dir is missing at $libprotobuf!"
   exit
fi

mkdir -p Ubuntu/usr/bin
mkdir -p Ubuntu/usr/share/blocksettle
mkdir -p Ubuntu/lib/x86_64-linux-gnu
mkdir -p $binpath/scripts

cp -f $binpath/* Ubuntu/usr/bin
cp -f $scriptpath/DealerAutoQuote.qml Ubuntu/usr/share/blocksettle
cp -f $scriptpath/RFQBot.qml $binpath/scripts
cp -fP $libprotobuf/libprotobuf.so* Ubuntu/lib/x86_64-linux-gnu
cp -f $libprotobuf/libprotobuf.la Ubuntu/lib/x86_64-linux-gnu

dpkg -b Ubuntu bsterminal.deb
echo "deb package generated"

rm -f Ubuntu/usr/bin/blocksettle
rm -f Ubuntu/usr/bin/blocksettle_signer
rm -f Ubuntu/usr/bin/bs_signer_gui
rm -f Ubuntu/usr/share/blocksettle/DealerAutoQuote.qml
rm -f Ubuntu/lib/x86_64-linux-gnu/*
