package com.yiran.agent;

import io.netty.buffer.Unpooled;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFutureListener;
import io.netty.handler.codec.http.*;
import io.netty.util.Recycler;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.UnsupportedEncodingException;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

public class AgentServiceRequestFuture implements Future<AgentServiceResponse> {
    private static Logger logger = LoggerFactory.getLogger(AgentServiceRequestFuture.class);
    private static final Recycler<AgentServiceRequestFuture> RECYCLER = new Recycler<AgentServiceRequestFuture>() {
        @Override
        protected AgentServiceRequestFuture newObject(Handle<AgentServiceRequestFuture> handle) {
            return new AgentServiceRequestFuture(handle);
        }
    };

    private static ScheduledExecutorService timeoutExecutorService = Executors.newScheduledThreadPool(4);

    private Recycler.Handle<AgentServiceRequestFuture> recyclerHandle;
    private long requestId;
    private AgentClient agentClient;

    private CountDownLatch latch = new CountDownLatch(1);
    private AgentServiceResponse agentServiceResponse = null;
    private AtomicBoolean isCancelled = new AtomicBoolean(false);
    private AtomicBoolean isDone = new AtomicBoolean(false);

    private AgentServiceRequest agentServiceRequest;

    private Channel httpChannel;

    public AgentServiceRequestFuture(AgentClient agentClient, AgentServiceRequest agentServiceRequest, Channel httpChannel){
        this.agentServiceRequest = agentServiceRequest;
        this.requestId = agentServiceRequest.getRequestId();
        this.agentClient = agentClient;
        this.httpChannel = httpChannel;
    }

    public AgentServiceRequestFuture(Recycler.Handle<AgentServiceRequestFuture> handle) {
        recyclerHandle = handle;
    }

    public static AgentServiceRequestFuture getFuture(AgentClient agentClient, AgentServiceRequest agentServiceRequest, Channel httpChannel){
        AgentServiceRequestFuture agentServiceRequestFuture = RECYCLER.get();
        agentServiceRequestFuture.agentServiceRequest = agentServiceRequest;
        agentServiceRequestFuture.requestId = agentServiceRequest.getRequestId();
        agentServiceRequestFuture.agentClient = agentClient;
        agentServiceRequestFuture.httpChannel = httpChannel;
        return agentServiceRequestFuture;
    }

    public void release(){
        latch = null;
        agentServiceResponse = null;
        agentServiceRequest = null;
        agentClient = null;
        httpChannel = null;
        isCancelled.set(false);
        isDone.set(false);
        recyclerHandle.recycle(this);
    }


    public boolean cancel(boolean mayInterruptIfRunning) {
        return false;
    }

    public boolean isCancelled() {
        return isCancelled.get();
    }

    public boolean isDone() {
        return isDone.get();
    }

    public AgentServiceResponse get() throws InterruptedException{
        latch.await();
        return agentServiceResponse;
    }

    public AgentServiceResponse get(long timeout, TimeUnit unit) throws InterruptedException{
        latch.await(timeout, unit);
        return agentServiceResponse;
    }


    public void done(AgentServiceResponse response) throws UnsupportedEncodingException {
        if (response != null) {
            int hashCode = Bytes.bytes2int(response.getReturnValue(), 0);
            String hashCodeString = String.valueOf(hashCode);
//            logger.info("Return hash code:{}", hashCodeString);
            DefaultFullHttpResponse httpResponse = new DefaultFullHttpResponse(HttpVersion.HTTP_1_1,HttpResponseStatus.OK, Unpooled.wrappedBuffer(hashCodeString.getBytes("utf-8")));
            setHeaders(httpResponse);
            httpChannel.writeAndFlush(httpResponse).addListener(ChannelFutureListener.CLOSE);
            agentServiceRequest.release();
        } else {
            logger.error("Request:{} error!", requestId);
        }
    }



    public long getRequestId() {
        return requestId;
    }

    private void setHeaders(FullHttpResponse response) {
        response.headers().set(HttpHeaderNames.CONTENT_TYPE, "text/html; charset=UTF-8");
    }
}
