#!/bin/bash

# create a pseudo terminal
socat -d -d pty,link=/tmp/ttyUSB0,raw,b9600 pty,link=/tmp/ttyUSB1,b9600,raw &

# run the tests
./tSerialSensor
