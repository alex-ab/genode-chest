/*
 * \brief  Export RAM dataspace as shared memory object
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* core includes */
#include <ram_dataspace_factory.h>
#include <platform.h>
#include <util.h>
#include <novae_util.h>

/* NOVAe includes */
#include <novae/syscalls.h>

using namespace Core;


void Ram_dataspace_factory::_revoke_ram_ds(Dataspace_component &) { }


static inline void * alloc_region(Dataspace_component &ds, const size_t size)
{
	/*
	 * Allocate range in core's virtual address space
	 *
	 * Start with trying to use natural alignment. If this does not work,
	 * successively weaken the alignment constraint until we hit the page size.
	 */
	void *virt_addr = 0;
	size_t align_log2 = log2(ds.size());
	for (; align_log2 >= get_page_size_log2(); align_log2--) {

		platform().region_alloc().alloc_aligned(size, (unsigned)align_log2).with_result(
			[&] (void *ptr) { virt_addr = ptr; },
			[&] (Range_allocator::Alloc_error) { /* try next iteration */ }
		);
		if (virt_addr)
			return virt_addr;
	}

	error("alloc_region of size ", size, " unexpectedly failed");
	return nullptr;
}


void Ram_dataspace_factory::_clear_ds(Dataspace_component &ds)
{
	size_t const size = align_addr(ds.size(), get_page_size_log2());

	size_t memset_count = size / 4;
	addr_t memset_ptr   = ds.core_local_addr();

	if ((memset_count * 4 == size) && !(memset_ptr & 0x3))
		asm volatile ("rep stosl" : "+D" (memset_ptr), "+c" (memset_count)
		                          : "a" (0)  : "memory");
	else
		memset(reinterpret_cast<void *>(memset_ptr), 0, size);

	/* we don't keep any core-local mapping */
	unmap_local(ds.core_local_addr(), size >> get_page_size_log2(), { });

	platform().region_alloc().free((void*)ds.core_local_addr(), size);

	ds.assign_core_local_addr(nullptr);
}


void Ram_dataspace_factory::_export_ram_ds(Dataspace_component &ds) {

	size_t page_rounded_size = align_addr(ds.size(), get_page_size_log2());

	/* allocate the virtual region contiguous for the dataspace */
	void * const virt_ptr = alloc_region(ds, page_rounded_size);
	if (!virt_ptr)
		throw Core_virtual_memory_exhausted();

	/* map it writeable for _clear_ds */
	if (!map_phys_to_core(ds.phys_addr(),
	                      reinterpret_cast<addr_t>(virt_ptr),
	                      page_rounded_size >> get_page_size_log2(),
	                      Novae::Rights::rw()))
		throw Core_virtual_memory_exhausted();

	/* assign virtual address to the dataspace to be used by clear_ds */
	ds.assign_core_local_addr(virt_ptr);
}
