# Building on Android

## Prerequisites

- [Android NDK](http://developer.android.com/ndk/downloads/index.html) - Somewhere in your search path
- Built against API level 23 (requires the SDK Platform installed)
- Python (Used for the build files)
- Target arch is set to arm64-v8a (change in Application.mk for other targets)

## Output
- Results are saved to ```/sdcard/hashtest_android_results.txt```

## Building

### Prepare SDK

```
setup.py
```

Will prepare the build for API level 23, generating a few files required for the build process.

### Build (and deploy)

```
build.py (-deploy)
```

If successful, the build process will output the `hashtest.apk` to the Android directory.

Use the "deploy" switch (if a device is connected) to install the apk after the build.

## Running

The application has no output. It will show a black screen while doing the hash tests and then exit.  