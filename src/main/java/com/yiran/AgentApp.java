package com.yiran;

import com.yiran.agent.AgentServer;
import com.yiran.agent.web.HttpServer;
import com.yiran.dubbo.DubboConnectManager;
import io.netty.channel.Channel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.autoconfigure.SpringBootApplication;

import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

@SpringBootApplication
public class AgentApp {
    // agent会作为sidecar，部署在每一个Provider和Consumer机器上
    // 在Provider端启动agent时，添加JVM参数-Dtype=provider -Dserver.port=30000 -Ddubbo.protocol.port=20889
    // 在Consumer端启动agent时，添加JVM参数-Dtype=consumer -Dserver.port=20000
    // 添加日志保存目录: -Dlogs.dir=/path/to/your/logs/dir。请安装自己的环境来设置日志目录。
    private static Logger logger = LoggerFactory.getLogger(AgentApp.class);
    //private static PerformanceMonitor performanceMonitor = PerformanceMonitor.getInstance();


    /**
     * provider-agent没必要用web服务器
     * 两个agent之间使用基于TCP的自定义协议通信，通过Netty实现
    * */
    public static void main(String[] args) throws Exception {
        String type = System.getProperty("type");
        logger.info("<---------------------Hello {}--------------------->", type);
        logger.info("MaxMemory:{}", Runtime.getRuntime().maxMemory());
        logger.info("TotalMemory:{}", Runtime.getRuntime().totalMemory());

        /*监听内存使用*/
        //Executors.newSingleThreadScheduledExecutor().scheduleAtFixedRate(() -> {
        //    logger.info("<----------------------monitor------------------------>");
        //    logger.info("----------------->Free memory:{}", Runtime.getRuntime().freeMemory());
        //    logger.info("----------------->Max memory:{}", Runtime.getRuntime().maxMemory());
        //    logger.info("----------------->Total memory:{}", Runtime.getRuntime().totalMemory());
        //    logger.info("----------------->Cpu usage:{}", performanceMonitor.getCpuUsage());
        //    logger.info("----------------->Ctxt per second:{}", performanceMonitor.getCtxtPerSecond());
//      //      logger.info("----------------->gc:\n{}", performanceMonitor.jstatGc());
        //}, 0, 2, TimeUnit.SECONDS);

//        if ("consumer".equals(type)) {
////        if (true) {
//            /*consumer-agent需要受理consumer发来的请求*/
////            SpringApplication.run(AgentApp.class,args);
//            HttpServer httpServer = new HttpServer(Integer.valueOf(System.getProperty("server.port")));
//            httpServer.run();
//        } else if("provider".equals(type)) {
        if (true) {
            /*provider-agent不需要启动web服务器*/
            //new ProviderAgentBootstrap().boot();

            /*先与Dubbo进行连接*/
            DubboConnectManager dubboConnectManager = new DubboConnectManager();
            Channel dubboChannel = null;
            while(dubboChannel == null) {
                logger.info("Connecting to Dubbo..");
                try{
                    dubboChannel = dubboConnectManager.getChannel();
                } catch (Exception e){
                    logger.error(e.getMessage());
                    Thread.sleep(500);
                }
            }
            /*往服务交换机注册支持的通道*/
            ServiceSwitcher.setRpcClientChannel(dubboConnectManager.getChannel());

            /*启动Agent服务*/
            for (int i = 0;i < 1;i++){
                try {
                    int port = 1090;
//                    int port = Integer.valueOf(System.getProperty("server.port" + i));
                    new AgentServer(port).run();
                } catch (Exception e) {
                    logger.error(e.getLocalizedMessage(), e);
                }
            }
        } else {
            logger.error("Environment variable type is needed to set to provider or consumer.");
        }

    }
}
