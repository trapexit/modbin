"""Check explicit workspace precedence and signed bytes through the real CLI."""

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from signing_test import MESSAGE_PREFIX, RETAIL_KEYS

HEADER_BYTES = 256
SIGNATURE_BYTES = 64
WORKSPACE_FIELD = 0x2C
SIGNATURE_OFFSET_FIELD = 0xB0
SIGNATURE_SIZE_FIELD = 0xB4
INITIAL_WORKSPACE = 0x00ABCDEF
FIELD_OPTIONS = ["--subsystype=1", "--type=5", "--name=misc"]


def make_image():
    body = bytes((index * 37 + 11) % 256 for index in range(260)) + b"\xff" * 4
    header = bytearray(HEADER_BYTES)
    words = {
        0x00: 0xE1A00000,
        0x04: 0xE1A00000,
        0x08: 0xE1A00000,
        0x0C: 0xE1A00000,
        0x10: 0xEF000011,
        0x14: HEADER_BYTES + len(body),
        0x24: 0x00008000,
        0x28: 0x81234567,
        WORKSPACE_FIELD: INITIAL_WORKSPACE,
        0x40: 0xEF00010A,
    }
    for offset, value in words.items():
        header[offset : offset + 4] = value.to_bytes(4, "big")
    assert len(header) == HEADER_BYTES, f"fixture header length={len(header)}"
    return bytes(header) + body


def word(image, offset):
    return int.from_bytes(image[offset : offset + 4], "big")


def check_equal(actual, expected, field, context):
    assert actual == expected, (
        f"{context}\n{field}: expected {expected!r}, actual {actual!r}"
    )


def run_cli(command, directory, image, options, label):
    source = directory / f"{label}-input.aif"
    target = directory / f"{label}-output.aif"
    source.write_bytes(image)
    environment = os.environ.copy()
    environment["UBSAN_OPTIONS"] = environment.get("UBSAN_OPTIONS", "") + ":halt_on_error=1"
    result = subprocess.run(
        [*command, *options, str(source), str(target)],
        capture_output=True,
        text=True,
        env=environment,
        check=False,
    )
    context = (
        f"case={label}, options={options!r}, exit={result.returncode}"
        f"\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )
    check_equal(result.returncode, 0, "exit status", context)
    check_equal(target.is_file(), True, "output file exists", context)
    return target.read_bytes(), context


def check_unsigned(actual, image, workspace, context):
    check_equal(word(actual, WORKSPACE_FIELD), workspace, "workspace word", context)
    check_equal(len(actual), len(image), "unsigned file length", context)
    check_equal(actual[HEADER_BYTES:], image[HEADER_BYTES:], "unsigned body", context)
    check_equal(word(actual, SIGNATURE_OFFSET_FIELD), 0, "unsigned signature offset", context)
    check_equal(word(actual, SIGNATURE_SIZE_FIELD), 0, "unsigned signature size", context)


def check_signed(actual, image, workspace, key, context):
    check_equal(word(actual, WORKSPACE_FIELD), workspace, "signed workspace word", context)
    check_equal(len(actual), len(image) + SIGNATURE_BYTES, "signed file length", context)
    offset = word(actual, SIGNATURE_OFFSET_FIELD)
    check_equal(offset, len(image), "signature offset", context)
    check_equal(word(actual, SIGNATURE_SIZE_FIELD), SIGNATURE_BYTES, "signature size", context)
    check_equal(actual[HEADER_BYTES:offset], image[HEADER_BYTES:], "signed body", context)

    # Hash the emitted header/body, undoing only the post-hash signature size.
    unsigned = bytearray(actual[:offset])
    unsigned[SIGNATURE_SIZE_FIELD : SIGNATURE_SIZE_FIELD + 4] = b"\0" * 4
    digest = hashlib.md5(unsigned).hexdigest()
    message = int(MESSAGE_PREFIX + digest, 16)
    modulus, exponent = RETAIL_KEYS[key]
    expected_signature = pow(message, exponent, modulus).to_bytes(SIGNATURE_BYTES, "big")
    check_equal(actual[offset:], expected_signature, f"{key} MD5/RSA signature", context)


def workspace_orders(value, fields):
    option = f"--workspace={value}"
    return ([option, *fields], [*fields, option])


def main():
    command = sys.argv[1:]
    if not command:
        raise SystemExit("usage: workspace_test.py [runner ...] modbin")
    image = make_image()
    with tempfile.TemporaryDirectory(prefix="modbin-workspace-") as temporary:
        directory = Path(temporary)
        for value in (0, 0x12345678, 0xFFFFFFFF):
            outputs = []
            for order, options in enumerate(workspace_orders(value, FIELD_OPTIONS)):
                actual, context = run_cli(
                    command, directory, image, options, f"unsigned-{value}-{order}"
                )
                check_unsigned(actual, image, value, context)
                outputs.append(actual)
            check_equal(outputs[1], outputs[0], "workspace option order output", context)

        actual, context = run_cli(command, directory, image, FIELD_OPTIONS, "default-marker")
        check_unsigned(actual, image, 0x40000000 | INITIAL_WORKSPACE, context)

        # Reset may change other metadata; only pin workspace and file boundaries.
        for value in (0, 0x12345678):
            for order, options in enumerate(
                workspace_orders(value, [*FIELD_OPTIONS, "--reset"])
            ):
                actual, context = run_cli(
                    command, directory, image, options, f"reset-{value}-{order}"
                )
                check_unsigned(actual, image, value, context)

        signed_zero = None
        for key in RETAIL_KEYS:
            for value in (0, 0x12345678):
                outputs = []
                for order, options in enumerate(workspace_orders(value, FIELD_OPTIONS)):
                    actual, context = run_cli(
                        command,
                        directory,
                        image,
                        [*options, f"--sign={key}"],
                        f"signed-{key}-{value}-{order}",
                    )
                    check_signed(actual, image, value, key, context)
                    outputs.append(actual)
                    if key == "app" and value == 0:
                        signed_zero = actual
                check_equal(outputs[1], outputs[0], "signed option order output", context)

        # A changed workspace and key must replace, not retain or append to, the
        # old signature even when the input has no 3DO marker byte.
        actual, context = run_cli(
            command,
            directory,
            signed_zero,
            ["--workspace=4294967295", *FIELD_OPTIONS, "--sign=3do"],
            "resigned-new-workspace",
        )
        check_signed(actual, image, 0xFFFFFFFF, "3do", context)


if __name__ == "__main__":
    main()
