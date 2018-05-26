package com.yiran.agent;

import com.yiran.ServiceSwitcher;
import com.yiran.agent.web.FormDataParser;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;



/**
 * 对发给Provider-Agent的服务请求报文的处理
 */
public class ProviderAgentServerHandler extends SimpleChannelInboundHandler<AgentServiceRequest> {
    private static Logger logger = LoggerFactory.getLogger(ProviderAgentServerHandler.class);

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, AgentServiceRequest agentServiceRequest) throws Exception {

        /*协议转换*/
        ServiceSwitcher.switchToDubbo(agentServiceRequest, ctx.channel());
    }
}
