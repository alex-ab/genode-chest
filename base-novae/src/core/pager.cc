/*
 * \brief  Pager framework
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-25
 */

/*
 * Copyright (C) 2010-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <rm_session/rm_session.h>

/* core includes */
#include <pager.h>
#include <platform.h>
#include <platform_thread.h>
#include <imprint_badge.h>
#include <cpu_thread_component.h>
#include <core_env.h>

/* NOVAe includes */
#include <novae/syscalls.h>
#include <novae_util.h> /* map_local */
#include <novae/capability_space.h>
#include <novae/util.h>


using namespace Core;
using namespace Novae;


/**
 * Pager threads - one thread per CPU
 */
struct Pager_thread: public Thread
{
	Pager_thread(Affinity::Location location)
	: Thread(Cpu_session::Weight::DEFAULT_WEIGHT, "pager", 2 * 4096, location)
	{
		/* creates local EC */
		Thread::start();
	}

	void entry() override { }
};

enum { PAGER_CPUS = Core::Platform::MAX_SUPPORTED_CPUS };
static Constructible<Pager_thread> pager_threads[PAGER_CPUS];

static void with_pager_thread(Affinity::Location location,
                              Core::Platform &platform, auto const &fn)
{
	unsigned const pager_index = platform.pager_index(location);
	unsigned const kernel_cpu_id = platform.kernel_cpu_id(location);

	if (pager_index < PAGER_CPUS && pager_threads[pager_index].constructed()) {

		fn(*pager_threads[pager_index]);

		return;
	}

	warning("invalid CPU parameter used in pager object: ",
	        pager_index, "->", kernel_cpu_id, " location=",
	        location.xpos(), "x", location.ypos(), " ",
	        location.width(), "x", location.height());
}


/**
 * Utility for the formatted output of page-fault information
 */
struct Page_fault_info
{
	char const * const pd;
	char const * const thread;
	unsigned const cpu;
	addr_t const ip, addr, sp;
	uint8_t const pf_type;

	Page_fault_info(char const *pd, char const *thread, unsigned cpu,
	                addr_t ip, addr_t addr, addr_t sp, unsigned type)
	:
		pd(pd), thread(thread), cpu(cpu), ip(ip), addr(addr),
		sp(sp), pf_type((uint8_t)type)
	{ }

	void print(Genode::Output &out) const
	{
		Genode::print(out, "pd='",     pd,      "' "
		                   "thread='", thread,  "' "
		                   "cpu=",     cpu,     " "
		                   "ip=",      Hex(ip), " "
		                   "address=", Hex(addr), " "
		                   "stack pointer=", Hex(sp), " "
		                   "qualifiers=", Hex(pf_type), " ",
		                   pf_type & Ipc_pager::ERR_I ? "I" : "i",
		                   pf_type & Ipc_pager::ERR_R ? "R" : "r",
		                   pf_type & Ipc_pager::ERR_U ? "U" : "u",
		                   pf_type & Ipc_pager::ERR_W ? "W" : "w",
		                   pf_type & Ipc_pager::ERR_P ? "P" : "p");
	}
};


void Pager_object::_page_fault_handler(Pager_object &obj, unsigned mtd)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	bool const normal_ipc = !(mtd & Mtd::QUAL);

	/* XXX not clear way to distingush whether page fault or IPC - mtd & QUAL not correct */
	Ipc_pager ipc_pager(utcb, obj.pd_sel_host(),
	                    platform_specific().kernel_host_sel(),
	                    normal_ipc);

	/* potential request to ask for EC cap or signal SM cap */
	if (normal_ipc && mtd + 1 == 2)
		_invoke_handler(obj, mtd);

	/*
	 * obj.pager() (pager thread) may issue a signal to the remote region
	 * handler thread which may respond via wake_up() (ep thread) before
	 * we are done here - we have to lock the whole page lookup procedure
	 */
	obj._state_lock.acquire();

	obj._state.thread.cpu.ip     = ipc_pager.fault_ip();
	obj._state.thread.cpu.sp     = 0;
	obj._state.thread.cpu.trapno = PT_SEL_PAGE_FAULT;

	obj._state.block();
	obj._state.block_pause_sm();

	/* lookup fault address and decide what to do */
	unsigned error = (obj.pager(ipc_pager) == Pager_object::Pager_result::STOP);

	if (!error && ipc_pager.syscall_result() != Novae::NOVA_OK) {
		/* something went wrong - by default don't answer the page fault */
		error = 4;
	}

	/* good case - found a valid region which is mappable */
	if (!error) {
		obj._state.unblock_pause_sm();
		obj._state.unblock();
		obj._state_lock.release();
		ipc_pager.reply_and_wait_for_fault();
	}

	char const * const client_thread = obj.client_thread();
	char const * const client_pd     = obj.client_pd();

	unsigned const cpu_id = platform_specific().pager_index(myself.affinity());

	Page_fault_info const fault_info(client_pd, client_thread, cpu_id,
	                                 ipc_pager.fault_ip(),
	                                 ipc_pager.fault_addr(),
	                                 ipc_pager.sp(),
	                                 (uint8_t)ipc_pager.fault_type());
	obj._state_lock.release();

	revoke(obj.pd_sel_obj(), Obj_crd(obj._exc_pt_base_child +
	                                 PT_SEL_PAGE_FAULT, 0));

	/* XXX block thread forever XXX */
	Genode::error("thread '", client_thread, "' of pd '", client_pd,
	              "' is dead - forever");

	ipc_pager.reply_and_wait_for_fault();
}


