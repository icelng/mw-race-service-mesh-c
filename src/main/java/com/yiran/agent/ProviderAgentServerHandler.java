package com.yiran.agent;

import com.yiran.ServiceSwitcher;
import com.yiran.agent.web.FormDataParser;
import io.netty.buffer.ByteBuf;
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

    private AppendableCharSequence formDataTemp = new AppendableCharSequence(2048);

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, AgentServiceRequest agentServiceRequest) throws Exception {

        /*协议转换*/
        agentServiceRequest.setChannel(ctx.channel());
        ServiceSwitcher.switchToDubbo(agentServiceRequest, parseFormData(agentServiceRequest.getData(), formDataTemp));
        //logger.info(agentServiceRequest.getData().toString(Charset.forName("utf-8")));
    }

    private int hex2dec(char c) {
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

    private Map<String, String> parseFormData(ByteBuf src, AppendableCharSequence formDataTemp) {
        Map<String, String> formDataMap = new HashMap<>();  // 表单map
        formDataTemp.reset();
        String key = null;

        while (src.readableBytes() > 0) {
            char c = (char)((src.readByte()) & 0xFF);

            if (src.readableBytes() == 0 || c == '&') {
                /*value结束*/
                formDataMap.put(key, formDataTemp.toString());
                formDataTemp.reset();
                continue;
            }

            if (c == '=') {
                /*key结束*/
                key = formDataTemp.toString();
                formDataTemp.reset();
                continue;
            }

            if (c != '%') {
                formDataTemp.append(c);
            } else {
                char c1 = (char)((src.readByte()) & 0xFF);
                char c0 = (char)((src.readByte()) & 0xFF);
                int num = hex2dec(c1) * 16 + hex2dec(c0);
                formDataTemp.append((char)(num & 0xFF));
            }
        }

        return formDataMap;
    }
}
