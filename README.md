# Double Trouble

VST3 (and AU on macOS) vocal plugin built with JUCE 9.

Two processors live in one editor:

1. **Alignment** — offline timing match of vocal takes to a reference take, plus optional grid quantize.
2. **Choir** — realtime 4–12 voice choir generated from the live input.

## Build

Requires CMake 3.22+, a C++17 compiler, and [JUCE](https://github.com/juce-framework/JUCE) 9.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=$HOME/JUCE
cmake --build build --config Release
```

Xcode generator is also fine:

```bash
cmake -B build -G Xcode -DJUCE_DIR=$HOME/JUCE
cmake --build build --config Release --target DoubleTrouble_VST3
```

With `COPY_PLUGIN_AFTER_BUILD` enabled the VST3 is copied to the system plug-in folder
(`~/Library/Audio/Plug-Ins/VST3` on macOS).

## Sample rates

Host rates 44.1 / 48 / 88.2 / 96 kHz are supported. Alignment always runs at the file’s native rate; preview is resampled to the host.

## Alignment workflow

1. Drop (or browse) a **Reference** vocal.
2. Drop one or more **Takes**.
3. Set Amount / Grid, then **Align**.
4. Preview Before/After and **Export** the selected take as WAV.

Heavy work (file decode, DTW, WSOLA) runs on a background thread. The Choir path stays realtime.