void Pager_object::exception(uint8_t exit_id, unsigned mtd)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	if (exit_id > PT_SEL_PARENT)
		nova_die();

	addr_t const fault_ip = utcb.ip();
	addr_t const fault_sp = utcb.sp();
	addr_t const fault_bp = utcb.bp();

	uint8_t res = 0xFF;
	        mtd = 0;

	_state_lock.acquire();

	/* remember exception type for Cpu_session::state() calls */
	_state.thread.cpu.trapno = exit_id;

	if (_exception_sigh.valid()) {
		_state.submit_signal();
		res = _unsynchronized_client_recall(true);
	}

	if (res != NOVA_OK) {
		/* nobody handles this exception - so thread will be stopped finally */
		_state.mark_dead();

		unsigned const cpu_id = platform_specific().pager_index(myself.affinity());

		warning("unresolvable exception ", exit_id,  ", "
		        "pd '",     client_pd(),            "', "
		        "thread '", client_thread(),        "', "
		        "cpu ",     cpu_id,                  ", "
		        "ip=",      Hex(fault_ip),            " "
		        "sp=",      Hex(fault_sp),            " "
		        "bp=",      Hex(fault_bp),            " ",
		        res == 0xFF ? "no signal handler"
		                    : (res == NOVA_OK ? "" : "recall failed"));

		revoke(pd_sel_obj(), Obj_crd(_exc_pt_base_child,
		                             NUM_INITIAL_PT_LOG2));


		enum { TRAP_BREAKPOINT = 3 };

		if (exit_id == TRAP_BREAKPOINT) {
			utcb.ip(fault_ip - 1);
			mtd     = Mtd::EIP;
		}
	}

	_state_lock.release();

	reply(myself.stack_top(), mtd);
}


void Pager_object::_recall_handler(Pager_object &obj, unsigned mtd)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	/* acquire mutex */
	obj._state_lock.acquire();

	if (obj._state.modified) {
		obj._copy_state_to_utcb(utcb, mtd);
		obj._state.modified = false;
	} else
		mtd = 0;

	/* switch on/off single step */
	bool singlestep_state = obj._state.thread.cpu.eflags & 0x100UL;
	if (obj._state.singlestep() && !singlestep_state) {
		utcb.fl(utcb.fl() | 0x100UL);
		mtd |= Mtd::EFL;
	} else if (!obj._state.singlestep() && singlestep_state) {
		utcb.fl(utcb.fl() & ~0x100UL);
		mtd |= Mtd::EFL;
	}

	/* deliver signal if it was requested */
	if (obj._state.to_submit())
		obj.submit_exception_signal();

	/* block until Cpu_session()::resume() respectively wake_up() call */

	unsigned long sm = 0;

	if (obj._state.blocked()) {
		sm = obj.sel_sm_block_pause();
		obj._state.block_pause_sm();
	}

	obj._state_lock.release();

	reply(myself.stack_top(), mtd, sm);
}


void Pager_object::_startup_handler(Pager_object &obj, unsigned)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	utcb.ip(obj._initial_eip);
	utcb.sp(obj._initial_esp);
	unsigned mtd = Mtd::EIP | Mtd::ESP;

	if (obj._state.singlestep()) {
		utcb.fl(0x100UL);
		mtd |= Mtd::EFL;
	}

	obj._state.unblock();

	reply(myself.stack_top(), mtd);
}


struct Recall {

	struct call {
		addr_t src_sel;
		addr_t src_pd;
		addr_t dst_sel;
		addr_t dst_pd;
	};

	struct call recall[2048];

	struct call transfer;
	addr_t      transfer_id;
	bool        transfer_valid;

