CC := gcc
LD := gcc
MAKE := make
EXEC := simulAT

CFLAGS := -g -W -Wall
LDFLAGS :=

LIBDIR_DEP :=
LIBRARY_DEPENDENCIES :=

LDFLAGS := $(LDFLAGS) $(LIBDIR_DEP:%=-L%)

DESTDIR := /tmp/simulAT

RULESMK_DEP := $(LIBDIR_DEP:%=%/Rules.mk)
BUILD_DEP := $(LIBRARY_DEPENDENCIES)
CLEAN_DEP := $(LIBRARY_DEPENDENCIES:%=clean-%)
MRPROPER_DEP := $(LIBRARY_DEPENDENCIES:%=mrproper-%)
DESTDIR_DEP := $(DEPSEXEC:%=destdir-%)

all: _all

include $(RULESMK_DEP)
include Rules.mk

_all: destdir $(BUILD_DEP) $(DESTDIR)/$(EXEC)

destdir: destdir-$(EXEC) $(DESTDIR_DEP)

.PHONY: clean mrproper

clean: clean-$(EXEC) $(CLEAN_DEP)

mrproper: mrproper-$(EXEC) $(MRPROPER_DEP)


