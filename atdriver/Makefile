# We are called from the kernel (this makefile call the kernel's one which
# call's this one. So if KERNELRELEASE is defined we are at the second called to
# this makefile
ifneq ($(KERNELRELEASE),)
	obj-m := at.o
	at-y := atsms.o n_atsms.o

# Here we are at the first call
else
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
CLEAN_DEP = $(DEPS:%=clean-%)
MRPROPER_DEP = $(DEPS:%=mrproper-%)
DESTDIR_DEP = $(DEPS:%=destdir-%)

default: build-$(TARGET)

.PHONY: clean mrproper

clean: clean-$(TARGET)

mrproper: mrproper-$(TARGET)

include Rules.mk

endif
