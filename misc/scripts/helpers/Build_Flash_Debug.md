# random-station: build, flash and debug

How to build the firmware, put it on the board, and debug it with OpenOCD,
GDB and VS Code. It also lists the debugger commands that are useful for
checking values, registers and memory.

For the first-time install of the tools and the VS Code extensions, see
[getting_started.md](getting_started.md). This file assumes that is done.

## How the pieces fit together

```
VS Code on Windows
   | Remote-SSH
   v
Debian machine  ---USB---  ST-Link on the board  ---SWD---  STM32F446
   |
   |-- cmake + ninja + arm-none-eabi-gcc   -> build/random-station.elf
   |-- OpenOCD       talks to the ST-Link, offers a GDB server
   |-- gdb-multiarch connects to OpenOCD (started for you by Cortex-Debug)
```

The board's USB cable goes into the **Debian** machine, because that is where
OpenOCD runs. Your laptop only shows the editor and the debug panels.

---

## 1. Quick reference

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm-gcc.cmake     # first time only
cmake --build build                                   # build
cmake --build build --target flash                    # flash
picocom -b 921600 /dev/ttyACM0                        # read the log (Ctrl+A, Ctrl+X to quit)
```

In VS Code: **Ctrl+Shift+B** builds, **F5** starts a debug session.

---

## 2. Build

### Configure and build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm-gcc.cmake
cmake --build build
```

- The toolchain file only takes effect when `build/` is first created. If CMake
  says `CMAKE_TOOLCHAIN_FILE` was not used, run `rm -rf build` and configure
  again.
- The output is `build/random-station.elf`. The name comes from `project()` in
  `CMakeLists.txt`, so if you rename the project, also change `executable` in
  `.vscode/launch.json`.
- Every link prints a flash and RAM usage summary.
- `cmake --build build -v` shows the full compiler command lines.
- `build/compile_commands.json` is what IntelliSense reads for include paths
  and defines.

### Build options

| Option | Effect |
|---|---|
| `-DLOG_ENABLE=ON` / `OFF` | builds the UART logger or removes it completely |
| `LOG_LEVEL=4` (compile definition) | includes `LOG_DBG` messages; 1 = errors only, 3 = default |
| `HSE_VALUE=8000000` (compile definition) | must match the board's HSE, or UART baud rates and clock helpers are wrong |

After changing `CMakeLists.txt` or an option, a normal `cmake --build build`
reconfigures automatically. Run `cmake -B build -DLOG_ENABLE=ON` to change a
cached option.

### Debug-friendly flags

The default `-Og -g3` is a compromise. If the debugger shows
`<optimized out>` for variables or steps in odd orders, temporarily compile
with `-O0` (change `-Og` to `-O0` in `target_compile_options`) and rebuild.
`-g3` keeps macro information, which lets GDB expand names like `USART2` and
`GPIOA`.

---

## 3. Flash

### With CMake

```bash
cmake --build build --target flash
```

This runs OpenOCD with these steps:

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/random-station.elf verify reset exit"
```

- `interface/stlink.cfg` selects the ST-Link probe.
- `target/stm32f4x.cfg` selects the STM32F4 chip family.
- `program ... verify reset exit` writes the ELF to flash, reads it back to
  compare, resets the chip so the new code starts, and quits.

### Check the connection first

```bash
lsusb | grep -i st-link
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
```

OpenOCD should report the ST-Link, the target voltage and a Cortex-M4 core,
then stay running. Press Ctrl+C to quit. Close it before starting a VS Code
debug session, because only one program can use the ST-Link at a time.

### If flashing fails

| Message or symptom | Likely cause and fix |
|---|---|
| `LIBUSB_ERROR_ACCESS` or `open failed` | udev rules missing or you are not in `plugdev`; replug after fixing |
| `init mode failed (unable to connect to the target)` | wiring, power, or firmware that stops SWD; try the connect-under-reset option below |
| `Error: couldn't bind ... port in use` | another OpenOCD or debug session is still running; stop it (`pkill openocd`) |
| flash verify failed | try `cmake --build build --target flash` once more; check power quality |

Connect under reset (for a chip that hangs right after reset or has a clock
problem):

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "reset_config srst_only srst_nogate connect_assert_srst" \
        -c "program build/random-station.elf verify reset exit"
