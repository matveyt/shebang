BUILDTYPE ?= minsize
NOSTDLIB ?= on
UNICODE ?= on
include yama.mk

LDLIBS += -lshlwapi

define NOSTDLIB.on
    CPPFLAGS += -DNOSTDLIB -DARGV=none
    LDFLAGS += -nostdlib
    LDLIBS += -lkernel32
    $$(call yama.goalExe,shebang,nocrt0c)
endef
define UNICODE.on
    CPPFLAGS += -D_UNICODE -DUNICODE
    LDFLAGS += -municode
endef
$(call yama.options,NOSTDLIB UNICODE)

all : shebang
$(call yama.goalExe,shebang,shebang)
$(call yama.rules)
