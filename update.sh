#!/bin/bash
VERSION=1.2.1
docker build -t logru/africapolis:$VERSION -t logru/africapolis:latest .
docker login 
docker push logru/africapolis:$VERSION 
docker push logru/africapolis:latest