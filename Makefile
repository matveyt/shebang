.DEFAULT_GOAL := $(notdir $(CURDIR))
CFLAGS := -O -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s -fno-ident -municode
ifneq (,$(wildcard nocrt0?.c))
LDFLAGS += -nostartfiles
endif
LDLIBS += -lshlwapi
