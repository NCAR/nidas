#!/bin/sh

set -e

# A --build-arg overrides an ARG in the Dockerfile
user=builder
uid=1000
group=eol
gid=1342

# Must do a #docker login" with your personal docker account.
# this account must be registered with ncar organiztion as an
# administor.  Call Gary.
docuser=ncar

image=nidas-build-ubuntu-i386:xenial

docker build -t $image \
    --build-arg=user=$user --build-arg=uid=$uid \
    --build-arg=group=$group --build-arg=gid=$gid \
    -f Dockerfile.ubuntu_i386_xenial .

docker tag $image $docuser/$image
docker push $docuser/$image
