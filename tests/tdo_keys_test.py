"""Check release-mode key helpers, including their fatal invalid-key contract."""

import os
import resource
import signal
import subprocess
import sys

from signing_test import MESSAGE_PREFIX, RETAIL_KEYS

COMPONENTS = ("n", "d", "m")
INVALID_KEY = "not-a-retail-key"


def run_helper(command, component, key, environment):
    return subprocess.run(
        [*command, component, key],
        capture_output=True,
        text=True,
        env=environment,
        check=False,
    )


def check_valid(command, component, key, expected, environment):
    result = run_helper(command, component, key, environment)
    context = (
        f"component={component}, key={key}, exit={result.returncode}"
        f"\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )
    if result.returncode != 0:
        raise AssertionError(context)
    try:
        actual = int(result.stdout.strip(), 16)
    except ValueError as error:
        raise AssertionError(context) from error
    if actual != expected:
        raise AssertionError(context)


def check_invalid(command, component, environment):
    result = run_helper(command, component, INVALID_KEY, environment)
    context = (
        f"component={component}, invalid key, exit={result.returncode}"
        f"\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )
    if result.returncode != -signal.SIGABRT:
        raise AssertionError(context)
    if result.stdout or "runtime error:" in result.stderr:
        raise AssertionError(context)


def main():
    command = sys.argv[1:]
    if not command:
        raise SystemExit("usage: tdo_keys_test.py [runner ...] tdo_keys_test")
    # Expected aborts must not leave core files; children inherit this limit.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    environment = os.environ.copy()
    environment["UBSAN_OPTIONS"] = environment.get("UBSAN_OPTIONS", "") + ":halt_on_error=1"
    digest = bytes(range(16)).hex()
    message = int(MESSAGE_PREFIX + digest, 16)
    for key, (modulus, exponent) in RETAIL_KEYS.items():
        for component, expected in zip(COMPONENTS, (modulus, exponent, message)):
            check_valid(command, component, key, expected, environment)
    for component in COMPONENTS:
        check_invalid(command, component, environment)


if __name__ == "__main__":
    main()
