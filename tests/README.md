
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

## Port assignments

Each network test binds a dedicated TCP port so tests can run in parallel.
Ports are assigned sequentially from `base_port` (22778) in `meson.build`:

| Test              | Port  |
|-------------------|-------|
| sodium_compat     | 22778 |
| fold              | 22779 |
| check             | 22780 |
| 5_card_showdown   | 22781 |
| 5_card_draw       | 22782 |
| 5_card_stud       | 22783 |
| deuces_wild       | 22784 |
| kick_ban          | 22785 |

## Troubleshooting: port already in use

If a test server process was left running after a crash or interrupt, its port
will still be occupied and the next test run will fail immediately on that
test. The other tests are unaffected.

To find and kill the leftover process:

    ss -tlnp | grep 2278
    # or
    lsof -i :22779

Then kill the PID shown, or kill all leftover server processes at once:

    pkill -f "dealers-choice.*--server"

On Windows, use:

    netstat -ano | findstr 2278
    taskkill /PID <pid> /F
