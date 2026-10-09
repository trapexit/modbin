"""Check retail CLI signatures with a standard-library MD5/RSA oracle."""

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

HEADER_BYTES = 256
SIGNATURE_BYTES = 64
SIGNATURE_OFFSET_FIELD = 0xB0
SIGNATURE_SIZE_FIELD = 0xB4

# Retail message encoding is an integer prefix, not a presumed PKCS block.
MESSAGE_PREFIX = (
    "1ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "003020300c06082a864886f70d020505000410"
)
RETAIL_KEYS = {
    "app": (
        int(
            "BC0B199086C7F26CBC9D50F404944DB4789FCBFCF7AD8DBC2120898ABEAAF311E"
            "EA20229035608841FA41073ABBD5D37500C60B53BFB46605740381B72C9DB71",
            16,
        ),
        int(
            "18B2207E61A51ACA7B0EF215CA102C105A9329F824130FFD38208CCFC2F0B2915"
            "B8AD1E7772334381737D232B183C869C34940BC8769C97E18D7B0E78C492991",
            16,
        ),
    ),
    "3do": (
        int(
            "B19462B00D8D6E1EC909AB385E06FE034BFD282E9FFDC584838C15F12593DD1E"
            "3A8B5626F1B9D0ED0C384EF6C5D14512BD72DDB85B44080E0472C03D0AFC4C97",
            16,
        ),
        int(
            "42F7CD9BCD109805BE150A60107D9C8F8BB9A5CCA78361588EEF665AF1ABE887"
            "DBC2593D0868F364A93C8CB8CC6F4BCC6A3DE57E04B17AC52F2649939C453F61",
            16,
        ),
    ),
}


def make_image():
    body = bytes((index * 73 + 19) % 256 for index in range(260)) + b"\xff" * 4
    header = bytearray(HEADER_BYTES)
    words = {
        0x00: 0xE1A00000,  # Uncompressed AIF NOP.
        0x04: 0xE1A00000,
        0x08: 0xE1A00000,
        0x0C: 0xE1A00000,
        0x10: 0xEF000011,
        0x14: HEADER_BYTES + len(body),
        0x24: 0x00008000,
        0x28: 0x81234567,
        0x40: 0xEF00010A,
        0xA8: 0x80000000,
        0xAC: 0xFFEEDDCC,
        0xBC: 0x87654321,
        0xE0: 0x01020304,
    }
    for offset, value in words.items():
        header[offset : offset + 4] = value.to_bytes(4, "big")
    header[0x2C : 0x2D] = b"\x40"
    header[0xC0 : 0xE0] = b"UB regression".ljust(32, b"\0")
    assert len(header) == HEADER_BYTES
    return bytes(header) + body


def expected_image(image, key):
    unsigned = bytearray(image)
    offset = len(image)
    unsigned[SIGNATURE_OFFSET_FIELD : SIGNATURE_OFFSET_FIELD + 4] = offset.to_bytes(4, "big")
    # The offset is patched BEFORE hashing; size remains zero until AFTER hashing.
    assert unsigned[SIGNATURE_SIZE_FIELD : SIGNATURE_SIZE_FIELD + 4] == b"\0" * 4
    digest = hashlib.md5(unsigned).hexdigest()
    message = int(MESSAGE_PREFIX + digest, 16)
    modulus, exponent = RETAIL_KEYS[key]
    signature = pow(message, exponent, modulus).to_bytes(SIGNATURE_BYTES, "big")
    unsigned[SIGNATURE_SIZE_FIELD : SIGNATURE_SIZE_FIELD + 4] = SIGNATURE_BYTES.to_bytes(4, "big")
    return bytes(unsigned) + signature


def check_signing(command, directory, image, key):
    source = directory / "unsigned.aif"
    target = directory / f"signed-{key}.aif"
    source.write_bytes(image)
    environment = os.environ.copy()
    # Append last so a caller's recoverable sanitizer setting cannot hide UB.
    environment["UBSAN_OPTIONS"] = environment.get("UBSAN_OPTIONS", "") + ":halt_on_error=1"
    result = subprocess.run(
        [*command, f"--sign={key}", str(source), str(target)],
        capture_output=True,
        text=True,
        env=environment,
        check=False,
    )
    context = f"key={key}, exit={result.returncode}\nstderr:\n{result.stderr}"
    assert result.returncode == 0, context
    assert target.is_file(), context
    actual = target.read_bytes()
    expected = expected_image(image, key)
    assert len(actual) == len(image) + SIGNATURE_BYTES, context
    offset = int.from_bytes(actual[SIGNATURE_OFFSET_FIELD : SIGNATURE_OFFSET_FIELD + 4], "big")
    size = int.from_bytes(actual[SIGNATURE_SIZE_FIELD : SIGNATURE_SIZE_FIELD + 4], "big")
    assert offset == len(image), context
    assert size == SIGNATURE_BYTES, context
    assert actual[HEADER_BYTES:offset] == image[HEADER_BYTES:], context
    assert actual[offset:] == expected[offset:], context
    assert actual[:HEADER_BYTES] == expected[:HEADER_BYTES], context


def main():
    command = sys.argv[1:]
    if not command:
        raise SystemExit("usage: signing_test.py [runner ...] modbin")
    image = make_image()
    with tempfile.TemporaryDirectory(prefix="modbin-signing-") as temporary:
        for key in RETAIL_KEYS:
            check_signing(command, Path(temporary), image, key)


if __name__ == "__main__":
    main()
