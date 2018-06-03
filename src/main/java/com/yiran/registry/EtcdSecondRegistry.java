package com.yiran.registry;

import mousio.etcd4j.EtcdClient;
import mousio.etcd4j.responses.EtcdKeysResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.net.URI;
import java.text.MessageFormat;
import java.util.List;

public class EtcdSecondRegistry{
    private final String rootPath = "dubbomesh";
    private Logger logger = LoggerFactory.getLogger(EtcdSecondRegistry.class);

    private EtcdClient etcdClient;

    public EtcdSecondRegistry(String etcdUrl){
        etcdClient = new EtcdClient(URI.create(etcdUrl));
    }

    public void register(String serviceName, int port, int loadLevel) throws Exception {
        String key = String.format("/%s/%s/%s:%d", rootPath, serviceName, IpHelper.getHostIp(), port);
        String value = String.valueOf(loadLevel);

        EtcdKeysResponse response = etcdClient.put(key, value).send().get();
        logger.info("Set key-value response:{}", response.node.value);
    }

    public List<Endpoint> find(String serviceName) throws Exception {
        return null;
    }
}
