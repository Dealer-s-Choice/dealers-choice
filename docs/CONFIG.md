# Config

When you run the client from a terminal, you'll see the path to your config
file. You can edit settings from the Settings menu on the startup screen, or
by editing the file directly.

## Sound

```
sound.volume = 7 # You can use a range of 0 - 10
sound.notify.turn = yes # Use "no" to disable your turn audio notification
```

## Language

To use a language that is different from your system's default:

```
language = de
```

The value must match the name of a `.po` translation file, without the `.po`
ending.

You can see the available languages in the [po
directory](https://github.com/Dealer-s-Choice/dealers_choice/tree/trunk/po).
