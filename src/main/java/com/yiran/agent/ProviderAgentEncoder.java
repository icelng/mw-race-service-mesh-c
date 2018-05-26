package com.yiran.agent;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToByteEncoder;

/**
 * 自定义协议编码
 */
public class ProviderAgentEncoder extends MessageToByteEncoder {
    private static final int RETURN_VALUE_SIZE_ALIGN_BIT = 2;  // 返回值大小对齐 2^2=4

    @Override
    protected void encode(ChannelHandlerContext ctx, Object msg, ByteBuf out) throws Exception {
        AgentServiceResponse agentServiceResponse = (AgentServiceResponse) msg;
        byte[] data = agentServiceResponse.getReturnValue();
        out.writeLong(agentServiceResponse.getRequestId());
        out.writeInt(data.length);
        out.writeBytes(data);
    }
}
