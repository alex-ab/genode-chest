#
# \brief  Using Seoul on Genode
# \author Norman Feske
# \author Markus Partheymueller
# \author Alexander Boettcher
# \date   2011-11-21

if {![info exists use_ahci]} { set use_ahci 0 }

# for uefi boot_fb works
set fb_name "boot_fb"
if {[have_include power_on/qemu] && ([have_include image/iso] || [have_include image/disk])} {
	set fb_name "vesa_fb"
}
if {[have_cmd_switch --autopilot]} {
	set fb_name "vesa_fb"
}

set use_drv_ahci  [expr  $use_ahci && ($use_block_vdi || $use_block_sata)]
set use_drv_nvme  [expr !$use_ahci && ($use_block_vdi || $use_block_sata)]

if {![info exists use_vfs_block]} {
	set use_vfs_block [expr $use_block_ram || $use_genode_iso] }

if {![info exists use_fs_rump]} { set use_fs_rump $use_block_vdi }

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/src/event_filter \
                  [depot_user]/src/init \
                  [depot_user]/src/nitpicker \
                  [depot_user]/src/pc_usb_host \
                  [depot_user]/src/usb_hid \
                  [depot_user]/src/vfs \
                  [depot_user]/src/vfs_import \
                  [depot_user]/src/ps2 \
                  [depot_user]/raw/drivers_interactive-pc \
                  [depot_user]/pkg/themed_wm \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/platform \
                  [depot_user]/src/acpi \
                  [depot_user]/src/pci_decode \
                  [depot_user]/src/vesa_fb \
                  [depot_user]/src/boot_fb

if {![info exists use_usb]}        { set use_usb        1 }
if {![info exists use_virtio]}     { set use_virtio     0 }
if {![info exists use_audio]}      { set use_audio      0 }
if {![info exists use_disk_count]} { set use_disk_count 1 }
if {![info exists cpuid_native]}   { set cpuid_native   no }
if {![info exists vdi_name]}       { set vdi_name       "guest.vdi" }
if {![info exists image]}          { set image          "seoul-disc.raw" }
if {![info exists use_model_xhci]} { set use_model_xhci 0 }

if {$use_fs_rump   } { import_from_depot [depot_user]/src/vfs_rump }
if {$use_part_block} { import_from_depot [depot_user]/src/part_block }
if {$use_drv_ahci  } { import_from_depot [depot_user]/src/ahci }
if {$use_drv_nvme  } { import_from_depot [depot_user]/src/nvme }
if {$use_audio     } { import_from_depot [depot_user]/src/bsd_audio }

if {[have_include power_on/qemu] && $use_block_vdi} {
	if {![file exists bin/seoul-disc-vdi.raw]} {
		if {![file exists bin/$vdi_name]} {
			puts "Please provide a disk image file to bin/$vdi_name"
			exit 1
		}

		set mke2fs [installed_command mke2fs]
		set dd     [installed_command dd]

		catch { exec $dd if=/dev/zero of=bin/seoul-disc-vdi.raw bs=1M count=12000 }
		catch { exec $mke2fs -F bin/seoul-disc-vdi.raw }

		exec [installed_command e2cp] bin/$vdi_name bin/seoul-disc-vdi.raw:$vdi_name
	}

	if {$use_drv_ahci} {
		append qemu_args " -drive id=disk,file=bin/seoul-disc-vdi.raw,format=raw,if=none"
		append qemu_args " -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0"
	}

	if {$use_drv_nvme} {
		append qemu_args " -device pcie-root-port,id=root_port1 "
		append qemu_args " -drive id=nvme0,file=bin/seoul-disc-vdi.raw,format=raw,if=none "
		append qemu_args " -device nvme,drive=nvme0,serial=fnord,id=nvme0n1,bus=root_port1 "
	}
}

