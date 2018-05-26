package com.yiran;

import com.yiran.agent.AgentClient;
import com.yiran.registry.Endpoint;
import com.yiran.registry.EtcdRegistry;
import com.yiran.registry.IRegistry;
import com.yiran.registry.ServiceInfo;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ThreadLocalRandom;

/**
 * 负责服务发现，负责负载均衡，负责agent客户端的管理
 */
public class LoadBalance {
    private static float MAX_PPL = 999999999;
    private static int MAX_TRY_COUNT = 5;
    private static Logger logger = LoggerFactory.getLogger(LoadBalance.class);

    private IRegistry registry;
    private ConcurrentHashMap<String, HashSet<String>>  serviceNameToAgentClientsMap;
    private ConcurrentHashMap<String, AgentClient> clientNameToAgentClientMap;
    private ConcurrentHashMap<Integer, List<String>> loadLevelToAgentClientsMap;
    private final Object lock = new Object();
    private int tryCount = 0;
    private Random random1 = new Random();
    private Random random2 = new Random();

    public LoadBalance(String registryAddress){
        registry = new EtcdRegistry(registryAddress);
        serviceNameToAgentClientsMap = new ConcurrentHashMap<>();
        clientNameToAgentClientMap = new ConcurrentHashMap<>();
        loadLevelToAgentClientsMap = new ConcurrentHashMap<>();
    }

    public boolean findService(String serviceName) throws Exception {
        List<Endpoint> endpoints = registry.find(serviceName);
        if (endpoints == null) {
            return false;
        }
        for (Endpoint endpoint : endpoints) {
            createAgentClient(endpoint);
        }
        return true;
    }

    public AgentClient createAgentClient(Endpoint endpoint) throws InterruptedException {
        String clientName = endpoint.getHost() + ":" + String.valueOf(endpoint.getPort());
        AgentClient agentClient = clientNameToAgentClientMap.getOrDefault(clientName, null);
        if(agentClient == null){
            agentClient = new AgentClient(endpoint.getHost(), endpoint.getPort());
            clientNameToAgentClientMap.put(clientName, agentClient);
            agentClient.setLoadLevel(endpoint.getLoadLevel());
            agentClient.run();
        }

        ServiceInfo service = endpoint.getSupportedService();
        agentClient.addSupportedService(service);
        String serviceName = service.getServiceName();
        HashSet<String> serviceNameAgentClients = serviceNameToAgentClientsMap.computeIfAbsent(serviceName, k -> new HashSet<>());
        serviceNameAgentClients.add(clientName);

        List<String> loadLevelAgentClients = loadLevelToAgentClientsMap.computeIfAbsent(endpoint.getLoadLevel(), k -> new ArrayList<>());
        loadLevelAgentClients.add(clientName);

        return agentClient;
    }

    /**
     * 查找最优的provider对应的agentClient
     * 如果得出最优的客户端，最好要进行请求。如果不请求，则一定要调用放弃强求放弃方法
     * 计算得到最优的客户端的过程中，如果没有带上服务发现，其时间是很短的。
     * @param serviceName
     * @return
     */
    public AgentClient findOptimalAgentClient(String serviceName) throws Exception {
        HashSet<String> agentClientNames;
        do {
            /*查询支持指定服务名的客户端集合*/
            agentClientNames = serviceNameToAgentClientsMap.getOrDefault(serviceName, null);
            if(agentClientNames == null || agentClientNames.size() == 0){
                synchronized (lock) {
                    agentClientNames = serviceNameToAgentClientsMap.getOrDefault(serviceName, null);
                    if(agentClientNames == null || agentClientNames.size() == 0){
                        logger.info("Service {} not found!Checking out etcd..tryCount:{}", serviceName, ++tryCount);
                        /*如果针对服务名找不到对应的客户端，则向ETCD中心查询服务*/
                        findService(serviceName);
                    }
                }
            }
        } while (agentClientNames == null || agentClientNames.size() == 0);

        ///*现在已经找到服务名对应agent客户端的集合，下面选出最优的agent客户端*/
        AgentClient optimalAgentClient = null;
//        float minPPL = MAX_PPL;  //  minimum processingRequestNum per loadLevel
//        for (String clientName : agentClientNames) {
//            AgentClient agentClient = clientNameToAgentClientMap.get(clientName);
//            if(agentClient == null){
//                logger.error("AgentClient {} not found!", clientName);
//                return null;
//            }
//
//            /*计算最小的ppl*/
//            long processingRequestNum = agentClient.getProcessingRequestNum().get();
//            int loadLevel = agentClient.getLoadLevel();
//            float currentPPL = ((float) processingRequestNum)/((float) loadLevel);
//            if (currentPPL < minPPL) {
//                minPPL = currentPPL;
//                optimalAgentClient = agentClient;
//            }
//        }
        optimalAgentClient = getOptimalByRandom();
        if (optimalAgentClient != null) {
            /*这里提前增加了请求数*/
            optimalAgentClient.requestReady();
        }

        return optimalAgentClient;
    }

    private AgentClient getOptimalByRandom() throws Exception {
        int randomNum = ThreadLocalRandom.current().nextInt(60);
        int selectedLoadLevel = 0;

        if (randomNum >= 0 && randomNum < 10) {
            selectedLoadLevel = 1;
        } else if (randomNum >= 10 && randomNum < 30) {
            selectedLoadLevel = 2;
        } else if (randomNum >= 30 && randomNum < 60) {
            selectedLoadLevel = 3;
        }

        List<String> loadLevelAgentClients = loadLevelToAgentClientsMap.getOrDefault(selectedLoadLevel, null);
        if (loadLevelAgentClients == null || loadLevelAgentClients.size() == 0) {
            logger.error("No agentClient for loadLevel:{} when getting optimal client!", selectedLoadLevel);
            return null;
        }

        String agentClientName = loadLevelAgentClients.get(0);
        AgentClient agentClient = clientNameToAgentClientMap.getOrDefault(agentClientName, null);

        return agentClient;
    }

}
