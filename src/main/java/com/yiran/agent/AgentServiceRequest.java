package com.yiran.agent;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.PooledByteBufAllocator;
import io.netty.channel.Channel;
import io.netty.util.Recycler;


public class AgentServiceRequest {
    private static final Recycler<AgentServiceRequest> RECYCLER = new Recycler<AgentServiceRequest>() {
        @Override
        protected AgentServiceRequest newObject(Handle<AgentServiceRequest> handle) {
            return new AgentServiceRequest(handle);
        }
    };

    /*对象池使用*/
    private Recycler.Handle<AgentServiceRequest> recyclerHandle;

    /*Netty用的成员变量*/
    private Channel channel;

    private long requestId;
    private ByteBuf data;

    public static AgentServiceRequest get(){
        return RECYCLER.get();
    }

    public AgentServiceRequest(Recycler.Handle<AgentServiceRequest> handle){
        this.data = PooledByteBufAllocator.DEFAULT.buffer(2048);
        this.recyclerHandle = handle;
    }

    public void release(){
        this.data.clear();
        recyclerHandle.recycle(this);
    }

    public Channel getChannel() {
        return channel;
    }

    public void setChannel(Channel channel) {
        this.channel = channel;
    }

    public ByteBuf getData() {
        return data;
    }

    public void setData(ByteBuf data) {
        this.data = data;
    }

    public long getRequestId() {
        return requestId;
    }

    public void setRequestId(long requestId) {
        this.requestId = requestId;
    }
}