if {[have_include power_on/qemu] && $use_block_sata} {
	if {$use_drv_ahci} {
		append qemu_args " -drive id=disk,file=bin/$image,format=raw,if=none"
		append qemu_args " -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0"

		if { $use_disk_count > 1 } {
			append qemu_args " -drive id=hdd,file=bin/$image2,format=raw,if=none"
			append qemu_args " -device ide-hd,drive=hdd,bus=ahci.1 "
		}
	}

	if {$use_drv_nvme} {
		append qemu_args " -device pcie-root-port,id=root_port1 "
		append qemu_args " -drive id=nvme0,file=bin/$image,format=raw,if=none "
		append qemu_args " -device nvme,drive=nvme0,serial=fnord,id=nvme0n1,bus=root_port1 "
	}

}

proc config_ahci { } {

	global use_drv_ahci
	global use_block_sata
	global use_part_block
	global use_block_vdi

	set config_ahci ""

	if {!$use_drv_ahci} {
		return $config_ahci
	}

	append config_ahci {
config
+ start ahci | priority: -2 | ram: 10M
  + provides
  | + service Block
  + config }

append_if [expr $use_block_sata && !$use_part_block] config_ahci {
    + policy | label: seoul -> disk0 | device: 0 | writeable: yes
    + policy | label: seoul -> disk1 | device: 1 | writeable: yes }
append_if [expr $use_block_vdi && !$use_part_block] config_ahci {
    + policy | label: rump_fs ->  | device: 0 | writeable: yes }
append_if [expr $use_block_vdi && $use_part_block] config_ahci {
    + policy | label_prefix: part_block ->  | device: 0 | writeable: yes }
append_if [expr $use_block_sata && $use_part_block] config_ahci {
    + policy | label_prefix: part_block ->  | device: 0 | writeable: yes }

	return $config_ahci
}

proc config_nvme { } {

	global use_drv_nvme
	global use_block_sata
	global use_part_block
	global use_block_vdi

	set config_nvme ""

	if {!$use_drv_nvme} {
		return $config_nvme
	}

	append config_nvme {
+ start nvme | caps: 120 | priority: -2 | ram: 24M
  + provides
  | + service Block
  + config | max_hmb_size: 16M | verbose_regs: yes | verbose_identify: yes }
append_if [expr $use_block_sata && !$use_part_block] config_nvme {
    + policy | label: seoul -> disk0 | writeable: yes }
append_if [expr $use_block_vdi && !$use_part_block] config_nvme {
    + policy | label: rump_fs -> | writeable: yes }
append_if [expr $use_block_vdi && $use_part_block] config_nvme {
    + policy | label_prefix: part_block -> | writeable: yes }
append_if [expr $use_block_sata && $use_part_block] config_nvme {
    + policy label_prefix: part_block -> | writeable: yes }

	return $config_nvme
}

proc config_part_block { } {

	global use_drv_nvme
	global use_drv_ahci
	global use_part_block
	global use_block_sata
	global use_block_vdi
	global use_fs_rump
	global partition_raw
	global partition_rump

	set config_part_block ""

	if {!$use_part_block} { return $config_part_block }
	if {!$use_block_sata} { return $config_part_block }

	if {!$use_fs_rump && ![info exists partition_raw]} {
		puts "\nusing part_block without partition_raw number will not work\n"
		exit 0
	}

	if {$use_fs_rump && ![info exists partition_rump]} {
		puts "\nusing part_block without partition_rump number will not work\n"
		exit 0
	}

	# set variable with dummies when not used
	if {!$use_fs_rump} { set partition_rump 10000 }
	if { $use_fs_rump} { set partition_raw  10000 }

	if {$use_drv_nvme} { set drv_name "nvme" }
	if {$use_drv_ahci} { set drv_name "ahci" }

	append config_part_block {
+ start part_block | priority: -2 | ram: 10M
  + provides
  | + service Block
  + route
  | + any-service
  |   + child } $drv_name {
  |   + parent
  + config }
	append_if [expr !$use_fs_rump] config_part_block {
    + policy
    |        label:     seoul -> disk0
    |        partition: } $partition_raw  {
    |        writeable: yes }
	append config_part_block {
    + policy
             label:     rump_fs ->
             partition: } $partition_rump {
             writeable: yes }

	return $config_part_block
}

