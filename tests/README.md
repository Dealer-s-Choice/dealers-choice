
##
To change the number of passes to 1 (default is 3 when using `meson test`) for
the 5_card_{stud,showdown} tests:

### Using additional meson test setup

    meson test -v --setup=One_pass

### Manually

Run the server:

    ./dealers-choice ---test --server

In another terminal, from the build directory:

    tests/test_5_card_showdown 1

To verify a completely successful check the return code:

    echo $?
