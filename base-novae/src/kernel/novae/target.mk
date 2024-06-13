include $(REP_DIR)/etc/board.conf
TARGET = novae-$(BOARD)
LIBS   = kernel-novae-$(BOARD)

$(INSTALL_DIR)/$(TARGET):
	cp $(INSTALL_DIR)/../kernel/novae/x86_64-nova $@