#
# Build
#

assert {[have_spec x86]}

if {[have_spec sel4]} {
	# seL4 has no AMD SVM support
	assert {![have_include power_on/qemu]}
}


set build_components {
	driver/rtc
	app/seoul
	server/record_play_mixer
}

lappend_if $use_block_vdi       build_components server/vdi_block
lappend_if $use_vfs_block       build_components server/vfs_block
lappend_if $use_nic_session     build_components driver/nic
lappend_if $use_nic_session     build_components server/nic_router
lappend_if $use_genode_iso      build_components server/iso9660
lappend_if $use_top             build_components app/top

build $build_components

set vm_cfg ""
append vm_cfg {config | cpuid_native: } $cpuid_native { | vmm_memory: 64M | ld_verbose: yes
.
. The performance is bad for some kernels when vmm and 1. vcpu is not on same physical CPU
.
+ kernel foc  | vmm_vcpu_same_cpu: yes | cpuid_native: no
+ kernel sel4 | vmm_vcpu_same_cpu: yes | cpuid_native: no | map_small: yes
+ machine | verbose: no
  .
  . guest physical layout:
  .
  . low area < 0xa9000
  . [1M- 3G)
  . [4G-68G)
  .
  + mem | start: 0x0         | end: 0x9a000
  + mem | start: 0x100000    | end: 0xc0000000
  + mem | start: 0x100000000 | end: 0x1100000000
  + mio | start: 0xe0000     | size: 0x10000 | readonly: 1
  x ioio
  + nullio    | io_base:   0x80
  + pic       | io_base:   0x20   | elcr_base: 0x4d0
  + pic       | io_base:   0xa0   | irq: 2 | elcr_base: 0x4d1
  + pit       | io_base:   0x40   | irq: 0
  + scp       | io_port_a: 0x92
  |           | io_port_b: 0x61
  + kbc       | io_base:   0x60   | irq_kbd: 1 | irq_aux: 12
  + keyb      | ps2_port:  0      | host_keyboard: 0x10000
  + mouse     | ps2_port:  1      | host_mouse: 0x10001
  + rtc       | io_base:   0x70   | irq: 8
  + serial    | io_base:   0x3f8  | irq: 0x4 | host_serial: 0x4711
  + hostsink  | host_dev:  0x4712 | buffer: 80
  + vga       | io_base:   0x03c0
  + acpimodel
}

if {!$use_multiboot} {
append vm_cfg {
  + vbios_disk | boot_disknr: 0 | disk_count: } $use_disk_count { }
}

append vm_cfg {
  + vbios_keyboard | host_keyboard: 0x10000
  + vbios_mem
  + vbios_time
  + vbios_reset
}

if {$use_multiboot} {
	if {[info exists use_multiboot_modaddr]} {
append vm_cfg {
  + vbios_multiboot | modaddr: } $use_multiboot_modaddr { }
	} else {
append vm_cfg {
  + vbios_multiboot}
	}
}

append vm_cfg {
  + msi
  + ioapic
  + pcihostbridge | bus_num: 0 | bus_count: 0x10 | io_base: 0xcf8 | mem_base: 0xe0000000
  + pmtimer       | io_port: 0x8000
}

for {set i 0} {$i < $vcpus_to_be_used} {incr i 1} {
append vm_cfg {
  + vcpu
  + halifax
  + vbios
  + lapic
}
}

if {!$use_multiboot} {

	if {$use_model_ahci} {
		append vm_cfg {
  + ahci  | mem: 0xe0800000
  + drive | sigma0drive: 0 | controller: 0 | port: 0 }

		if {$use_disk_count > 1} {
			append vm_cfg {
  + drive | sigma0drive: 1 | controller: 0 | port: 1
			}
		}
	}
	if {$use_model_ide} {
		append vm_cfg {
  + ide | port0: 0x1f0 | port1: 0x3f6 | irq: 14 | bdf: 0x38 | disk: 0
		}

		if {$use_disk_count > 1} {
			puts "unsupported disk count for IDE model"
			exit 1
		}
	}
}

