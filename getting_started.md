# Getting Started: STM32 + CMake + VS Code (Remote SSH) + LL drivers

A from-scratch guide for a bare-metal STM32 project that:

- builds with your own `CMakeLists.txt` and `arm-none-eabi-gcc` (no CubeIDE),
- pulls ST's LL drivers in as git submodules (no copying files around),
- is edited and debugged in VS Code over **Remote-SSH** on a Debian machine,
- has working IntelliSense (click-through to headers), an 80-column ruler, and a Cppcheck lint.

The examples target an **STM32F446RE** (Nucleo-F446RE, LED on PA5). The last section is a cheat sheet for switching chips.

---


## 1. Install the tools on the Debian machine

### 1.1 Build tools and debugger

```bash
sudo apt update
sudo apt install git cmake ninja-build openocd gdb-multiarch cppcheck
```

| Package | What it is for |
|---|---|
| `git` | submodules for the ST libraries |
| `cmake`, `ninja-build` | build system (CMake generates, Ninja builds) |
| `openocd` | GDB server and flasher that talks to the ST-Link |
| `gdb-multiarch` | a GDB that understands ARM (plain `gdb` on x86 does not) |
| `cppcheck` | static analysis for the lint extension |

Check versions:

```bash
cmake --version        # need 3.22 or newer (CMakeLists says 3.22)
openocd --version      # want 0.12 or newer (older has no stm32g4x config, F4 is fine)
```

