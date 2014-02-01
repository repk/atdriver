_FILE := $(dir $(lastword $(MAKEFILE_LIST)))
_DIR := $(_FILE:%/=%)

DESTDIR ?= .

include $(_DIR)/Conf.mk

OBJ := $(DESTDIR)/$(_DIR)/$(SRC:.c=.o)
DEP := $(DESTDIR)/$(_DIR)/$(SRC:%.c=%.d)

$(_DIR)/$(EXEC): $(_DIR)/$(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

$(_DIR)/%.o: $(_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD


$(DESTDIR):
	mkdir -p $(DESTDIR)	

destdir-$(EXEC): $(DESTDIR)

.PHONY: clean-$(EXEC) mrproper-$(EXEC)

clean-$(EXEC):

mrproper-$(EXEC): clean
ifneq ($(abspath $(DESTDIR)),$(realpath $(_DIR)))
	rm -rf $(DESTDIR)
endif