append_if $use_nic_session vm_cfg {
  x rtl8029 | irq: 9 | port: 0x300
  + intel82576vf | mem_mmio: 0xe0100000 | mem_msix: 0xe0120000
}

append_if $use_virtio vm_cfg {
  + virtio_input | mem: 0xe0200000
  + virtio_gpu   | mem: 0xe0400000
}

append_if $use_audio vm_cfg {
  + virtio_sound | mem: 0xe0600000
}

append_if $use_model_xhci vm_cfg {
  + xhci | mem: 0xe0700000
}

if {$use_multiboot} {
append vm_cfg {
+ multiboot}
append_if $use_multiboot vm_cfg $multiboot_files
}

append vm_cfg {
-}

# write Seoul config file
set    vm_cfg_fd [open "bin/vm_seoul.cfg" w]
puts  $vm_cfg_fd $vm_cfg
close $vm_cfg_fd

#
# Generate Genode config
#

set config {config | verbose: no | prio_levels: 4
+ parent-provides
  + service ROM
  + service IRQ
  + service IO_MEM
  + service IO_PORT
  + service PD
  + service RM
  + service CPU
  + service VM
  + service LOG
  + service TRACE
+ default-route
  + any-service
    + parent
    + any-child
+ default | caps: 100

+ start timer | ram: 2M
  + provides
    + service Timer

+ start platform_reports | priority: -2 | ram: 2M | caps: 200
  + binary report_rom
  + provides
  | + service Report
  | + service ROM
  + config
    + policy | label: pci_decode -> system | report: acpi -> acpi
    + policy | label: platform -> devices  | report: pci_decode -> devices
    + policy | label: usb_hid -> report    | report: usb -> devices

+ start acpi | caps: 350 | ram: 4M | priority: -1
  + route
    + service Report | + child platform_reports
    + any-service
      + parent
      + any-child

+ start pci_decode | ram: 2M | caps: 350 | priority: -1
  + route
    + service ROM | label: system | + child platform_reports
    + service Report              | + child platform_reports
    + any-service
      + parent
      + any-child

+ start platform | caps: 400 | ram: 6M | managing_system: yes | priority: -2
  + provides
  | + service Platform
  + config
  | + policy | label_prefix: ps2
  | | + device ps2
  | + policy | label_prefix: ahci
  | | + pci | class: AHCI
  | + policy | label_prefix: nvme | info: yes
  | | + pci | class: NVME
  | + policy | label_prefix: nic | info: yes
  | | + pci | class: ETHERNET
  | + policy | label_prefix: audio
  | | + pci | class: AUDIO
  | | + pci | class: HDAUDIO
  | + policy | label_prefix: fb | info: yes
  | | + pci | class: VGA
  | + policy | label_prefix: usb | info: yes
  |   + pci | class: USB
  + route
    + service ROM | label: devices
    | + child platform_reports
    + any-service
      + parent
      + any-child

+ start pc_rtc | priority: -2 | ram: 2M
  + provides
    + service Rtc

+ start ps2 | priority: -2 | ram: 3M
  + config
  + route
    + service Event
    | + child event_filter | label: ps2
    + any-service
      + parent
      + any-child

+ start event_filter | caps: 90 | priority: -2 | ram: 2M
  + provides
  | + service Event
  + route
    + service ROM | label: config
    | + parent | label: event_filter.config
    + service Event
    | + child nitpicker
    + any-service
      + parent
      + any-child}

append_if $use_usb config {
+ start usb | priority: -2 | caps: 200 | ram: 12M
  + binary pc_usb_host
  + provides
  | + service Usb
  + config
  | + report | devices: yes
  | + policy | label_prefix: usb_hid
  | | + device | class: 0x3
  | + policy | label_prefix: seoul
  |   + device
  + route
    + service Report
    | + child report_rom
    + any-service
      + parent
      + any-child

+ start usb_hid | priority: -2 | caps: 140 | ram: 11M
  + config | use_report: yes
  + route
    + service Report
    | + child report_rom
    + service Event
    | + child event_filter | label: usb
    + service ROM | label: report
    | + child report_rom
    + service Usb
    | + child usb
    + any-service
      + parent
      + any-child}

