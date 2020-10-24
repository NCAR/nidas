#!/bin/sh

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 release
    for example:  $0 bionic"
    exit 1
fi

release=$1

# A --build-arg overrides an ARG in the Dockerfile
user=ads
uid=12900
group=eol
gid=1342

# Must do a #docker login" with your personal docker account.
# this account must be registered with ncar organization as an
# administor.  Call Gary.
docuser=ncar

image=nidas-build-ubuntu-i386:$release

docker build -t $image \
    --build-arg=user=$user --build-arg=uid=$uid \
    --build-arg=group=$group --build-arg=gid=$gid \
    -f Dockerfile.ubuntu_i386_$release .

docker tag $image $docuser/$image
docker push $docuser/$image