If `cmake --version` is older than 3.22, either lower `cmake_minimum_required` or install a newer CMake (`pip install cmake` in a venv, or Kitware's binary).

### 1.2 The ARM compiler

**Option A: from apt (simplest)**

```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libnewlib-nano-arm-none-eabi
```

The two `libnewlib` packages matter. Without them you get missing `stdint.h` or errors about `nano.specs`.

**Option B: ARM's own release (newer compiler)**

1. Go to developer.arm.com and search for "Arm GNU Toolchain downloads".
2. Pick the **x86_64 Linux hosted, AArch32 bare-metal target (arm-none-eabi)** tarball.
3. Extract it, for example into `/opt`, and add its `bin/` to your `PATH` (in `~/.bashrc`).
4. If you use this option, remember the compiler is no longer in `/usr/bin`. Update `compilerPath` in `c_cpp_properties.json` and `cortex-debug.armToolchainPath` in `settings.json` (section 6).

**Verify (either option):**

```bash
arm-none-eabi-gcc --version
which arm-none-eabi-gcc        # note this path, you need it later
```

### 1.3 ST-Link access without root (udev rules)

```bash
sudo cp /usr/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo usermod -aG plugdev $USER
```

Log out and back in, then replug the board. Check:

```bash
lsusb | grep -i st-link
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
```

OpenOCD should report the target and stay running. Press Ctrl+C. If it fails here, it will fail in VS Code too, so fix it first.

### 1.4 CubeMX (optional, on Windows)

Use it only as a reference generator: set **Project Manager → Advanced Settings** drivers to **LL**, generate, then read the generated clock setup, pin init and linker script. You can copy the linker script (`STM32F446RETx_FLASH.ld`) from the generated project. Nothing else from CubeMX is needed on the Debian side.

---

## 2. Install VS Code and the extensions

On your Windows machine install VS Code, then the **Remote - SSH** extension. Connect to the Debian machine (command palette: **Remote-SSH: Connect to Host**) and open the project folder.

Extensions must be installed **on the remote**. In the Extensions panel, an extension showing an **Install in SSH: <host>** button is not installed there yet. Click it.

| Extension | Id | Why |
|---|---|---|
| C/C++ | `ms-vscode.cpptools` | IntelliSense, click-through to headers |
| CMake Tools | `ms-vscode.cmake-tools` | optional, handy for configure and build; see the warning below |
| Cortex-Debug | `marus25.cortex-debug` | debugging through OpenOCD |
| Peripheral Viewer | `mcu-debug.peripheral-viewer` | register view (needs an SVD file) |
| C/C++ Advanced Lint | `jbenden.c-cpp-flylint` | runs Cppcheck as you type |

**CMake Tools warning:** if you keep it, never use its debug button, which launches the x86 host debugger (`cppdbg`) and gives `Exec format error` on an ARM ELF. Use Run and Debug (Ctrl+Shift+D) with the Cortex-Debug entry. Also never choose a "GCC x.y" kit, since kits override your toolchain file.

---

## 3. Create the project

### 3.1 Folder layout

```
random-station/
├── CMakeLists.txt
├── STM32F446xx_FLASH.ld
├── cmake/
│   └── arm-gcc.cmake
├── src/
│   └── main.c
├── lib/                          (git submodules)
│   ├── cmsis_core
│   ├── cmsis_device_f4
│   └── stm32f4xx_hal_driver
├── .vscode/
│   ├── tasks.json
│   ├── launch.json
│   ├── c_cpp_properties.json
│   └── settings.json
├── .gitignore
└── .gitmodules                   (created by git)
```

### 3.2 Create it and add the libraries

```bash
mkdir -p random-station/{src,cmake,.vscode,lib} && cd random-station
git init
git submodule add --depth 1 https://github.com/STMicroelectronics/cmsis_core.git           lib/cmsis_core
git submodule add --depth 1 https://github.com/STMicroelectronics/cmsis_device_f4.git      lib/cmsis_device_f4
git submodule add --depth 1 https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git lib/stm32f4xx_hal_driver
```

What each one gives you:

- `cmsis_core`: the ARM core headers (`core_cm4.h`, `cmsis_gcc.h`).
- `cmsis_device_f4`: the chip headers (`stm32f4xx.h`, `stm32f446xx.h`), `system_stm32f4xx.c` and the startup `.s` file.
- `stm32f4xx_hal_driver`: contains the **LL** sources and headers (`stm32f4xx_ll_*.c/h`). This repo also holds HAL, but you only compile the LL files you list.

Cloning the project somewhere else later:

```bash
git clone --recurse-submodules <url>
# or, if you forgot:
git submodule update --init --recursive
```

**Check the core include path.** The location of the CMSIS core headers depends on which repo you added. Run:

```bash
find lib/cmsis_core -name core_cm4.h
```

Whatever folder contains that file is what goes in your include path. In your uploaded files it is `lib/cmsis_core/CMSIS/Core/Include`. If `find` shows `lib/cmsis_core/Include` instead, use that in both `CMakeLists.txt` and `c_cpp_properties.json`.

### 3.3 `.gitignore`

```
build/
.cache/
*.map
```

---

## 4. The build files

### 4.1 `cmake/arm-gcc.cmake` (the toolchain file)

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_SIZE         arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

Main lines:

- `CMAKE_SYSTEM_NAME Generic` tells CMake this is bare metal, so it is a cross-compile with no OS.
- The compiler lines replace the host `gcc` with the ARM one.
- `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` stops CMake from trying to link a test program during configure. That would fail on bare metal because there is no startup code yet.
- `CMAKE_SIZE` is used by the post-build step that prints flash and RAM usage.

**Important:** CMake reads this file only when the `build/` directory is first created. If you change it, or you see "Manually-specified variables were not used: CMAKE_TOOLCHAIN_FILE", delete `build/` and configure again.

### 4.2 `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.22)
project(blink C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
set(LL   ${CMAKE_SOURCE_DIR}/lib/stm32f4xx_hal_driver)
set(DEV  ${CMAKE_SOURCE_DIR}/lib/cmsis_device_f4)
set(CORE ${CMAKE_SOURCE_DIR}/lib/cmsis_core)

add_executable(${PROJECT_NAME}.elf
  src/main.c
  ${DEV}/Source/Templates/system_stm32f4xx.c
  ${DEV}/Source/Templates/gcc/startup_stm32f446xx.s
  ${LL}/Src/stm32f4xx_ll_utils.c
  ${LL}/Src/stm32f4xx_ll_rcc.c
  ${LL}/Src/stm32f4xx_ll_gpio.c
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
  src ${CORE}/CMSIS/Core/Include ${DEV}/Include ${LL}/Inc)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
  STM32F446xx USE_FULL_LL_DRIVER)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
  ${CPU_FLAGS} -Og -g3 -Wall -Wextra
  -ffunction-sections -fdata-sections)

