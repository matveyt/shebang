TARGET := $(notdir $(CURDIR))
NOCRT0 := $(wildcard nocrt0?.c)

CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Wpedantic -Werror
LDFLAGS += -s -fno-ident -municode
LDFLAGS += $(if $(NOCRT0),-nostartfiles)
LDLIBS += -lshlwapi
MAKEFLAGS += -r

$(TARGET) : $(TARGET).c $(NOCRT0)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@
clean :
	-rm -f $(TARGET)
.PHONY : clean
