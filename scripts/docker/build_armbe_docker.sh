#!/bin/sh

set -e

# image will have a "builder" user, and a group eol:1342
group=eol
gid=1342

image=fedora25-armbe-cross:ael

docker build --volume=$PWD:/tmp/docker-files:ro,Z -t $image \
    --build-arg=group=$group --build-arg=gid=$gid \
    -f Dockerfile.cross_ael_armeb .

docker tag $image maclean/$image
docker push maclean/$image