target_link_options(${PROJECT_NAME}.elf PRIVATE
  ${CPU_FLAGS}
  -T${CMAKE_SOURCE_DIR}/STM32F446xx_FLASH.ld
  -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map
  -Wl,--print-memory-usage
  --specs=nano.specs --specs=nosys.specs)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>)

# cmake --build build --target flash
add_custom_target(flash
  COMMAND openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
          -c "program $<TARGET_FILE:${PROJECT_NAME}.elf> verify reset exit"
  DEPENDS ${PROJECT_NAME}.elf)
```

Main lines:

| Line | Meaning |
|---|---|
| `project(blink C ASM)` | enables the C and assembler languages (the startup file is `.s`). The name `blink` gives `build/blink.elf` |
| `CMAKE_EXPORT_COMPILE_COMMANDS ON` | writes `build/compile_commands.json`, which IntelliSense reads to learn include paths and defines |
| `CPU_FLAGS` | `cortex-m4` core, Thumb instructions, hardware FPU (`fpv4-sp-d16`, single precision, which the F446 has). These must be on both compile and link lines |
| `add_executable(...)` | the source list. **Only files listed here get compiled.** Add `stm32f4xx_ll_<peripheral>.c` when you use a peripheral (for example `_usart.c`, `_tim.c`) |
| `target_include_directories` | where `#include "stm32f4xx.h"` and friends are found |
| `STM32F446xx` | selects the chip in `stm32f4xx.h`. The wrong value here gives a compile error |
| `USE_FULL_LL_DRIVER` | enables the function-style LL drivers (`LL_GPIO_Init` and so on) |
| `-Og -g3` | light optimization that is still debuggable, with full debug info including macros |
| `-ffunction-sections -fdata-sections` plus `--gc-sections` | puts every function and variable in its own section so the linker can drop unused ones |
| `-T...ld` | the linker script, which describes flash and RAM layout |
| `-Wl,--print-memory-usage` | prints flash and RAM used after every link |
| `--specs=nano.specs --specs=nosys.specs` | small C library (newlib-nano) and stub system calls, so no `_write`/`_sbrk` link errors |
| `flash` target | `cmake --build build --target flash` programs the board through OpenOCD, verifies, resets and exits |

Optional extra warnings for firmware: `-Wshadow -Wundef -Wdouble-promotion -Wformat=2`. Add `-Wconversion` later, since it is noisy with register code.

### 4.3 The linker script

Use the one from CubeMX (`STM32F446RETx_FLASH.ld`) and keep the file name in sync with the `-T` line. The part that depends on your chip is the `MEMORY` block. For an STM32F446RE:

```ld
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}
```

Check yours against the datasheet for your exact part. The rest of the script (sections, `_estack`, `ENTRY(Reset_Handler)`) is generic Cortex-M. If the linker script's symbols (`_sidata`, `_sdata`, `_edata`, `_sbss`, `_ebss`, `_estack`) don't match what the startup `.s` file expects, you get undefined-symbol errors at link time. The CubeMX script is matched to ST's startup file, which is why I recommend taking it from there.

### 4.4 `src/main.c` (F4 blink)

```c
#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_utils.h"

volatile uint32_t blink_count; /* watch this in the debugger */

int main(void)
{
    /* On F4, GPIO ports live on AHB1 (on G4 it was AHB2). */
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_5, LL_GPIO_MODE_OUTPUT);

    /* After reset the chip runs from the 16 MHz HSI. */
    LL_Init1msTick(SystemCoreClock);

    while (1) {
        LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_5);
        blink_count++;
        LL_mDelay(500);
    }
}
```

