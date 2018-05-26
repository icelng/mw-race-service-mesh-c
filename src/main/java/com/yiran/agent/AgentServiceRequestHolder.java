package com.yiran.agent;

import java.util.concurrent.ConcurrentHashMap;

public class AgentServiceRequestHolder {

    private static ConcurrentHashMap<String, AgentServiceRequestFuture> processingRequest = new ConcurrentHashMap<String, AgentServiceRequestFuture>();

    public static void put(String requestId, AgentServiceRequestFuture future) {
        processingRequest.put(requestId, future);
    }

    public static AgentServiceRequestFuture get(String requestId) {
        return processingRequest.get(requestId);
    }

    public static void remove(String requestId) {
        processingRequest.remove(requestId);
    }
}
