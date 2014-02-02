ATTEST_DIR := $(CURDIR)

include $(ATTEST_DIR)/Files.mk

#Fix some variables
ATTEST_EXEC := $(EXEC)
ATTEST_SRC := $(SRC)
ATTEST_OBJ := $(OBJ)
ATTEST_DEP := $(DEPENDS)
ATTEST_DESTDIR := $(DST)
ATTEST_ABSDESTDIR := $(abspath $(ATTEST_DESTDIR))


build-$(TARGET): destdir-$(TARGET) $(ATTEST_DESTDIR)/$(ATTEST_EXEC) $(DEPS)


$(ATTEST_DESTDIR)/$(ATTEST_EXEC): $(ATTEST_OBJ)
	$(CC) -o $@ $< $(LDFLAGS)

$(ATTEST_DESTDIR)/%.o: $(ATTEST_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD

$(ATTEST_ABSDESTDIR):
	mkdir -p $(ATTEST_ABSDESTDIR)

destdir-$(TARGET): $(ATTEST_ABSDESTDIR)

.PHONY: clean-$(ATTEST_EXEC) mrproper-$(ATTEST_EXEC)

clean-$(TARGET):
	rm -f $(ATTEST_ABSDESTDIR)/*.o
	rm -f $(ATTEST_ABSDESTDIR)/*.d

mrproper-$(TARGET): clean
ifneq ($(ATTEST_ABSDESTDIR),$(realpath $(ATTEST_DIR)))
	rm -rf $(ATTEST_ABSDESTDIR)
else
	rm -f $(ATTEST_EXEC)
endif

-include $(RULESMK_DEP)
-include $(ATTEST_DEP)
