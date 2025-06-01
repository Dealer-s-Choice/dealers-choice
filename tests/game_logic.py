#!/usr/bin/env python3
import os
import sys
import subprocess
import time
import signal

# Ensure a test name was given
if len(sys.argv) != 2:
    print("Usage: {} <test_name>".format(sys.argv[0]), file=sys.stderr)
    sys.exit(1)

test_name = sys.argv[1]

# Get environment variables
meson_build_root = os.environ.get("MESON_BUILD_ROOT")
meson_build_test_root = os.environ.get("MESON_BUILD_TEST_ROOT")

if not meson_build_root or not meson_build_test_root:
    print("MESON_BUILD_ROOT and MESON_BUILD_TEST_ROOT must be set in the environment", file=sys.stderr)
    sys.exit(1)

# Launch the server
server_path = os.path.join(meson_build_root, "dealerschoice")
server_proc = subprocess.Popen([server_path, "--server", "---test"])

def cleanup():
    print("Cleaning up server...")
    try:
        server_proc.terminate()
        server_proc.wait(timeout=5)
    except Exception:
        try:
            server_proc.kill()
        except Exception:
            pass

# Ensure cleanup happens no matter what
import atexit
atexit.register(cleanup)
signal.signal(signal.SIGINT, lambda sig, frame: sys.exit(1))
signal.signal(signal.SIGTERM, lambda sig, frame: sys.exit(1))

# Wait for the server to initialize
time.sleep(5)

# Run the test
test_path = os.path.join(meson_build_test_root, test_name)
result = subprocess.run([test_path])

# Exit with the test's status code
sys.exit(result.returncode)
