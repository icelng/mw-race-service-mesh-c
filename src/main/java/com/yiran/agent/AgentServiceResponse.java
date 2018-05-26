package com.yiran.agent;

public class AgentServiceResponse {
    private long requestId;
    private byte[] returnValue;

    public AgentServiceResponse(){

    }


    public byte[] getReturnValue() {
        return returnValue;
    }

    public void setReturnValue(byte[] returnValue) {
        this.returnValue = returnValue;
    }

    public long getRequestId() {
        return requestId;
    }

    public void setRequestId(long requestId) {
        this.requestId = requestId;
    }
}
