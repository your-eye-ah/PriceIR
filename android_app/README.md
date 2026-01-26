# PrecIR Android App

This Android application works as a companion to the PriceIR Flipper Zero app. It allows you to:
1. Load an image from your gallery.
2. Enter a Product Label ID (PLID) or Barcode.
3. Configure settings (PP16/PP4 protocol).
4. Generate an `.esl` file that can be transmitted by FlipIR.

## Project Structure
- `app/src/main/java/com/example/precir/EslEncoder.java`: Contains the core encoding logic ported from the C implementation.
- `app/src/main/java/com/example/precir/MainActivity.java`: UI logic.

## Compilation Instructions
This project uses Gradle.

**Prerequisites:**
- Android Studio or JDK 11+ and Gradle installed.

**Build:**
1. Open this folder in Android Studio.
2. Allow it to sync and download the Gradle Wrapper.
3. Run the app on a connected device or emulator.

**Command Line:**
If you have Gradle installed:
```bash
gradle assembleDebug
```