	void apply(struct call const &check, auto const &fn, auto const &fn_error)
	{
		if (check.src_sel == Capability_space::INVALID_INDEX)
			return;

		for (auto &entry : recall) {
			if (!entry.src_sel)
				continue;

			if (entry.src_sel == check.src_sel && entry.dst_pd == check.dst_pd)
				if (fn(entry))
					return;
		}

		fn_error();
	}

	void lookup_src_sel(auto const &check, auto const &fn) const
	{
		if (check.dst_sel == Capability_space::INVALID_INDEX)
			return;

		for (auto const &entry : recall) {
			if (!entry.src_sel)
				continue;

			if (entry.dst_sel == check.dst_sel &&
			    entry.dst_pd  == check.dst_pd  &&
			    entry.src_pd  == check.src_pd)
				if (fn(entry))
					return;
		}
	}

	void lookup_dst(auto const &check, auto const &fn) const
	{
		if (check.src_sel == Capability_space::INVALID_INDEX)
			return;

		for (auto const &entry : recall) {
			if (!entry.src_sel)
				continue;

			if (entry.src_sel == check.src_sel &&
			    entry.src_pd  == check.src_pd)
				fn(entry);
		}
	}

	bool insert(struct call const &add)
	{
		if (add.src_sel == Capability_space::INVALID_INDEX)
			return false;

		for (auto &entry : recall) {
			if (entry.src_sel)
				continue;

			entry = add;
			return true;
		}

		return false;
	}
};


void Pager_object::track_delegation(uint64_t src_core, uint64_t dst, unsigned count)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	if (count > 1)
		warning(__func__, " ", count, " improve ?");

	for (unsigned i = 0; i < count; i++) {
		unsigned const mtd = 2 - 1;

		utcb.msg()[0] = 0; /* XXX id */
		utcb.msg()[1] = (src_core + i) << 2; /* grant */

		_delegate_handler(*reinterpret_cast<Pager_object *>(2), mtd);

		utcb.msg()[0] = 0; /* XXX id */
		utcb.msg()[1] = ((dst + i) << 2) | 1; /* take */

		_delegate_handler(*this, mtd);
	}
}


enum { CORE_BADGE = 1, CORE_BADGE_L = 2 };


