package com.yiran.dubbo;

import com.yiran.ServiceSwitcher;
import com.yiran.dubbo.model.RpcResponse;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;

import java.io.UnsupportedEncodingException;

public class RpcClientHandler extends SimpleChannelInboundHandler<RpcResponse> {

    @Override
    protected void channelRead0(ChannelHandlerContext channelHandlerContext, RpcResponse response) throws UnsupportedEncodingException {
        ServiceSwitcher.responseFromDubbo(response);
    }
}
