#!/bin/bash

BASEDIR=`dirname $0`
cd $BASEDIR

./listener.py &
sleep 1
echo "audio socket started"
sleep 1
./volume-bars --led-cols=64 --led-rows=32 --led-slowdown-gpio=3