FROM opensuse/leap:15.5 AS base

COPY config/proxy /etc/sysconfig/proxy

RUN zypper install -y gcc make tar zip vim

WORKDIR /work
COPY Makefile .
COPY src /work/

RUN make server

ENTRYPOINT ["./server", "8082"]