void Pager_object::_delegate_handler(Pager_object &obj, unsigned mtd)
{
	enum { GRANT = 0, TAKE = 1 };

	struct track {
		addr_t id;
		addr_t src_sel;
		addr_t src_pd;
		addr_t chk_pd1; /* creator PD */
		addr_t chk_pd2; /* callee  PD */
		addr_t chk_pd3; /* ?       PD */
		addr_t chk_pd4; /* ?       PD */
		addr_t chk_pd5; /* ?       PD */
		addr_t chk_pd6; /* ?       PD */
		addr_t chk_pd7; /* ?       PD */
		bool   valid;
		bool   noreply;
	};

	static struct Recall recall;
	static struct track  transfers[20];
	static Mutex         mutex; /* due to pager per core */

	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	/* if protocol is violated ignore request */
	if (mtd != 1 && mtd != 2) {
		utcb.msg()[0] = 0;
		mtd           = 0;
		reply(myself.stack_top(), mtd);
	}

	auto const id_trans   = utcb.msg()[0];
	auto const id_action  = utcb.msg()[1] & 0x3;
	auto const selector   = uint64_t(int64_t(utcb.msg()[1]) >> 2);
	auto const pt_ipc_dst = utcb.msg()[2];

	bool const core_local = !! (reinterpret_cast<addr_t>(&obj) == CORE_BADGE_L);
	bool const is_core    = !!((reinterpret_cast<addr_t>(&obj) == CORE_BADGE) ||
	                           core_local);
	auto const core_pd    = platform_specific().core_obj_sel();
	uint8_t    res        = Novae::NOVA_ABORTED;
	bool const verbose    = false;

	/* Guard can't be used because of explicit Novae::reply */
	mutex.acquire();

	unsigned slot = ~0U;

	switch (id_action) {
	case GRANT:
		for (unsigned i = 0; i < sizeof(transfers) / sizeof(transfers[0]); i++) {
			if (!transfers[i].valid) {
				slot = i;
				break;
			}
		}
		break;
	case TAKE:
		for (unsigned i = 0; i < sizeof(transfers) / sizeof(transfers[0]); i++) {
			if (transfers[i].valid && transfers[i].id == id_trans) {
				slot = i;
				break;
			}
		}
		break;
	default:
		break;
	}

	if (slot == ~0U) {
		mutex.release();

		error("IPC transaction failed");
		utcb.msg()[0] = 0;
		mtd           = 0;
		reply(myself.stack_top(), mtd);
	}

	auto     & transfer = transfers[slot];
	bool const do_reply = !(core_local || (transfer.valid && transfer.noreply));

	if (verbose) {
		auto client = is_core ? nullptr
		                      : reinterpret_cast<Platform_thread *>(obj._badge);

		log(__func__, " mtd=", Hex(mtd),
		              " 0=", Hex(id_trans),
		              " 1=", Hex(id_action), " ",
		              id_action == 0 ? "GRANT" :
		              id_action == 1 ? "TAKE " : "unknown",
		              " 2=", Hex(selector),
		              " caller=", &obj, " pd_sel=",
		              Hex(is_core ? core_pd : obj.pd_sel_obj()), " ",
		              "'", is_core ? "core" : client->pd_name(), "'",
		              ":'", is_core ? "core" : client->name(), "' ",
		              do_reply ? "reply" : "no reply");
	}

	/* one word as default answer */
	mtd = 0;

	switch (id_action) {
	case GRANT: { /* grant cap */

		if (transfer.valid)
			warning("still valid transfer will be overwritten");

		auto chk_pd_1 = Capability_space::INVALID_INDEX;
		auto chk_pd_2 = Capability_space::INVALID_INDEX;
		auto chk_pd_3 = Capability_space::INVALID_INDEX;
		auto chk_pd_4 = Capability_space::INVALID_INDEX;
		auto chk_pd_5 = Capability_space::INVALID_INDEX;
		auto chk_pd_6 = Capability_space::INVALID_INDEX;
		auto chk_pd_7 = Capability_space::INVALID_INDEX;

		if (is_core) {

			Recall::call dst_ipc = { .src_sel = pt_ipc_dst,
			                         .src_pd  = core_pd,
			                         .dst_sel = 0,
			                         .dst_pd  = 0 };

			unsigned cnt = 0;
			recall.lookup_dst(dst_ipc, [&](auto const &e) {
				if (verbose)
					error(cnt, " -> ", Hex(e.dst_pd) , ":", Hex(e.dst_sel));
				if (cnt == 0) chk_pd_1 = e.dst_pd;
				if (cnt == 1) chk_pd_2 = e.dst_pd;
				if (cnt == 2) chk_pd_3 = e.dst_pd;
				if (cnt == 3) chk_pd_4 = e.dst_pd;
				if (cnt == 4) chk_pd_5 = e.dst_pd;
				if (cnt == 5) chk_pd_6 = e.dst_pd;
				if (cnt == 6) chk_pd_7 = e.dst_pd;
				cnt ++;
			});

			if (cnt > 7)
				warning("too many clients for tracking cnt=", cnt, " (core)");
			else
			if (cnt == 0)
				if (verbose)
					warning("unknown pt_ipc_dst in core");
		}

		if (!is_core) {

			Recall::call ipc = { .src_sel = 0,
			                     .src_pd  = core_pd,
			                     .dst_sel = pt_ipc_dst,
			                     .dst_pd  = is_core ? core_pd : obj.pd_sel_obj() };

			recall.lookup_src_sel(ipc, [&](auto const &entry) {

				Recall::call dst_ipc = { .src_sel = entry.src_sel,
				                         .src_pd  = core_pd,
				                         .dst_sel = 0,
				                         .dst_pd  = 0 };

				unsigned cnt = 0;
				recall.lookup_dst(dst_ipc, [&](auto const &e) {
					if (verbose)
						error(cnt, " -> ", Hex(e.dst_pd) , ":", Hex(e.dst_sel),
						      ipc.dst_pd == e.dst_pd ? " same PD" : " other PD");
					if (cnt == 0) chk_pd_1 = e.dst_pd;
					if (cnt == 1) chk_pd_2 = e.dst_pd;
					if (cnt == 2) chk_pd_3 = e.dst_pd;
					if (cnt == 3) chk_pd_4 = e.dst_pd;
					if (cnt == 4) chk_pd_5 = e.dst_pd;
					if (cnt == 5) chk_pd_6 = e.dst_pd;
					if (cnt == 6) chk_pd_7 = e.dst_pd;
					cnt ++;
				});

				if (cnt > 7)
					warning("too many clients for tracking cnt=", cnt);

				return true;
			});
		}

		/* lookup in core selectors of non core IPC */
		if (!is_core) {

			Recall::call ipc = { .src_sel = 0,
			                     .src_pd  = core_pd,
			                     .dst_sel = selector,
			                     .dst_pd  = obj.pd_sel_obj() };

			recall.lookup_src_sel(ipc, [&](auto const &entry) {

				transfer = {
					.id      = id_trans,
					.src_sel = entry.src_sel,
					.src_pd  = entry.src_pd,
					.chk_pd1 = chk_pd_1,
					.chk_pd2 = chk_pd_2,
					.chk_pd3 = chk_pd_3,
					.chk_pd4 = chk_pd_4,
					.chk_pd5 = chk_pd_5,
					.chk_pd6 = chk_pd_6,
					.chk_pd7 = chk_pd_7,
					.valid   = true,
					.noreply = core_local,
				};

				res = Novae::NOVA_OK;

				return true;
			});

			if (res != Novae::NOVA_OK &&
			    selector != Capability_space::INVALID_INDEX) {
				error("unknown selector ?? non core transfer !! ",
				      Hex(selector));
			}
		}

		if (res != Novae::NOVA_OK) {

			transfer = {
				.id      = id_trans,
				.src_sel = selector,
				.src_pd  = is_core ? core_pd : obj.pd_sel_obj(),
				.chk_pd1 = chk_pd_1,
				.chk_pd2 = chk_pd_2,
				.chk_pd3 = chk_pd_3,
				.chk_pd4 = chk_pd_4,
				.chk_pd5 = chk_pd_5,
				.chk_pd6 = chk_pd_6,
				.chk_pd7 = chk_pd_7,
				.valid   = true,
				.noreply = core_local,
			};

			res = Novae::NOVA_OK;
		}

		break;
	}
	case TAKE: /* take cap */

		if (!(transfer.valid && transfer.id == id_trans)) {
			warning ("unexpected transfer id -> parallel usage by multiple "
			         "clients ? ", Hex(transfer.id), " ", Hex(id_trans));
			utcb.msg()[1] = Capability_space::INVALID_INDEX;
			mtd += 1;
			break;
		}

		if (transfer.src_sel == Capability_space::INVALID_INDEX) {

			transfer = { };

			utcb.msg()[1] = Capability_space::INVALID_INDEX;
			mtd += 1;

			break;
		}

		if (transfer.src_pd != core_pd) {
			error("non src core transfer ????");
		}

		/* track_delegation is not checked */
		if (!transfer.noreply) {

			auto const check_dst = is_core ? core_pd : obj.pd_sel_obj();

			/* security check, core is trusted to behave correctly */
			if (transfer.chk_pd1 != check_dst &&
			    transfer.chk_pd2 != check_dst &&
			    transfer.chk_pd3 != check_dst &&
			    transfer.chk_pd4 != check_dst &&
			    transfer.chk_pd5 != check_dst &&
			    transfer.chk_pd6 != check_dst &&
			    transfer.chk_pd7 != check_dst &&
			    core_pd          != check_dst) {

				/* no transfer registered for IPC */
				error("IPC callee check failed ", Hex(check_dst), " ",
				      Hex(transfer.chk_pd1), " ",
				      Hex(transfer.chk_pd2), " ",
				      Hex(transfer.chk_pd3), " ",
				      Hex(transfer.chk_pd4), " ",
				      Hex(transfer.chk_pd5), " ",
				      Hex(transfer.chk_pd6), " ",
				      Hex(transfer.chk_pd7), " ",
				      is_core ? " core" : " remote");

				transfer = { };

				utcb.msg()[1] = Capability_space::INVALID_INDEX;
				mtd += 1;

				break;
			}
		}

		if (!is_core && transfer.src_pd == core_pd) {

			Recall::call ipc = { .src_sel = transfer.src_sel,
			                     .src_pd  = transfer.src_pd,
			                     .dst_sel = selector,
			                     .dst_pd  = obj.pd_sel_obj() };

			recall.apply(ipc, [&](auto const &entry) {

				if (verbose)
					log("---- same entry ", Hex(ipc.src_sel), "->",
					    Hex(ipc.dst_sel), "(", Hex(ipc.dst_sel), ") "
					    "dst=", Hex(ipc.dst_pd));

				transfer = { };

				res = async_map(entry.src_pd,
				                entry.dst_pd,
				                Obj_crd(entry.src_sel, 0),
				                Obj_crd(entry.dst_sel, 0));

				utcb.msg()[1] = entry.dst_sel;
				mtd += 1;

				return true;
			}, [&](){

				if (!recall.insert(ipc))
					error("---- remember ", Hex(ipc.src_sel), "->",
					      Hex(ipc.dst_sel), " src=", Hex(ipc.src_pd),
					      " dst=", Hex(ipc.dst_pd), " failed");
				else
					if (verbose)
						log("---- remember ", Hex(ipc.src_sel), "->",
						    Hex(ipc.dst_sel),
					        " src=", Hex(ipc.src_pd), " dst=", Hex(ipc.dst_pd));
			});
		}

		if (res != Novae::NOVA_OK) {

			if (is_core && transfer.src_pd == core_pd) {
				utcb.msg()[1] = transfer.src_sel;
				mtd += 1;
			} else {
				res = async_map(transfer.src_pd,
				                is_core ? core_pd : obj.pd_sel_obj(),
				                Obj_crd(transfer.src_sel, 0),
				                Obj_crd(selector, 0));
			}

			transfer.valid  = false;
		}

		break;

	default:
		res = Novae::NOVA_ABORTED;
		break;
	}

	if (verbose && res)
		error(__func__, " res=", res);

	utcb.msg()[0] = (res == Novae::NOVA_OK) ? 1 : 0;

	mutex.release();

	if (do_reply)
		reply(myself.stack_top(), mtd);
}


