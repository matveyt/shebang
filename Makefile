CFLAGS := -O -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s -fno-ident -municode
.DEFAULT_GOAL := $(notdir $(CURDIR))

LDFLAGS += -nostartfiles
LDLIBS += -lshlwapi
