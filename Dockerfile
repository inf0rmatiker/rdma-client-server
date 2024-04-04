FROM opensuse/leap:15.5 AS base

COPY config/proxy /etc/sysconfig/proxy

RUN zypper install -y gcc make tar zip vim

WORKDIR /work
COPY Makefile .
RUN mkdir /work/src
COPY src/ /work/src/

RUN make server

ENTRYPOINT ["./server", "8082"]