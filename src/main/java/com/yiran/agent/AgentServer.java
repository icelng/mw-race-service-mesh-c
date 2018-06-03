package com.yiran.agent;

import com.yiran.ServiceSwitcher;
import com.yiran.dubbo.DubboConnectManager;
import com.yiran.registry.EtcdRegistry;
import com.yiran.registry.EtcdSecondRegistry;
import com.yiran.registry.IRegistry;
import com.yiran.registry.ServiceInfo;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.buffer.PooledByteBufAllocator;
import io.netty.channel.*;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class AgentServer {
    private static Logger logger = LoggerFactory.getLogger(AgentServer.class);
    private EtcdSecondRegistry registry;

    private int port;

    public AgentServer(int port){
        this.port = port;
    }

    public void run() throws Exception {

        /*启动netty服务*/
        logger.info("Starting netty server for agent...");
        EventLoopGroup bossGroup = new NioEventLoopGroup(1);
        EventLoopGroup workerGroup = new NioEventLoopGroup(16);
        ServerBootstrap b = new ServerBootstrap();

        b.group(bossGroup, workerGroup)
                .channel(NioServerSocketChannel.class)
                .childHandler(new ChannelInitializer<SocketChannel>() {
                    @Override
                    protected void initChannel(SocketChannel ch) throws Exception {
                        ch.pipeline().addLast(new ProviderAgentDecoder());
                        ch.pipeline().addLast(new ProviderAgentEncoder());
                        ch.pipeline().addLast(new ProviderAgentServerHandler());
                    }
                })
                .option(ChannelOption.SO_BACKLOG, 128)
                .childOption(ChannelOption.SO_KEEPALIVE, true)
                .childOption(ChannelOption.TCP_NODELAY, true)
                .childOption(ChannelOption.ALLOCATOR, PooledByteBufAllocator.DEFAULT);
        b.bind(port).sync();

        /*向etcd注册服务*/
        logger.info("Register service!");
        registry = new EtcdSecondRegistry(System.getProperty("etcd.url"));
        int loadLevel = Integer.valueOf(System.getProperty("load.level"));
//        if (loadLevel != 3) {
//            return;  // 只启动large
//        }
        try {
            registry.register("com.alibaba.dubbo.performance.demo.provider.IHelloService", this.port, loadLevel);
        } catch (Exception e) {
            logger.error("Failed to register service!:{}", e);
            return;
        }
        logger.info("Register success!");

    }

    public static void main(String[] args) throws Exception {
        try {
            logger.info("starting server");
            new AgentServer(2334).run();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
}