/*
 * \brief  NOVA-specific convenience functions
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

/* NOVAe includes */
#include <novae/syscalls.h>

/* core includes */
#include <types.h>
#include <util.h>


/**
 * Return boot CPU number. It is required if threads in core should be placed
 * on the same CPU as the main thread.
 */
inline Genode::addr_t boot_cpu()
{
	/**
	 * Initial value of ax and di register, saved by the crt0 startup code
	 * and SOLELY VALID in 'core' !!!
	 *
	 * For x86_32 - __initial_ax contains the number of the boot CPU.
	 * For x86_64 - __initial_di contains the number of the boot CPU.
	 */
	extern Genode::addr_t __initial_ax;
	extern Genode::addr_t __initial_di;

	return (sizeof(void *) > 4) ? __initial_di : __initial_ax;
}

/**
 * Establish a mapping
 *
 * \param utcb       UTCB of the calling EC
 * \param src_crd    capability range descriptor of source
 *                   resource to map locally
 * \param dst_crd    capability range descriptor of mapping
 *                   target
 * \param kern_pd    Whether to map the items from the kernel or from core
 * \param dma_mem    Whether the memory is usable for DMA or not
 */
static int map_local(Genode::addr_t const pd, Novae::Utcb &utcb,
                     Novae::Crd const src_crd, Novae::Crd const dst_crd,
                     bool const kern_pd = false, bool const dma_mem = false,
                     bool const write_combined = false)
{
	/* asynchronously map capabilities */
	utcb.set_msg_word(0);

	/* ignore return value as one item always fits into the utcb */
	bool const ok = utcb.append_item(src_crd, 0, kern_pd, false, false,
	                                 dma_mem, write_combined);
	(void)ok;

	Novae::uint8_t res = Novae::delegate(pd, pd, dst_crd);
	if (res != Novae::NOVA_OK) {

		using Hex = Genode::Hex;
		error("map_local failed ",
		      Hex(src_crd.addr()), ":", Hex(src_crd.order()), ":", Hex(src_crd.type()), "->",
		      Hex(dst_crd.addr()), ":", Hex(dst_crd.order()), ":", Hex(dst_crd.type()), " - ",
		      "result=", Hex(res), " "
		      "msg=",    Hex(utcb.msg_items()), ":",
		                 Hex(utcb.msg_words()), ":",
		                 Hex(utcb.msg()[0]), " !!! "
		      "utcb=",   &utcb, " "
		      "kern=",   kern_pd);
		return res > 0 ? res : -1;
	}
	/* clear receive window */
	utcb.crd_rcv = 0;

	return 0;
}


inline Novae::uint8_t async_map(Genode::addr_t const source_pd,
                                Genode::addr_t const target_pd,
                                Novae::Crd const source,
                                Novae::Crd const target)
{
	return Novae::pd_ctrl(source_pd, target_pd,
	                      source.addr() | source.order(),
	                      target.addr() | target.rights(),
	                      0);
}


static inline int unmap_local(Novae::Crd crd, bool self = true) {
	return Novae::revoke(crd, self); }

inline int map_local_phys_to_virt(Novae::Utcb &utcb, Novae::Crd const src,
                                  Novae::Crd const dst, Genode::addr_t const pd)
{
	return map_local(pd, utcb, src, dst, true);
}


/**
 * Find least significant set bit in value
 */
inline unsigned char
lsb_bit(unsigned long const &value, unsigned char const shift = 0)
{
	unsigned long const scan  = value >> shift;
	if (scan == 0) return 0;

	unsigned char pos = (unsigned char)__builtin_ctzl(scan);
	unsigned char res = shift ? pos + shift : pos;
	return res;
}

/**
 * Remap pages in the local address space
 *
 * \param utcb        UTCB of the main thread
 * \param from_start  physical source address
 * \param to_start    local virtual destination address
 * \param num_pages   number of pages to map
 */
