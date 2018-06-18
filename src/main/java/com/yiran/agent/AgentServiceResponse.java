package com.yiran.agent;

import io.netty.buffer.ByteBuf;

public class AgentServiceResponse {
    private long requestId;
    private ByteBuf returnValue;

    public AgentServiceResponse(){

    }

    public ByteBuf getReturnValue() {
        return returnValue;
    }

    public void setReturnValue(ByteBuf returnValue) {
        this.returnValue = returnValue;
    }

    public long getRequestId() {
        return requestId;
    }

    public void setRequestId(long requestId) {
        this.requestId = requestId;
    }
}
