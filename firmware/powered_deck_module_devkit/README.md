# Powered Deck Module firmware

## Board IDs

Each module must have a unique CAN board ID. The supported fleet range is:

```text
100 through 115
```

The normal CMake default is board ID `110`. The root-level `just` recipes override
the ID at configure time and use a separate build directory for each board.

Run the board-specific commands from this module directory:

```bash
cd /Users/seb/trilo/1.x_devkit/firmware/powered_deck_module_devkit
```

## Build one board

Build board `102` as a Release image:

```bash
just build-module 102
```

Build a Debug image instead:

```bash
just build-module 102 Debug
```

The outputs are kept in the board-specific directory:

```text
firmware/powered_deck_module_devkit/build/board-102-Release/pdm_devkit.elf
firmware/powered_deck_module_devkit/build/board-102-Release/pdm_devkit.bin
```

## Build all 16 boards

```bash
just build-all
```

This builds board IDs `100` through `115`, producing one `.elf` and `.bin` per
board. Use `Debug` to build Debug images:

```bash
just build-all Debug
```

## Flash one board

Connect the ST-Link to the target module, then run:

```bash
just flash 102
```

This builds the Release image for board `102`, then flashes:

```text
firmware/powered_deck_module_devkit/build/board-102-Release/pdm_devkit.elf
```

For a Debug image:

```bash
just flash 102 Debug
```

Only flash one physically connected module at a time.

## Default-board recipes

The basic local recipes use the default board ID (`110`):

```bash
cd firmware/powered_deck_module_devkit
just build
just flash
```

Use the board-specific recipes when selecting a particular board ID:

```bash
just build-module 102
just build-all
just flash 102
```