```

---

## 4. Debugging in VS Code

### launch.json

`.vscode/launch.json` tells Cortex-Debug how to start a session:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug random-station (OpenOCD + ST-Link)",
      "type": "cortex-debug",
      "request": "launch",
      "servertype": "openocd",
      "cwd": "${workspaceFolder}",
      "executable": "${workspaceFolder}/build/random-station.elf",
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
| `type: cortex-debug` | use the Cortex-Debug extension (not `cppdbg`, which would try to run the ARM binary on the host) |
| `servertype: openocd` | start OpenOCD as the GDB server |
| `executable` | the ELF to flash and to read symbols from |
| `device` | chip name, used for register views |
| `configFiles` | OpenOCD config files, same as for flashing |
| `gdbPath` | a GDB that understands ARM |
| `runToEntryPoint` | stop at `main()` after flashing |
| `preLaunchTask` | rebuild before every session (task `build` in `tasks.json`) |

Optional additions:

- `"svdFile": "<path>/STM32F446.svd"` enables the peripheral register viewer.
  The SVD comes from ST's CMSIS pack.
- `"showDevDebugOutput": "raw"` prints the full GDB conversation, which helps
  when a session fails to start.
- `"liveWatch": { "enabled": true, "samplesPerSecond": 4 }` updates watched
  globals while the program runs (feature name from memory, check the
  Cortex-Debug docs for your version).

In `settings.json`, `"cortex-debug.armToolchainPath": "/usr/bin"` lets
Cortex-Debug find `arm-none-eabi-objdump` and `nm`.

### Starting a session

1. Connect to the Debian machine with Remote-SSH and open the project.
2. Plug the board into the Debian machine.
3. Open **Run and Debug** (Ctrl+Shift+D), choose the Cortex-Debug entry, press
   **F5**. Do not use the CMake Tools debug button.
4. It builds, starts OpenOCD, flashes, and stops at `main()`.

OpenOCD's own output is in the **gdb-server** terminal tab, and the extension's
messages are in the **Cortex-Debug** output channel. Look there first when a
session fails.

### Controls

| Key | Action |
|---|---|
| F5 | continue (run until the next breakpoint) |
| F10 | step over (run the line, do not enter functions) |
| F11 | step into |
| Shift+F11 | step out (run until the current function returns) |
| Ctrl+Shift+F5 | restart (reflash and reset) |
| Shift+F5 | stop the session |
| F9 | toggle a breakpoint on the current line |

### The panels

- **Variables:** locals and globals in the current scope. Click the arrow on a
  struct or array to expand it.
- **Watch:** type any expression, for example `blink_count`, `USART2->BRR`
  or `buf[0]`. It is re-evaluated each time the program stops.
- **Call Stack:** how you got here. Click a frame to see its variables.
- **Breakpoints:** list, enable and disable them.
- **Cortex Peripherals** (needs `svdFile`): peripheral registers with named
  fields.
- **Cortex Registers:** the CPU registers.
- **Memory view** (the memory-view extension): raw memory at an address.
- **Disassembly:** run **Debug: Open Disassembly View** from the command
  palette to step through machine instructions.

---

## 5. Breakpoints and watchpoints

- **Line breakpoint:** click in the gutter next to a line number.
- **Conditional breakpoint:** right-click the gutter, choose **Add Conditional
  Breakpoint**, and enter an expression such as `count == 9`. Use this to stop
  in a loop only when something specific happens.
- **Function breakpoint:** use the Breakpoints panel, or `break function_name`
  in the Debug Console.
- **Watchpoint** (stop when a variable changes): in the Debug Console type
  `watch blink_count`. The CPU's debug unit does this in hardware, so the
  program runs at full speed.

Hardware limits on the Cortex-M4: about **6 breakpoints in flash** and **4
watchpoints** at once. If GDB refuses a new one, delete some. Breakpoints in
flash are always hardware ones, so you cannot have dozens.

---

## 6. GDB commands in the Debug Console

The Debug Console is the panel next to Problems and Terminal while a session
is active. **Type the GDB command directly, with no prefix.** A leading `-exec`
does not work with Cortex-Debug (it reports "Undefined MI command").

### Print values

| Command | What it does |
|---|---|
| `p blink_count` | print a variable |
| `p/x blink_count` | in hexadecimal (`/d` decimal, `/t` binary, `/c` char) |
| `p SystemCoreClock` | check the clock the firmware believes in |
| `p/x USART2->BRR` | a peripheral register, by name (needs `-g3`) |
| `p/x GPIOA->ODR` | the GPIOA output register |
| `p *ptr` | what a pointer points to |
| `p buf` | a whole array |
| `p/x *buf@8` | 8 elements starting at `buf`, in hex |
| `p sizeof(log_line_t)` | size of a type |
| `p &variable` | the address of a variable |
| `set print pretty on` | print structs one field per line |
| `info locals` | all local variables of the current frame |
| `info args` | the current function's arguments |
| `display blink_count` | print it automatically at every stop (`undisplay` to remove) |

If a register name does not expand (macro information missing), read it by
address instead, as below.

### Memory

`x/NFU address` reads N units of memory. F is the format and U is the unit
size.

| Command | What it does |
|---|---|
| `x/wx 0x40004408` | one 32-bit word in hex (here USART2 `BRR`) |
| `x/8xw $sp` | 8 words from the stack pointer |
| `x/16xb &buf` | 16 bytes |
| `x/s text` | a C string |
| `x/10i $pc` | next 10 machine instructions |
| `x/4dw &arr` | 4 words in decimal |

Formats: `x` hex, `d` decimal, `u` unsigned, `t` binary, `c` char, `s` string,
`i` instruction. Units: `b` byte, `h` halfword, `w` word.

### Registers

| Command | What it does |
|---|---|
| `info registers` | all core registers |
| `info registers pc sp lr` | selected ones |
| `p $pc` / `p/x $sp` | one register as an expression |
| `p/x $xpsr` | the program status register |
| `info registers msp psp` | main and process stack pointers (if your GDB exposes them) |

### Control the program

| Command | What it does |
|---|---|
| `continue` | run |
| `next` / `step` | step over / into one source line |
| `finish` | run until the current function returns, and print its result |
| `until` | finish the current loop |
| `stepi` / `nexti` | one machine instruction |
| `bt` | backtrace, the call stack |
| `frame 2` | select stack frame 2 (then `info locals`) |
| `tbreak main` | one-time breakpoint |
| `break file.c:123` | breakpoint at a line |
| `info breakpoints` | list breakpoints and watchpoints |
| `delete 3` | remove breakpoint number 3 (`delete` alone removes all) |
| `watch var` / `rwatch var` / `awatch var` | stop on write / read / either |

### Change values while halted

| Command | What it does |
|---|---|
| `set var blink_count = 0` | change a variable |
| `set var GPIOA->ODR = 0x20` | write a peripheral register (use with care) |
| `set var $pc = main` | jump somewhere else (rarely a good idea) |

### Find your way around

| Command | What it does |
|---|---|
| `info line *0x080012AC` | which source line an address belongs to |
| `info symbol 0x080012AC` | which function or variable an address is in |
| `list` / `list function_name` | show source around the current spot |
| `disassemble` | the current function as machine code |

### Talk to OpenOCD (`monitor`)

`monitor` forwards a command to OpenOCD itself. Behavior inside the
Cortex-Debug console is from memory, so if one does not work, use the
telnet route in section 7.

| Command | What it does |
|---|---|
| `monitor reset halt` | reset the chip and stop at the first instruction |
| `monitor reset init` | reset and run OpenOCD's init script |
| `monitor halt` / `monitor resume` | stop / start the CPU |
| `monitor mdw 0x40004408 1` | read a word from memory |
| `monitor mww 0x20000000 0x1234` | write a word to memory |
| `monitor targets` | show target state |

---

## 7. Talking to OpenOCD directly (optional)

Cortex-Debug starts OpenOCD with these ports (visible in the Cortex-Debug
output channel when a session starts): GDB on **50000**, Tcl on **50001**,
telnet on **50002**. While a session is running, from the Debian terminal:

```bash
telnet localhost 50002
```

Useful OpenOCD commands there: `halt`, `resume`, `reset halt`,
`mdw <address> [count]` (read words), `mww <address> <value>` (write a word),
`reg` (CPU registers), `targets`. Quit with `exit`. Avoid typing resets here
while GDB is mid-step, since the two will disagree about the state.

---

## 8. Debugging with FreeRTOS

- **Stopping at a breakpoint stops every task.** The whole system is halted
  while you inspect it. SysTick stops with it, so timing-based behavior is
  distorted when you step through it.
- **Do not step over `vTaskDelay` or `vTaskDelayUntil`.** Stepping over a
  delay switches to other tasks. Put a breakpoint on the line after the delay
  and press F5.
- **Which task is running:** `p pxCurrentTCB->pcTaskName` (the name given to
  `xTaskCreate`). Variable names inside the kernel can vary with its version.
- **Kernel state variables:** static ones need the file name, for example
  `p 'tasks.c'::xTickCount`.
- **`configASSERT` stops in the debugger.** The macro executes `bkpt`, so a
  failed assertion halts at that point. Use the Call Stack to see which check
  failed and who called it.
- **Stack overflow:** the hook `vApplicationStackOverflowHook` loops forever.
  If the system seems frozen, press Pause and look at the call stack. If it
  is inside the hook, the task shown in `pxCurrentTCB->pcTaskName` ran out of
  stack, so increase its size in `rtos_task_create`.
- **Task listing in the debugger** is possible with the RTOS views extension
  and `"rtos": "FreeRTOS"` in `launch.json`, but it needs extra symbols from
  the kernel. Set it up when you need it.

---

## 9. Debugging crashes and faults

`src/faults.c` replaces the weak default handlers. After a crash, the CPU
stops in `fault_report()` (when a debugger is attached, via `bkpt`) or loops
there. The details are saved in globals:

```
p/x fault_pc        # address of the faulting instruction
p/x fault_lr
p/x fault_cfsr      # which fault happened (decode below)
p/x fault_hfsr
p/x fault_bfar      # bad address, valid if CFSR bit 15 is set
p/x fault_mmfar     # valid if CFSR bit 7 is set
info line *fault_pc
```

Common `fault_cfsr` values:

| Value | Meaning |
|---|---|
| `0x00008200` | precise bus error on a data access; `fault_bfar` holds the address |
| `0x00000400` | imprecise bus error; the PC is not exact (see below) |
| `0x00020000` | invalid state, often a bad function pointer |
| `0x00010000` | undefined instruction, often a wild jump |
| `0x00080000` | FPU instruction with the FPU disabled |
| `0x02000000` | divide by zero (when the trap is enabled) |
| `0x00001000` / `0x00000010` | stack error while entering the exception; often a stack overflow |

For an imprecise bus fault, uncomment the `DISDEFWBUF` line in `fault_init()`
so faults are reported at the exact instruction while debugging.

---

## 10. Reading the log while debugging

With `-DLOG_ENABLE=ON` the firmware prints to USART2. On the Debian machine:

```bash
picocom -b 921600 /dev/ttyACM0
```

This is a separate USB interface of the ST-Link, so it works at the same time
as the debug session. If the text is garbled, check `HSE_VALUE` and read the
baud divisor register (`x/wx 0x40004408`) after `uart2_init()` has run.

---

## 11. Troubleshooting

| Problem | Check |
|---|---|
| F5 does nothing useful, or `Exec format error` | the wrong debugger started; use the Cortex-Debug entry, not the CMake Tools debug button |
| `Remote connection closed` | OpenOCD died; read the **gdb-server** terminal tab, and stop other OpenOCD processes |
| Debug session cannot find the ELF | `executable` in `launch.json` does not match the name in `project()` |
| Breakpoint says "not set" or is greyed out | more than 6 flash breakpoints, or the source differs from the flashed ELF; rebuild |
| Variables show `<optimized out>` | compile with `-O0` for debugging |
| Source lines jump around when stepping | same: optimization; try `-O0` |
| `Undefined MI command: exec` | type the GDB command without `-exec` |
| `ENOENT` for `objdump-multiarch` | set `cortex-debug.armToolchainPath` to `/usr/bin` |
| Peripheral names do not expand in `p` | the code was built without `-g3`, or use the address with `x/wx` |
| Timing looks wrong (tick too fast or slow) | `p SystemCoreClock` and `p SysTick->LOAD`; `HSE_VALUE` and the clock setup |
| Program seems frozen | press Pause and read the call stack; look for the fault handler, the stack overflow hook, or `configASSERT` |

---

## 12. Command cheat sheet

```
p var            p/x var           p *ptr           p buf[0]@8
x/wx addr        x/8xw $sp         x/s str          x/10i $pc
info registers   info locals       bt               frame N
break f          tbreak main       watch var        info breakpoints
continue         next   step       finish           stepi
set var x = 1    info line *addr   info symbol addr  display var
monitor reset halt    monitor mdw addr 1
```

Not every command here has been run on your exact setup. If one behaves
differently, the Cortex-Debug and GDB documentation is the reference.
