#!/bin/sh

# save idas-build-debian-base-armel:jessie image.
# I don't believe the armel cross-development tools are available in 
# in debian repositories anymore, so we don't want to lose this image
podman save --compress --format oci-dir -o base-armel.dir docker.io/ncar/nidas-build-debian-base-armel:jessie
