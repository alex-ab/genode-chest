PORT_DIR_SEOUL := $(call port_dir,$(REP_DIR)/ports/seoul)

SRC_DIR = src/app/seoul

content: $(SRC_DIR)

$(SRC_DIR):
	mkdir -p $@
	cp -rH $(REP_DIR)/$@/* $@/
	cp -r $(PORT_DIR_SEOUL)/$@/* $@/
	cp $(PORT_DIR_SEOUL)/$@/LICENSE .

MIRROR_FROM_CHEST_DIR := lib/mk/seoul-qemu-usb.mk

content: $(MIRROR_FROM_CHEST_DIR)

$(MIRROR_FROM_CHEST_DIR):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/chest/$@ $(dir $@)

MIRROR_FROM_LIBPORTS := lib/import/import-qemu-usb_include.mk \
                        lib/mk/qemu-usb_include.mk \
                        lib/mk/qemu-usb.inc \
                        include/qemu \
                        src/lib/qemu-usb

content: $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)

QEMU_USB_PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/qemu-usb)

MIRROR_FROM_QEMU_USB_PORT_DIR := src/lib/qemu

content: $(MIRROR_FROM_QEMU_USB_PORT_DIR)

$(MIRROR_FROM_QEMU_USB_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(QEMU_USB_PORT_DIR)/$@ $(dir $@)

MIRROR_FROM_OS := include/pointer/shape_report.h

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)
