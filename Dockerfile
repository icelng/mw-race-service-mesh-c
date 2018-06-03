# Builder container
FROM registry.cn-hangzhou.aliyuncs.com/aliware2018/services AS builder

COPY . /root/workspace/agent
WORKDIR /root/workspace/agent
RUN set -ex && mvn clean package
WORKDIR /root/workspace/agent/src/main/c
RUN set -ex && mkdir bin
RUN set -ex && apt-get update
RUN set -ex && apt-get install make -y
RUN set -ex && apt-get install gcc -y
RUN set -ex && make clean
RUN set -ex && make


# Runner container
FROM registry.cn-hangzhou.aliyuncs.com/aliware2018/debian-jdk8

COPY --from=builder /root/workspace/services/mesh-provider/target/mesh-provider-1.0-SNAPSHOT.jar /root/dists/mesh-provider.jar
COPY --from=builder /root/workspace/services/mesh-consumer/target/mesh-consumer-1.0-SNAPSHOT.jar /root/dists/mesh-consumer.jar
COPY --from=builder /root/workspace/agent/target/tianchi-agent-1.0-SNAPSHOT.jar /root/dists/mesh-agent.jar
COPY --from=builder /root/workspace/agent/src/main/c/bin/consumer-agent /root/dists/consumer-agent

COPY --from=builder /usr/local/bin/docker-entrypoint.sh /usr/local/bin
COPY start-agent.sh /usr/local/bin

RUN set -ex && apt-get update
RUN set -ex && apt-get install libcurl4-openssl-dev -y

RUN set -ex && mkdir -p /root/logs
RUN set -ex && chmod 777 /root/dists/consumer-agent
RUN set -ex && chmod 777 /usr/local/bin/docker-entrypoint.sh
RUN set -ex && chmod 777 /usr/local/bin/start-agent.sh

ENTRYPOINT ["docker-entrypoint.sh"]
