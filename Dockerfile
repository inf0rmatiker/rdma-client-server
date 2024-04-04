FROM opensuse/leap:15.5 AS base

COPY config/proxy /etc/sysconfig/proxy

WORKDIR /work
COPY Makefile .
COPY src /work/

RUN make server

ENTRYPOINT ["./server", "8082"]