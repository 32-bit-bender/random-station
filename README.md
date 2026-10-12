# random-statios

> Work in progress. Right now this is just the code workspace (build, flash and
> debug setup) with a blink test on an STM32F446.

## Hardware

- MCU: STM32F446 (Cortex-M4F)
- Programmer/debugger: ST-Link, connected to the machine that builds the code

## Requirements

`arm-none-eabi-gcc`, `cmake`, `ninja`, `openocd`, `gdb-multiarch`, `cppcheck`.
Full install steps, VS Code extensions and settings are in
[getting_started.md](getting_started.md).

## Clone

The ST libraries are git submodules, so clone with:

```bash
git clone --recurse-submodules <repo-url>
cd random-station
```

If you already cloned without the flag:

```bash
git submodule update --init --recursive
```

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm-gcc.cmake
cmake --build build
```

Output: `build/blink.elf` (the name comes from `project()` in `CMakeLists.txt`).

If configure behaves strangely or ignores the toolchain file, delete the build
folder and configure again:

```bash
rm -rf build
```

In VS Code, `Ctrl+Shift+B` runs the same build.

## Flash

With the board connected:

```bash
cmake --build build --target flash
```

## Debug (VS Code)

1. Connect to the build machine with Remote-SSH and open this folder.
2. Plug the ST-Link into that machine.
3. Open Run and Debug (`Ctrl+Shift+D`), pick
   **Debug blink (OpenOCD + ST-Link)** and press `F5`.

It builds, flashes, and stops at `main()`. Do not use the CMake Tools debug
button, since it tries to run the ARM binary on the host.

If OpenOCD fails to start, test it directly:

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
```

## Project layout

```
src/        application code
lib/        ST libraries (git submodules: cmsis_core, cmsis_device_f4,
            stm32f4xx_hal_driver)
cmake/      toolchain file
.vscode/    tasks, debug and IntelliSense settings
```
