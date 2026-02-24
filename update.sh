#!/bin/bash
VERSION=1.1.0
docker build -t logru/africapolis:$VERSION -t logru/africapolis:latest .
docker login 
docker push logru/africapolis:$VERSION 
docker push logru/africapolis:latest