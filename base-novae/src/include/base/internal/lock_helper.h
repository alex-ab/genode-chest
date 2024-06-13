/*
 * \brief  Helper functions for the Lock implementation
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
#include <base/stdint.h>

/* base-internal includes */
#include <base/internal/native_thread.h>


static inline bool thread_check_stopped_and_restart(Genode::Thread *thread_base)
{
	Genode::error(__func__, " implement me ", thread_base);
	return true;
}


static inline void thread_switch_to(Genode::Thread *)
{
	Genode::error(__func__, " implement me ");
}


static inline void thread_stop_myself(Genode::Thread *myself)
{
	using namespace Genode;

	error(__func__, " implement me ", myself);
}


static inline void thread_yield()
{
	Genode::error(__func__, " implement me ");
}