The LED on the Nucleo-F446RE is PA5. Another board will need another pin.

---

## 5. Build and flash from the terminal

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm-gcc.cmake
cmake --build build
cmake --build build --target flash
```

A good first configure prints `The C compiler identification is GNU ...` and takes a second or two. Verify the compiler it picked:

```bash
grep CMAKE_C_COMPILER: build/CMakeCache.txt
```

It should point to `arm-none-eabi-gcc`. If it points to `/usr/bin/cc`, delete `build/` and configure again with the toolchain file.

A successful build ends with a size table and the memory usage summary. The LED should blink at 1 Hz after flashing.

### Optional: `CMakePresets.json`

Presets make the command line and VS Code configure the same way, so the toolchain file cannot get lost:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build",
      "toolchainFile": "${sourceDir}/cmake/arm-gcc.cmake",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    }
  ],
  "buildPresets": [
    { "name": "debug", "configurePreset": "debug" }
  ]
}
```

Use it with `cmake --preset debug && cmake --build --preset debug`.

---

## 6. The `.vscode` files, explained

All four go in `.vscode/` in the project root, on the remote machine. VS Code accepts comments in these files, but they are kept out of the JSON below so you can copy it as is.

### 6.1 `tasks.json`: commands VS Code can run

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "configure",
      "type": "shell",
      "command": "cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=cmake/arm-gcc.cmake"
    },
    {
      "label": "build",
      "type": "shell",
      "command": "cmake --build build",
      "dependsOn": "configure",
      "group": { "kind": "build", "isDefault": true },
      "problemMatcher": "$gcc"
    },
    {
      "label": "flash",
      "type": "shell",
      "command": "cmake --build build --target flash"
    }
  ]
}
```

| Key | Meaning |
|---|---|
| `label` | the task name. `launch.json` refers to `build` by this name |
| `type: shell` | runs the command in a shell |
| `dependsOn: configure` | `build` runs `configure` first |
| `group.isDefault` | makes `build` the Ctrl+Shift+B task |
| `problemMatcher: $gcc` | parses GCC output so errors and warnings show in the Problems panel and are clickable |

Run `flash` from **Terminal → Run Task**.

### 6.2 `launch.json`: how F5 debugging starts

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug blink (OpenOCD + ST-Link)",
      "type": "cortex-debug",
      "request": "launch",
      "servertype": "openocd",
      "cwd": "${workspaceFolder}",
      "executable": "${workspaceFolder}/build/blink.elf",
      "device": "STM32F446RE",
      "configFiles": ["interface/stlink.cfg", "target/stm32f4x.cfg"],
      "gdbPath": "gdb-multiarch",
      "runToEntryPoint": "main",
      "preLaunchTask": "build"
    }
  ]
}
```

| Key | Meaning |
|---|---|
| `type: cortex-debug` | use the Cortex-Debug extension. Not `cppdbg`, which runs the ELF on the host and gives `Exec format error` |
| `servertype: openocd` | Cortex-Debug starts OpenOCD as the GDB server |
| `executable` | the ELF to flash and to read symbols from. The name must match your `project()` name |
| `device` | chip name for the register views and SVD lookup |
| `configFiles` | OpenOCD configs: the ST-Link probe and the F4 target |
| `gdbPath` | GDB that supports ARM |
| `runToEntryPoint: main` | halts at `main()` after flashing |
| `preLaunchTask: build` | rebuilds before every debug session |
| `cwd` | working directory for the session |

Optional keys:

- `"svdFile": "/path/to/STM32F446.svd"` gives the peripheral register viewer. Get the SVD from ST's CMSIS pack or the `cmsis-svd` repos.
- `"showDevDebugOutput": "raw"` shows the raw GDB conversation when something fails.

What happens on F5: build, then OpenOCD starts on port 50000, then `gdb-multiarch` connects, flashes the ELF, and runs to `main`.

