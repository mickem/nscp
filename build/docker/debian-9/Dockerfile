# Change to debian:9 or debian:sid for other Debian releases
FROM debian:9

RUN mkdir /nscp
ADD . /nscp/

ARG DEBIAN_FRONTEND=noninteractive

#Debian moved sources to archive, see https://stackoverflow.com/questions/76094428/debian-stretch-repositories-404-not-found
RUN sed -i -e 's/deb.debian.org/archive.debian.org/g' \
           -e 's|security.debian.org/|archive.debian.org/|g' \
           -e '/stretch-updates/d' /etc/apt/sources.list

RUN apt-get update && apt-get install -y build-essential

RUN apt-get install -y cmake python python-dev libssl-dev libboost-all-dev protobuf-compiler python-protobuf libprotobuf-dev \
python-sphinx libcrypto++-dev libcrypto++ liblua5.1-0-dev libgtest-dev
RUN apt-get install -y git wget dos2unix debhelper dh-virtualenv python-pip zip devscripts 

ENV GH_TOKEN=UPDATE_ME

#get around Failed loading extension "toc(permalink=true)". 
#see https://github.com/mkdocs/mkdocs/issues/1640#issue-364121171
RUN pip install Markdown==2.6.11
#mkdocs does not support python 2 after 0.16.x
RUN pip install jinja2 mkdocs==0.16.3 mkdocs-material==1.4.1

# pip package wrapt on arm64 fails, lock to older version to get things to install correctly
RUN apt-get install -y libffi-dev && pip install wrapt==1.12.1 cffi && pip install github3.py

RUN mkdir -p /build
RUN mkdir -p /packages

VOLUME /packages

WORKDIR /build

CMD /nscp/build/sh/build-debian.sh
