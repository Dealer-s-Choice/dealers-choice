#!/usr/bin/env python3

import subprocess
import sys
import os
import time

def cleanup(server_proc):
    print("Cleaning up server...")
    try:
        server_proc.terminate()
        server_proc.wait(timeout=3)
    except Exception:
        server_proc.kill()

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <test_binary_name>", file=sys.stderr)
        sys.exit(1)

    build_root = os.environ.get("MESON_BUILD_ROOT")
    test_root = os.environ.get("MESON_BUILD_TEST_ROOT")

    if not build_root or not test_root:
        print("Error: MESON_BUILD_ROOT and MESON_BUILD_TEST_ROOT must be set in the environment.", file=sys.stderr)
        sys.exit(1)

    test_binary = os.path.join(test_root, sys.argv[1])
    server_binary = os.path.join(build_root, "dealers-choice")

    port = int(os.environ.get("DC_PORT", "22777"))

    # Launch the server in test mode, then wait briefly for it to start.
    # A TCP probe connection is not used because the server would accept it as
    # a real client slot.  The test binary retries connections internally if
    # the server is not yet ready within this window.
    server_cmd = [server_binary, "--server", "---test", "--port", str(port)]
    # Opt-in network-health logging (slow-send / ping-spike / recv-wait), e.g.
    # in CI, so an intermittent failure leaves diagnostics in the test log.
    if os.environ.get("DC_VERBOSE"):
        server_cmd.append("--verbose")

    server_proc = subprocess.Popen(
        server_cmd,
        stdout=sys.stderr,
        stderr=sys.stderr
    )

    time.sleep(1)

    try:
        result = subprocess.run([test_binary], stdout=sys.stdout, stderr=sys.stderr)
        exit_code = result.returncode
    finally:
        cleanup(server_proc)

    sys.exit(exit_code)

if __name__ == "__main__":
    main()
