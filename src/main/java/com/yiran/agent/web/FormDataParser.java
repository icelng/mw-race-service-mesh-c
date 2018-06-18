package com.yiran.agent.web;

import io.netty.buffer.*;
import io.netty.handler.codec.TooLongFrameException;
import io.netty.handler.codec.http.HttpConstants;
import io.netty.util.ByteProcessor;
import io.netty.util.CharsetUtil;
import io.netty.util.Recycler;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.nio.charset.Charset;
import java.util.HashMap;
import java.util.Map;

public class FormDataParser implements ByteProcessor {

    private static Logger logger = LoggerFactory.getLogger(FormDataParser.class);
    private static final byte CHAR_AND = 38;  // &符号
    private static final byte CHAR_EQUAL = 61;  // =号

    private ByteBufAllocator alloc;
    private boolean nextIsValue = false;
    private int size;
    private ByteBuf seq;
    private final int maxLength;
    private Recycler.Handle<FormDataParser> recyclerHandle;
    private boolean isUrlDec = false;
    private boolean isC0 = false;
    private boolean isC1 = false;
    private byte c1;
    private byte c0;


    public FormDataParser(ByteBuf seq, int maxLength, ByteBufAllocator alloc) {
        this.seq = seq;
        this.maxLength = maxLength;
        this.alloc = alloc;
    }

    public FormDataParser(ByteBuf seq, int maxLength) {
        this.seq = seq;
        this.maxLength = maxLength;
    }


    public String parseInterface(ByteBuf buffer) throws UnsupportedEncodingException {

        int oldReaderIndex = buffer.readerIndex();
        int k = 0;
        this.nextIsValue = false;
        isC0 =false;
        isC1 = false;
        size = 0;
        String key = null;
        String value;
        while(true){
            seq.clear();
            int i = buffer.forEachByte(this);
            k++;
            if (k % 2 == 1) {
                /*key*/
                key = URLDecoder.decode(seq.toString(Charset.forName("utf-8")), "utf-8");
                if (key == null) {
                    logger.error("Failed to parse key!");
                }
            } else if (k % 2 == 0) {
                /*value*/
                value = URLDecoder.decode(seq.toString(Charset.forName("utf-8")), "utf-8");
                if ("interface".equals(key)) {
                    buffer.readerIndex(oldReaderIndex);  // 恢复buffer
                    return value;
                }
            }
            if (i == -1) {
                buffer.readerIndex(oldReaderIndex);  // 恢复buffer
                return null;
            }
            buffer.readerIndex(i + 1);
        }

    }

    public Map<String, ByteBuf> parse(ByteBuf buffer) throws UnsupportedEncodingException {
        Map<String, ByteBuf> parameterMap = new HashMap<>();

        int oldReaderIndex = buffer.readerIndex();
        int k = 0;
        this.nextIsValue = false;
        size = 0;
        String key = null;
        ByteBuf value = null;
        seq.clear();
        while(true){
            isUrlDec = false;
            seq.readerIndex(seq.writerIndex());
            int i = buffer.forEachByte(this);
            k++;
            if (k % 2 == 1) {
                /*key*/
                //key = URLDecoder.decode(seq.toString(Charset.forName("utf-8")), "utf-8");
                key = seq.toString(Charset.forName("utf-8"));
                if (key == null) {
                    logger.error("Failed to parse key!");
                }
            } else if (k % 2 == 0) {
                /*value*/
                //value = URLDecoder.decode(seq.toString(Charset.forName("utf-8")), "utf-8");
                //value = seq.toString(Charset.forName("utf-8"));
                value = seq.slice();
                parameterMap.put(key, value);
                //if (value == null && this.nextIsValue) {
                //    parameterMap.put(key, null);
                //    //logger.info("key:{} value:{}", key, parameterMap.get(key));
                //} else {
                //    parameterMap.put(key, value);
                //    //logger.info("key:{} value:{}", key, parameterMap.get(key));
                //}
                logger.info("key:{}  value:{}", key, value.toString(CharsetUtil.UTF_8));
            }
            if (i == -1) {
                buffer.readerIndex(oldReaderIndex);  // 恢复buffer
                return parameterMap;
            }
            buffer.readerIndex(i + 1);
        }
    }

    @Override
    public boolean process(byte value) throws Exception {
        if (value == CHAR_EQUAL) {
            this.nextIsValue = true;
            return false;
        }
        if (value == CHAR_AND) {
            this.nextIsValue = false;
            return false;
        }
        if (value == HttpConstants.CR) {
            return false;
        }
        if (value == HttpConstants.LF) {
            return false;
        }

        if (++ size > maxLength) {
            throw newException(maxLength);
        }

        if (value == '%') {
            isUrlDec = true;
            isC0 = true;
            isC1 = true;
            return true;
        }

        if (isUrlDec) {
            if (isC1) {
                c1 = value;
                isC1 = false;
                return true;
            } else if (isC0) {
                c0 = value;
                isC0 = false;
                seq.writeByte((byte) ((hex2dec(c1) * 16 + hex2dec(c0)) & 0xFF));
                isUrlDec = false;
                return true;
            }
        }

        seq.writeByte(value);
        return true;
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

    protected TooLongFrameException newException(int maxLength) {
        return new TooLongFrameException("HTTP content is larger than " + maxLength + " bytes.");
    }
}
