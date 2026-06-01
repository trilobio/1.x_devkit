#!/usr/bin/env python3
"""
Interactive bootloader flashing script for TriloOpenBootloader.
Downloads bootloader binaries from GitHub releases.
Flashes via STM32CubeCLT (STM32_Programmer_CLI).
"""

import subprocess
import re
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, List, Tuple

from rich.console import Console

console = Console()


@dataclass
class Variant:
    name: str
    display_name: str
    short_name: str


VARIANTS = [
    Variant("DECK_SLOT", "Deck Slot", "DECK"),
    Variant("FTS", "FTS", "FTS"),
    Variant("INTERMODULARITY", "Intermodularity", "IM"),
    Variant("J1_CONTROL", "J1 Control", "J1"),
    Variant("J2_CONTROL", "J2 Control", "J2"),
    Variant("J3_4_CONTROL", "J3/J4 Control", "J3_4"),
    Variant("LED", "LED Board", "LED"),
    Variant("TOOL_CONTROL", "Tool Control", "TOOL"),
]

# GitHub repository details
GITHUB_OWNER = "trilobio"
GITHUB_REPO = "TriloOpenBootloader"
GITHUB_REPO_URL = f"https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}"

# STM32CubeCLT programmer CLI install location
PROGRAMMER_BASE = "/opt/st"

# Local special firmware binary (placed manually next to this script)
LED_FRAME_HEX = Path(__file__).parent / "led_frame.hex"

# Sentinel values returned by numbered_menu for navigation
_BACK = -1


def numbered_menu(
    title: str,
    items: list[str],
    *,
    allow_back: bool = False,
) -> Optional[int]:
    """Display a numbered menu and return the selected index.

    Returns ``_BACK`` for 'b', or ``None`` on Ctrl-C/Ctrl-D.
    Empty input re-displays the menu.
    """
    while True:
        console.print(f"\n[bold]{title}[/bold]")
        for i, item in enumerate(items, 1):
            console.print(f"  [cyan]{i:>2}[/cyan]) {item}", highlight=False)
        if allow_back:
            console.print(f"  [cyan] b[/cyan]) Back")
        console.print(f"  [cyan] x[/cyan]) Exit")

        try:
            choice = console.input("[bold]#> [/bold]").strip()
        except (EOFError, KeyboardInterrupt):
            return None
        if not choice:
            continue
        if choice.lower() == "x":
            return None
        if choice.lower() == "b" and allow_back:
            return _BACK
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(items):
                return idx
        except ValueError:
            pass
        console.print("[red]Invalid selection.[/red]")


def find_programmer() -> Optional[str]:
    """Locate STM32_Programmer_CLI in /opt/st/stm32cubeclt_* directories."""
    base_path = Path(PROGRAMMER_BASE)
    
    if not base_path.exists():
        return None
    
    # Find all stm32cubeclt_* directories and sort by name (reverse = newest first)
    matching = sorted(base_path.glob("stm32cubeclt_*"), reverse=True)
    
    for version_dir in matching:
        if not version_dir.is_dir():
            continue
        
        programmer_path = version_dir / "STM32CubeProgrammer" / "bin" / "STM32_Programmer_CLI"
        if programmer_path.exists():
            try:
                subprocess.run([str(programmer_path), "--version"], check=True, capture_output=True)
                return str(programmer_path)
            except subprocess.CalledProcessError:
                continue
    
    return None


def get_cache_dir() -> Path:
    """Get the cache directory for downloaded binaries."""
    cache_dir = Path.home() / ".cache" / "trilobootloader"
    cache_dir.mkdir(parents=True, exist_ok=True)
    return cache_dir


