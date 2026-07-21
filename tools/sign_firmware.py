#!/usr/bin/env python3
"""
Sign a SentinelOS OTA firmware image with the ECDSA P-256 release key.

The value verified on-device (ota.c: ota_verify_signature) is:
    ECDSA_sign(private_key, SHA256(embedded_image_hash))
where embedded_image_hash is the SHA-256 ESP-IDF appends to the image
(the same value used for the plain integrity check, esp_partition_get_sha256).

With Secure Boot enabled, espsecure appends its own signature block AFTER
that hash, so it's no longer the last 32 bytes of the final signed .bin --
use --hash-source to point at the pre-signing "*-unsigned.bin" (or any
build without CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES) to extract it
correctly, while --bin stays the file that actually gets published/flashed.

The private key never lives in the SentinelOS repo -- keep it on the
release/update-server machine only.
"""

import argparse
import json
import sys
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec


def embedded_image_hash(hash_source_path: Path) -> bytes:
    data = hash_source_path.read_bytes()
    if len(data) < 32:
        raise ValueError("binary too small to contain an appended hash")
    return data[-32:]


def sign(hash_source_path: Path, key_path: Path) -> tuple[str, str]:
    digest = embedded_image_hash(hash_source_path)

    private_key = serialization.load_pem_private_key(key_path.read_bytes(), password=None)
    if not isinstance(private_key, ec.EllipticCurvePrivateKey):
        raise ValueError("key is not an EC private key")

    signature = private_key.sign(digest, ec.ECDSA(hashes.SHA256()))
    return digest.hex(), signature.hex()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin", required=True, type=Path, help="Path to sentinel_os.bin (the file to publish)")
    parser.add_argument(
        "--hash-source",
        type=Path,
        help="File to extract the embedded hash from, if different from --bin "
        "(e.g. sentinel_os-unsigned.bin when Secure Boot is enabled). "
        "Defaults to --bin.",
    )
    parser.add_argument("--key", required=True, type=Path, help="Path to the EC private key (PEM)")
    parser.add_argument("--manifest", type=Path, help="manifest.json to update in place")
    args = parser.parse_args()

    hash_source = args.hash_source or args.bin
    sha256_hex, signature_hex = sign(hash_source, args.key)

    print(f"sha256:    {sha256_hex}")
    print(f"signature: {signature_hex}")

    if args.manifest:
        manifest = json.loads(args.manifest.read_text())
        manifest["sha256"] = sha256_hex
        manifest["signature"] = signature_hex
        args.manifest.write_text(json.dumps(manifest, indent=4) + "\n")
        print(f"updated {args.manifest}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
