#!/bin/sh

dir=$(dirname $0)
cd $dir/..

docker run --rm --volume $PWD:/home/debbuilder/nidas:rw,Z \
    -i -t maclean/debian-armel-cross:jessie /bin/bash

