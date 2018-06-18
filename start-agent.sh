#!/bin/bash

ETCD_HOST=etcd
ETCD_PORT=2379
ETCD_URL=http://$ETCD_HOST:$ETCD_PORT

echo ETCD_URL = $ETCD_URL

if [[ "$1" == "consumer" ]]; then
  echo "Starting consumer agent..."
  /root/dists/consumer-agent $ETCD_URL
elif [[ "$1" == "provider-small" ]]; then
  echo "Starting small provider agent..."
  java -jar \
       -Xms512M \
       -Xmx512M \
       -Dtype=provider \
       -Dload.level=4 \
       -Dio.netty.leakDetection.level=disabled \
       -Dserver.port0=30001\
       -Dserver.port1=30002\
       -Dserver.port2=30003\
       -Dserver.port3=30004\
       -Ddubbo.protocol.port=20880 \
       -Detcd.url=$ETCD_URL \
       -Dlogs.dir=/root/logs \
       /root/dists/mesh-agent.jar
elif [[ "$1" == "provider-medium" ]]; then
  echo "Starting medium provider agent..."
  java -jar \
       -Xms1536M \
       -Xmx1536M \
       -Dtype=provider \
       -Dload.level=10 \
       -Dio.netty.leakDetection.level=disabled \
       -Dserver.port0=30011\
       -Dserver.port1=30012\
       -Dserver.port2=30013\
       -Dserver.port3=30014\
       -Ddubbo.protocol.port=20880 \
       -Detcd.url=$ETCD_URL \
       -Dlogs.dir=/root/logs \
       /root/dists/mesh-agent.jar
elif [[ "$1" == "provider-large" ]]; then
  echo "Starting large provider agent..."
  java -jar \
       -Xms2560M \
       -Xmx2560M \
       -Dtype=provider \
       -Dload.level=12 \
       -Dio.netty.leakDetection.level=disabled \
       -Dserver.port0=30021\
       -Dserver.port1=30022\
       -Dserver.port2=30023\
       -Dserver.port3=30024\
       -Ddubbo.protocol.port=20880 \
       -Detcd.url=$ETCD_URL \
       -Dlogs.dir=/root/logs \
       /root/dists/mesh-agent.jar
else
  echo "Unrecognized arguments, exit."
  exit 1
fi
