package com.yiran;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.Date;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;



public class PerformanceMonitor {
    private static Logger logger = LoggerFactory.getLogger(PerformanceMonitor.class);
    private static PerformanceMonitor INSTANCE = new PerformanceMonitor();

    private long lastTime = 0;
    private long currentTime = 0;

    private long lastTotalCpuTime;
    private long lastIdleCpuTime;
    private long currentTotalCpuTime;
    private long currentIdleCpuTime;
    private long lastCtxt;
    private long currentCtxt;
    private float cpuUsage;
    private long ctxtPerSecond;  // 每秒上下文切换

    private int jmapCnt = 0;

    public static PerformanceMonitor getInstance(){
        return INSTANCE;
    }


    public PerformanceMonitor(){

        runCpuMonitor();
//        runMemoryMonitor();
    }

    private void runCpuMonitor(){
        Executors.newSingleThreadScheduledExecutor().scheduleAtFixedRate(() -> {
            boolean cpuFlag = true;
            String command = "cat /proc/stat";
            try {
                Process process = Runtime.getRuntime().exec(command);
                BufferedReader in = new BufferedReader(new InputStreamReader(process.getInputStream()));
                lastTime = currentTime;
                currentTime = (new Date()).getTime() / 1000;
                String line;
                long totalCpuTime = 0;
                while((line=in.readLine()) != null){
                    if(line.startsWith("cpu")){
                        if (!cpuFlag) {
                            continue;
                        }
                        line = line.trim();
                        String[] temp = line.split("\\s+");
                        lastIdleCpuTime = currentIdleCpuTime;
                        currentIdleCpuTime = Long.parseLong(temp[4]);
                        for(String s : temp){
                            if(!s.equals("cpu")){
                                totalCpuTime += Long.parseLong(s);
                            }
                        }
                        lastTotalCpuTime = currentTotalCpuTime;
                        currentTotalCpuTime = totalCpuTime;
                        cpuFlag = false;
                        continue;
                    }

                    if(line.startsWith("ctxt")){
                        /*上下文切换数*/
                        line = line.trim();
                        lastCtxt = currentCtxt;
                        currentCtxt = Long.parseLong(line.split("\\s+")[1]);
                    }
                }
                in.close();
                process.destroy();
            } catch (IOException e) {
                logger.error("", e);
            }

            if (currentTotalCpuTime != lastTotalCpuTime) {
                cpuUsage = 1 - (float) (currentIdleCpuTime - lastIdleCpuTime) / (float) (currentTotalCpuTime - lastTotalCpuTime);
            }
            if (currentTime != lastTime) {
                ctxtPerSecond = (currentCtxt - lastCtxt) / (currentTime - lastTime);
            }

        }, 0, 100, TimeUnit.MILLISECONDS);
    }

    public float getCpuUsage(){
        return cpuUsage;
    }

    public long getCtxtPerSecond() {
        return ctxtPerSecond;
    }

    public static void main(String args[]) throws InterruptedException {
        PerformanceMonitor performanceMonitor = PerformanceMonitor.getInstance();
        while(true) {
            String gcInfo = performanceMonitor.jstatGc();
            System.out.println(gcInfo);
            Thread.sleep(1000);
        }
    }

    public int getProcessID() {
        RuntimeMXBean runtimeMXBean = ManagementFactory.getRuntimeMXBean();
        return Integer.valueOf(runtimeMXBean.getName().split("@")[0])
                .intValue();
    }

    public String jstatGc(){
        String command = "jstat -gc " + getProcessID();
        Process process;
        String gcInfo = "";
        try {
            process = Runtime.getRuntime().exec(command);
            BufferedReader in = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line;
            while ((line = in.readLine()) != null) {
                gcInfo += (line + "\n");
            }
            in.close();
            process.destroy();
        } catch (IOException e) {
            logger.error("", e);
        }
        return gcInfo;
    }

    public void runMemoryMonitor(){
        Executors.newSingleThreadScheduledExecutor().scheduleAtFixedRate(() -> {
            String command = "jmap -dump:format=b,file=" + System.getProperty("logs.dir")  + "/dumpfile" + jmapCnt++ + ".hprof " + getProcessID();
            Process process;
            try {
                process = Runtime.getRuntime().exec(command);
                BufferedReader in = new BufferedReader(new InputStreamReader(process.getInputStream()));
                String line = in.readLine();
                logger.info("dump file:{}", line);
                in.close();
                process.destroy();
            } catch (IOException e) {
                logger.error("", e);
            }

        }, 0, 250, TimeUnit.SECONDS);

    }

}
