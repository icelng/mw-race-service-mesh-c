package com.yiran.agent;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToByteEncoder;

/**
 * 自定义协议编码
 */
public class ProviderAgentEncoder extends MessageToByteEncoder {
    private static final int RETURN_VALUE_SIZE_ALIGN_BIT = 2;  // 返回值大小对齐 2^2=4
    private static final int HEADER_LENGTH = 12;
    private byte[] header = new byte[HEADER_LENGTH];

    @Override
    protected void encode(ChannelHandlerContext ctx, Object msg, ByteBuf out) throws Exception {
        AgentServiceResponse agentServiceResponse = (AgentServiceResponse) msg;
        ByteBuf data = agentServiceResponse.getReturnValue();
        Bytes.long2bytes(agentServiceResponse.getRequestId(), header, 0);
        Bytes.int2bytes(data.readableBytes(), header, 8);
        out.writeBytes(header, 0, HEADER_LENGTH);
        out.writeBytes(data);
    }
}
