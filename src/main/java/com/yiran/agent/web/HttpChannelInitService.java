package com.yiran.agent.web;

import io.netty.channel.ChannelInitializer;
import io.netty.channel.socket.SocketChannel;
import io.netty.handler.codec.http.HttpRequestDecoder;
import io.netty.handler.codec.http.HttpResponseEncoder;

public class HttpChannelInitService extends ChannelInitializer<SocketChannel> {
    @Override
    protected void initChannel(SocketChannel sc)
            throws Exception {
        sc.pipeline().addLast(new HttpResponseEncoder());

        sc.pipeline().addLast(new HttpRequestDecoder());

        sc.pipeline().addLast(new HttpChannelHandler());
    }

}
