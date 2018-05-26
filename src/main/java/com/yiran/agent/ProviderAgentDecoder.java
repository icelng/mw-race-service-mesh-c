package com.yiran.agent;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.List;

/**
 * 自定义协议解码
 */
public class ProviderAgentDecoder extends ByteToMessageDecoder {
    private static Logger logger = LoggerFactory.getLogger(ProviderAgentDecoder.class);

    private static final int HEADER_LENGTH = 12;

    private boolean isHeader = true;
    private AgentServiceRequest agentServiceRequest;

    private int dataLength;



    /*对客户端发来的请求进行解码*/
    @Override
    protected void decode(ChannelHandlerContext ctx, ByteBuf in, List<Object> out) throws Exception {
        if (isHeader) {
            /*表示正在接收头部*/
            if (in.readableBytes() < HEADER_LENGTH){
                return;
            } else {
                /*解析头部*/
                agentServiceRequest = AgentServiceRequest.get();
                /*获取requestId*/
                agentServiceRequest.setRequestId(in.readLong());
                /*获取数据长度*/
                dataLength = in.readInt();
                isHeader = false;
            }
        }

        /*接收数据*/
        if (in.readableBytes() < dataLength) {
            return;
        }
        in.readBytes(agentServiceRequest.getData());

        out.add(agentServiceRequest);

        /*去掉引用*/
        agentServiceRequest = null;

        isHeader = true;
    }

    public static void main(String[] args){
        byte[] intBytes = new byte[4];
        intBytes[0] = 32;
        intBytes[1] = 0;
        intBytes[2] = 0;
        intBytes[3] = -1;
        int size;
        size = ((intBytes[0] << 24) | (intBytes[1] << 16) | (intBytes[2] << 8) | (intBytes[3] & 0xFF));
        System.out.println(size);

    }

}