append_if $use_audio config {
+ start mixer | priority: -1 | ram: 2M
  + binary record_play_mixer
  + provides
  | + service Record
  | + service Play
  + config | jitter_ms: 10
  | + mix left
  | | + play | label_suffix: left
  | + mix right
  | | + play | label_suffix: right
  | + policy | label_suffix: left | record: left | volume: 1.0
  | + policy | label_suffix: right | record: right | volume: 1.0
  + route
    + service Report
    | + child report_rom
    + any-service
      + parent
      + any-child

+ start audio | priority: -1 | ram: 8M
  + binary pci_audio
  + provides
  | + service Audio_in
  | + service Audio_out
  + config | report_mixer: yes | record_play: yes
  | + mixer
  |         field: outputs.master
  |         value: 200,200
  + route
    + service Report
    | + child report_rom
    + any-service
      + parent
      + any-child}

append config { } [config_nvme] { } [config_ahci] { } [config_part_block] { }

append_if [expr $use_vfs_block && $use_block_ram] config {
+ start vfs_block | priority: -2 | caps: 400 | ram: 300M
  + provides
  | + service Block
  + config
    + vfs
    | + ram
    | + import
    |   + rom } $image {
    + default-policy | file: /} $image { | block_size: 512 | writeable: yes }

append_if [expr $use_vfs_block && $use_block_sata] config {
+ start vfs_block | priority: -2 | caps: 300 | ram: 300M
  + provides
  | + service Block
  + config
    + vfs
    | + fs
    + default-policy | file: /} $image { | block_size: 512 | writeable: yes }

append_if $use_fs_rump config {
+ start rump_fs | priority: -2 | caps: 500 | ram: 356M
  + binary vfs
  + provides | + service File_system
  + config | ld_verbose: yes
  | + vfs
  | | + rump | fs: ext2fs | ram: 50M
  | + default-policy | root: / | writeable: yes
  + route
    + service Timer | + child timer }
append_if [expr $use_fs_rump && $use_part_block] config {
    + service Block | + child part_block }
append_if [expr $use_fs_rump && !$use_part_block && $use_drv_ahci] config {
    + service Block | + child ahci }
append_if [expr $use_fs_rump && !$use_part_block && $use_drv_nvme] config {
    + service Block | + child nvme }
append_if $use_fs_rump config {
    + any-service   | + parent }

append_if $use_block_vdi config {
+ start vdi_block | priority: -2 | ram: 8M
  + provides
  | + service Block
  + config | file: /} $vdi_name { | writeable: yes
  | + vfs
  | | + fs | buffer_size: 1M
  | + policy | label: seoul -> disk0 | device: 0 | writeable: yes
  + route
    + service File_system
    | + child rump_fs
    + any-service
      + parent
      + any-child }

append_if $use_genode_iso config {
+ start vfs_block | priority: -2 | caps: 100 | ram: 16M
  + provides
  | + service Block
  + config
    + vfs
    | + rom genode.iso
    + default-policy | file: /genode.iso | block_size: 2048
+ start iso9660 | priority: -2 | ram: 16M
  + provides
  | + service ROM
  + route
    + service Block
    | + child vfs_block
    + any-service
      + parent }

