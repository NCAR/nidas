#!/bin/sh

set -e

# A --build-arg overrides an ARG in the Dockerfile
user=builder
uid=1000
group=eol
gid=1342

image=fedora25-armbe-cross:ael

docker build -t $image \
    --build-arg=user=$user --build-arg=uid=$uid \
    --build-arg=group=$group --build-arg=gid=$gid \
    -f Dockerfile.cross_ael_armeb .

docker tag $image maclean/$image
docker push maclean/$image
