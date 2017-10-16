FROM centos:6.9

RUN mkdir -p /src/nscp
ADD . /src/nscp/

RUN yum update -y epel-release
RUN yum install -y cmake python-devel openssl-devel boost-devel lua-devel redhat-lsb rpmdevtools
RUN yum groupinstall -y 'Development Tools'
RUN yum install -y epel-release
RUN yum install --enablerepo=epel -y gtest-devel python-argparse.noarch
RUN yum install -y protobuf-devel protobuf-compiler protobuf-python cryptopp-devel python-pip
RUN mkdir /usr/lib/python2.6/site-packages/google/protobuf/compiler || true
ADD build/docker/files/plugin_pb2.py /usr/lib/python2.6/site-packages/google/protobuf/compiler/plugin_pb2.py
RUN touch /usr/lib/python2.6/site-packages/google/protobuf/compiler/__init__.py

ENV GH_TOKEN=UPDATE_ME

# Workaround for python and outdated ssl libs
#RUN yum install -y libffi-devel
#RUN pip install urllib3[secure]


RUN pip install jinja2 mkdocs mkdocs-material github3.py

RUN mkdir -p /src/build
RUN mkdir -p /src/packages

VOLUME /src/packages

CMD /src/nscp/build/sh/build-centos.sh

