@echo off
if "%GH_TOKEN%"=="" goto :err
docker build -f build/docker/centos-6.9/Dockerfile --tag builder .
docker run --rm -it -e GH_TOKEN=%GH_TOKEN% builder
docker build -f build/docker/centos-7/Dockerfile --tag builder .
docker run --rm -it -e GH_TOKEN=%GH_TOKEN% builder
docker build -f build/docker/debian-8/Dockerfile --tag builder .
docker run --rm -it -e GH_TOKEN=%GH_TOKEN% builder

goto :end
:err
echo Please specify the github key
echo set GH_TOKEN=WHATEVER

:end