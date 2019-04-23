FROM ubuntu:bionic

RUN apt-get update && apt-get -y install \
    curl clang g++ g++-5 git make ccache \
    libedit-dev zlib1g-dev \
llvm-3.9-tools wget mercurial
RUN curl https://sh.rustup.rs -sSf | \
    sh -s -- --default-toolchain stable -y

ENV PATH=~/.cargo/bin:$PATH
RUN wget -q https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py -O bootstrap.py && python bootstrap.py --application-choice=browser --no-interactive

ENV SHELL /bin/bash
ENV TRAVIS true
ENV CCACHE_DIR /ccache
RUN mkdir /build /ccache
WORKDIR /build