void Pager_object::_invoke_handler(Pager_object &obj, unsigned mtd)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	/* if protocol is violated ignore request */
	if (mtd + 1 != 2) {
		utcb.msg()[0] = 0;
		reply(myself.stack_top(), 0);
	}

	addr_t const event = utcb.msg()[0];

	/* semaphore for signaling thread is requested */
	if (event == ~0UL - 1) {
		auto const dst_sel = utcb.msg()[1];

		/* create semaphore only once */
		if (!obj._state.has_signal_sm()) {

			bool res = Novae::create_sm(obj.exc_pt_sel_client() + SM_SEL_SIGNAL,
			                           platform_specific().core_pd_sel(), 0);
			if (res != Novae::NOVA_OK)
				reply(myself.stack_top(), 0);

			obj._state.mark_signal_sm();
		}

		async_map(platform_specific().core_obj_sel(),
		          obj.pd_sel_obj(),
		          Obj_crd(obj.exc_pt_sel_client() + SM_SEL_SIGNAL, 0),
		          Obj_crd(dst_sel, 0));
	}

	utcb.msg()[0] = 0;

	reply(myself.stack_top(), 0);
}


void Pager_object::wake_up()
{
	Mutex::Guard _state_lock_guard(_state_lock);

	if (!_state.blocked())
		return;

	_state.thread.state = Thread_state::State::VALID;

	_state.unblock();

	if (_state.blocked_pause_sm()) {

		uint8_t res = sm_ctrl(sel_sm_block_pause(), SEMAPHORE_UP);

		if (res == NOVA_OK)
			_state.unblock_pause_sm();
		else
			warning("canceling blocked client failed (thread sm)");
	}
}


