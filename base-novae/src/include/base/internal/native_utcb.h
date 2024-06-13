/*
 * \brief  UTCB definition
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

#include <base/stdint.h>

namespace Genode { struct Native_utcb; }

class Genode::Native_utcb
{
	private:

		/**
		 * Size of the NOVA-specific user-level thread-control
		 * block
		 */
		enum { UTCB_SIZE = 4096 };

		/**
		 * User-level thread control block
		 *
		 * The UTCB is one 4K page, shared between the kernel
		 * and the user process. It is not backed by a
		 * dataspace but provided by the kernel.
		 */
		addr_t _utcb[UTCB_SIZE/sizeof(addr_t)];

	public:

		Native_utcb() { }
};
