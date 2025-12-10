TARGET = seoul
REQUIRES = x86

SEOUL_CONTRIB_DIR = $(call select_from_ports,seoul)/src/app/seoul

LIBS   += base blit format
LIBS   += seoul-qemu-usb
SRC_BIN = mono.tff

GENODE_SRC_CC   += $(notdir $(wildcard $(SEOUL_CONTRIB_DIR)/genode/*.cc))
MODEL_SRC_CC    += $(notdir $(wildcard $(SEOUL_CONTRIB_DIR)/model/*.cc))
EXECUTOR_SRC_CC += $(notdir $(wildcard $(SEOUL_CONTRIB_DIR)/executor/*.cc))

ifneq ($(filter x86_64, $(SPECS)),)
CC_CXX_OPT += -mcmodel=large
CC_CXX_OPT += -march=x86-64-v2
#else
CC_CXX_OPT += -march=core2
endif

SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix genode/,$(GENODE_SRC_CC)))
SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix model/,$(MODEL_SRC_CC)))
SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix executor/,$(EXECUTOR_SRC_CC)))
SRC_CC += $(filter-out $(FILTER_OUT),base/lib/runtime/string.cc)

INC_DIR += $(SEOUL_CONTRIB_DIR)/include
INC_DIR += $(SEOUL_CONTRIB_DIR)/genode/include

CC_WARN += -Wno-unused

# 32bit builds need this ssse3 enforcement
CC_OPT_model/intel82576vf := -mssse3
CC_OPT_model/rtl8029      := -mssse3
CC_OPT_genode/network     := -mssse3

CC_OPT_PIC :=

# due to Qemu::Timer_queue, Qemu:Pci_device, Qemu::Controller
CC_OPT_genode/xhci := -Wno-non-virtual-dtor

CC_OPT_base/lib/runtime/string := -Dstrcmp=strcmp_renamed

vpath %.cc  $(SEOUL_CONTRIB_DIR)
vpath %.tff $(SEOUL_CONTRIB_DIR)/genode
