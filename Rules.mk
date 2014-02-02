_DIR := $(CURDIR)

include $(_DIR)/Files.mk

# Fix important variables
_OBJ := $(OBJ)
_DEPS := $(DEPS)
_RULESMK_DEP := $(RULESMK_DEP)
_CLEAN_DEP := $(CLEAN_DEP)
_MRPROPER_DEP := $(MRPROPER_DEP)

build-$(TARGET): destdir-$(EXEC) $(BUILD_DEP)

$(DESTDIR):
	mkdir -p $(DESTDIR)

destdir-$(TARGET): $(DESTDIR)

.PHONY: clean-$(TARGET) mrproper-$(TARGET)

clean-$(TARGET): $(_CLEAN_DEP)

mrproper-$(TARGET): clean $(MRPROPER_DEP)
ifneq ($(abspath $(DESTDIR)),$(realpath $(_DIR)))
	rm -rf $(DESTDIR)
endif

include $(_RULESMK_DEP)
