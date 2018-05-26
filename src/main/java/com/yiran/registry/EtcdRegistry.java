package com.yiran.registry;

import com.coreos.jetcd.Client;
import com.coreos.jetcd.KV;
import com.coreos.jetcd.Lease;
import com.coreos.jetcd.data.ByteSequence;
import com.coreos.jetcd.data.KeyValue;
import com.coreos.jetcd.kv.GetResponse;
import com.coreos.jetcd.options.GetOption;
import com.coreos.jetcd.options.PutOption;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.text.MessageFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executors;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class EtcdRegistry implements IRegistry{
    private Logger logger = LoggerFactory.getLogger(EtcdRegistry.class);
    // 该EtcdRegistry没有使用etcd的Watch机制来监听etcd的事件
    // 添加watch，在本地内存缓存地址列表，可减少网络调用的次数
    // 使用的是简单的随机负载均衡，如果provider性能不一致，随机策略会影响性能

    private final String rootPath = "dubbomesh";
    private Lease lease;
    private KV kv;
    private long leaseId;

    public EtcdRegistry(String registryAddress) {
        Client client = Client.builder().endpoints(registryAddress).build();
        this.lease   = client.getLeaseClient();
        this.kv      = client.getKVClient();
        try {
            this.leaseId = lease.grant(30).get().getID();
        } catch (Exception e) {
            e.printStackTrace();
        }
        keepAlive();
    }

    private void registMap(String serviceName, String mapName, Map<Integer, String> map) throws ExecutionException, InterruptedException {
        String prefix = MessageFormat.format("/{0}/{1}/{2}", rootPath, serviceName, mapName);
        for(int id : map.keySet()){
            String keyStr = MessageFormat.format("{0}/{1}/{2}", prefix, id, map.get(id));
            logger.info("Register key:{}", keyStr);
            ByteSequence key = ByteSequence.fromString(keyStr);
            ByteSequence val = ByteSequence.fromString("");
            kv.put(key,val, PutOption.newBuilder().withLeaseId(leaseId).build()).get();
        }
    }

    // 向ETCD中注册服务
    public void register(ServiceInfo serviceInfo, int port, int loadLevel) throws Exception {
        // 服务注册的key为:    /dubbomesh/com.some.package.IHelloService/192.168.100.100:2000
        String serviceName = serviceInfo.getServiceName();


        /*注册服务名*/
        logger.info("Register serviceName");
        String strKey = MessageFormat.format("/{0}/{1}/{2}",rootPath, serviceName, serviceInfo.getServiceId());
        ByteSequence key = ByteSequence.fromString(strKey);
        ByteSequence val = ByteSequence.fromString("");
        kv.put(key,val, PutOption.newBuilder().withLeaseId(leaseId).build()).get();

        /*注册方法*/
        /*有可能冲突的，暂不处理*/
        logger.info("Register method");
        registMap(serviceName, "method", serviceInfo.getMethodMap());

        /*注册参数类型*/
        /*有可能冲突的，暂不处理*/
        logger.info("Register parameterType");
        registMap(serviceName, "parameterType", serviceInfo.getParameterTypeMap());

        /*注册节点信息*/
        strKey = MessageFormat.format("/{0}/{1}/endpoints/{2}:{3}",rootPath,serviceName,IpHelper.getHostIp(),String.valueOf(port));
        logger.info("Register a new service at:" + strKey);
        key = ByteSequence.fromString(strKey);
        val = ByteSequence.fromString(String.valueOf(loadLevel));
        kv.put(key,val, PutOption.newBuilder().withLeaseId(leaseId).build()).get();
    }

    // 发送心跳到ETCD,表明该host是活着的
    public void keepAlive(){
        Executors.newSingleThreadExecutor().submit(
                () -> {
                    try {
                        Lease.KeepAliveListener listener = lease.keepAlive(leaseId);
                        listener.listen();
                        logger.info("KeepAlive lease:" + leaseId + "; Hex format:" + Long.toHexString(leaseId));
                    } catch (Exception e) { e.printStackTrace(); }
                }
        );
    }


    public List<Endpoint> find(String serviceName){
       /*获取服务对应的所有信息*/
        String strKey = MessageFormat.format("/{0}/{1}",rootPath,serviceName);
        ByteSequence key  = ByteSequence.fromString(strKey);
        GetResponse response = null;
        try {
            response = kv.get(key, GetOption.newBuilder().withPrefix(key).build()).get();
        } catch (Exception e) {
            logger.error("Failed to get key-values:{}", e.getMessage());
            return null;
        }

        List<KeyValue> kvs = response.getKvs();
        if(kvs == null || kvs.size() == 0){
            logger.error("The response have not key-values");
            return null;
        }

        /*解析服务信息*/
        String methodsRegx = "^/(.*?)/(.*?)/(method)/(\\d+)/(\\w+)";
        String parameterTypesRegx = "^/(.*?)/(.*?)/(parameterType)/(\\d+)/(.+)";
        String endpointsRegx = "^/(.*?)/(.*?)/(endpoints)/(.+?):(\\d+)";
        String serviceNameRegx = "^/(.*?)/(.*?)/(\\d+)";
        List<Endpoint> endpoints = new ArrayList<>();
        ServiceInfo serviceInfo = new ServiceInfo();
        for (com.coreos.jetcd.data.KeyValue kv : kvs){
            String keyStr = kv.getKey().toStringUtf8();
            logger.info("Get a key:{}", keyStr);
            if (keyStr.matches(methodsRegx)){
                /*如果是方法名与id映射*/
                /*使用正则从key获取信息*/
                Pattern p = Pattern.compile(methodsRegx);
                Matcher m = p.matcher(keyStr);
                m.find();
                if (m.groupCount() == 5) {
                    serviceInfo.setMethod(Integer.parseInt(m.group(4)), m.group(5));
                    logger.info("Get method---id:{}  name:{}", m.group(4), m.group(5));
                }
            } else if (keyStr.matches(parameterTypesRegx)){
                /*如果是参数;注意，是参数类型与Id的映射表，不是方法对应参数*/
                Pattern p = Pattern.compile(parameterTypesRegx);
                Matcher m = p.matcher(keyStr);
                m.find();
                if (m.groupCount() == 5) {
                    serviceInfo.setParameterType(Integer.parseInt(m.group(4)), m.group(5));
                    logger.info("Get parameterType---id:{}  name:{}", m.group(4), m.group(5));
                }
            } else if (keyStr.matches(endpointsRegx)){
                /*如果是节点信息*/
                Pattern p = Pattern.compile(endpointsRegx);
                Matcher m = p.matcher(keyStr);
                m.find();
                if(m.groupCount() == 5){
                    String host = m.group(4);
                    int port = Integer.parseInt(m.group(5));
                    int loadLevel = Integer.parseInt(kv.getValue().toStringUtf8());
                    logger.info("Get endpoint---ip:{}  port:{}  loadLevel:{}", host, port, loadLevel);
                    Endpoint endpoint = new Endpoint(host, port, loadLevel);
                    endpoint.setSupportedService(serviceInfo);
                    endpoints.add(endpoint);
                }
            } else if(keyStr.matches(serviceNameRegx)) {
                Pattern p = Pattern.compile(serviceNameRegx);
                Matcher m = p.matcher(keyStr);
                m.find();
                if (m.groupCount() == 3) {
                    serviceInfo.setServiceId(Integer.parseInt(m.group(3)));
                    serviceInfo.setServiceName(m.group(2));
                    logger.info("Get serviceName---id:{}  name:{}", m.group(3), m.group(2));
                }
            } else {
                logger.info("No match for:{}", keyStr);
            }
        }

        return endpoints;
    }

    public static void main(String[] args){
        String rootPath = "root";
        String serviceName = "com.alibaba.performance.dubbomesh.provider.IHelloService";
        String methodName = "hash";
        String testStr = MessageFormat.format("/{0}/{1}/methods/1/{2}", rootPath, serviceName, methodName);
//        String enpointStr = MessageFormat.format("/{0}/{1}/endpoints/192.168.0.1:3000", rootPath, serviceName);
        String enpointStr = "/dubbomesh/com.alibaba.dubbo.performance.demo.provider.IHelloService/endpoints/127.0.0.1:30000";

        String regex = "^/(.*?)/(.*?)/(method)/(\\d+)/(.+)";
        String regexEndpoints = "^/(.*?)/(.*?)/(endpoints)/(.*?):(\\d+)";
        if(enpointStr.matches(regexEndpoints)) {
            System.out.print("匹配！\n");
            Pattern pattern = Pattern.compile(regexEndpoints);
            Matcher matcher = pattern.matcher(enpointStr);
            System.out.println("group count:" + matcher.groupCount());
            if (matcher.find()) {
                System.out.println("Found value:" + matcher.group(0));
                System.out.println("Found value:" + matcher.group(1));
                System.out.println("Found value:" + matcher.group(2));
                System.out.println("Found value:" + matcher.group(3));
                System.out.println("Found value:" + matcher.group(4));
                System.out.println("Found value:" + matcher.group(5));
            }
        }

    }
}
