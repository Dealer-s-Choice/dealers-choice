#!/usr/bin/env python3

import socket
import time
import subprocess
import sys
import os
import signal

def wait_for_server(host, port, timeout=10):
    """Wait until a TCP server is listening on (host, port)."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        try:
            with socket.create_connection((host, port), timeout=1):
                return True
        except (ConnectionRefusedError, OSError):
            time.sleep(0.2)
    return False

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

    test_binary = os.path.join(os.environ["MESON_BUILD_TEST_ROOT"], sys.argv[1])
    server_binary = os.path.join(os.environ["MESON_BUILD_ROOT"], "dealers-choice")

    # Launch the server in test mode
    server_proc = subprocess.Popen(
        [server_binary, "--server", "---test"],
        stdout=sys.stderr,  # redirect stdout to stderr
        stderr=sys.stderr
    )

    try:
        # Wait until the server is ready on port 22777
        if not wait_for_server("127.0.0.1", 22777):
            print("Server did not become ready in time", file=sys.stderr)
            cleanup(server_proc)
            sys.exit(1)

        # Run the test client
        result = subprocess.run([test_binary], stdout=sys.stdout, stderr=sys.stderr)
        exit_code = result.returncode
    finally:
        cleanup(server_proc)

    sys.exit(exit_code)

if __name__ == "__main__":
    main()
