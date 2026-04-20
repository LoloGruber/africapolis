#!/bin/bash
VERSION=$(grep -oP 'project\s*\([^)]*VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt)
docker build -t logru/africapolis:$VERSION -t logru/africapolis:latest .
docker login 
docker push logru/africapolis:$VERSION 
docker push logru/africapolis:latest