### 6.3 `c_cpp_properties.json`: IntelliSense

```json
{
  "version": 4,
  "configurations": [
    {
      "name": "STM32F4",
      "compilerPath": "/usr/bin/arm-none-eabi-gcc",
      "cStandard": "c11",
      "intelliSenseMode": "gcc-arm",
      "compileCommands": "${workspaceFolder}/build/compile_commands.json",
      "includePath": [
        "${workspaceFolder}/src",
        "${workspaceFolder}/lib/cmsis_core/CMSIS/Core/Include",
        "${workspaceFolder}/lib/cmsis_device_f4/Include",
        "${workspaceFolder}/lib/stm32f4xx_hal_driver/Inc"
      ],
      "defines": [
        "STM32F446xx",
        "USE_FULL_LL_DRIVER"
      ]
    }
  ]
}
```

| Key | Meaning |
|---|---|
| `compilerPath` | lets IntelliSense find the compiler's built-in headers (`stdint.h`). Use the output of `which arm-none-eabi-gcc` |
| `intelliSenseMode: gcc-arm` | parse code the way GCC for ARM does |
| `compileCommands` | the file CMake generates. When it exists, IntelliSense uses the exact include paths and defines of your build, so new include directories are picked up automatically |
| `includePath`, `defines` | a fallback for when `build/` doesn't exist yet. Keep them in sync with `CMakeLists.txt` |

**Test:** Ctrl+click `LL_GPIO_TogglePin` in `main.c`. It should open the LL header. If not, run **C/C++: Log Diagnostics** from the command palette to see the paths being used.

### 6.4 `settings.json`: editor rules, toolchain path, lint

This is the full recommended file, merging everything:

```json
{
  "editor.rulers": [80],
  "editor.tabSize": 4,
  "editor.insertSpaces": true,
  "files.trimTrailingWhitespace": true,
  "files.insertFinalNewline": true,

  "cortex-debug.armToolchainPath": "/usr/bin",

  "c-cpp-flylint.language": "c",
  "c-cpp-flylint.standard": ["c11"],
  "c-cpp-flylint.clang.enable": false,
  "c-cpp-flylint.flawfinder.enable": false,
  "c-cpp-flylint.lizard.enable": false,
  "c-cpp-flylint.flexelint.enable": false,
  "c-cpp-flylint.pclp.enable": false,
  "c-cpp-flylint.cppcheck.enable": true,
  "c-cpp-flylint.cppcheck.force": false,
  "c-cpp-flylint.cppcheck.unusedFunction": false,
  "c-cpp-flylint.cppcheck.includePaths": [
    "${workspaceFolder}/src"
  ],
  "c-cpp-flylint.excludeFromWorkspacePaths": [
    "${workspaceFolder}/lib",
    "${workspaceFolder}/build"
  ]
}
```

Note what is **not** in there: `C_Cpp.autocomplete` and `C_Cpp.errorSquiggles` set to `disabled`. Those switch off IntelliSense completions and red squiggles. Delete them if you still have them.

| Key | Meaning |
|---|---|
| `editor.rulers: [80]` | draws a vertical line at column 80. It is only a visual guide and does not wrap or block anything. Enforcing the limit needs a formatter such as clang-format |
| `editor.tabSize`, `insertSpaces` | 4 spaces per indent, no tab characters |
| `files.trimTrailingWhitespace`, `insertFinalNewline` | cleans up on save |
| `cortex-debug.armToolchainPath` | folder holding `arm-none-eabi-objdump` and `nm`. Without it Cortex-Debug looks for `objdump-multiarch` and prints `ENOENT` errors. This is the **folder** (`/usr/bin`), not the compiler path |
| `c-cpp-flylint.language`, `standard` | lint C as C11 |
| `...clang.enable`, `flawfinder`, `lizard`, `flexelint`, `pclp` set to `false` | the extension tries to start every analyzer it supports. Turning off the ones you don't have removes the "Unable to activate ..." warnings |
| `...cppcheck.enable: true` | the one analyzer you use |
| `...cppcheck.force: false` | don't check every `#ifdef` combination, which is slow |
| `...cppcheck.unusedFunction: false` | a function unused in one file may be used in another, so this gives false positives |
| `...cppcheck.includePaths` | only your own `src/`. Feeding Cppcheck the ST headers produces noise |
| `...excludeFromWorkspacePaths` | don't lint the ST submodules or build output |

