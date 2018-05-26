package com.yiran.agent;

import com.google.common.util.concurrent.ListenableFuture;
import com.yiran.ServiceSwitcher;
import com.yiran.registry.ServiceInfo;
import io.netty.bootstrap.Bootstrap;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.PooledByteBufAllocator;
import io.netty.channel.Channel;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioSocketChannel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.ResponseEntity;
import org.springframework.web.context.request.async.DeferredResult;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

public class AgentClient {
    private String name;
    private String host;
    private int port;
    private Channel channel;
    private AtomicLong requestId = new AtomicLong(0);

    /*表示正在处理的请求数，负载均衡用*/
    private AtomicLong processingRequestNum;
    private int loadLevel;

    /*支持的服务*/
    private ConcurrentHashMap<String, ServiceInfo> supportedServiceMap;

    private Bootstrap bootstrap;


    private static Logger logger = LoggerFactory.getLogger(AgentClient.class);


    public AgentClient(String host, int port){
        this.host = host;
        this.port = port;
        this.name = host + ":" + String.valueOf(port);
        processingRequestNum = new AtomicLong(0);
        supportedServiceMap = new ConcurrentHashMap<>();
    }

    public void run() throws InterruptedException {

        EventLoopGroup workerGroup = new NioEventLoopGroup(16);
        bootstrap = new Bootstrap();
        bootstrap.group(workerGroup);
        bootstrap.channel(NioSocketChannel.class);
        bootstrap.option(ChannelOption.SO_KEEPALIVE, true);
        bootstrap.option(ChannelOption.TCP_NODELAY, true);
        bootstrap.option(ChannelOption.ALLOCATOR, PooledByteBufAllocator.DEFAULT);
        bootstrap.handler(new ChannelInitializer<SocketChannel>() {
            protected void initChannel(SocketChannel ch) throws Exception {
                ch.pipeline().addLast(new ConsumerAgentDecoder());
                ch.pipeline().addLast(new ConsumerAgentEncoder());
                ch.pipeline().addLast(new ConsumerAgentClientHandler());
            }
        });

        /*连接*/
        channel = bootstrap.connect(host, port).sync().channel();

    }

    /**
     * 异步发起请求
     * @param serviceId
     * @param methodId
     * @param parameterTypes
     * @param parameters
     * @return
     */
    public ListenableFuture asynRequest(int serviceId, byte methodId, List<Integer> parameterTypes, List<byte[]> parameters){

        return null;
    }


    public AgentServiceRequestFuture request(Channel httpChannel, ByteBuf data) throws Exception {
        long requestId = this.requestId.addAndGet(1);

        AgentServiceRequest agentServiceRequest = AgentServiceRequest.get();
        agentServiceRequest.setRequestId(requestId);
        agentServiceRequest.getData().writeBytes(data);

        AgentServiceRequestFuture future = AgentServiceRequestFuture.getFuture(this, agentServiceRequest, httpChannel);
        AgentServiceRequestHolder.put(String.valueOf(agentServiceRequest.getRequestId()), future);

//        float ppl = ((float) processingRequestNum.get())/((float) loadLevel);
//        logger.info("requestId:{}>>>>>>>>>:{}, loadLevel:{} ppl:{}", agentServiceRequest.getRequestId(), this.getName(), this.getLoadLevel(), ppl);
        channel.writeAndFlush(agentServiceRequest);  // 开始发送报文

        return future;
    }

    public AtomicLong getProcessingRequestNum() {
        return processingRequestNum;
    }

    public void addSupportedService(ServiceInfo service){
        supportedServiceMap.put(service.getServiceName(), service);
    }

    public ServiceInfo getSupportedService(String serviceName){
        return supportedServiceMap.getOrDefault(serviceName, null);
    }

    public int getLoadLevel() {
        return loadLevel;
    }

    public void setLoadLevel(int loadLevel) {
        this.loadLevel = loadLevel;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public void requestReady(){
        this.processingRequestNum.addAndGet(1);
    }

    public void requestDone(){
        this.processingRequestNum.decrementAndGet();
    }
}
