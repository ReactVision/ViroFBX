FROM ubuntu:24.04 AS builder

WORKDIR /app

COPY . /app

RUN apt update
RUN apt install -y \
    openjdk-21-jdk-headless \
    g++ \
    make \
    zlib1g-dev
ENV JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
ENV CPLUS_INCLUDE_PATH=$JAVA_HOME/include:$JAVA_HOME/include/linux

RUN cd ViroFBX && make

FROM ubuntu:24.04
WORKDIR /app
COPY --from=builder /app/ViroFBX/virofbx .

ENTRYPOINT ["./virofbx"]