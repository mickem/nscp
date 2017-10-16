@echo off
if "%1"=="" goto :err

if "%1"=="" goto :err
docker build -f build/docker/%1/Dockerfile --tag builder .
docker run --rm -it builder /src/nscp/build/sh/build-local.sh

goto :end
:err
echo Please specify version

:end