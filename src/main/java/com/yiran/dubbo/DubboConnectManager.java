package com.yiran.dubbo;

import io.netty.bootstrap.Bootstrap;
import io.netty.buffer.PooledByteBufAllocator;
import io.netty.buffer.UnpooledByteBufAllocator;
import io.netty.channel.Channel;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.nio.NioSocketChannel;

import java.util.concurrent.atomic.AtomicLong;

public class DubboConnectManager {
    private EventLoopGroup eventLoopGroup = new NioEventLoopGroup(16);

    private Bootstrap bootstrap;

    private AtomicLong channelSelectNum;
    private int connectionNum;
    private Channel channels[];
    private Object lock = new Object();

    public DubboConnectManager(int connectionNum) {
        this.connectionNum = connectionNum;
        eventLoopGroup = new NioEventLoopGroup(connectionNum);
        channels = new Channel[connectionNum];
        channelSelectNum = new AtomicLong(0);
    }

    public void connect() throws InterruptedException {

        if (null == bootstrap) {
            synchronized (lock) {
                if (null == bootstrap) {
                    initBootstrap();
                }
            }
        }

        int port = Integer.valueOf(System.getProperty("dubbo.protocol.port"));
        for (int i = 0;i < connectionNum;i++) {
            channels[i] = bootstrap.connect("127.0.0.1", port).sync().channel();
        }

    }

    public Channel getChannel(){

        if (null == bootstrap) {
            return null;
        }
        return channels[(int) (channelSelectNum.getAndIncrement() % connectionNum)];
    }

    public void initBootstrap() {

        bootstrap = new Bootstrap()
                .group(eventLoopGroup)
                .option(ChannelOption.SO_KEEPALIVE, true)
                .option(ChannelOption.TCP_NODELAY, true)
                .option(ChannelOption.ALLOCATOR, PooledByteBufAllocator.DEFAULT)
                .channel(NioSocketChannel.class)
                .handler(new RpcClientInitializer());
    }
}
