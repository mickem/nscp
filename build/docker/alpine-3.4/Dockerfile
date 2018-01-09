FROM alpine:3.4

RUN mkdir -p /src/nscp
ADD . /src/nscp/

# fix possibly lost exec bit
RUN chmod +x /src/nscp/ext/md-protobuf/protoc-gen-md

RUN apk update && \
    apk add cmake make gcc build-base python py-pip python-dev protobuf-dev openssl-dev boost-dev

# install python dependencies (installing mkdocs and mkdocs-material here will
# break running make because of "logger Invalid log level: no-std-err" when
# building the documentation)
#RUN pip install protobuf jinja2 mkdocs mkdocs-material
RUN pip install protobuf jinja2

RUN mkdir -p /src/build
