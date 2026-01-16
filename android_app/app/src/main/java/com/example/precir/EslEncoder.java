package com.example.precir;

import java.util.ArrayList;
import java.util.List;

public class EslEncoder {

    // Helper class for ESL ID
    public static class PrecIrLabelId {
        public byte[] id = new byte[4];
    }

    public static byte[] encode(int[] pixels, int width, int height, String barcode, boolean pp16, boolean colorMode) {
        // 1. Parse PLID
        PrecIrLabelId plid = getPlid(barcode);

        // 2. Convert Image to Bits (BW)
        List<Integer> pixelBits = imageConvert(pixels, width, height, false);
        if (colorMode) {
            pixelBits.addAll(imageConvert(pixels, width, height, true));
        }

        // 3. Compress
        List<Integer> compressedBits = compress(pixelBits);

        // Decide compression
        List<Integer> dataBits;
        int compressionType;
        if (compressedBits.size() < pixelBits.size()) {
            dataBits = compressedBits;
            compressionType = 2; // Compressed
        } else {
            dataBits = pixelBits;
            compressionType = 0; // Raw
        }

        // 4. Pad data to multiple of bits_per_frame (20 * 8 = 160)
        int bitsPerFrame = 160;
        int padding = bitsPerFrame - (dataBits.size() % bitsPerFrame);
        if (padding < bitsPerFrame) {
            for (int i = 0; i < padding; i++) {
                dataBits.add(0);
            }
        }

        int paddedDataSizeBits = dataBits.size();
        int paddedDataSizeBytes = paddedDataSizeBits / 8;
        int frameCount = paddedDataSizeBits / bitsPerFrame;

        // 5. Construct Frames
        List<byte[]> frames = new ArrayList<>();

        // Wake-up ping frame (Repeats: 400)
        frames.add(makePingFrame(plid, pp16));

        // Parameters frame (0x05)
        byte[] paramFrame = makeMcuFrame(plid, 0x05);
        List<Byte> payload = new ArrayList<>();
        appendWord(payload, paddedDataSizeBytes);
        payload.add((byte) 0x00);
        payload.add((byte) compressionType);
        payload.add((byte) 0); // Page 0
        appendWord(payload, width);
        appendWord(payload, height);
        appendWord(payload, 0); // x
        appendWord(payload, 0); // y
        appendWord(payload, 0); // Keycode
        payload.add((byte) 0x88); // Update + Set Base Page
        appendWord(payload, 0); // Enabled pages
        payload.add((byte) 0); payload.add((byte) 0); payload.add((byte) 0); payload.add((byte) 0);

        frames.add(finalizeFrame(paramFrame, payload, pp16));

        // Data frames (0x20)
        int bitIndex = 0;
        for (int fr = 0; fr < frameCount; fr++) {
            byte[] dataFrameHeader = makeMcuFrame(plid, 0x20);
            List<Byte> dataPayload = new ArrayList<>();
            appendWord(dataPayload, fr);

            for (int by = 0; by < 20; by++) {
                int v = 0;
                for (int bi = 0; bi < 8; bi++) {
                    v <<= 1;
                    if (bitIndex < dataBits.size()) {
                        v += dataBits.get(bitIndex);
                    }
                    bitIndex++;
                }
                dataPayload.add((byte) v);
            }
            frames.add(finalizeFrame(dataFrameHeader, dataPayload, pp16));
        }

        // Refresh frame
        frames.add(makeRefreshFrame(plid, pp16));

        // 6. Serialize to .esl File Format
        // Header: [Flags (1B)]
        // Frames: [Repeats(2B)] [Len(1B)] [Data]

        List<Byte> fileContent = new ArrayList<>();
        fileContent.add((byte) (pp16 ? 1 : 0)); // Protocol Flags

        for (int i = 0; i < frames.size(); i++) {
            byte[] frameData = frames.get(i);
            int repeats = 1;
            if (i == 0) repeats = 400; // Ping frame

            // Repeats (Little Endian)
            fileContent.add((byte) (repeats & 0xFF));
            fileContent.add((byte) ((repeats >> 8) & 0xFF));

            // Length
            fileContent.add((byte) frameData.length);

            // Data
            for (byte b : frameData) {
                fileContent.add(b);
            }
        }

        byte[] result = new byte[fileContent.size()];
        for (int i = 0; i < fileContent.size(); i++) {
            result[i] = fileContent.get(i);
        }
        return result;
    }

    private static List<Integer> imageConvert(int[] pixels, int width, int height, boolean colorPass) {
        List<Integer> bits = new ArrayList<>();
        for (int i = 0; i < pixels.length; i++) {
            int rgb = pixels[i];
            int r = (rgb >> 16) & 0xFF;
            int g = (rgb >> 8) & 0xFF;
            int b = rgb & 0xFF;

            // Luma = 0.21 R + 0.72 G + 0.07 B
            double luma = (0.21 * r + 0.72 * g + 0.07 * b) / 255.0;

            if (colorPass) {
                // 0 codes color (mid grey)
                bits.add((luma >= 0.1 && luma < 0.9) ? 0 : 1);
            } else {
                // 0 codes black
                bits.add((luma < 0.5) ? 0 : 1);
            }
        }
        return bits;
    }