inline int map_local(Genode::addr_t const pd, Novae::Utcb &utcb,
                     Genode::addr_t from_start, Genode::addr_t to_start,
                     Genode::size_t num_pages,
                     Novae::Rights const &permission,
                     bool kern_pd = false, bool dma_mem = false,
                     bool write_combined = false)
{
	using namespace Novae;
	using namespace Genode;

	size_t const size = num_pages << get_page_size_log2();

	addr_t const from_end = from_start + size;
	addr_t const to_end   = to_start   + size;

	for (addr_t offset = 0; offset < size; ) {

		addr_t const from_curr = from_start + offset;
		addr_t const to_curr   = to_start   + offset;

		/*
		 * The common alignment corresponds to the number of least significant
		 * zero bits in both addresses.
		 */
		addr_t const common_bits = from_curr | to_curr;

		/* find least set bit in common bits */
		size_t order = lsb_bit(common_bits, get_page_size_log2());

		/* look if flexpage fits into both 'from' and 'to' address range */
		if ((from_end - from_curr) < (1UL << order))
			order = log2(from_end - from_curr);

		if ((to_end - to_curr) < (1UL << order))
			order = log2(to_end - to_curr);

		if (order >= sizeof(void *)*8)
			return 1;

		int const res = map_local(pd, utcb,
		                          Mem_crd((from_curr >> 12), order - get_page_size_log2(), permission),
		                          Mem_crd((to_curr   >> 12), order - get_page_size_log2(), permission),
		                          kern_pd, dma_mem, write_combined);
		if (res) return res;

		/* advance offset by current flexpage size */
		offset += (1UL << order);
	}
	return 0;
}


/**
 * Unmap pages from the local address space
 *
 * \param utcb        UTCB of the main thread
 * \param start       local virtual address
 * \param num_pages   number of pages to unmap
 * \param self        unmap from this pd or solely from other pds
 * \param self        map from this pd or solely from other pds
 * \param rights      rights to be revoked, default: all rwx
 */
inline void unmap_local(Novae::Utcb &, Genode::addr_t start,
                        Genode::size_t num_pages,
                        bool const self = true,
                        Novae::Rights const rwx = Novae::Rights(true, true, true))
{
	using namespace Novae;
	using namespace Genode;

	Genode::addr_t base = start >> get_page_size_log2();

	if (start & (get_page_size() - 1)) {
		error("unmap failed - unaligned address specified");
		return;
	}

	while (num_pages) {
		unsigned char const base_bit  = lsb_bit(base);
		unsigned char const order_bit = (unsigned char)min(log2(num_pages), 31U);
		unsigned char const order     = min(order_bit, base_bit);

		Mem_crd const crd(base, order, rwx);

		unmap_local(crd, self);

		num_pages -= 1UL << order;
		base      += 1UL << order;
	}
}


inline Novae::uint8_t syscall_retry(Core::Pager_object &, auto const &fn)
{
	Novae::uint8_t res;

	res = fn();

	return res;
}


inline Novae::uint8_t map_vcpu_portals(Core::Pager_object &pager,
                                      Genode::addr_t const source_exc_base,
                                      Genode::addr_t const target_exc_base,
                                      Novae::Utcb &,
                                      Genode::addr_t const source_pd)
{
	using Novae::Obj_crd;
	using Novae::NUM_INITIAL_VCPU_PT_LOG2;

	Obj_crd const source_initial_caps(source_exc_base, NUM_INITIAL_VCPU_PT_LOG2);
	Obj_crd const target_initial_caps(target_exc_base, NUM_INITIAL_VCPU_PT_LOG2);

	return async_map(source_pd, pager.pd_sel(),
	                 source_initial_caps, target_initial_caps);
}

inline Novae::uint8_t map_pagefault_portal(Core::Pager_object &,
                                           Genode::addr_t const source_exc_base,
                                           Genode::addr_t const target_exc_base,
                                           Genode::addr_t const target_pd,
                                           Novae::Utcb &)
{
	using Novae::Obj_crd;
	using Novae::PT_SEL_PAGE_FAULT;

	Genode::addr_t const source_pd = Core::platform_specific().core_pd_sel();

	Obj_crd const source_initial_caps(source_exc_base + PT_SEL_PAGE_FAULT, 0);
	Obj_crd const target_initial_caps(target_exc_base + PT_SEL_PAGE_FAULT, 0);

	return async_map(source_pd, target_pd,
	                 source_initial_caps, target_initial_caps);
}

inline Novae::Hip const &kernel_hip()
{
	/**
	 * Initial value of esp register, saved by the crt0 startup code.
	 * This value contains the address of the hypervisor information page.
	 */
	extern Genode::addr_t __initial_sp;
	return *reinterpret_cast<Novae::Hip const *>(__initial_sp);
}
