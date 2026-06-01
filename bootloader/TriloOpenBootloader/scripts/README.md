# TriloOpenBootloader Flashing Tools

Interactive scripts for flashing TriloOpenBootloader to STM32H523 boards via ST-Link.

## Prerequisites

### Required Tools

1. **STM32CubeCLT** (includes STM32_Programmer_CLI)
   - Download from: https://www.st.com/en/development-tools/stm32cubeclt.html
   - Default install path: `/opt/ST/STM32CubeCLT/`
   - After install, the script looks for `STM32_Programmer_CLI` automatically

2. **GitHub CLI (`gh`)** - Used to download release assets with your existing credentials
   - Ubuntu/Debian: `sudo apt-get install gh`
   - Fedora/RHEL: `sudo dnf install gh`
   - Or: https://cli.github.com
   - Authenticate once with: `gh auth login`

3. **Python 3.8+** with **rich** library
   - Built-in on most Linux distributions
   - Check: `python3 --version`
   - Install rich: `pip install rich`

### Hardware Setup

- ST-Link v2, v2-1, or v3 debugger connected to target board
- Target board powered and connected to host computer via ST-Link

## Installation

Make scripts executable:
```bash
chmod +x scripts/*.sh scripts/*.py
```

## Usage

### Interactive Flash (Recommended)

Start the interactive menu:
```bash
./scripts/flash_bootloader.sh
```

Or directly with Python:
```bash
python3 scripts/flash_bootloader.py
```

The script will:
1. **Show cached versions** - Previously downloaded bootloaders in `~/.cache/trilobootloader/`
2. **Option to check GitHub** - Fetch latest releases from GitHub
3. **Select variant** - Choose which board variant to flash
4. **Download or use local** - Auto-download if needed or use recently built binary
5. **Confirm and flash** - Final confirmation before programming the board with HEX file
6. **Repeat** - Option to flash another board

### Cached Binaries

Downloaded HEX files are cached in `~/.cache/trilobootloader/` on Linux. The script checks here first before downloading from GitHub.

Current cached HEX files:
```bash
ls -lh ~/.cache/trilobootloader/
```

Clear cache if needed:
```bash
rm -rf ~/.cache/trilobootloader/
```

### Using Local Builds

If you've recently built the bootloader locally, the script will prefer local binaries over downloading:

1. Build locally:
   ```bash
   cmake --preset J1_CONTROL
   cmake --build build/J1_CONTROL
   ```

2. Run the flash script - it will automatically find and use the local build

## Bootloader Variants

| Variant | CMake Preset       | Display Name        |
|---------|-------------------|---------------------|
| FTS     | FTS-Release          | FTS                 |
| TOOL    | TOOL_CONTROL-Release | Tool Control       |
| J1      | J1_CONTROL-Release   | J1 Control         |
| J2      | J2_CONTROL-Release   | J2 Control         |
| J3_4    | J3_4_CONTROL-Release | J3/J4 Control      |
| LED     | LED-Release          | LED Board           |
| DECK    | DECK_SLOT-Release    | Deck Slot           |
| IM      | INTERMODULARITY-Release | Intermodularity |

## Troubleshooting

### STM32_Programmer_CLI not found
Download and install STM32CubeCLT from ST's website:
- https://www.st.com/en/development-tools/stm32cubeclt.html
- Default install path is `/opt/ST/STM32CubeCLT/`

If installed to a custom location, the script can be updated by adding the path to `PROGRAMMER_CANDIDATES` in `flash_bootloader.py`.

### Permission denied when flashing
ST-Link devices require udev rules or sudo. Option 1 (recommended):

Create `/etc/udev/rules.d/99-stlink.rules`:
```
# ST-Link v2
SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3748", MODE="0666"
# ST-Link v2-1
SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="0666"
# ST-Link v3
SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374e", MODE="0666"
# ST-Link v3 SET
SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374f", MODE="0666"
```

Reload udev rules:
```bash
sudo udevadm control --reload
sudo udevadm trigger
```

Or run with sudo:
```bash
sudo ./scripts/flash_bootloader.sh
```

### Download failed: GitHub connection
- Check internet connection: `ping github.com`
- Verify git credentials: `git config --global user.name` and `git config --global user.email`
- Try manual download and place in `~/.cache/trilobootloader/`

### Binary not found for variant
Either:
1. Build locally first: `cmake --preset <VARIANT>-Release && cmake --build build/<VARIANT>-Release`
2. Or select a version that has releases on GitHub (tagged with `v*.*.*`)

## Building and Releasing

### Create a Release

1. Tag a commit with semantic version:
   ```bash
   git tag -a v1.2.3 -m "Release v1.2.3"
   git push origin v1.2.3
   ```

2. GitHub Actions automatically builds all variants and uploads them as release artifacts

3. Run the flash script and select the new version from the list

## Examples

### Flash J1 Control with v1.0.0
```
./scripts/flash_bootloader.sh
→ Select: 1 (v1.0.0)
→ Select: 3 (J1 Control)
→ Confirm: yes
```

### Batch update multiple boards
```
# Terminal 1: Flash J1 boards
./scripts/flash_bootloader.sh
# Select J1_CONTROL, flash several boards

# Terminal 2: Flash J2 boards  
./scripts/flash_bootloader.sh
# Select J2_CONTROL, flash several boards
```

## Script Details

### Python Implementation (`flash_bootloader.py`)

**Features:**
- Lists cached versions first (fast, no network calls)
- Option to check GitHub for new releases
- Downloads HEX files to `~/.cache/trilobootloader/`
- **Uses HEX files** for safety (address info embedded in file)
- Prefers local builds if available
- Interactive menu-driven interface
- Full write → verify → reset sequence
- **Linux only** (optimized for Linux environments)

**Dependencies:**
- Built-in Python libraries only (urllib, subprocess, pathlib, json)
- No external Python packages required

### Shell Wrapper (`flash_bootloader.sh`)

Simple wrapper that:
- Locates the Python script relative to its own directory
- Calls it with `python3`
- Works from any directory

## Manual Flashing

For advanced users, flash manually with HEX files (address embedded in file):

```bash
# Flash
STM32_Programmer_CLI --connect port=SWD mode=UR --write ~/.cache/trilobootloader/TriloOpenBootloader-J1-v1.0.0.hex --verify --rst

# Or with a local build
STM32_Programmer_CLI --connect port=SWD mode=UR --write build/J1_CONTROL-Release/TriloOpenBootloader-J1-*.hex --verify --rst
```

## Support

For issues:
1. Check that st-flash is installed and functioning
2. Verify ST-Link is connected and powered
3. Check GitHub connectivity
4. Review detailed error messages from the script
5. File an issue on the GitHub repository
