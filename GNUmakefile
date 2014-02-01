CC := gcc
LD := gcc
MAKE := make

CFLAGS := -W -Wall
LDFLAGS :=

DEPS := simulAT/simulAT test/attest atdriver/at.ko
DEPSDIR := $(dir $(DEPS))
DEPSEXEC := $(notdir $(DEPS))

LDFLAGS := $(LDFLAGS)

DESTDIR := ./build

RULESMK_DEP := $(DEPSDIR:%=%Rules.mk)
BUILD_DEP := $(DEPS:%=$(DESTDIR)/%)
CLEAN_DEP := $(DEPSEXEC:%=clean-%)
MRPROPER_DEP := $(DEPSEXEC:%=mrproper-%)
DESTDIR_DEP := $(DEPSEXEC:%=destdir-%)


all: _all

include $(RULESMK_DEP)
include Rules.mk

_all: destdir $(BUILD_DEP)

destdir: destdir-$(EXEC) $(DESTDIR_DEP)

.PHONY: clean mrproper

clean: $(CLEAN_DEP) clean-$(EXEC)

mrproper: $(MRPROPER_DEP) mrproper-$(EXEC)
