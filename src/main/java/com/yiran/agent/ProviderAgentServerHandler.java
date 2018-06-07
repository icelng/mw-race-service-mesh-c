package com.yiran.agent;

import com.yiran.ServiceSwitcher;
import com.yiran.agent.web.FormDataParser;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.UnpooledByteBufAllocator;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.util.internal.AppendableCharSequence;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.UnsupportedEncodingException;
import java.nio.charset.Charset;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;


/**
 * 对发给Provider-Agent的服务请求报文的处理
 */
public class ProviderAgentServerHandler extends SimpleChannelInboundHandler<AgentServiceRequest> {
    private static Logger logger = LoggerFactory.getLogger(ProviderAgentServerHandler.class);
    private Executor executor = Executors.newFixedThreadPool(256);

    private ByteBuf formDataTemp = UnpooledByteBufAllocator.DEFAULT.buffer(2048);
    //private AppendableCharSequence formDataTemp = new AppendableCharSequence(2048);

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, AgentServiceRequest agentServiceRequest) throws Exception {

        /*协议转换*/
        //logger.info("reqId:{}", agentServiceRequest.getRequestId());
        FormDataParser formDataParser = new FormDataParser(formDataTemp);
        agentServiceRequest.setFormDataMap(formDataParser.parse(agentServiceRequest.getData()));
        //formDataParser.release();
        agentServiceRequest.setChannel(ctx.channel());
        //ServiceSwitcher.switchToDubbo(agentServiceRequest);
        //logger.info(agentServiceRequest.getData().toString(Charset.forName("utf-8")));
        executor.execute(() -> {
            try {
                Thread.sleep(50);
            } catch (InterruptedException e) {
                logger.error("", e);
            }
            String respStr = String.format("1\n%d\n", agentServiceRequest.getFormDataMap().get("parameter").hashCode());
            logger.info(respStr);
            AgentServiceResponse agentServiceResponse = new AgentServiceResponse();
            agentServiceResponse.setRequestId(agentServiceRequest.getRequestId());
            try {
                agentServiceResponse.setReturnValue(respStr.getBytes("utf-8"));
                ctx.writeAndFlush(agentServiceResponse);
            } catch (UnsupportedEncodingException e) {
                logger.error("", e);
            }
        });
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        super.exceptionCaught(ctx, cause);
        logger.error("", cause);
    }
}
