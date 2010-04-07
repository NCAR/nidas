#!/bin/sh

export ISFF=$PWD/config
export PROJECT=TREX
export TREX_CONFIG=trex
export RAWDATADIR=$PWD/data

valgrind prep -D "u.5m#1,v.5m#2,w.5m#3" -B "2006 mar 31 00:00:00.05" -E" 2006 apr 02 23:59"

