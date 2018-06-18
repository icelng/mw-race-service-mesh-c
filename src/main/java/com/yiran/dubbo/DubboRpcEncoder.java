package com.yiran.dubbo;

import com.yiran.dubbo.model.*;
import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToByteEncoder;
import io.netty.util.CharsetUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.ByteArrayOutputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;

public class DubboRpcEncoder extends MessageToByteEncoder {
    private static Logger logger = LoggerFactory.getLogger(DubboRpcEncoder.class);

    // header length.
    protected static final int HEADER_LENGTH = 16;
    // magic header.
    protected static final short MAGIC = (short) 0xdabb;
    // message flag.
    protected static final byte FLAG_REQUEST = (byte) 0x80;
    protected static final byte FLAG_TWOWAY = (byte) 0x40;
    protected static final byte FLAG_EVENT = (byte) 0x20;

    private byte[] header;
    private byte[] dubboVersion;
    private byte[] serviceVersion;
    private byte[] attachmentStart;
    private byte[] attachmentEnd;

    private int dubboVersionLen;
    private int serviceVersionLen;
    private int attachmentExtraLen;

    @Override
    public void handlerAdded(ChannelHandlerContext ctx) throws Exception {
        header = new byte[4];

        // set magic number.
        Bytes.short2bytes(MAGIC, header);

        // set request and serialization flag.
        header[2] = (byte) (FLAG_REQUEST | 6);

        header[2] |= FLAG_TWOWAY;

        dubboVersion = "\"2.6.0\"\n".getBytes(CharsetUtil.UTF_8);
        serviceVersion = "\"0.0.0\"\n".getBytes(CharsetUtil.UTF_8);
        attachmentStart = "{\"path\":\"".getBytes(CharsetUtil.UTF_8);
        attachmentEnd = "\"}\n".getBytes(CharsetUtil.UTF_8);

        dubboVersionLen = dubboVersion.length;
        serviceVersionLen = serviceVersion.length;
        attachmentExtraLen = attachmentStart.length + attachmentEnd.length;

        super.handlerAdded(ctx);
    }

    @Override
    protected void encode(ChannelHandlerContext ctx, Object msg, ByteBuf buffer) throws Exception {
        if (msg instanceof Request) {
            softEncode(ctx, msg, buffer);
        } else if (msg instanceof HalfHardRequest) {
            halfHardEncode(ctx, (HalfHardRequest) msg, buffer);
        }
    }

    public void halfHardEncode(ChannelHandlerContext ctx, HalfHardRequest req, ByteBuf out) {
        long reqId = req.getRequestId();
        ByteBuf interfaceName = req.getServiceName().slice();

        int len = 0;
        len += req.getServiceName().readableBytes();
        len += req.getMethod().readableBytes();
        len += req.getParameterTypes().readableBytes();
        len += req.getParameter().readableBytes();
        len += (dubboVersionLen + serviceVersionLen + attachmentExtraLen + 12 + interfaceName.readableBytes());  // 12为双引号和换行

        out.writeBytes(header);
        out.writeLong(reqId);
        out.writeInt(len);
        out.writeBytes(dubboVersion);
        writeJson(req.getServiceName(), out);
        out.writeBytes(serviceVersion);
        writeJson(req.getMethod(), out);
        writeJson(req.getParameterTypes(), out);
        writeJson(req.getParameter(), out);
        out.writeBytes(attachmentStart);
        out.writeBytes(interfaceName);
        out.writeBytes(attachmentEnd);

        req.getData().release();
    }

    private void writeJson(ByteBuf data, ByteBuf out) {
        out.writeByte('\"');
        out.writeBytes(data);
        out.writeByte('\"');
        out.writeByte('\n');
    }

    public void softEncode(ChannelHandlerContext ctx, Object msg, ByteBuf buffer) throws Exception {
        Request req = (Request)msg;

        // header.
        byte[] header = new byte[HEADER_LENGTH];
        // set magic number.
        Bytes.short2bytes(MAGIC, header);

        // set request and serialization flag.
        header[2] = (byte) (FLAG_REQUEST | 6);

        if (req.isTwoWay()) header[2] |= FLAG_TWOWAY;
        if (req.isEvent()) header[2] |= FLAG_EVENT;

        // set request id.
        Bytes.long2bytes(req.getId(), header, 4);

        // encode request data.
        int savedWriteIndex = buffer.writerIndex();
        buffer.writerIndex(savedWriteIndex + HEADER_LENGTH);
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        encodeRequestData(bos, req.getData());

        int len = bos.size();
        buffer.writeBytes(bos.toByteArray());
        Bytes.int2bytes(len, header, 12);

        // write
        buffer.writerIndex(savedWriteIndex);
        buffer.writeBytes(header); // write header.
        buffer.writerIndex(savedWriteIndex + HEADER_LENGTH + len);
        req.release();

    }

    public void encodeRequestData(OutputStream out, Object data) throws Exception {
        RpcInvocation inv = (RpcInvocation)data;

        PrintWriter writer = new PrintWriter(new OutputStreamWriter(out));

        JsonUtils.writeObject(inv.getAttachment("dubbo", "2.0.1"), writer);
        JsonUtils.writeObject(inv.getAttachment("path"), writer);
        JsonUtils.writeObject(inv.getAttachment("version"), writer);
        JsonUtils.writeObject(inv.getMethodName(), writer);
        JsonUtils.writeObject(inv.getParameterTypes(), writer);

        JsonUtils.writeBytes(inv.getArguments(), writer);
        JsonUtils.writeObject(inv.getAttachments(), writer);
    }

}
