TARGET   = menu_view_ab
SRC_CC   = main.cc
LIBS     = base libc libm vfs libpng zlib blit file
PRG_MENU_DIR = $(call select_from_repositories,src/app/menu_view)
INC_DIR += $(PRG_MENU_DIR)

CC_OLEVEL := -O3

CUSTOM_TARGET_DEPS += menu_view_style.tar

BUILD_ARTIFACTS := $(TARGET) menu_view_style.tar

.PHONY: menu_view_style.tar

menu_view_style.tar:
	$(MSG_CONVERT)$@
	$(VERBOSE)tar $(TAR_OPT) -cf $@ -C $(PRG_MENU_DIR) style
	$(VERBOSE)ln -sf $(BUILD_BASE_DIR)/$(PRG_REL_DIR)/$@ $(INSTALL_DIR)/$@
