package com.yiran.dubbo.model;

import io.netty.buffer.ByteBuf;

public class RpcResponse {

    private long requestId;
    private ByteBuf retValue;

    public long getRequestId() {
        return requestId;
    }

    public void setRequestId(long requestId) {
        this.requestId = requestId;
    }

    public ByteBuf getRetValue() {
        return retValue;
    }

    public void setRetValue(ByteBuf retValue) {
        this.retValue = retValue;
    }
}
