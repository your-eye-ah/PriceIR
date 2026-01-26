---
description: How to compile the Android App
---

# Compile PrecIR Android App

This workflow guides you through compiling the Android application found in `android_app`.

## Prerequisites
- Java Development Kit (JDK) 11 or higher.
- Gradle (unless using the wrapper which is currently missing).
- Android SDK (usually installed with Android Studio).

## Steps

1. **Verify Environment**
   Run the following command to check if Gradle is installed:
   ```powershell
   gradle -v
   ```
   If this command fails, you must install Gradle or open the project in Android Studio to generate the wrapper.

2. **Navigate to App Directory**
   ```powershell
   cd 'c:\Users\UHB\Documents\Coding Projects\FlipIR\android_app'
   ```

3. **Build the APK**
   Run the assemble task:
   ```powershell
   gradle assembleDebug
   ```
   *Note: If you have generated a wrapper (gradlew), use `./gradlew assembleDebug` instead.*

4. **Locate Output**
   The APK will be located at:
   `app/build/outputs/apk/debug/app-debug.apk`

5. **Install**
   You can install it via ADB:
   ```powershell
   adb install app/build/outputs/apk/debug/app-debug.apk
   ```

## Troubleshooting
- **Missing Wrapper**: The repo looks fresh and is missing `gradlew`. Open in Android Studio to fix this automatically.
- **SDK Location**: Create a `local.properties` file in `android_app` with `sdk.dir=PATH_TO_SDK` if building from CLI.
