FROM opensuse/leap:15.5 AS base

WORKDIR /work
COPY Makefile .
COPY src /work/

RUN make server

ENTRYPOINT ["/bin/bash"]