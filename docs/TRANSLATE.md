# 🌐 Translate

This project uses [GNU gettext](https://www.gnu.org/software/gettext/) for translations.

## Creating a New Translation

In the `po/` directory (from the repository root), run:

```
msginit -l ??
```

Replace `??` with your [ISO 639-1 language code](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes).

👉 **Example:** To create a French `.po` file:

```
msginit -l fr
```

In some cases, you may need to include your **country code** as well.
👉 **Example for Belgian Dutch:**

```
msginit -l nl_BE
```

This will create a new `.po` file in the current directory.

## Important: UTF-8 Charset

In the generated `.po` file, ensure the `Content-Type` line uses **UTF-8** encoding:

```
"Content-Type: text/plain; charset=UTF-8\n"
```

---

## Testing Your Translation

1. Make sure you have a server running or can connect to one.
2. In the build directory, run:

```
meson devenv
```

This sets the environment variable `DEALERSCHOICE_LOCALEDIR` to the directory where translations are located when **not installed**.

3. Run the client. You should see the translated text on the **connect screen**.

---

✅ If you run into any issues, feel free to ask for help.
