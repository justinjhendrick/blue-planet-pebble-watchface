#!/usr/bin/env python3
from collections import defaultdict
import os
import time
from pathlib import Path
import sys
from subprocess import check_call, CalledProcessError

THIS_DIR = Path(__file__).absolute().parent


def reset() -> None:
    check_call(["pebble", "kill"])
    check_call(["pebble", "wipe"])

def sleep_until(t: float) -> None:
    now = time.monotonic()
    d = t - now
    if d > 0.0:
        time.sleep(d)

def run(platforms: list[str]) -> int:
    reset()
    screenshot_fnames = defaultdict(list)
    check_call(["pebble", "build"])
    for platform in platforms:
        check_call(["pebble", "install", "--emulator", platform])
        for minute in range(12 * 60 // 2):
            begin = time.monotonic()
            screenshot_fname = f"screenshot_{platform}_{minute}.png"
            check_call(["pebble", "screenshot", "--emulator", platform, screenshot_fname])
            screenshot_fnames[platform].append(screenshot_fname)
            sleep_until(begin + 1.0)
    reset()

    # Combine all the pngs into one gif with ImageMagick
    # https://joeldare.com/combining-multiple-images-into-an-animated-gif-with-imagemagick
    for platform, fnames in screenshot_fnames.items():
        check_call(
            [
                "convert",
                "-delay",
                "10",
                "-loop",
                "0",
                "-dispose",
                "previous",
                *fnames,
                f"screenshots_{platform}.gif",
            ]
        )

    return 0


def main() -> int:
    return run(["emery"])


if __name__ == "__main__":
    sys.exit(main())
