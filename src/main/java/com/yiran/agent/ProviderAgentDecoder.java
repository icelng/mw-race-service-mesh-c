package com.yiran.agent;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.UnpooledByteBufAllocator;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;
import io.netty.util.internal.AppendableCharSequence;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * 自定义协议解码
 */
public class ProviderAgentDecoder extends ByteToMessageDecoder {
    private static Logger logger = LoggerFactory.getLogger(ProviderAgentDecoder.class);

    private static final int HEADER_LENGTH = 12;

    private boolean isHeader = true;
    private AgentServiceRequest agentServiceRequest;

    private int dataLength;
    private byte[] header = new byte[HEADER_LENGTH];

    private ByteBuf formDataTemp = UnpooledByteBufAllocator.DEFAULT.buffer(2048);



    /*对客户端发来的请求进行解码*/
    @Override
    protected void decode(ChannelHandlerContext ctx, ByteBuf in, List<Object> out) throws Exception {
        if (isHeader) {
            /*表示正在接收头部*/
            if (in.readableBytes() < HEADER_LENGTH){
                return;
            } else {
                in.readBytes(header, 0, HEADER_LENGTH);
                /*解析头部*/
                agentServiceRequest = new AgentServiceRequest();
                /*获取requestId*/
                agentServiceRequest.setRequestId(Bytes.bytes2long(header, 0));
                /*获取数据长度*/
                dataLength = Bytes.bytes2int(header, 8);
//                logger.info("requestId:{}, dataLength:{}", agentServiceRequest.getRequestId(), dataLength);
                isHeader = false;
            }
        }

        /*接收数据*/
        if (in.readableBytes() < dataLength) {
            return;
        }

        /*解析表单*/
        agentServiceRequest.setFormDataMap(parseFormData(in, dataLength, formDataTemp));

        out.add(agentServiceRequest);
//        logger.info(agentServiceRequest.getData().toString(Charset.forName("utf-8")));

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

    private int hex2dec(byte c) {
        if ('0' <= c && c <= '9') {
            return c - '0';
        } else if ('a' <= c && c <= 'f') {
            return c - 'a' + 10;
        } else if ('A' <= c && c <= 'F') {
            return c - 'A' + 10;
        } else {
            return -1;
        }
    }

    private Map<String, String> parseFormData(ByteBuf src, int length, ByteBuf formDataTemp) {
        Map<String, String> formDataMap = new HashMap<>();  // 表单map
        formDataTemp.clear();
        String key = null;

        for (int i = 1;i <= length;i++) {
            byte c = src.readByte();

            if (i == length || c == '&') {
                /*value结束*/
                if (c == '=') {
                    /*有可能以等号的结束,说明key对应的value是空的*/
                    key = formDataTemp.toString();
                    formDataMap.put(key, "");
                } else if (i == length) {
                    /*如果value以字符串结束来结束的*/
                    formDataTemp.writeByte(c);
                    formDataMap.put(key, formDataTemp.toString());
                } else if (formDataTemp.readableBytes() == 0){
                    /*如果是以&结束的value为空*/
                    formDataMap.put(key, "");
                } else {
                    formDataMap.put(key, formDataTemp.toString());
                }
                //logger.info("key:{}  value:{}", key, formDataMap.get(key));
                formDataTemp.clear();
                continue;
            }

            if (c == '=') {
                /*key结束*/
                key = formDataTemp.toString();
                formDataTemp.clear();
                continue;
            }


            if (c != '%') {
                formDataTemp.writeByte(c);
            } else {
                byte c1 = src.readByte();
                byte c0 = src.readByte();
                int num = hex2dec(c1) * 16 + hex2dec(c0);
                formDataTemp.writeByte((byte) (num & 0xFF));
                if (src.readableBytes() == 0) {
                    formDataMap.put(key, formDataTemp.toString());
                }
            }

        }

        return formDataMap;
    }

}
