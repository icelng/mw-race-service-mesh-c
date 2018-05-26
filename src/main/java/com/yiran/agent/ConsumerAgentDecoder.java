package com.yiran.agent;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;

import java.util.List;

public class ConsumerAgentDecoder extends ByteToMessageDecoder {
    private static final int HEADER_LENGTH = 12;
    private static final int PARAMETER_SIZE_ALIGN_BIT = 2;  // 参数大小对齐


    private AgentServiceResponse agentServiceResponse;
    private int dataLength;
    private boolean isHeader = true;

    /*接收响应解码*/
    protected void decode(ChannelHandlerContext ctx, ByteBuf in, List<Object> out) throws Exception {
        /*接收头部*/
        if (isHeader) {
            if(in.readableBytes() <  HEADER_LENGTH){
                return;
            }
            /*解析头部*/
            agentServiceResponse = new AgentServiceResponse();
            agentServiceResponse.setRequestId(in.readLong());
            dataLength = in.readInt();

            isHeader = false;
        }

        /*接收返回值*/
        if (in.readableBytes() < dataLength) {
            return;
        }
        byte[] returnValue = new byte[dataLength];
        in.readBytes(returnValue, 0, dataLength);
        agentServiceResponse.setReturnValue(returnValue);

        out.add(agentServiceResponse);
        /*释放引用*/
        agentServiceResponse = null;

        /*下一个字节开始是头*/
        isHeader = true;
    }
}
