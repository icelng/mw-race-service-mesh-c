package com.yiran.registry;

import java.util.List;

public interface IRegistry {

    // 注册服务
    void register(ServiceInfo service, int port, int loadLevel) throws Exception;

    List<Endpoint> find(String serviceName) throws Exception;
}
