package com.yiran;

import com.alibaba.fastjson.JSON;
import com.yiran.agent.AgentServiceRequest;
import com.yiran.agent.AgentServiceResponse;
import com.yiran.agent.Bytes;
import com.yiran.agent.web.FormDataParser;
import com.yiran.dubbo.model.JsonUtils;
import com.yiran.dubbo.model.Request;
import com.yiran.dubbo.model.RpcInvocation;
import com.yiran.dubbo.model.RpcResponse;
import com.yiran.registry.ServiceInfo;
import io.netty.channel.Channel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.*;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;

/**
 * 单例,主要做服务协议转换
 */
public class ServiceSwitcher {
    private static Logger logger = LoggerFactory.getLogger(ServiceSwitcher.class);
    private static Channel rpcClientChannel = null;
    private static CountDownLatch rpcChannelReady = new CountDownLatch(1);
    /*使用可并发Hash表*/
    private static ConcurrentHashMap<String, AgentServiceRequest> processingRequest = new ConcurrentHashMap<>();

    /*一个转换两个表,使用普通Hash表，但是要保证一次操作两个表*/
    /*service*/
    private static Map<Integer, ServiceInfo> serviceIdMap = new HashMap<>();
    private static Map<String, ServiceInfo> serviceNameMap = new HashMap<>();


    /**
     * 设置实现Dubbo协议的Netty客户端通道(Channel)
     * @param client
     * 客户端Channel
     */
    public static void setRpcClientChannel(Channel client){
        rpcClientChannel = client;
        rpcChannelReady.countDown();
    }

    /**
     * 服务协议转换至Dubbo
     * @param agentServiceRequest
     * agent 接收到的请求
     * @param formData
     * 接收请求对应的Netty Channel
     * @throws IOException
     */
    public static void switchToDubbo(AgentServiceRequest agentServiceRequest, Map<String, String> formData) throws IOException {
        try {
            /*转换服务协议之前，一定要保证已经跟dubbo建立好连接*/
            rpcChannelReady.await();
        } catch (InterruptedException e) {
            logger.error(e.getMessage());
            return;
        }

        long requestId = agentServiceRequest.getRequestId();
//        logger.info("Switch service for requestId:{}", requestId);
        //FormDataParser formDataParser = FormDataParser.get();
        //Map<String, String> argumentsMap = formDataParser.parse(agentServiceRequest.getData());
        //formDataParser.release();

        RpcInvocation invocation = new RpcInvocation();
        invocation.setMethodName(formData.get("method"));
        invocation.setAttachment("path", formData.get("interface"));
        /*先写死一个参数*/
        String parameterTypeName = formData.get("parameterTypesString");
        invocation.setParameterTypes(parameterTypeName);    // Dubbo内部用"Ljava/lang/String"来表示参数类型是String

        /*转换参数，先固定成一个，并且是String类型的*/
        String parameter = formData.get("parameter");
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        PrintWriter writer = new PrintWriter(new OutputStreamWriter(out));
        JsonUtils.writeObject(parameter, writer);
        invocation.setArguments(out.toByteArray());

        Request request = Request.get();
        request.setVersion("2.0.0");
        request.setTwoWay(true);
        request.setData(invocation);
        request.setId(requestId);

        processingRequest.put(String.valueOf(requestId), agentServiceRequest);

        rpcClientChannel.writeAndFlush(request);
    }

    /**
     * 响应转换
     * @param rpcResponse
     * Dubbo响应报文
     */
    public static void responseFromDubbo(RpcResponse rpcResponse) throws UnsupportedEncodingException {

        /*获取到rpc请求相应的agent的请求*/
        AgentServiceRequest agentServiceRequest = processingRequest.get(String.valueOf(rpcResponse.getRequestId()));
        if (agentServiceRequest == null){
            logger.warn("No rpcResponse for requestId:{}", rpcResponse.getRequestId());
            return;
        }
        logger.info("Receive response(id:{}) from provider:{}", rpcResponse.getRequestId(), new String(rpcResponse.getBytes()));
        processingRequest.remove(String.valueOf(rpcResponse.getRequestId()));

        /*获取得到consumer-agent 与 provider-agent之间的Channel*/
        Channel agentChannel =  agentServiceRequest.getChannel();

        /*生成响应报文*/
        AgentServiceResponse agentServiceResponse = new AgentServiceResponse();
        agentServiceResponse.setRequestId(agentServiceRequest.getRequestId());
        agentServiceRequest.release();

        /*设置返回值为整形，hashcode*/
        //String returnStr = new String(rpcResponse.getBytes(), "utf-8");
        //String intStr = returnStr.substring(2, returnStr.length() - 1);
        //byte[] intBytes = new byte[4];
        //Bytes.int2bytes(Integer.valueOf(intStr), intBytes, 0);
        agentServiceResponse.setReturnValue(rpcResponse.getBytes());

        /*向客户端发送响应*/
        agentChannel.writeAndFlush(agentServiceResponse);

    }


    public static String serviceIdToName(int serviceId){
        ServiceInfo service = serviceIdMap.getOrDefault(serviceId, null);
        if (service == null) {
            logger.error("No service for id:" + serviceId);
            return null;
        }

        return service.getServiceName();
    }

    public static int serviceNameToId(String serviceName){
        ServiceInfo service = serviceNameMap.getOrDefault(serviceName, null);
        if (service == null) {
            logger.error("No service for serviceName:" + serviceName);
            return 0;
        }

        return service.getServiceId();
    }

    public static String methodIdToName(int serviceId, int methodId){
        ServiceInfo service = serviceIdMap.getOrDefault(serviceId, null);
        if (service == null) {
            logger.error("No service for id:" + serviceId);
            return null;
        }

        String methodName = service.getMethodName(methodId);
        if (methodName == null) {
            logger.error("No methodName for id:{} in service:{}", methodId, service.getServiceName());
            return null;
        }

        return methodName;
    }

    public static int methodNameToId(int serviceId, String methodName){
        ServiceInfo service = serviceIdMap.getOrDefault(serviceId, null);
        if (service == null) {
            logger.error("No service for id:" + serviceId);
            return 0;
        }

        int methodId = service.getMethodId(methodName);
        if (methodId == 0) {
            logger.error("No methodId for name:{} in service:{}", methodName, service.getServiceName());
            return 0;
        }

        return methodId;
    }

    public static String parameterTypeIdToName(int serviceId, int id){
        ServiceInfo service = serviceIdMap.getOrDefault(serviceId, null);
        if (service == null) {
            logger.error("No service for id:" + serviceId);
            return null;
        }

        String typeName = service.getParameterTypeName(id);
        if(typeName == null){
            logger.error("No name for parameterTypeId:{} in service:{}", id, service.getServiceName());
            return null;
        }

        return typeName;
    }

    public static int parameterTypeNameToId(int serviceId, String name){
        ServiceInfo service = serviceIdMap.getOrDefault(serviceId, null);
        if (service == null) {
            logger.error("No service for id:" + serviceId);
            return 0;
        }

        int typeId = service.getParameterTypeId(name);
        if(typeId == 0){
            logger.error("No id for parameterType:{} in service:{}", name, service.getServiceName());
            return 0;
        }

        return typeId;
    }

    /**
     * 添加服务支持
     * @param service
     */
    synchronized public static void addSupportedService(ServiceInfo service){
        serviceIdMap.put(service.getServiceId(), service);
        serviceNameMap.put(service.getServiceName(), service);
    }

}
