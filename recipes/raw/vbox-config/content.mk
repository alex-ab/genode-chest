CONFIG_FILES = machine.vbox6

content: $(CONFIG_FILES)

$(CONFIG_FILES):
	cp $(REP_DIR)/recipes/raw/vbox-config/$@ $@
