content: block.vdi block_ext4.vdi uuid.txt
# block_ext2.vdi

%.vdi:
	cp $(REP_DIR)/recipes/raw/vdi_empty/$@.bz2 $@.bz2
	bunzip2 $@.bz2

uuid.txt:
	cp $(REP_DIR)/recipes/raw/vdi_empty/$@ $@
