#!/bin/bash
VERSION=1.1.0
docker build -t logru/fishnet-deps:$VERSION -t logru/fishnet-deps:latest -f Dockerfile.deps .
docker login 
docker push logru/fishnet-deps:$VERSION 
docker push logru/fishnet-deps:latest