def get_git_root() -> Path:
    """Get the project root directory."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True,
        )
        return Path(result.stdout.strip())
    except (subprocess.CalledProcessError, FileNotFoundError):
        return Path(__file__).parent.parent


def _version_sort_key(tag: str) -> Tuple:
    """Parse a version tag like 'v1.2.3' into a tuple for sorting."""
    match = re.match(r"v?(\d+)(?:\.(\d+))?(?:\.(\d+))?", tag)
    if match:
        return tuple(int(g) if g else 0 for g in match.groups())
    return (0, 0, 0)


def get_last_tags(count: int = 3) -> List[str]:
    """Get the last N git tags from GitHub using git ls-remote with SSH."""
    ssh_url = f"git@github.com:{GITHUB_OWNER}/{GITHUB_REPO}.git"
    
    try:
        result = subprocess.run(
            ["git", "ls-remote", "--tags", ssh_url],
            capture_output=True,
            text=True,
            check=True,
            timeout=10,
        )
        
        tags = []
        for line in result.stdout.strip().split('\n'):
            if not line or '\t' not in line:
                continue
            parts = line.split('\t')
            if len(parts) >= 2:
                tag_ref = parts[1]
                if tag_ref.startswith('refs/tags/'):
                    tag = tag_ref.replace('refs/tags/', '').rstrip('^{}')
                    if tag not in tags:
                        tags.append(tag)
        
        tags.sort(key=_version_sort_key, reverse=True)
        return tags[:count]
        
    except subprocess.CalledProcessError as e:
        console.print(f"[red]Failed to fetch tags from GitHub: {e.stderr}[/red]")
        return []
    except Exception as e:
        console.print(f"[red]Error fetching tags: {e}[/red]")
        return []


def check_gh_installed() -> bool:
    """Check if GitHub CLI (gh) is installed."""
    try:
        subprocess.run(["gh", "--version"], check=True, capture_output=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def download_binary(tag: str, variant: Variant) -> Optional[Path]:
    """Download a HEX file for a specific tag and variant using gh CLI."""
    cache_dir = get_cache_dir()
    
    hex_filename = f"TriloOpenBootloader-{variant.short_name}-{tag}.hex"
    hex_path = cache_dir / hex_filename
    
    if hex_path.exists():
        console.print(f"  [green]Using cached: {hex_filename}[/green]")
        return hex_path
    
    if not check_gh_installed():
        console.print("[red]GitHub CLI (gh) not found. Install with:[/red]")
        console.print("  sudo apt-get install gh    # Ubuntu/Debian")
        console.print("  sudo dnf install gh        # Fedora/RHEL")
        console.print("  Or: https://cli.github.com")
        return None
    
    console.print(f"  [cyan]Downloading {hex_filename}...[/cyan]")
    
    try:
        result = subprocess.run(
            [
                "gh", "release", "download", tag,
                "--repo", f"{GITHUB_OWNER}/{GITHUB_REPO}",
                "--pattern", hex_filename,
                "--dir", str(cache_dir),
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        
        if hex_path.exists():
            console.print(f"  Downloaded {hex_filename}")
            return hex_path
        else:
            console.print(f"  [red]File not found in release: {hex_filename}[/red]")
            return None
        
    except subprocess.CalledProcessError as e:
        console.print(f"  [red]Download failed: {e.stderr.strip()}[/red]")
        if hex_path.exists():
            hex_path.unlink()
        return None


def flash_bootloader(binary_path: Path) -> bool:
    """Flash the bootloader using STM32_Programmer_CLI with HEX file."""
    console.print(f"\n[bold]Flashing: {binary_path.name}[/bold]")
    console.print(f"  Size: {binary_path.stat().st_size} bytes")

    programmer = find_programmer()
    if not programmer:
        console.print("[red]STM32_Programmer_CLI not found.[/red]")
        console.print("  Install STM32CubeCLT from:")
        console.print("  https://www.st.com/en/development-tools/stm32cubeclt.html")
        console.print("  Install to: /opt/st/stm32cubeclt_X.X.X/")
        return False

    try:
        console.print("  [cyan]Programming and verifying flash...[/cyan]")
        subprocess.run(
            [
                programmer,
                "--connect", "port=SWD", "mode=UR",
                "--write", str(binary_path),
                "--verify",
                "--rst",
            ],
            check=True,
        )

        console.print("[bold green]Flash successful![/bold green]")
        return True

    except subprocess.CalledProcessError as e:
        console.print(f"[bold red]Flash failed![/bold red] Error code: {e.returncode}")
        return False


def get_cached_versions() -> List[str]:
    """Get list of cached bootloader versions."""
    cache_dir = get_cache_dir()
    versions = set()
    
    # Extract version from filename: TriloOpenBootloader-<VARIANT>-<VERSION>.hex
    for hex_file in cache_dir.glob("TriloOpenBootloader-*-*.hex"):
        parts = hex_file.stem.split("-")
        if len(parts) >= 3:
            # Version is after the second dash
            version = "-".join(parts[2:])
            versions.add(version)
    
    return sorted(versions, reverse=True)  # Newest first


def _cleanup_stale_cache(valid_tags: List[str]) -> None:
    """Delete cached binaries whose version is not in *valid_tags*."""
    cache_dir = get_cache_dir()
    for hex_file in cache_dir.glob("TriloOpenBootloader-*-*.hex"):
        parts = hex_file.stem.split("-")
        if len(parts) >= 3:
            version = "-".join(parts[2:])
            if version not in valid_tags:
                console.print(f"  [dim]Removing stale cache: {hex_file.name}[/dim]")
                try:
                    hex_file.unlink()
                except OSError as e:
                    console.print(
                        f"  [yellow]Could not remove stale cache {hex_file.name}: {e}[/yellow]"
                    )


def select_version() -> Optional[str]:
    """Prompt user to select a firmware version, checking cache first."""
    cached_versions = get_cached_versions()

    if cached_versions:
        items = list(cached_versions) + ["Check for new versions on GitHub"]
        idx = numbered_menu("Select firmware version:", items)
        if idx is None:
            return None
        if idx < len(cached_versions):
            return cached_versions[idx]
        # Fall through to fetch from GitHub

    console.print("[dim]Fetching available versions from GitHub...[/dim]")
    tags = get_last_tags(3)

    if not tags:
        console.print("[red]No firmware releases found.[/red]")
        return None

    _cleanup_stale_cache(tags)

    idx = numbered_menu("Select firmware version:", tags)
    if idx is None:
        return None
    return tags[idx]


def select_variant(version: str) -> Optional[Variant]:
    """Prompt user to select a bootloader variant or change version.

    Returns a Variant, ``"CHANGE_VERSION"``, ``"EXIT"``, or ``None``.
    """
    items = [v.display_name for v in VARIANTS]
    items.append(f"Change version (current: {version})")
    idx = numbered_menu(
        f"Select bootloader variant  [dim](version: {version})[/dim]:",
        items,
    )
    if idx is None:
        return "EXIT"
    if idx == len(VARIANTS):
        return "CHANGE_VERSION"
    return VARIANTS[idx]


def main():
    """Main interactive loop."""
    console.rule("[bold bright_blue]Bootloader Flash Tool[/bold bright_blue]")
    
    version = None
    
    while True:
        # Top-level menu
        top_items = ["Flash bootloader", "LED - EMPTY FRAME ONLY"]
        top_idx = numbered_menu("Select action:", top_items)
        if top_idx is None:
            console.print("[dim]Exiting.[/dim]")
            break

        # Handle LED frame binary
        if top_idx == 1:
            if not LED_FRAME_HEX.exists():
                console.print(
                    f"[red]led_frame.hex not found at {LED_FRAME_HEX}[/red]"
                )
                try:
                    console.input("[dim]Press Enter to continue...[/dim]")
                except (EOFError, KeyboardInterrupt):
                    break
                continue
            binary = LED_FRAME_HEX
            console.print(
                f"\n[yellow]About to flash LED - EMPTY FRAME ONLY[/yellow]"
            )
            console.print(f"  Binary: {binary.name}")
            try:
                confirm = console.input("[bold]Continue? (y/n) [/bold]").strip().lower()
            except (EOFError, KeyboardInterrupt):
                break
            if confirm in ["yes", "y"]:
                flash_bootloader(binary)
            else:
                console.print("[dim]Cancelled.[/dim]")
            continue

        # Bootloader flow
        while True:
            # Step 1: Select version (only when not yet chosen or explicitly changed)
            if not version:
                version = select_version()
                if not version:
                    break
            
            # Step 2: Select variant (shows current version, offers change option)
            result = select_variant(version)
            if result == "EXIT":
                break
            if result == "CHANGE_VERSION":
                version = None
                continue
            if result is None:
                continue
            variant = result
            
            # Step 3: Get or download binary
            console.print(
                f"\n[bold]Preparing {variant.display_name} bootloader "
                f"({version})...[/bold]"
            )
            binary = download_binary(version, variant)
            
            if not binary or not binary.exists():
                console.print(f"[red]Could not obtain binary for {variant.display_name}[/red]")
                try:
                    console.input("[dim]Press Enter to continue...[/dim]")
                except (EOFError, KeyboardInterrupt):
                    break
                continue
            
            # Step 4: Confirm and flash
            console.print(
                f"\n[yellow]About to flash {variant.display_name} "
                f"(version: {version})[/yellow]"
            )
            console.print(f"  Binary: {binary.name}")
            try:
                confirm = console.input("[bold]Continue? (y/n) [/bold]").strip().lower()
            except (EOFError, KeyboardInterrupt):
                break
            
            if confirm in ["yes", "y"]:
                flash_bootloader(binary)
            else:
                console.print("[dim]Cancelled.[/dim]")


if __name__ == "__main__":
    main()

