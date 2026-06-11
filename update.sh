#!/bin/bash
RELEASE=false
[[ "$1" == "-p" || "$1" == "--push" ]] && RELEASE=true

VERSION=$(grep -oP 'project\s*\([^)]*VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt)
docker build -t logru/africapolis:$VERSION -t logru/africapolis:latest .

if [ "$RELEASE" = "true" ]; then
  docker login 
  docker push logru/africapolis:$VERSION 
  docker push logru/africapolis:latest
fi