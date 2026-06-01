# TriloOpenBootloader

[![Build All Presets](https://github.com/trilobio/TriloOpenBootloader/actions/workflows/build.yml/badge.svg)](https://github.com/trilobio/TriloOpenBootloader/actions/workflows/build.yml)
[![Release Build](https://github.com/trilobio/TriloOpenBootloader/actions/workflows/release.yml/badge.svg)](https://github.com/trilobio/TriloOpenBootloader/actions/workflows/release.yml)
![MCU](https://img.shields.io/badge/MCU-STM32H523CE-03234b?logo=stmicroelectronics)
![Protocol](https://img.shields.io/badge/Protocol-FDCAN-blue)

FDCAN bootloader for STM32H523CE based on ST's [OpenBootloader](https://github.com/STMicroelectronics/stm32-mw-openbl) middleware. Supports multiple board variants via CMake presets.

## Overview

The bootloader initialises FDCAN2 and waits for host commands using the STM32 bootloader protocol. Memory interfaces (Flash, RAM, OTP, Option Bytes, ENGI Bytes, System Memory, UUID) are registered so the host can read/write/erase them remotely.

> [!NOTE]
> The first 32 KB sector is write-protected at startup to prevent accidental self-erasure during mass-erase operations.

---

## Build Variants

Each variant is compiled with a board-specific preprocessor define that selects pin mappings and identity in [`Core/Inc/version.h`](Core/Inc/version.h).

| Preset | `BOARD_VARIANT` | Define | Description |
|:--|:--|:--|:--|
| `FTS` | `FTS` | `BLD_FTS` | FTS board |
| `TOOL_CONTROL` | `TOOL` | `BLD_TOOL_CONTROL` | Tool control board |
| `J1_CONTROL` | `J1` | `BLD_J1_CONTROL` | J1 joint control board |
| `J2_CONTROL` | `J2` | `BLD_J2_CONTROL` | J2 joint control board |
| `J3_4_CONTROL` | `J3_4` | `BLD_J3_CONTROL` | J3/J4 joint control board |
| `LED` | `LED` | `BLD_LED` | LED board |
| `DECK_SLOT` | `DECK` | `BLD_DECK_SLOT` | Deck slot board |
| `INTERMODULARITY` | `IM` | `BLD_INTERMODULARITY` | Intermodularity board |

Every variant has a corresponding `-Release` preset (e.g. `J1_CONTROL-Release`) that builds with `-O2` and no debug symbols.

---

## Building

**Requirements:** `arm-none-eabi-gcc`, `cmake` ≥ 3.20, `ninja`

```bash
# Debug
cmake --preset J1_CONTROL
cmake --build build/J1_CONTROL

# Release
cmake --preset J1_CONTROL-Release
cmake --build build/J1_CONTROL-Release
```

Post-build commands automatically produce versioned `.elf`, `.hex`, and `.bin` files:

```
build/J1_CONTROL-Release/TriloOpenBootloader-J1-v1.0.0.{elf,hex,bin}
```

> [!TIP]
> Version is determined by [`cmake/version.cmake`](cmake/version.cmake):
> - **Tagged releases** — version from the `-DFIRMWARE_VERSION` argument (e.g. `v1.0.0` from a CI/CD git tag)
> - **Development builds** — short git hash, with `-dirty` suffix for uncommitted changes

---

## Flashing

An interactive script in [`scripts/`](scripts/) downloads release binaries from GitHub and programs boards via ST-Link.

> [!IMPORTANT]
> **Prerequisites:** [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html), [GitHub CLI (`gh`)](https://cli.github.com), Python 3.8+ with `rich`, ST-Link debugger

```bash
./scripts/flash_bootloader.sh
# or
python3 scripts/flash_bootloader.py
```

The script will:
1. Show cached versions or fetch the latest releases from GitHub
2. Prompt you to select a board variant
3. Download the `.hex` file (cached in `~/.cache/trilobootloader/`)
4. Flash via `STM32_Programmer_CLI`

See [`scripts/README.md`](scripts/README.md) for details and troubleshooting.

---

## CI/CD

| Workflow | Trigger | Description |
|:--|:--|:--|
| [`build.yml`](.github/workflows/build.yml) | Pull requests | Builds all Debug + Release variants |
| [`release.yml`](.github/workflows/release.yml) | `v*` tags | Builds Release variants, uploads `.hex`/`.bin` as release assets |

---

## Project Structure

<details>
<summary>Click to expand</summary>

| Path | Description |
|:--|:--|
| `Core/Src/main.c` | Main program — clock config, bootloader init, polling loop |
| `Core/Src/stm32h5xx_hal_msp.c` | MSP (MCU Support Package) initialization |
| `Core/Src/stm32h5xx_hal_timebase_tim.c` | HAL timebase (TIM6) |
| `Core/Src/stm32h5xx_it.c` | Interrupt handlers |
| `Core/Src/system_stm32h5xx.c` | System clock configuration |
| `Core/Inc/version.h` | Firmware version and variant identifier |
| `Application/Core/syscalls.c` | Newlib syscall stubs |
| `Application/Core/sysmem.c` | Newlib memory allocation stubs |
| `Application/Startup/startup_stm32h523cetx.s` | Startup assembly |
| `OpenBootloader/App/app_openbootloader.c` | Bootloader entry — registers FDCAN + IWDG + memory interfaces |
| `OpenBootloader/Target/fdcan_interface.c` | FDCAN protocol interface |
| `OpenBootloader/Target/flash_interface.c` | Flash memory interface |
| `OpenBootloader/Target/ram_interface.c` | RAM interface |
| `OpenBootloader/Target/iwdg_interface.c` | Independent watchdog interface |
| `OpenBootloader/Target/optionbytes_interface.c` | Option bytes interface |
| `OpenBootloader/Target/otp_interface.c` | OTP memory interface |
| `OpenBootloader/Target/engibytes_interface.c` | Engineering bytes interface |
| `OpenBootloader/Target/systemmemory_interface.c` | System memory interface |
| `OpenBootloader/Target/common_interface.c` | Shared utility functions |
| `OpenBootloader/Target/interfaces_conf.h` | Board-specific pin/peripheral configuration |
| `Application/OpenBootloader/Target/uuid_interface.c` | UUID interface |
| `cmake/TriloOpenBootloader.cmake` | Source file list and include paths |
| `cmake/version.cmake` | Git-based version extraction |
| `cmake/gcc-arm-none-eabi.cmake` | Cross-compilation toolchain file |
| `scripts/flash_bootloader.py` | Interactive flash tool |

</details>

---

## Hardware

- **MCU:** STM32H523CE (Cortex-M33, 160 MHz, 512 KB Flash, 272 KB RAM)
- **Protocol:** FDCAN2 — connect an external transceiver to the FDCAN2 TX/RX pins, then wire CAN-H / CAN-L to your host adapter
