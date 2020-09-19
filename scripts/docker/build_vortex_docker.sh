#!/bin/sh

set -e

# image will have a "builder" user, and a group eol:1342
group=eol
gid=1342

image=ubuntu-i386:xenial

docker build -t $image -f Dockerfile.ubuntu_i386_xenial .

docker tag $image maclean/$image
docker push maclean/$image
