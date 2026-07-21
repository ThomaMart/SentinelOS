#!/usr/bin/env python3

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRMWARE = os.path.join(ROOT, "firmware")

ESP_IDF_EXPORT = os.path.expanduser("~/esp/esp-idf-v6/export.sh")


def run(command: str) -> None:
    print(f"\n>>> {command}\n")

    result = subprocess.run(
        [
            "bash",
            "-c",
            f"""
            source "{ESP_IDF_EXPORT}"
            {command}
            """
        ],
        cwd=FIRMWARE,
    )

    if result.returncode != 0:
        sys.exit(result.returncode)


def main():
    print("=" * 60)
    print(" SentinelOS Build Tool")
    print("=" * 60)

    run("idf.py fullclean")
    run("idf.py build")
    run("idf.py flash")
    run("idf.py monitor")


if __name__ == "__main__":
    main()