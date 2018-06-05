package com.yiran.agent;

import com.yiran.ServiceSwitcher;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.UnpooledByteBufAllocator;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.util.internal.AppendableCharSequence;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.nio.charset.Charset;
import java.util.HashMap;
import java.util.Map;


/**
 * 对发给Provider-Agent的服务请求报文的处理
 */
public class ProviderAgentServerHandler extends SimpleChannelInboundHandler<AgentServiceRequest> {
    private static Logger logger = LoggerFactory.getLogger(ProviderAgentServerHandler.class);

    private ByteBuf formDataTemp = UnpooledByteBufAllocator.DEFAULT.buffer(2048);
    //private AppendableCharSequence formDataTemp = new AppendableCharSequence(2048);

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, AgentServiceRequest agentServiceRequest) throws Exception {

        logger.info("requestId:{}", agentServiceRequest.getRequestId());
        /*协议转换*/
        agentServiceRequest.setChannel(ctx.channel());
        ServiceSwitcher.switchToDubbo(agentServiceRequest, parseFormData(agentServiceRequest.getData(), formDataTemp));
        //logger.info(agentServiceRequest.getData().toString(Charset.forName("utf-8")));
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

    private Map<String, String> parseFormData(ByteBuf src, ByteBuf formDataTemp) {
        Map<String, String> formDataMap = new HashMap<>();  // 表单map
        formDataTemp.clear();
        String key = null;

        while (src.readableBytes() > 0) {
            byte c = src.readByte();

            if (src.readableBytes() == 0 || c == '&') {
                /*value结束*/
                if (formDataTemp.readableBytes() == 0) {
                    formDataMap.put(key, "");
                } else {
                    formDataMap.put(key, formDataTemp.toString(Charset.forName("utf-8")));
                }
                logger.info("key:{} value:{}", key, formDataMap.get(key));
                formDataTemp.clear();
                continue;
            }

            if (c == '=') {
                /*key结束*/
                key = formDataTemp.toString(Charset.forName("utf-8"));
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
            }
        }

        return formDataMap;
    }
}
