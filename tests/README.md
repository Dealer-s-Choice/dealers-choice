
## Running tests

    meson test -v

## Changing the number of game passes

The default is 3 passes. To run with 1 pass:

    meson test -v --setup=One_pass

## Troubleshooting: port already in use

Each network test binds a dedicated TCP port (assigned sequentially from
`base_port` in `meson.build`) so tests can run in parallel. If a test server
process was left running after a crash or interrupt, its port will still be
occupied and the next test run will fail on that test. The other tests are
unaffected.

Kill the leftover process:

    pkill -f "dealers-choice.*--server"

Or look it up first:

    ss -tlnp | grep <port>

On Windows:

    netstat -ano | findstr <port>
    taskkill /PID <pid> /F
