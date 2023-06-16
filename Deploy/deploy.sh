#!/bin/sh

set -o errexit  # exit on error
set -o nounset  # trigger error when expanding unset variables

binpath="../build_terminal/RelWithDebInfo/bin"
binary=$binpath/blocksettle
scriptpath="../Scripts"

if [ ! -x $binary ]; then
    echo "Release terminal binary $binary doesn't exist!"
    exit
fi

mkdir -p Ubuntu/usr/bin
mkdir -p Ubuntu/lib/x86_64-linux-gnu
rm -f Ubuntu/usr/bin/RFQBot.qml
rm -rf Ubuntu/usr/share/blocksettle/scripts
mkdir -p Ubuntu/usr/share/blocksettle/scripts

cp $binpath/blocksettle Ubuntu/usr/bin/
#cp $scriptpath/DealerAutoQuote.qml Ubuntu/usr/share/blocksettle/scripts/
#cp $scriptpath/RFQBot.qml Ubuntu/usr/share/blocksettle/scripts/

dpkg -b Ubuntu bsterminal.deb
echo "deb package generated"

rm -f Ubuntu/usr/bin/blocksettle
rm -f Ubuntu/usr/share/blocksettle/scripts/*
rm -f Ubuntu/lib/x86_64-linux-gnu/*
