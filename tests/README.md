
## Running tests

    meson test -v

## Changing the number of game passes

The default is 3 passes for the `5_card_{stud,showdown}` tests. To run with
1 pass:

### Using the meson test setup

    meson test -v --setup=One_pass

### Manually

Run the server in one terminal:

    ./dealers-choice --test --server

Then, from the build directory in another terminal:

    tests/test_5_card_showdown 1

Check the return code to verify success:

    echo $?

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
