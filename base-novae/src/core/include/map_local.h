/*
 * \brief  Core-local mapping
 * \author Alexander Boettcher
 * \date   2024-06-19
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

/* Genode includes */
#include <base/thread.h>

/* core includes */
//#include <nova_util.h>

namespace Core {

	/**
	 * Map pages locally within core
	 *
	 * On NOVA, address-space mappings from core to core originate always from
	 * the physical address space.
	 *
	 * \param from_phys  physical source address
	 * \param to_virt    core-local destination address
	 * \param num_pages  number of pages to map
	 *
	 * \return true on success
	 */
	inline bool map_local(addr_t /*from_phys*/, addr_t /*to_virt*/, size_t /*num_pages*/,
	                      bool read = true, bool write = true, bool exec = true)
	{
		(void)read;
		(void)write;
		(void)exec;
while(true) { };
#if 0
		return (::map_local(platform_specific().core_pd_sel(),
		                    *(Nova::Utcb *)Thread::myself()->utcb(),
		                    from_phys, to_virt, num_pages,
		                    Nova::Rights(read, write, exec), true) == 0);
#endif
	}

	/**
	 * Unmap pages locally within core
	 *
	 * \param virt       core-local address
	 * \param num_pages  number of pages to unmap
	 */
	inline void unmap_local(addr_t /* virt */, size_t /* num_pages */)
	{
while(true) { };
//		::unmap_local(*(Nova::Utcb *)Thread::myself()->utcb(), virt, num_pages);
	}
}
