#!/bin/sh

docks=$(docker ps -qa --no-trunc --filter "status=exited")
[ "$docks" ] && docker rm $docks

docks=$(docker images --filter "dangling=true" -q --no-trunc)
[ "$docks" ] && docker rmi $docks

docks=$(docker images | grep "<none>" | awk '/ / { print $3 }')
[ "$docks" ] && docker rmi $docks