    private static List<Integer> compress(List<Integer> pixelBits) {
        List<Integer> compressed = new ArrayList<>();
        if (pixelBits.isEmpty()) return compressed;

        int runPixel = pixelBits.get(0);
        int runCount = 1;
        compressed.add(runPixel);

        for (int i = 1; i < pixelBits.size(); i++) {
            int pixel = pixelBits.get(i);
            if (pixel == runPixel) {
                runCount++;
            } else {
                recordRun(compressed, runCount);
                runCount = 1;
                runPixel = pixel;
            }
        }
        if (runCount > 0) {
            recordRun(compressed, runCount);
        }
        return compressed;
    }

    private static void recordRun(List<Integer> compressed, int runCount) {
        List<Integer> bits = new ArrayList<>();
        int tempCount = runCount;

        // Convert to binary (LSB first extraction, but need MSB first order?)
        // Python: bits.insert(0, run_count & 1). So it builds [MSB...LSB]
        while (tempCount > 0) {
            bits.add(0, tempCount & 1);
            tempCount >>= 1;
        }

        // Zero length coding - 1
        // Python: for b in bits[1:]: compressed.append(0)
        for (int i = 1; i < bits.size(); i++) {
            compressed.add(0);
        }

        // Python: compressed.extend(bits)
        compressed.addAll(bits);
    }

    private static PrecIrLabelId getPlid(String barcode) {
        PrecIrLabelId plid = new PrecIrLabelId();
        if (barcode == null || barcode.length() < 12) return plid;

        try {
            // Python: int(barcode[2:7]) + (int(barcode[7:12]) << 16)
            int val1 = Integer.parseInt(barcode.substring(2, 7));
            int val2 = Integer.parseInt(barcode.substring(7, 12));
            long idValue = val1 + ((long)val2 << 16);

            plid.id[0] = (byte) ((idValue >> 8) & 0xFF);
            plid.id[1] = (byte) (idValue & 0xFF);
            plid.id[2] = (byte) ((idValue >> 24) & 0xFF);
            plid.id[3] = (byte) ((idValue >> 16) & 0xFF);
        } catch (NumberFormatException e) {
            // ignore
        }
        return plid;
    }

    private static byte[] makePingFrame(PrecIrLabelId plid, boolean pp16) {
        // make_raw_frame(0x85, PLID, 0x17)
        // frame = [protocol, PLID[3], PLID[2], PLID[1], PLID[0], cmd]
        byte[] header = new byte[] {
            (byte)0x85, plid.id[3], plid.id[2], plid.id[1], plid.id[0], (byte)0x17
        };
        List<Byte> payload = new ArrayList<>();
        payload.add((byte)0x01);
        payload.add((byte)0x00);
        payload.add((byte)0x00);
        payload.add((byte)0x00);
        for(int i=0; i<22; i++) payload.add((byte)0x00);

        return finalizeFrame(header, payload, pp16);
    }

    private static byte[] makeRefreshFrame(PrecIrLabelId plid, boolean pp16) {
        byte[] header = makeMcuFrame(plid, 0x01);
        List<Byte> payload = new ArrayList<>();
        for(int i=0; i<22; i++) payload.add((byte)0x00);
        return finalizeFrame(header, payload, pp16);
    }

    private static byte[] makeMcuFrame(PrecIrLabelId plid, int cmd) {
        // frame = [0x85, PLID[3], PLID[2], PLID[1], PLID[0], 0x34, 0x00, 0x00, 0x00, cmd]
        return new byte[] {
            (byte)0x85, plid.id[3], plid.id[2], plid.id[1], plid.id[0],
            (byte)0x34, (byte)0x00, (byte)0x00, (byte)0x00, (byte)cmd
        };
    }

    private static byte[] finalizeFrame(byte[] header, List<Byte> payload, boolean pp16) {
        List<Byte> frame = new ArrayList<>();
        for(byte b : header) frame.add(b);
        frame.addAll(payload);

        int crc = crc16(frame);

        // If PP16, prepend header [0x00, 0x00, 0x00, 0x40]
        if (pp16) {
            frame.add(0, (byte)0x40);
            frame.add(0, (byte)0x00);
            frame.add(0, (byte)0x00);
            frame.add(0, (byte)0x00);
        }

        frame.add((byte) (crc & 0xFF));
        frame.add((byte) ((crc >> 8) & 0xFF));

        // Note: We DO NOT append repeats here, as they are metadata in the file

        byte[] res = new byte[frame.size()];
        for(int i=0; i<frame.size(); i++) res[i] = frame.get(i);
        return res;
    }

    private static int crc16(List<Byte> data) {
        int result = 0x8408;
        int poly = 0x8408;

        for (byte b : data) {
            result ^= (b & 0xFF);
            for (int i = 0; i < 8; i++) {
                if ((result & 1) != 0) {
                    result >>= 1;
                    result ^= poly;
                } else {
                    result >>= 1;
                }
            }
        }
        return result;
    }

    private static void appendWord(List<Byte> list, int value) {
        list.add((byte) ((value >> 8) & 0xFF));
        list.add((byte) (value & 0xFF));
    }
}
