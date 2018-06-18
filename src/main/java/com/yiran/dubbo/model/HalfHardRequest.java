package com.yiran.dubbo.model;

import io.netty.buffer.ByteBuf;

public class HalfHardRequest {

    private long requestId;
    private ByteBuf data;
    private ByteBuf serviceName;
    private ByteBuf method;
    private ByteBuf parameterTypes;
    private ByteBuf parameter;

    public long getRequestId() {
        return requestId;
    }

    public void setRequestId(long requestId) {
        this.requestId = requestId;
    }

    public ByteBuf getServiceName() {
        return serviceName;
    }

    public void setServiceName(ByteBuf serviceName) {
        this.serviceName = serviceName;
    }

    public ByteBuf getMethod() {
        return method;
    }

    public void setMethod(ByteBuf method) {
        this.method = method;
    }

    public ByteBuf getParameterTypes() {
        return parameterTypes;
    }

    public void setParameterTypes(ByteBuf parameterTypes) {
        this.parameterTypes = parameterTypes;
    }

    public ByteBuf getParameter() {
        return parameter;
    }

    public void setParameter(ByteBuf parameter) {
        this.parameter = parameter;
    }

    public ByteBuf getData() {
        return data;
    }

    public void setData(ByteBuf data) {
        this.data = data;
    }
}
