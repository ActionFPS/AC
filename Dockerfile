FROM debian:jessie-slim

RUN apt-get -y update && \
    apt-get install -y gcc make clang zlib1g-dev

ADD / /source

RUN cd /source/src/ && \
    make server_install

