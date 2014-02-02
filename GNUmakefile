CONF ?= default

include Conf.$(CONF).mk
include Files.mk

.SUFFIXES:
	MAKEFLAGS += --no-builtin-rules --no-builtin-variables \
		     --warn-undefined-variables

CURFILE = $(dir $(lastword $(MAKEFILE_LIST)))
CURDIR = $(CURFILE:%/=%)

OBJ = $(DESTDIR)/$(CURDIR)/$(SRC:.c=.o)
DEPENDS = $(OBJ:%.o=.d)

DEPSDIR = $(dir $(DEPS))
DEPSEXEC = $(notdir $(DEPS))

DST = $(patsubst %/.,%, $(DESTDIR)/$(CURDIR))
TARGET = $(patsubst ./%,%, $(CURDIR)/$(EXEC))

RULESMK_DEP = $(DEPSDIR:%=%Rules.mk)
BUILD_DEP = $(DEPS:%=build-%)
CLEAN_DEP = $(DEPSEXEC:%=clean-%)
MRPROPER_DEP = $(DEPSEXEC:%=mrproper-%)
DESTDIR_DEP = $(DEPSEXEC:%=destdir-%)

all: build-$(TARGET)

.PHONY: clean mrproper

clean: clean-$(EXEC)

mrproper: mrproper-$(EXEC)

include Rules.mk
