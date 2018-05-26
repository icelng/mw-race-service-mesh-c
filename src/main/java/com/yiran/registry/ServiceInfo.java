package com.yiran.registry;

import java.util.HashMap;
import java.util.Map;

public class ServiceInfo {
    private int serviceId;
    private String serviceName;
    private Map<String, Integer> methodNameToIdMap;
    private Map<Integer, String> methodIdToNameMap;
    private Map<String, Integer> parameterTypeNameToIdMap;
    private Map<Integer, String> parameterTypeIdToNameMap;

    public ServiceInfo(){
        methodIdToNameMap = new HashMap<>();
        methodNameToIdMap = new HashMap<>();
        parameterTypeIdToNameMap = new HashMap<>();
        parameterTypeNameToIdMap = new HashMap<>();
    }

    public int getServiceId() {
        return serviceId;
    }

    public void setServiceId(int serviceId) {
        this.serviceId = serviceId;
    }

    public String getServiceName() {
        return serviceName;
    }

    public void setServiceName(String serviceName) {
        this.serviceName = serviceName;
    }

    synchronized public void setMethod(int methodId, String methodName){
        this.methodNameToIdMap.put(methodName, methodId);
        this.methodIdToNameMap.put(methodId, methodName);
    }

    synchronized public void setParameterType(int typeId, String typeName){
        this.parameterTypeNameToIdMap.put(typeName, typeId);
        this.parameterTypeIdToNameMap.put(typeId, typeName);
    }

    public int getMethodId(String methodName){
        return this.methodNameToIdMap.getOrDefault(methodName, 0);
    }

    public String getMethodName(int methodId){
        return this.methodIdToNameMap.getOrDefault(methodId, null);
    }

    public int getParameterTypeId(String typeName){
        return this.parameterTypeNameToIdMap.getOrDefault(typeName, 0);
    }

    public String getParameterTypeName(int typeId){
        return this.parameterTypeIdToNameMap.getOrDefault(typeId, null);
    }

    public Map<Integer, String> getMethodMap(){
        return this.methodIdToNameMap;
    }

    public Map<Integer, String> getParameterTypeMap(){
        return this.parameterTypeIdToNameMap;
    }
}
