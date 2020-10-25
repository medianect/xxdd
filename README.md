# xxdd

Simple tool to dump files as C "words" tables or Golang raw strings.

## Purpose

Typical use case is to embed files (resources, assets... names vary) into a
static binary.

An example is supplied in the `Usage` section.

# Usage

`xxdd -h` will give a more comprehensive list of options, TLDR:

```
$ xxdd -i BINARY_FILE >> SOURCE_FILE
```

More convoluted example:

```
$ for p in resources/*; do
        n="$(basename "$p")"
        e="${n#*.}"
        n="${n%.*}"
        (
                printf "/* Copyright (C) You, All rights reserved. */\n"
                printf "\n"
                printf "#include \"config.h\"\n"
                printf "#include <stdint.h>\n"
                printf "\n"
                printf "const uint8_t %s_%s[] = {\n" "$n" "$e"
                xxdd -i "$p"
                printf "};\n"
        ) > "src/${n}_${e}.c"
        unset n e
done
unset p
```

# Build

Use the Makefile (`make`) or Bazel yourself or even `cc`. The build
configuration provided within this repository requires Bazel.
