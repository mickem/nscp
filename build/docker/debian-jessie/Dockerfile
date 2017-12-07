FROM debian:jessie-slim

RUN mkdir -p /src/nscp
ADD . /src/nscp/
RUN apt-get update && apt-get upgrade -y
RUN apt-get update
RUN apt-get install -y git build-essential debhelper cmake python-dev libssl-dev libboost-all-dev 
RUN apt-get install -y libprotobuf-dev libcrypto++-dev libgtest-dev liblua5.1-0-dev 
RUN apt-get install -y protobuf-compiler python-protobuf python-pip
RUN pip install jinja2 mkdocs

RUN mkdir -p /src/build


