package com.yiran.agent.web;

import io.netty.buffer.ByteBuf;
import io.netty.handler.codec.TooLongFrameException;
import io.netty.handler.codec.http.HttpConstants;
import io.netty.util.ByteProcessor;
import io.netty.util.internal.AppendableCharSequence;

public class LineParser implements ByteProcessor {
    private final AppendableCharSequence seq;
    private final int maxLength;
    private int size;

    LineParser(AppendableCharSequence seq, int maxLength) {
        this.seq = seq;
        this.maxLength = maxLength;
    }

    public AppendableCharSequence parse(ByteBuf buffer) {
        final int oldSize = size;
        seq.reset();
        int i = buffer.forEachByte(this);
        if (i == -1) {
            size = oldSize;
            return null;
        }
        buffer.readerIndex(i + 1);
        return seq;
    }

    public void reset() {
        size = 0;
    }

    @Override
    public boolean process(byte value) throws Exception {
        char nextByte = (char) (value & 0xFF);
        if (nextByte == HttpConstants.CR) {
            return true;
        }
        if (nextByte == HttpConstants.LF) {
            return false;
        }

        if (++ size > maxLength) {
            // TODO: Respond with Bad Request and discard the traffic
            //    or close the connection.
            //       No need to notify the upstream handlers - just log.
            //       If decoding a response, just throw an exception.
            throw newException(maxLength);
        }

        seq.append(nextByte);
        return true;
    }

    protected TooLongFrameException newException(int maxLength) {
        return new TooLongFrameException("HTTP header is larger than " + maxLength + " bytes.");
    }
}

