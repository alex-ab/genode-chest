/*
 * \brief  Implementation of the SIGNAL interface
 * \author Alexander Boettcher
 * \date   2024-11-17
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <signal_source_component.h>


using namespace Core;


void Signal_source_component::release(Signal_context_component &context)
{
	if (context.enqueued())
		_signal_queue.remove(context);
}


void Signal_source_component::submit(Signal_context_component &context,
                                     unsigned long             cnt)
{
	/*
	 * If the client does not block in 'wait_for_signal', the
	 * signal will be delivered as result of the next
	 * 'wait_for_signal' call.
	 */
	context.increment_signal_cnt((int)cnt);

	if (context.enqueued())
		return;

	_signal_queue.enqueue(context);

	Novae::sm_ctrl(_notify.local_name(), Novae::SEMAPHORE_UP);
}


Signal_source::Signal Signal_source_component::wait_for_signal()
{
	Signal result(0, 0);  /* just a dummy in the case of no signal pending */

	/* dequeue and return pending signal */
	_signal_queue.dequeue([&result] (Signal_context_component &context) {
		result = Signal(context.imprint(), context.cnt());
		context.reset_signal_cnt();
	});
	return result;
}


Signal_source_component::Signal_source_component(Rpc_entrypoint &ep)
: _entrypoint(ep) { }


Signal_source_component::~Signal_source_component() { }
