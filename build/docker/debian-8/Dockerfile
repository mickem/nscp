# Change to debian:9 or debian:sid for other Debian releases
FROM debian:8

RUN mkdir /nscp
ADD . /nscp/

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y build-essential

RUN apt-get install -y cmake python python-dev libssl-dev libboost-all-dev protobuf-compiler python-protobuf libprotobuf-dev python-sphinx libcrypto++-dev libcrypto++ liblua5.1-0-dev libgtest-dev
RUN apt-get install -y git wget dos2unix debhelper dh-virtualenv python-pip zip devscripts 

ENV GH_TOKEN=UPDATE_ME

RUN chmod u+x /nscp/ext/md-protobuf/protoc-gen-md

RUN pip install jinja2 mkdocs mkdocs-material github3.py

RUN mkdir -p /build
RUN mkdir -p /packages

VOLUME /packages

WORKDIR /build

CMD /nscp/build/sh/build-debian.sh

