NAME    = xxdd
PACKAGE = src
TARGET  = bazel-bin/${PACKAGE}/${NAME}

.PHONY: all ${TARGET}

all: ${TARGET}

${TARGET}:
	bazel build //${PACKAGE}:${NAME}

install: ${TARGET}
	install ${TARGET} ~/bin
