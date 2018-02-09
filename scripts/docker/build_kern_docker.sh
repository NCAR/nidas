#!/bin/sh

# Create new image from debian-armel-cross:jessie, adding Debian packages from
# build-essential to get the native build tools.  Cross builds of kernels
# also require native build tools.

docker build -t debian-armel-kern-cross:jessie -f Dockerfile.armel_kernel .
