ATTEST_FILE := $(dir $(lastword $(MAKEFILE_LIST)))
ATTEST_DIR := $(ATTEST_FILE:%/=%)

include $(ATTEST_DIR)/Conf.mk

ATTEST_EXEC := $(EXEC)
ATTEST_SRC := $(SRC)
ATTEST_OBJ := $(ATTEST_SRC:%.c=%.o)
ATTEST_DEP := $(ATTEST_SRC:%.c=%.d)
ifneq ($(ATTEST_DIR),.)
ATTEST_DESTDIR := $(DESTDIR)/$(ATTEST_DIR)
else
ATTEST_DESTDIR := $(DESTDIR)
endif
ATTEST_ABSDESTDIR := $(abspath $(ATTEST_DESTDIR))


$(ATTEST_DESTDIR)/$(ATTEST_EXEC): $(ATTEST_DESTDIR)/$(ATTEST_OBJ)
	$(CC) -o $@ $< $(LDFLAGS)

$(ATTEST_DESTDIR)/%.o: $(ATTEST_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD

$(ATTEST_ABSDESTDIR):
	mkdir -p $(ATTEST_ABSDESTDIR)

destdir-$(EXEC): $(ATTEST_ABSDESTDIR)

.PHONY: clean-$(ATTEST_EXEC) mrproper-$(ATTEST_EXEC)

clean-$(ATTEST_EXEC):
	rm -f $(ATTEST_ABSDESTDIR)/*.o
	rm -f $(ATTEST_ABSDESTDIR)/*.d

mrproper-$(ATTEST_EXEC): clean
ifneq ($(ATTEST_ABSDESTDIR),$(realpath $(ATTEST_DIR)))
	rm -rf $(ATTEST_ABSDESTDIR)
else
	rm -f $(ATTEST_EXEC)
endif
