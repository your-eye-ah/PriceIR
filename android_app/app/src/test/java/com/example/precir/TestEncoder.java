package com.example.precir;

import java.util.Arrays;

public class TestEncoder {
    public static void main(String[] args) {
        System.out.println("Running TestEncoder...");

        // Mock Image (16x8 pixels)
        int width = 16;
        int height = 8;
        int[] pixels = new int[width * height];
        Arrays.fill(pixels, 0xFFFFFFFF); // White
        // Draw a black line
        for (int i = 0; i < width; i++) {
            pixels[i] = 0xFF000000; // Black
        }

        String barcode = "A4108251199313541"; // Example from README/Code

        try {
            byte[] eslFile = EslEncoder.encode(pixels, width, height, barcode, false, false);

            System.out.println("Generated .esl file size: " + eslFile.length);

            if (eslFile.length < 1) {
                throw new RuntimeException("File too short");
            }

            // Check Header
            if (eslFile[0] != 0) {
                throw new RuntimeException("Header should be 0 (PP4)");
            }

            // Check First Frame (Ping)
            // Repeats: 400 (0x90 0x01)
            int offset = 1;
            int r1 = eslFile[offset++] & 0xFF;
            int r2 = eslFile[offset++] & 0xFF;
            int repeats = r1 + (r2 << 8);
            System.out.println("Ping Frame Repeats: " + repeats);
            if (repeats != 400) {
                throw new RuntimeException("Ping repeats wrong: " + repeats);
            }

            int len = eslFile[offset++] & 0xFF;
            System.out.println("Ping Frame Length: " + len);

            // Check Frame content (Header 0x85 ...)
            int frameStart = offset;
            if ((eslFile[frameStart] & 0xFF) != 0x85) {
                 throw new RuntimeException("Ping Frame Header wrong: " + String.format("%02X", eslFile[frameStart]));
            }
            offset += len;

            // Check Param Frame
            r1 = eslFile[offset++] & 0xFF;
            r2 = eslFile[offset++] & 0xFF;
            repeats = r1 + (r2 << 8);
            if (repeats != 1) {
                throw new RuntimeException("Param repeats wrong: " + repeats);
            }
            len = eslFile[offset++] & 0xFF;
            System.out.println("Param Frame Length: " + len);
             offset += len;

            System.out.println("Test PASSED");

        } catch (Exception e) {
            e.printStackTrace();
            System.out.println("Test FAILED");
            System.exit(1);
        }
    }
}
