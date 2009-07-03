#!/bin/bash
rm -rf /tmp/dmtx*
killall dmtxpluginfront
mkdir /tmp/dmtxdatadir
mkdir /tmp/dmtxsymboldir
mkdir /tmp/dmtxoutputdir
cp ./sample-symbol.png /tmp/dmtxsymboldir/
./src/dmtxpluginfront  sample-symbol.png
#cat /tmp/dmtxdatadir/dmtxd.lock
cat /tmp/dmtxdatadir/dmtxd.log
#cat /tmp/dmtxoutputdir/symbol.txt


