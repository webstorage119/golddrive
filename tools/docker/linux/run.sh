#!/bin/sh
echo running ssh-server container...
docker build -t ssh-server .
docker run --rm -d -p 2222:22 -t ssh-server