uint8_t Pager_object::client_recall(bool get_state_and_block)
{
	Mutex::Guard _state_lock_guard(_state_lock);
	return _unsynchronized_client_recall(get_state_and_block);
}


uint8_t Pager_object::_unsynchronized_client_recall(bool get_state_and_block)
{
	enum { STATE_REQUESTED = 1UL, STATE_INVALID = ~0UL };

	uint8_t res = ec_ctrl(EC_RECALL, _state.sel_client_ec,
	                      get_state_and_block ? STATE_REQUESTED : STATE_INVALID);

	if (res != NOVA_OK)
		return res;

	if (get_state_and_block) {
		Utcb &utcb = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());
		_copy_state_from_utcb(utcb);
		_state.block();
	}

	return res;
}


void Pager_object::cleanup_call()
{
	_state.mark_dissolved();

	auto const core_pd = platform_specific().core_obj_sel();

	/* revoke ec and sc cap of client before the sm cap */
	if (_state.sel_client_ec != Native_thread::INVALID_INDEX)
		revoke(core_pd, Obj_crd(_state.sel_client_ec, 2));

	/* revoke all portals handling the client. */
	revoke(pd_sel_obj(), Obj_crd(_exc_pt_base_child,  NUM_INITIAL_PT_LOG2));
	revoke(core_pd     , Obj_crd(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

	Utcb & utcb   = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());
	utcb.msg()[0] = 0;
	unsigned  mtd = 0;
	if (auto res = call(sel_pt_cleanup(), mtd))
		error(&utcb, " - cleanup call to pager failed res=", res);
}


void Pager_object::print(Output &out) const
{
	Platform_thread const * const faulter = reinterpret_cast<Platform_thread *>(_badge);
	Genode::print(out, "pager_object: pd='",
			faulter ? faulter->pd_name() : "unknown", "' thread='",
			faulter ? faulter->name() : "unknown", "'");
}


static uint8_t create_portal(addr_t pt, addr_t pd, addr_t ec, Mtd mtd,
                             addr_t eip, Pager_object * obj)
{
	uint8_t res = syscall_retry(*obj,
		[&] { return create_pt(pt, pd, ec, eip); });

	if (res != NOVA_OK)
		return res;

	addr_t const badge = reinterpret_cast<addr_t>(obj);

	return imprint_badge(platform_specific().core_obj_sel(), pt, badge, mtd.value()) ?
	       NOVA_OK : NOVA_INV_PARAMETER;

	return res;
}


/************************
 ** Exception handlers **
 ************************/

template <uint8_t EV>
void Exception_handlers::register_handler(Pager_object &obj, Mtd mtd,
                                          void (* __attribute__((regparm(2))) func)(Pager_object &, unsigned))
{
	uint8_t res = !Novae::NOVA_OK;
	with_pager_thread(obj.location(), platform_specific(), [&] (Pager_thread &pager_thread) {
		addr_t const ec_sel = pager_thread.native_thread().ec_sel;

		/* compiler generates instance of exception entry if not specified */
		addr_t entry = func ? (addr_t)func : (addr_t)(&_handler<EV>);
		res = create_portal(obj.exc_pt_sel_client() + EV,
		                    platform_specific().core_pd_sel(), ec_sel, mtd, entry, &obj);
	});

	if (res != Novae::NOVA_OK)
		error("failed to register exception handler");
}


template <uint8_t EV>
void Exception_handlers::_handler(Pager_object &obj, unsigned mtd)
{
	obj.exception(EV, mtd);
}


Exception_handlers::Exception_handlers(Pager_object &obj)
{
	Mtd const mtd (Mtd::GPR_0_7 | Mtd::ESP | Mtd::EIP);

	register_handler<0>(obj, mtd);
	register_handler<1>(obj, mtd);
	register_handler<2>(obj, mtd);
	register_handler<3>(obj, mtd);
	register_handler<4>(obj, mtd);
	register_handler<5>(obj, mtd);
	register_handler<6>(obj, mtd);
	register_handler<7>(obj, mtd);
	register_handler<8>(obj, mtd);
	register_handler<9>(obj, mtd);
	register_handler<10>(obj, mtd);
	register_handler<11>(obj, mtd);
	register_handler<12>(obj, mtd);
	register_handler<13>(obj, mtd);

	register_handler<15>(obj, mtd);
	register_handler<16>(obj, mtd);
	register_handler<17>(obj, mtd);
	register_handler<18>(obj, mtd);
	register_handler<19>(obj, mtd);
	register_handler<20>(obj, mtd);
	register_handler<21>(obj, mtd);
	register_handler<22>(obj, mtd);
	register_handler<23>(obj, mtd);
	register_handler<24>(obj, mtd);
	register_handler<25>(obj, mtd);
	register_handler<26>(obj, mtd);
	register_handler<27>(obj, mtd);
	register_handler<28>(obj, mtd);
	register_handler<29>(obj, mtd);
	register_handler<30>(obj, mtd);
	register_handler<31>(obj, mtd);
}


/******************
 ** Pager object **
 ******************/

void Pager_object::_construct_pager()
{
	/* create portal for page-fault handler - 14 */
	_exceptions.register_handler<14>(*this, Mtd::QUAL | Mtd::ESP | Mtd::EIP,
	                                 _page_fault_handler);

	/* create portal for recall handler */
	Mtd const mtd_recall(Mtd::ESP | Mtd::EIP | Mtd::EFL |
	                     Mtd::GPR_0_7 | Mtd::FSGS);
	_exceptions.register_handler<PT_SEL_RECALL>(*this, mtd_recall,
	                                            _recall_handler);

	addr_t const pd_sel = platform_specific().core_pd_sel();

	uint8_t res = !Novae::NOVA_OK;

	with_pager_thread(_location, platform_specific(), [&] (Pager_thread &pager_thread) {

		addr_t const ec_sel = pager_thread.native_thread().ec_sel;

		/* create portal for final cleanup call used during destruction */
		res = create_portal(sel_pt_cleanup(), pd_sel, ec_sel, Mtd(0),
		                    reinterpret_cast<addr_t>(_invoke_handler),
		                    this);
	});
	if (res != Novae::NOVA_OK) {
		error("could not create pager cleanup portal, error=", res);
		return;
	}

	/* semaphore used to block paged thread during recall */
	res = Novae::create_sm(sel_sm_block_pause(), pd_sel, 0);
	if (res != Novae::NOVA_OK) {
		error("failed to initialize sel_sm_block_pause, error=", res);
		return;
	}
}


Pager_object::Pager_object(Cpu_session_capability cpu_session_cap,
                           Thread_capability thread_cap, unsigned long badge,
                           Affinity::Location location, Session_label const &,
                           Cpu_session::Name const &)
:
	_badge(badge),
	_selectors(cap_map().insert(1)),
	_exc_pt_base_core(cap_map().insert(NUM_INITIAL_PT_LOG2)),
	_cpu_session_cap(cpu_session_cap),
	_thread_cap(thread_cap),
	_location(location),
	_exceptions(*this)
{
	_state._status       = 0;
	_state.modified      = false;
	_state.sel_client_ec = Native_thread::INVALID_INDEX;
	_state.block();

	if (Native_thread::INVALID_INDEX == _selectors ||
	    Native_thread::INVALID_INDEX == _exc_pt_base_core) {
		error("failed to complete construction of pager object");
		return;
	}

	_construct_pager();

	/* create portal for startup handler */
	Mtd const mtd_startup(Mtd::ESP | Mtd::EIP);
	_exceptions.register_handler<PT_SEL_STARTUP>(*this, mtd_startup,
	                                             _startup_handler);

	_exceptions.register_handler<PT_SEL_DELEGATE>(*this, mtd_startup,
	                                              _delegate_handler);
	/*
	 * Create semaphore required for Genode locking. It can be later on
	 * requested by the thread the same way as all exception portals.
	 */
	auto const pd_sel = platform_specific().core_pd_sel();
	auto const res    = Novae::create_sm(exc_pt_sel_client() + SM_SEL_EC,
	                                     pd_sel, 0);
	if (res != Novae::NOVA_OK)
		error("failed to create locking semaphore for pager object");
}


Pager_object::~Pager_object()
{
	/* sanity check that object got dissolved already - otherwise bug */
	if (!_state.dissolved())
		error(__func__, " pager object in wrong dissolve state");

	auto const core_pd = platform_specific().core_obj_sel();

	/* revoke portal used for the cleanup call and sm cap for blocking state */
	revoke(core_pd, Obj_crd(_selectors, 1));

	cap_map().remove(_selectors, 1, false);
	cap_map().remove(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2, false);
}


const char * Pager_object::client_thread() const
{
	Platform_thread * client = reinterpret_cast<Platform_thread *>(_badge);
	return client ? client->name() : "unknown";
}


const char * Pager_object::client_pd() const
{
	Platform_thread * client = reinterpret_cast<Platform_thread *>(_badge);
	return client ? client->pd_name() : "unknown";
}


void Pager_object::enable_delegation(addr_t const pt_base, bool doit)
{
	static addr_t defer[10];

	if (!doit)
		return;

	if (!pager_threads[0].constructed()) {
		unsigned i;

		for (i = 0; sizeof(defer) / sizeof(defer[0]); i++) {
			if (defer[i])
				continue;
			defer[i] = pt_base;
			break;
		}

		if (i >= sizeof(defer) / sizeof(defer[0]))
			error("could not enable delegation support");
		return;
	}

	auto fn = [&](auto const base) {
		auto ec_sel = pager_threads[0]->native_thread().ec_sel;

		auto ret = create_pt(base + PT_SEL_DELEGATE,
		                     platform_specific().core_pd_sel(),
		                     ec_sel,
		                     (addr_t)Pager_object::_delegate_handler);
		if (ret != Novae::NOVA_OK)
			error(__func__, ":", __LINE__, " returned ", ret);

		unsigned badge = 1;
		if (!imprint_badge(platform_specific().core_obj_sel(),
		                   base + PT_SEL_DELEGATE, badge, 0 /* mtd */))
			error(__func__, ":", __LINE__, " returned ", ret);
	};

	fn(pt_base);

	for (unsigned i = 0; i < sizeof(defer) / sizeof(defer[0]); i++) {
		if (!defer[i])
			continue;

		fn(defer[i]);
		defer[i] = 0;
	}
}


/**********************
 ** Pager entrypoint **
 **********************/

Pager_entrypoint::Pager_entrypoint(Rpc_cap_factory &)
{
	/* sanity check for pager threads */
	if (kernel_hip().cpu_num > PAGER_CPUS) {
		error("kernel supports more CPUs (", kernel_hip().cpu_num, ") "
		      "than Genode (", (unsigned)PAGER_CPUS, ")");
		nova_die();
	}

	/* detect enabled CPUs and create per CPU a pager thread */
	platform_specific().for_each_location([&](Affinity::Location &location) {
		unsigned const pager_index = platform_specific().pager_index(location);

		pager_threads[pager_index].construct(location);

		Pager_object::enable_delegation(0, true); /* exc pt base of first thread */
	});
}


void Pager_entrypoint::dissolve(Pager_object &obj)
{
	/* take care that no faults are in-flight */
	obj.cleanup_call();
}