Some `c-cpp-flylint.*` setting names differ between extension versions. If VS Code underlines one as unknown, search "flylint" in the Settings UI and use the current name.

**Where settings can live.** VS Code has User, Remote [SSH], and Workspace settings. If a setting seems ignored, check all three (Ctrl+, then the tabs). Commands: **Preferences: Open Remote Settings (JSON)**, **Preferences: Open User Settings (JSON)**.

---

## 7. Verify the Cppcheck lint

```bash
cppcheck --version
```

Create `src/lint_test.c` with deliberate bugs. It doesn't need to be in `CMakeLists.txt`:

```c
void lint_test(void)
{
    int arr[5];
    arr[5] = 1;        /* out of bounds */

    int *p = 0;
    *p = 3;            /* null pointer dereference */

    int x;
    int y = x + 1;     /* uninitialized variable */
    (void)y;
}
```

Save it. The Problems panel (Ctrl+Shift+M) should show `arrayIndexOutOfBounds`, `nullPointer` and `uninitvar`. Delete the file afterwards.

If nothing appears:

1. Run `cppcheck --std=c11 src/lint_test.c`. If the terminal finds the bugs but VS Code doesn't, it is the extension config. If neither does, it is cppcheck.
2. Check the output channel (View → Output, then the "C/C++ Advanced Lint" entry).
3. Confirm the extension is installed on the SSH remote.
4. Save the file, then **Developer: Reload Window**.

---

## 8. Debug

1. Plug the board's ST-Link USB into the **Debian** machine.
2. Open **Run and Debug** (Ctrl+Shift+D), select **Debug blink (OpenOCD + ST-Link)**, press F5.
3. It builds, starts OpenOCD, flashes, and stops at `main`. Add `blink_count` to Watch. Use F10 (step over), F11 (step in), F5 (continue).

**If the board is plugged into your Windows machine instead:** the GDB server has to run next to the hardware. Run OpenOCD on Windows, set `"servertype": "external"` and `"gdbTarget": "<windows-ip>:3333"` in `launch.json`, and reach it over an SSH tunnel or VPN. Or use USB/IP to forward the ST-Link to Debian. Plugging into Debian is much simpler.

---

## 9. Troubleshooting (errors you have already seen)