append_if $use_nic_session config {
+ start nic | caps: 150 | priority: -3 | ram: 16M
  + binary pc_nic
  + route
    + service Uplink
    | + child nic_router
    + any-service
      + parent
      + any-child

+ start nic_router | caps: 200 | priority: -2 | ram: 10M
  + provides
  | + service Nic
  | + service Uplink
  + config | verbose_domain_state: yes
    + policy | label_prefix: seoul | domain: downlink
    + policy | label_prefix: nic | domain: uplink
    + domain uplink
    | + nat
    |       domain:    downlink
    |       tcp-ports: 16384
    |       udp-ports: 16384
    |       icmp-ids:  16384
    + domain downlink | interface: 10.0.3.1/24
      + dhcp-server
      |             ip_first:        10.0.3.55
      |             ip_last:         10.0.3.155
      |             dns_config_from: uplink
      + tcp | dst: 0.0.0.0/0
      | + permit-any | domain: uplink
      + udp | dst: 0.0.0.0/0
      | + permit-any | domain: uplink
      + icmp | dst: 0.0.0.0/0 | domain: uplink }

append config {
+ start fb | priority: -2 | caps: 200 | ram: 20M
  + binary } $fb_name {
  + route
  | + service Report
  | | + child report_rom
  | + service Timer
  | | + child timer
  | + service Capture
  | | + child nitpicker
  | + service Platform
  | | + any-child
  | + any-service
  |   + parent }
append_if [expr ![have_include "power_on/qemu"]] config {
  + config | width: 1280 | height: 960 }
append config {
  + config }

append_if $use_top config {
+ start top | ram: 2M
  + config | period_ms: 15000
  + route
    + service TRACE
    | + parent | label:
    + any-service
      + parent
      + any-child }

append config {
+ start seoul | priority: -3 | caps: 2000 | ram: } $memory_vmm_vm {
  + binary seoul
  + route
    + service Timer | + child timer
    + service ROM | label: config | + parent | label: vm_seoul.cfg }
append_if $use_genode_iso config {
    + service ROM | unscoped_label: seoul     | + parent
    + service ROM | unscoped_label: ld.lib.so | + parent
    + service ROM | label: platform_info      | + parent
    + service ROM                             | + child iso9660 }
append_if [expr  $use_block_vdi && !$use_vfs_block] config {
    + service Block | + child vdi_block }
append_if [expr !$use_block_vdi &&  $use_vfs_block] config {
    + service Block | + child vfs_block }
append_if [expr !$use_block_vdi && !$use_vfs_block && $use_block_sata && $use_part_block] config {
    + service Block | + child part_block }
append config {
    + service Gui                   | + child wm
    + service Rtc                   | + child pc_rtc
    + service Usb                   | + child usb
    + service Report | label: shape | + child wm
    + service Report                | + child report_rom
    + any-service
      + parent
      + any-child

+ start nitpicker | priority: -2 | caps: 200 | ram: 12M
  + provides
  | + service Gui
  | + service Capture
  | + service Event
  + config
  | + capture
  | + event
  | + report | focus: yes | hover: yes
  | + domain pointer | layer: 1 | content: client | label: no | origin:  pointer
  | + domain panel   | layer: 2 | content: client | label: no | focus:   none
  | + domain default | layer: 3 | content: client | label: no | focus:   click | hover:   always
  | + policy         | label_prefix: pointer | domain:       pointer
  | + default-policy | domain: default
  + route
    + service Timer  | + child timer
    + service Report | + child report_rom
    + any-service
      + parent
      + any-child

+ start pointer | priority: -2 | ram: 2M
  + provides
  | + service Report
  + config | shapes: yes | verbose: no
  + route
    + service Gui
    | + child nitpicker
    + service ROM | label: hover
    | + child report_rom
    + service ROM | label: xray
    | + child report_rom
    + any-service
      + parent

+ start report_rom | priority: -2 | ram: 2M
  + provides
  | + service Report
  | + service ROM
  + config
    + policy | label: pointer -> hover | report: nitpicker -> hover
    + policy | label: pointer -> xray | report: nitpicker -> xray
    + policy | label: usb_hid -> report | report: usb -> devices

+ start wm | caps: 1200 | priority: -2 | ram: 34M
  + binary init
  + provides
  | + service Gui
  | + service Report
  | + service ROM
  + route
    + service ROM | label: config
    | + parent | label: wm.config
    + service Report | label_last: shape
    | + child pointer
    + service Gui
    | + child nitpicker
    + any-service
      + parent
      + any-child
- }

