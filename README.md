[![Linux](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/linux.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/linux.yml)
[![MacOS](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/macos.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/macos.yml)
[![Windows](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/windows.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/windows.yml)

# aapokerhands
Will analyze for poker hands

## Dependencies

* [meson](http://mesonbuild.com/Quick-guide.html) (for the build system)

## Building

    meson setup builddir
    cd builddir
    ninja

## Usage
    ./aapokerhands: -n [hands] -s -v

# Downloads
[Releases](https://github.com/Dealer-s-Choiceaapokerhands/releases)

## Example Output

```
$ ./aapokerhands -n 100000
Out of 100000 hands:

                Pair:     42155
            Two Pair:      4717
     Three-of-a-Kind:      2144
            Straight:       411
               Flush:       188
          Full House:       150
      Four-of-a-Kind:        24
      Straight Flush:         0
         Royal Flush:         0


$ ./aapokerhands -n 1000000
Out of 1000000 hands:

                Pair:    421917
            Two Pair:     47511
     Three-of-a-Kind:     21150
            Straight:      3907
               Flush:      2020
          Full House:      1481
      Four-of-a-Kind:       224
      Straight Flush:        13
         Royal Flush:         5
```