| Symptom | Cause and fix |
|---|---|
| `Manually-specified variables were not used: CMAKE_TOOLCHAIN_FILE`, configure takes 0.0s | `build/` was configured earlier without the toolchain (often by CMake Tools). `rm -rf build` and configure again |
| `cannot execute binary file: Exec format error`, `Microsoft-MIEngine` in the log | wrong debugger. A `cppdbg` launch tried to run the ARM ELF on x86. Select the Cortex-Debug entry in Run and Debug. Don't use the CMake Tools debug button |
| `Failed to launch GDB: Remote connection closed` | OpenOCD died before GDB connected. Open the **gdb-server** terminal tab, or run `openocd -f interface/stlink.cfg -f target/stm32f4x.cfg` by hand |
| OpenOCD `LIBUSB_ERROR_ACCESS` or `open failed` | udev rules missing, you are not in `plugdev` yet, or the board is not attached to this machine (section 1.3) |
| OpenOCD `init mode failed (unable to connect to the target)` | wiring or power. If firmware on the chip disables SWD, connect under reset: add `-c "reset_config srst_only srst_nogate connect_assert_srst"` to the OpenOCD options |
| OpenOCD cannot find `target/stm32f4x.cfg` | OpenOCD too old. Use a current version |
| `ENOENT` for `objdump-multiarch` or `nm-multiarch` | set `cortex-debug.armToolchainPath` (section 6.4), or `apt install binutils-multiarch` |
| `Please select first the target STM32F4xx device` | the `STM32F446xx` define is missing or wrong in `CMakeLists.txt` |
| `fatal error: stdint.h` or `nano.specs not found` at build time | install `libnewlib-arm-none-eabi` and `libnewlib-nano-arm-none-eabi` |
| Undefined reference to an `LL_...` function | the matching `stm32f4xx_ll_<peripheral>.c` isn't in `add_executable` |
| Undefined `_sbrk`, `_write`, `_init` and similar | `--specs=nosys.specs` and `--specs=nano.specs` must both be on the link line |
| Undefined `_sidata`, `_estack` and similar | the linker script doesn't match the startup file. Use the CubeMX linker script |
| LED doesn't blink | wrong pin or wrong clock enable macro for the chip. Check the board schematic and the bus (AHB1 on F4) |
| Header click-through doesn't work | `C_Cpp.intelliSenseEngine` set to `disabled` somewhere (User, Remote or Workspace settings), or the include paths in `c_cpp_properties.json` don't match the real folders (use the `find` check in section 3.2) |
| "IntelliSense-related commands cannot be executed when `C_Cpp.intelliSenseEngine` is set to `disabled`" | same as above, reset that setting |
| "Unable to activate Lizard / Flexelint analyzer" | add the `lizard`, `flexelint` and `pclp` disables from section 6.4 |
| A header you edit in `lib/` is shown with errors | IntelliSense only knows files it has flags for. Make sure the `.c` file is in `add_executable` and `build/compile_commands.json` is up to date (reconfigure) |

---

## 10. Switching to another STM32

Change all of these together:

| What | Where | F446 value (example) |
|---|---|---|
| Device family submodules | `.gitmodules` / `lib/` | `cmsis_device_f4`, `stm32f4xx_hal_driver` (for G4: `_g4`) |
| Chip define | `CMakeLists.txt` and `c_cpp_properties.json` | `STM32F446xx` |
| System source | `CMakeLists.txt` | `system_stm32f4xx.c` |
| Startup file | `CMakeLists.txt` | `startup_stm32f446xx.s` |
| LL source files | `CMakeLists.txt` | `stm32f4xx_ll_*.c` |
| Include paths | `CMakeLists.txt` and `c_cpp_properties.json` | `.../cmsis_device_f4/Include`, `.../stm32f4xx_hal_driver/Inc` |
| CPU flags | `CMakeLists.txt` | M4 + FPU: `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard`. M0/M0+/M3 have no FPU, drop the `-mfpu` and `-mfloat-abi` flags and change `-mcpu` |
| Linker script | repo root and `-T` line | from CubeMX for your exact part |
| OpenOCD target | `CMakeLists.txt` flash target and `launch.json` | `target/stm32f4x.cfg` (G4: `stm32g4x.cfg`) |
| Device name | `launch.json` | `STM32F446RE` |
| Clock-enable macro and pins in `main.c` | `src/main.c` | `LL_AHB1_GRP1_...` on F4, `LL_AHB2_GRP1_...` on G4 |

After changing the chip: `rm -rf build`, configure, build.

---

## 11. Quick reference

```bash
# one-time
sudo apt install git cmake ninja-build openocd gdb-multiarch cppcheck \
                 gcc-arm-none-eabi libnewlib-arm-none-eabi libnewlib-nano-arm-none-eabi

# every project
git clone --recurse-submodules <url> && cd <project>
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=cmake/arm-gcc.cmake
cmake --build build
cmake --build build --target flash

# something weird with configure?
rm -rf build
```

In VS Code: **Ctrl+Shift+B** builds, **F5** debugs, **Ctrl+Shift+M** shows lint and compiler problems.
