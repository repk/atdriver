ATDRIVER_DIR := $(CURDIR)

KERNDIR ?= /lib/modules/$(shell uname -r)/build

include $(ATDRIVER_DIR)/Files.mk

ATDRIVER_EXEC := $(EXEC)
ATDRIVER_SRC := $(SRC)
KBUILD := $(DST)
ABSKBUILD := $(abspath $(KBUILD))

build-$(TARGET): destdir-$(TARGET) $(KBUILD)/$(ATDRIVER_EXEC) $(BUILD_DEP)

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

destdir-$(TARGET): $(ABSKBUILD)

.PHONY: clean-$(ATDRIVER_EXEC) mrproper-$(ATDRIVER_EXEC)

clean-$(TARGET):
ifneq ($(ABSKBUILD),$(realpath $(ATDRIVER_DIR)))
	rm -rf $(ABSKBUILD)/*
else
	$(MAKE) -C $(KERNDIR) M=$(realpath $(KBUILD)) clean
endif

mrproper-$(TARGET): clean-$(TARGET)
ifneq ($(ABSKBUILD),$(realpath $(ATDRIVER_DIR)))
	rm -rf $(ABSKBUILD)
endif

-include $(RULESMK_DEP)