install_config $config

#
# Sanity checks for known to be usable Qemu version
#
proc check_qemu_version {qemu_version qemu_min qemu_max} {
	set version_min_list [split $qemu_min ".-"]
	set version_min_list_len [llength $version_min_list]

	set version_max_list [split $qemu_max ".-"]
	set version_max_list_len [llength $version_max_list]

	set version_list [split $qemu_version ".-"]
	set version_list_len [llength $version_list]

	set cmp 0
	set cmp_min 0
	set cmp_max 0
	set i 0

	foreach number $version_list {
		set min 0
		set max 0
		if { $i < $version_min_list_len } { set min [lindex $version_min_list $i] }
		if { $i < $version_max_list_len } { set max [lindex $version_max_list $i] }

		set cmp     [expr {$cmp  + $number * pow(1000, $version_list_len - $i) }]
		set cmp_min [expr {$cmp_min + $min * pow(1000, $version_list_len - $i) }]
		set cmp_max [expr {$cmp_max + $max * pow(1000, $version_list_len - $i) }]

		incr i
	}

	return [expr {($cmp_min < $cmp) && ($cmp < $cmp_max)}]
}

#
# Boot modules
#

set boot_modules { vm_seoul.cfg }
lappend_if $use_genode_iso boot_modules genode.iso
lappend_if $use_block_ram  boot_modules $image

#
# Add OS binaries of guest
#

if {$use_multiboot} {
	set guest_os_binary_missing 0
	set binary_counter 0
	foreach binary $guest_os_binaries {
		if {![file exists bin/$binary]} {
			puts stderr "Error: guest OS binary \"bin/$binary\" does not exist"
			set guest_os_binary_missing 1
		}

		if {[info exists sha1_os_binaries]} {
			set sha1 [exec sha1sum bin/$binary]
			set sha1 [regexp -inline {[0-9a-h]+} $sha1]
			if {[string compare $sha1 [lindex $sha1_os_binaries $binary_counter]]} {
				puts "SHA1 sum of binary does not match with expected one - abort"
				puts "$binary $sha1 != [lindex $sha1_os_binaries $binary_counter]"
				set guest_os_binary_missing 1
			}
		}
		incr binary_counter 1
	}

	if {$guest_os_binary_missing} { exit 1 }

	append boot_modules $guest_os_binaries
}

lappend boot_modules {*}[build_artifacts]
build_boot_image $boot_modules

# A copy of the config is placed in the run folder.
#exec rm -f bin/vm_seoul.cfg

if {$use_usb} {
	append qemu_args " -device nec-usb-xhci,id=xhci"
	append qemu_args " -device usb-kbd,bus=xhci.0"
	append qemu_args " -device usb-mouse,bus=xhci.0"
}

append qemu_args " -accel kvm -cpu host"

if { [have_include power_on/qemu] } {
	if {[regexp -- {-accel kvm -cpu host} $qemu_args dummy]} {
	} else {
		# Qemu TCG SVM is broken after $qemu_good_old and until before $qemu_good_new

		catch {exec qemu-system-x86_64 --version} qemu_version
		set qemu_version [regexp -inline {version[ ][0-9]+\.[0-9]+[\.0-9]*} $qemu_version]
		set qemu_version [regexp -inline {[0-9]+\.[0-9]+[\.0-9]*} $qemu_version]

		set qemu_good_old "6.0.1"
		set qemu_good_new "100.0.0-unknown"

		# Beginning with 6.1.0 the initial NPT fault is missing and actually
		# no guest memory is mapped by our test VMM. Nevertheless something
		# is executed by Qemu ???
		if {[check_qemu_version $qemu_version $qemu_good_old $qemu_good_new]} {
			puts "\nYour Qemu version '$qemu_version' is not working with AMD SVM virtualisation"
			puts "Known good Qemu versions are until $qemu_good_old and starting with $qemu_good_new\n"
			exit 1
		}

		# only AMD CPUs have Qemu TCG virtualization support
		append qemu_args " -cpu EPYC"
	}
}
