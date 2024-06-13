/*
 * \brief  NOVA-specific implementation of the Thread API for core
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

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/stack.h>

/* NOVAe includes */
#include <novae/syscalls.h>

/* core includes */
#include <platform.h>
#include <novae_util.h>
#include <trace/source_registry.h>
#include <pager.h>


using namespace Core;


void Thread::_init_platform_thread(size_t, Type type)
{
	/*
	 * This function is called for constructing server activations and pager
	 * objects. It allocates capability selectors for the thread's execution
	 * context and a synchronization-helper semaphore needed for 'Lock'.
	 */
	using namespace Novae;

	if (type == MAIN)
	{
		/* set EC selector according to NOVA spec */
		native_thread().ec_sel = platform_specific().core_pd_sel() + 1;

		/*
		 * Exception base of first thread in core is 0. We have to set
		 * it here so that Thread code finds the semaphore of the
		 * main thread.
		 */
		native_thread().exc_pt_sel = 0;

		return;
	}
	native_thread().ec_sel     = cap_map().insert(2);
	native_thread().exc_pt_sel = cap_map().insert(NUM_INITIAL_PT_LOG2);

	/* create running semaphore required for locking */
	addr_t rs_sel = native_thread().exc_pt_sel + SM_SEL_EC;
	uint8_t res = create_sm(rs_sel, platform_specific().core_pd_sel(), 0);
	if (res != NOVA_OK)
		error("Thread::_init_platform_thread: create_sm returned ", res);
}


void Thread::_deinit_platform_thread()
{
	error(__func__, " sleep forever");
	while (true) { }
#if 0
	unmap_local(Novae::Obj_crd(native_thread().ec_sel, 2));
	unmap_local(Novae::Obj_crd(native_thread().exc_pt_sel, Novae::NUM_INITIAL_PT_LOG2));
#endif

	cap_map().remove(native_thread().ec_sel, 2, false);
	cap_map().remove(native_thread().exc_pt_sel, Novae::NUM_INITIAL_PT_LOG2, false);

	/* revoke utcb */
	Novae::Rights rwx(true, true, true);
	addr_t utcb = reinterpret_cast<addr_t>(&_stack->utcb());
	Novae::revoke(Novae::Mem_crd(utcb >> 12, 0, rwx));
}


Thread::Start_result Thread::start()
{
	using namespace Novae;

	/* create local EC */
	auto res = create_ec(native_thread().ec_sel,
	                     platform_specific().core_pd_sel(),
	                     platform_specific().kernel_cpu_id(_affinity),
	                     reinterpret_cast<addr_t>(&_stack->utcb()),
	                     _stack->top(),
	                     native_thread().exc_pt_sel,
	                     false);
	if (res != NOVA_OK) {
		error("Thread::start: create_ec returned ", res);
		return Start_result::DENIED;
	}

	res = map_pagefault_portal(0, native_thread().exc_pt_sel,
	                           platform_specific().core_obj_sel(),
	                           platform_specific().core_obj_sel());

	if (res != NOVA_OK) {
		error("Thread::start: failed to create page-fault portal");
		return Start_result::DENIED;
	}

	Pager_object::enable_delegation(native_thread().exc_pt_sel,
	                                name() != "pager");

	struct Core_trace_source : public  Core::Trace::Source::Info_accessor,
	                           private Core::Trace::Control,
	                           private Core::Trace::Source
	{
		Thread &thread;

		/**
		 * Trace::Source::Info_accessor interface
		 */
		Info trace_source_info() const override
		{
			uint64_t ec_time = 0;

			uint8_t res = Novae::ec_time(thread.native_thread().ec_sel, ec_time);
			if (res != Novae::NOVA_OK)
				warning("ec_time for core thread failed res=", res);

			return { Session_label("core"), thread.name(),
			         Trace::Execution_time(ec_time, 0), thread._affinity };
		}

		Core_trace_source(Core::Trace::Source_registry &registry, Thread &t)
		:
			Core::Trace::Control(),
			Core::Trace::Source(*this, *this), thread(t)
		{
			registry.insert(this);
		}
	};

	new (platform().core_mem_alloc())
		Core_trace_source(Core::Trace::sources(), *this);

	return Start_result::OK;
}
