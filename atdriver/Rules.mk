ATDRIVER_FILE := $(dir $(lastword $(MAKEFILE_LIST)))
ATDRIVER_DIR := $(ATDRIVER_FILE:%/=%)
KERNDIR ?= /lib/modules/$(shell uname -r)/build

include $(ATDRIVER_DIR)/Conf.mk

ATDRIVER_EXEC := $(EXEC)
ATDRIVER_SRC := $(SRC)
ifneq ($(ATDRIVER_DIR),.)
KBUILD := $(DESTDIR)/$(ATDRIVER_DIR)
else
KBUILD := $(DESTDIR)
endif
ABSKBUILD := $(abspath $(DESTDIR)/$(ATDRIVER_DIR))


$(KBUILD)/$(ATDRIVER_EXEC): $(ATDRIVER_SRC:%=$(ATDRIVER_DIR)/%)
ifneq ($(ABSKBUILD),$(realpath $(ATDRIVER_DIR)))
	cp $^ $(KBUILD)/
endif
	$(MAKE) -C $(KERNDIR) M=$(realpath $(KBUILD)) modules
ifneq ($(ABSKBUILD),$(realpath $(ATDRIVER_DIR)))
	rm $(ATDRIVER_SRC:%=$(KBUILD)/%)
endif

$(ABSKBUILD):
	mkdir -p $(ABSKBUILD)

destdir-$(EXEC): $(ABSKBUILD)

.PHONY: clean-$(ATDRIVER_EXEC) mrproper-$(ATDRIVER_EXEC)

clean-$(ATDRIVER_EXEC):
ifneq ($(ABSKBUILD),$(realpath $(ATDRIVER_DIR)))
	rm -rf $(ABSKBUILD)/*
else
	$(MAKE) -C $(KERNDIR) M=$(realpath $(KBUILD)) clean
endif

mrproper-$(ATDRIVER_EXEC): clean-$(ATDRIVER_EXEC)
ifneq ($(ABSKBUILD),$(realpath $(ATDRIVER_DIR)))
	rm -rf $(ABSKBUILD)
endif
