/*
 * \brief  Application to show highest CPU consumer per CPU
 * \author Alexander Boettcher
 * \date   2018-06-15
 */

/*
 * Copyright (C) 2018-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <util/dictionary.h>

#include "button.h"
#include "trace.h"
#include "storage.h"

static constexpr unsigned DIV = 10;
enum SORT_TIME { EC_TIME = 0, SC_TIME = 1};
enum { CHECKBOX_ID_FIRST = 7, CHECKBOX_ID_SECOND = 9 };

using Genode::uint64_t;
typedef Genode::Affinity::Location Location;

struct Subjects
{
	private:

		Top::Components               _components { };
		Genode::Avl_tree<Top::Thread> _threads    { };
		Genode::Trace::Timestamp      _timestamp  { 0 };

		Top::Thread *_lookup_thread(Genode::Trace::Subject_id const & id)
		{
			Top::Thread * thread = _threads.first();
			if (thread)
				thread = thread->find_by_id(id);
			return thread;
		}

		template <typename FN, typename T>
		void _for_each_element(T * const element, FN const &fn) const
		{
			if (!element) return;

			_for_each_element(element->child(T::LEFT), fn);

			fn(*element);

			_for_each_element(element->child(T::RIGHT), fn);
		}

		template <typename FN>
		void for_each_thread(FN const &fn) const
		{
			_for_each_element(_threads.first(), fn);
		}

		template <typename FN>
		void for_each_pd(FN const &fn) const
		{
			_components.for_each(fn);
		}

		enum { MAX_CPUS_X = 64, MAX_CPUS_Y = 2, MAX_ELEMENTS_PER_CPU = 20};

		/* accumulated execution time on all CPUs */
		Genode::uint64_t total_first  [MAX_CPUS_X][MAX_CPUS_Y] { };
		Genode::uint64_t total_second [MAX_CPUS_X][MAX_CPUS_Y] { };
		Genode::uint64_t total_idle   [MAX_CPUS_X][MAX_CPUS_Y] { };

		Genode::uint64_t total_cpu_first(Location const &aff) const {
			return total_first[aff.xpos()][aff.ypos()]; }

		Genode::uint64_t total_cpu_second(Location const &aff) const {
			return total_second[aff.xpos()][aff.ypos()]; }

		/* most significant consumer per CPU */
		Top::Thread const * load[MAX_CPUS_X][MAX_CPUS_Y][MAX_ELEMENTS_PER_CPU] { };

		/* disable report for given CPU */
		bool _cpu_show [MAX_CPUS_X][MAX_CPUS_Y] { };
		/* state whether cpu is supposed to be available */
		bool _cpu_online [MAX_CPUS_X][MAX_CPUS_Y] { };
		/* state whether topmost threads should be reported to graph */
		bool _cpu_graph_top [MAX_CPUS_X][MAX_CPUS_Y] { };
		/* state whether topmost threads w/o idle should be reported to graph */
		bool _cpu_graph_top_no_idle [MAX_CPUS_X][MAX_CPUS_Y] { };

		Button_hub<1, 1, 20, 2> _cpu_num [MAX_CPUS_X][MAX_CPUS_Y] { };

		bool & cpu_show(Location const &loc) {
			return _cpu_show[loc.xpos()][loc.ypos()]; }

		bool cpu_show(Location const &loc) const {
			return cpu_online(loc) && _cpu_show[loc.xpos()][loc.ypos()]; }

		bool cpu_online(Location const &loc) const {
			return _cpu_online[loc.xpos()][loc.ypos()]; }

		bool & cpu_online(Location const &loc) {
			return _cpu_online[loc.xpos()][loc.ypos()]; }

		Button_hub<1, 1, 20, 2> & _cpu_number(Location const &loc) {
			return _cpu_num[loc.xpos()][loc.ypos()]; }

		Button_hub<1, 1, 20, 2> const & _cpu_number(Location const &loc) const {
			return _cpu_num[loc.xpos()][loc.ypos()]; }

		bool & _graph_top_most(Location const &loc) {
			return _cpu_graph_top[loc.xpos()][loc.ypos()]; }

		bool & _graph_top_most_no_idle(Location const &loc) {
			return _cpu_graph_top_no_idle[loc.xpos()][loc.ypos()]; }

		unsigned _num_subjects       {  0 };
		unsigned _num_pds            {  0 };
		unsigned _config_pds_per_cpu { 40 };

		Genode::Trace::Subject_id _hovered_subject    { };
		unsigned                  _hovered_sub_id     { 0 };
		Genode::Trace::Subject_id _detailed_view      { };
		bool                      _detailed_view_back { false };

		Button_state  _button_cpus    { 0, MAX_CPUS_X * MAX_CPUS_Y };
		Button_state  _button_numbers { 2, 100, _config_pds_per_cpu };
		Button_state  _pd_scroll      { 0, ~0U };
		Button_hub<5, 0, 9, 0> _button_trace_period { };
		Button_hub<5, 0, 9, 0> _button_view_period  { };

		Location _button_cpu   { };
		Location _last_cpu     { };
		Location _button_top_most { };
		Location _button_top_most_no_idle { };
		Location _button_cpu_num  { };

		unsigned      _button_number  { 2 };

		unsigned _tracked_threads     { 0 };

		bool _enable_view                { false };

		bool _button_enable_view_hovered { false };
		bool _button_setting             { false };
		bool _button_thread_hovered      { false };
		bool _button_component_hovered   { false };
		bool _button_setting_hovered     { false };
		bool _button_reset_graph_hovered { false };
		bool _button_g_top_all_hovered   { false };
		bool _button_g_top_idle_hovered  { false };
		bool _button_ec_hovered          { false };
		bool _button_sc_hovered          { false };

		bool _trace_top_most             { false };
		bool _trace_top_no_idle          { false };

		bool _show_second_time             { false };

		enum { THREAD, COMPONENT } _sort { THREAD };

		static constexpr unsigned PD_SCROLL_DOWN = ~0U / DIV;
		static constexpr unsigned PD_SCROLL_UP   = (~0U - DIV) / DIV;
		static constexpr unsigned MAX_SUBJECT_ID =  PD_SCROLL_UP;

		bool _same(Location a, Location b) {
			return a.xpos() == b.xpos() && a.ypos() == b.ypos(); }

	public:

		void init(Genode::Affinity::Space const &space)
		{
			Genode::memset(_cpu_show, 1, sizeof(_cpu_show));

			_button_cpus.max = Genode::max(8u, space.total() / 2);
		}

		void read_config(Genode::Node const &);
		void write_config(Genode::Generator &) const;

		bool trace_top_most() const { return _trace_top_most || _trace_top_no_idle; }
		bool tracked_threads() const { return _tracked_threads; }

		void period(unsigned period_trace, unsigned period_view)
		{
			_button_trace_period.set(period_trace);
			_button_view_period.set(period_view);
		}

		unsigned period_trace() const { return _button_trace_period.value(); }
		unsigned period_view() const { return _button_view_period.value(); }

		void _destroy_thread_object(Top::Thread               &thread,
		                            Genode::Trace::Connection &trace,
		                            Genode::Allocator         &alloc)
		{
			trace.free(thread.id());

			_threads.remove(&thread);

			Top::Thread::destroy(thread, alloc, _num_pds);

			_num_subjects --;
		}

		void flush(Genode::Trace::Connection &trace, Genode::Allocator &alloc)
		{
			while (Top::Thread * const thread = _threads.first()) {
				_destroy_thread_object(*thread, trace, alloc);
			}

			/* clear old calculations */
			Genode::memset(total_first , 0, sizeof(total_first));
			Genode::memset(total_second, 0, sizeof(total_second));
			Genode::memset(total_idle  , 0, sizeof(total_idle));
			Genode::memset(load        , 0, sizeof(load));
		}

		bool update(Genode::Trace::Connection &trace,
		            Genode::Allocator &alloc, SORT_TIME const sort,
		            Genode::Constructible<Top::Storage> &storage)
		{
			constexpr Genode::uint32_t const INVALID_ID = ~0U;

			/* quirk for platforms where timestamp() does not work */
			{
				auto const timestamp = Genode::Trace::timestamp();
				if (timestamp <= _timestamp)
					_timestamp += 1;
				else
					_timestamp = timestamp;
			}

			/* XXX - right place ?! */
			if (storage.constructed())
				storage->write(Type_a { INVALID_ID /* invalid - means data start */,
				                        Genode::Trace::Execution_time { _timestamp, 0 },
				                        0, 0 });

			auto res = trace.for_each_subject_info([&](Genode::Trace::Subject_id const &id,
			                                           Genode::Trace::Subject_info const &info)
			{
				Top::Thread * thread = _lookup_thread(id);
				if (!thread) {
					if (!_components.exists(info.session_label())) {
						new (alloc) Top::Component(_components, info.session_label());
						_num_pds ++;
					}

					_components.with_element(info.session_label(),
					                         [&](Top::Component &c) {

						thread = new (alloc) Top::Thread(c, id, info);
						_threads.insert(thread);
						_num_subjects ++;

						/* XXX - right place ?! */
						if (storage.constructed())
							storage->write(Type_b { thread->id(),
							                        thread->session_label(),
							                        thread->thread_name(),
							                        unsigned(thread->affinity().xpos()),
							                        unsigned(thread->affinity().ypos()) });
					}, [&](){ });
				}

				if (!thread) {
					Genode::error("thread of component ", info.session_label(),
					              " could not be added");
					return;
				}

				thread->update(info);

				/* remove dead threads which did not run in the last period */
				if (thread->state() == Genode::Trace::Subject_info::DEAD &&
				    !thread->recent_ec_time() && !thread->recent_sc_time())
				{
					_destroy_thread_object(*thread, trace, alloc);
				}
			});

			/* clear old calculations */
			Genode::memset(total_first , 0, sizeof(total_first));
			Genode::memset(total_second, 0, sizeof(total_second));
			Genode::memset(total_idle  , 0, sizeof(total_idle));
			Genode::memset(load        , 0, sizeof(load));

			for_each_thread([&] (Top::Thread &thread) {
				/* collect highest execution time per CPU */
				unsigned const x = thread.affinity().xpos();
				unsigned const y = thread.affinity().ypos();
				if (x >= MAX_CPUS_X || y >= MAX_CPUS_Y) {
					Genode::error("cpu ", thread.affinity().xpos(), ".",
					              thread.affinity().ypos(), " is outside "
					              "supported range ",
					              (int)MAX_CPUS_X, ".", (int)MAX_CPUS_Y);
					return;
				}

				total_first [x][y] += thread.recent_time(sort == EC_TIME);
				total_second[x][y] += thread.recent_time(sort == SC_TIME);

				if (thread.thread_name() == "idle") {
					total_idle[x][y] = thread.recent_time(sort == EC_TIME);

					Location const location(x, y);

					if (!cpu_online(location))
						cpu_online(location) = true;
				}

				enum { NONE = ~0U };
				unsigned replace = NONE;

				unsigned const max = _cpu_number(thread.affinity()).value();
				for (unsigned i = 0; i < max; i++) {
					if (load[x][y][i])
						continue;

					replace = i;
					break;
				}

				if (replace != NONE) {
					load[x][y][replace] = &thread;
					return;
				}

				for (unsigned i = 0; i < max; i++) {
					if (thread.recent_time(sort == EC_TIME)
					    <= load[x][y][i]->recent_time(sort == EC_TIME))
						continue;

					if (replace == NONE) {
						replace = i;
						continue;
					}
					if (load[x][y][replace]->recent_time(sort == EC_TIME)
					    > load[x][y][i]->recent_time(sort == EC_TIME))
						replace = i;
				}

				if (replace != NONE)
					load[x][y][replace] = &thread;
			});

			/* sort */
			for (int x = 0; x < MAX_CPUS_X; x++) {
				for (int y = 0; y < MAX_CPUS_Y; y++) {
					Location const loc {x, y};
					unsigned const max = _cpu_number(loc).value();

					for (unsigned k = 0; k < max;) {
						if (!load[x][y][k])
							break;

						unsigned i = k;
						for (unsigned j = i; j < max; j++) {
							if (!load[x][y][j])
								break;

							if (load[x][y][i]->recent_time(sort == EC_TIME)
							    < load[x][y][j]->recent_time(sort == EC_TIME)) {

								Top::Thread const * tmp = load[x][y][j];
								load[x][y][j] = load[x][y][i];
								load[x][y][i] = tmp;

								i++;
								if (i >= max || !load[x][y][i])
									break;
							}
						}
						if (i == k)
							k++;
					}
				}
			}

			if (storage.constructed()) {
				for_each_thread([&] (Top::Thread &thread) {
					if (!thread.recent_ec_time() && !thread.recent_sc_time())
						return;

					Genode::uint64_t const tf = total_cpu_first(thread.affinity());
					Genode::uint64_t const ts = total_cpu_second(thread.affinity());

					Genode::uint16_t fraq_ec = 0;
					Genode::uint16_t fraq_sc = 0;

					if (sort == EC_TIME)
						fraq_ec = Genode::uint16_t(tf ? thread.recent_ec_time() * 10000 / tf : 0ull);
					else
						fraq_ec = Genode::uint16_t(ts ? thread.recent_ec_time() * 10000 / ts : 0ull);

					if (sort == SC_TIME)
						fraq_sc = Genode::uint16_t(tf ? thread.recent_sc_time() * 10000 / tf : 0ull);
					else
						fraq_sc = Genode::uint16_t(ts ? thread.recent_sc_time() * 10000 / ts : 0ull);

					storage->write(Type_a { thread.id(),
					                        thread.execution_time(),
					                        fraq_ec,
					                        fraq_sc });
				});

				storage->write(Type_c{INVALID_ID});
				storage->write(Type_c{Genode::uint32_t(_timestamp)});
				storage->write(Type_c{Genode::uint32_t(_timestamp >> 32)});
//				Genode::log("---- ", Genode::Hex(_timestamp));

				if (_trace_top_most || _trace_top_no_idle) {
					for_each([&] (Top::Thread const &thread, uint64_t const) {
						if (!_graph_top_most(thread.affinity())) return;

						if (!_graph_top_most_no_idle(thread.affinity()) ||
							!(thread.thread_name() == "idle"))
							storage->write(Type_c{thread.id()});
					});
				} else {
					for_each_thread([&] (Top::Thread &thread) {
						if (thread.track(sort == EC_TIME)) {
							storage->write(Type_c{thread.id()});
						}
						if (thread.track(sort == SC_TIME)) {
							storage->write(Type_c{thread.id()});
						}
					});
				}
			}

			/* hacky XXX */
			_show_second_time =  total_first[0][0] && total_second[0][0] &&
			                    (total_first[0][0] != total_second[0][0]);

			/* move it before and don't evaluate results ? XXX */
			return res.count < res.limit;
		}

		template <typename T>
		Genode::String<8> string(T percent, T rest) {
			return Genode::String<8> (percent < 10 ? "  " : (percent < 100 ? " " : ""),
			                          percent, ".", rest < 10 ? "0" : "", rest, "%");
		}

		void for_each(auto const &fn) const
		{
			for (int x = 0; x < MAX_CPUS_X; x++) {
				for (int y = 0; y < MAX_CPUS_Y; y++) {
					if (!_cpu_online[x][y]) continue;

					Location const loc {x, y};
					unsigned const max = _cpu_number(loc).value();

					for (unsigned i = 0; i < max; i++) {
						if (!load[x][y][i]) continue;

						/*
						 * total[x][y] may be 0 in case we remotely request too
						 * quick and no change happened between two requests,
						 * e.g. idle thread sleeping several seconds or
						 * thread with long quantum did not get de-scheduled ...
						 */
						fn(*load[x][y][i], total_first[x][y]);
					}
				}
			}
		}

		void for_each_online_cpu(auto const &fn) const {
			for (unsigned x = 0; x < MAX_CPUS_X; x++) {
				for (unsigned y = 0; y < MAX_CPUS_Y; y++) {
					if (_cpu_online[x][y])
						fn(Location(x, y));
				}
			}
		}

		void top(SORT_TIME const sort)
		{
			for_each([&] (Top::Thread const &thread, unsigned long long const total) {
				auto const percent = total ? thread.recent_time(sort == EC_TIME) * 100 / total : 0ull;
				auto const rest    = total ? thread.recent_time(sort == EC_TIME) * 10000 / total - (percent * 100) : 0ull;

				Genode::log("cpu=", thread.affinity().xpos(), ".",
				                    thread.affinity().ypos(), " ",
				                    string(percent, rest), " ",
				                    "thread='", thread.thread_name(), "'",
				                    " label='", thread.session_label(), "'");
			});

			if (load[0][0][0] && load[0][0][0]->recent_time(sort == EC_TIME))
				Genode::log("");
		}

		void buttons(Genode::Generator &g, Button_state &state)
		{
			g.attribute("name", Genode::String<12>("cpusbox", state.current));

			if (state.current > 0) {
				g.node("button", [&] () {
					g.attribute("name", "<");
					if (state.prev)
						g.attribute("hovered","yes");
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted("..."); });
					});
				});
			} else
				state.prev = false;

			unsigned i = 0;

			for_each_online_cpu([&] (Location const &loc) {
				i++;
				if (i <= state.current) return;
				if (i > state.current + state.max) return;

				Genode::String<12> cpu_name("cpu", loc.xpos(), ".", loc.ypos());

				g.node("hbox", [&] () {
					g.attribute("name", Genode::String<14>("cc-", cpu_name));

					g.node("button", [&] () {
						g.attribute("name", cpu_name);

						if (_sort == THREAD && cpu_show(loc))
							g.attribute("selected","yes");
						if (_sort == COMPONENT && _same(_last_cpu, loc))
							g.attribute("selected","yes");

						if (state.hovered && _same(_button_cpu, loc))
							g.attribute("hovered","yes");

						g.node("label", [&] () {
							g.node("text", [&] { g.append_quoted(cpu_name); });
						});
					});

					if (_sort == THREAD) {
						g.node("button", [&] () {
							g.attribute("name", Genode::String<18>("most", cpu_name));
							g.node("label", [&] () {
								g.node("text", [&] { g.append_quoted("topmost"); });
							});
							if (_graph_top_most(loc))
								g.attribute("selected","yes");
							if (_button_g_top_all_hovered && _same(_button_top_most, loc))
								g.attribute("hovered","yes");
						});
						g.node("button", [&] () {
							g.attribute("name", Genode::String<18>("idle", cpu_name));
							g.node("label", [&] () {
								g.node("text", [&] { g.append_quoted("w/o idle"); });
							});
							if (_graph_top_most_no_idle(loc))
								g.attribute("selected","yes");
							if (_button_g_top_idle_hovered && _same(_button_top_most_no_idle, loc))
								g.attribute("hovered","yes");
						});


						/* XXX cpu_name too long with hub- stuff */
						Genode::String<12> const cpu(loc.xpos(), ".", loc.ypos());
						hub(g, _cpu_number(loc), cpu.string());
					}
				});
			});

			if (i > state.current + state.max) {
				g.node("button", [&] () {
					g.attribute("name", ">");
					if (state.next)
						g.attribute("hovered", "yes");
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted("..."); });
					});
				});
			} else {
				state.last = i;
				state.next = false;
			}
		}

		void hub(Genode::Generator &g, auto &hub, char const *name)
		{
			hub.for_each([&](Button_state &state, unsigned pos) {
				g.attribute("name", Genode::String<20>("hub-", name, "-", pos));

				Genode::String<12> number(state.current);

				g.node("button", [&] () {
					g.attribute("name", Genode::String<20>("hub-", name, "-", pos));
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(number); });
					});
				});
			});
		}

		void numbers(Genode::Generator &g, Button_state &state)
		{
			if (_sort != COMPONENT) return;

			g.attribute("name", Genode::String<12>("numbersbox", state.current));

			if (state.current > state.first) {
				g.node("button", [&] () {
					g.attribute("name", "number<");
					if (state.prev)
						g.attribute("hovered","yes");
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted("..."); });
					});
				});
			} else
				state.prev = false;

			unsigned i = 0;

			for (i = state.current; i <= state.last && i < state.current + state.max; i++) {
				Genode::String<12> number(i);

				g.node("button", [&] () {

					if (_config_pds_per_cpu == i)
						g.attribute("selected","yes");

					g.attribute("name", Genode::String<18>("number", number));
					if (state.hovered && _button_number == i)
						g.attribute("hovered","yes");

					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(number); });
					});
				});
			}

			if (i <= state.last) {
				g.node("button", [&] () {
					g.attribute("name", "number>");
					if (state.next)
						g.attribute("hovered","yes");
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted("..."); });
					});
				});
			}
		}

		bool hover_detailed(SORT_TIME const &sort_time)
		{
			if (_detailed_view_back) {
				_detailed_view.id   = 0;
				_button_cpus.reset();
				_button_numbers.reset();
				_detailed_view_back = false;

				return true;
			}

			if (!_hovered_subject.id)
				return false;

			Top::Thread * thread = _lookup_thread(_hovered_subject);
			if (!thread)
				return false;

			if (_hovered_sub_id == CHECKBOX_ID_FIRST) {
				if (thread->track(sort_time == EC_TIME))
					_tracked_threads --;
				else
					_tracked_threads ++;

				thread->track(sort_time == EC_TIME, !thread->track(sort_time == EC_TIME));

				return true;
			}

			if (_hovered_sub_id == CHECKBOX_ID_SECOND) {
				if (thread->track(sort_time == SC_TIME))
					_tracked_threads --;
				else
					_tracked_threads ++;

				thread->track(sort_time == SC_TIME, !thread->track(sort_time == SC_TIME));

				return true;
			}

			return false;
		}

		struct hover_result { bool report_menu; bool flush_config; };

		hover_result hover(Genode::String<12> const button,
		                   Genode::String<12> const click,
		                   bool const click_valid,
		                   Genode::Trace::Subject_id const id,
		                   unsigned sub_id,
		                   SORT_TIME &sort_time)
		{
			if (click_valid) {

				if (click == "wheel_up" || click == "wheel_down") {
					if (_detailed_view.id) return { false, false };

					if (_button_cpus.hovered) {
						_button_cpus.prev = (click == "wheel_up");
						_button_cpus.next = (click == "wheel_down") && (_button_cpus.current + _button_cpus.max < _button_cpus.last);

						return { _button_cpus.advance(), false };
					}

					if (_button_numbers.hovered) {
						_button_numbers.prev = (click == "wheel_up");
						_button_numbers.next = (click == "wheel_down") && (_button_numbers.current + _button_numbers.max < _button_numbers.last);
						return { _button_numbers.advance(), false };
					}

					if (_sort == COMPONENT && _hovered_subject.id) {
						_pd_scroll.prev = (click == "wheel_up");
						_pd_scroll.next = (click == "wheel_down") && (_pd_scroll.current + _config_pds_per_cpu <= _pd_scroll.last);
						return { _pd_scroll.advance(), false };
					}

					return { false, false };
				}

				if (_detailed_view.id)
					return { hover_detailed(sort_time), false };

				bool report_update = false;
				bool flush_config = false;

				if (_button_cpus.hovered) {
					if (_sort == THREAD) {
						cpu_show(_button_cpu) = !cpu_show(_button_cpu);
						flush_config = true;
					}
					_last_cpu = _button_cpu;
					report_update = true;
				}
				if (_hovered_subject.id) {
					_detailed_view = _hovered_subject;
					report_update = true;
				}
				if (_button_numbers.hovered) {
					if (_sort == COMPONENT)
						_config_pds_per_cpu = _button_number;
					report_update = true;
				}
				if (_button_reset_graph_hovered) {
					for_each_thread([&] (Top::Thread &thread) {
						if (thread.track_ec()) thread.track_ec(false);
						if (thread.track_sc()) thread.track_sc(false);
					});
					for (unsigned x = 0; x < MAX_CPUS_X; x++) {
						for (unsigned y = 0; y < MAX_CPUS_Y; y++) {
							_cpu_graph_top[x][y] = false;
							_cpu_graph_top_no_idle[x][y] = false;
						}
					}
					_tracked_threads = 0;
					_trace_top_most = false;
					_trace_top_no_idle = false;
					report_update = true;
				}
				if (_button_g_top_all_hovered) {
					_graph_top_most(_button_top_most) = !_graph_top_most(_button_top_most);
					_trace_top_most = _graph_top_most(_button_top_most);

					if (!_trace_top_most) {
						_graph_top_most_no_idle(_button_top_most) = false;
						_trace_top_no_idle = false;

						for (unsigned x = 0; x < MAX_CPUS_X; x++) {
							for (unsigned y = 0; y < MAX_CPUS_Y; y++) {
								if (_cpu_graph_top[x][y]) {
									_trace_top_most = true;
								}
								if (_cpu_graph_top_no_idle[x][y]) {
									_trace_top_no_idle = true;
								}
								if (_trace_top_most && _trace_top_no_idle) {
									x = MAX_CPUS_X;
									y = MAX_CPUS_Y;
								}
							}
						}
					}

					report_update = true;
				}
				if (_button_g_top_idle_hovered) {
					_graph_top_most_no_idle(_button_top_most_no_idle) = !_graph_top_most_no_idle(_button_top_most_no_idle);
					_trace_top_no_idle = _graph_top_most_no_idle(_button_top_most_no_idle);
					if (_trace_top_no_idle) {
						_graph_top_most(_button_top_most_no_idle) = true;
						_trace_top_most = true;
					} else {
						for (unsigned x = 0; x < MAX_CPUS_X; x++) {
							for (unsigned y = 0; y < MAX_CPUS_Y; y++) {
								if (_cpu_graph_top_no_idle[x][y]) {
									_trace_top_no_idle = true;
									x = MAX_CPUS_X;
									y = MAX_CPUS_Y;
								}
							}
						}
					}

					report_update = true;
				}
				if (_button_setting_hovered) {
					_button_setting = !_button_setting;
					report_update = true;
				}
				if (_button_enable_view_hovered) {
					_enable_view = !_enable_view;
					flush_config  = true;
					report_update = true;
				}
				if (_button_thread_hovered) {
					if (!flush_config)
						flush_config = _sort != THREAD;
					_sort = THREAD;
					report_update = true;
				}
				if (_button_component_hovered) {
					if (!flush_config)
						flush_config = _sort != COMPONENT;
					_sort = COMPONENT;
					report_update = true;
				}
				if (_button_ec_hovered) {
					sort_time = EC_TIME;
					report_update = true;
				}
				if (_button_sc_hovered) {
					sort_time = SC_TIME;
					report_update = true;
				}
				if (click == "left" && _button_trace_period.update_inc())
					report_update = true;
				if (click == "right" && _button_trace_period.update_dec())
					report_update = true;
				if (click == "left" && _button_view_period.update_inc())
					report_update = true;
				if (click == "right" && _button_view_period.update_dec())
					report_update = true;
				if (click == "left" && _cpu_number(_button_cpu_num).update_inc()) {
					report_update = true;
					flush_config  = true;
				}
				if (click == "right" && _cpu_number(_button_cpu_num).update_dec()) {
					report_update = true;
					flush_config  = true;
				}

				report_update = report_update || _button_cpus.advance();
				report_update = report_update || _button_numbers.advance();
				report_update = report_update || _pd_scroll.advance();

				return { report_update, flush_config };
			}

			if (id.id == PD_SCROLL_DOWN || id.id == PD_SCROLL_UP) {
				_pd_scroll.hovered = false;
				_pd_scroll.prev = id.id == PD_SCROLL_UP;
				_pd_scroll.next = id.id == PD_SCROLL_DOWN;
				_hovered_subject = { };
				_hovered_sub_id  = 0;
			} else {
				_pd_scroll.reset();
				_hovered_subject = id;
				_hovered_sub_id  = sub_id;
			}

			/* XXX more to avoid redraws when with mouse over it and not to miss some redraws on leaving */
			bool button_hovered_before = false;

			_button_cpus.reset();
			_button_numbers.reset();
			_button_trace_period.reset();
			_button_view_period.reset();
			_cpu_number(_button_cpu_num).reset();

			button_hovered_before |= _button_setting_hovered;
			bool button_setting_hovered_before = _button_setting_hovered;
			_button_setting_hovered = false;

			_button_reset_graph_hovered = false;
			_button_g_top_all_hovered = false;
			_button_g_top_idle_hovered = false;
			_button_thread_hovered = false;
			_button_enable_view_hovered = false;
			_button_component_hovered = false;
			_button_ec_hovered = false;
			_button_sc_hovered = false;

			_detailed_view_back = false;

			if (button == "")
				return { button_hovered_before, false };

			if (button == "settings") {
				_button_setting_hovered = true;
				return { !button_setting_hovered_before, false };
			}
			if (button == "graph_reset") {
				_button_reset_graph_hovered = true;
				return { true, false };
			}
			if (button == "top_idle") {
				_button_g_top_idle_hovered = true;
				return { true, false };
			}
			if (button == "enable_view") {
				_button_enable_view_hovered = true;
				return { true, false };
			}
			if (button == "threads") {
				_button_thread_hovered = true;
				return { true, false };
			}
			if (button == "components") {
				_button_component_hovered = true;
				return { true, false };
			}
			if (button == "ec") {
				_button_ec_hovered = true;
				return { true, false };
			}
			if (button == "sc") {
				_button_sc_hovered = true;
				return { true, false };
			}

			if (Genode::String<4>(button) == "hub") {
				Genode::String<9> hub_view("hub-view");
				if (Genode::String<9>(button) == hub_view) {
					_button_view_period.for_each([&](Button_state &state, unsigned pos) {
						Genode::String<12> pos_name { hub_view, "-", pos };
						if (Genode::String<12>(button) == pos_name) {
							state.hovered = true;
						}
					});
				} else {
					Genode::String<10> hub_trace("hub-trace");
					if (Genode::String<10>(button) == hub_trace) {
						_button_trace_period.for_each([&](Button_state &state, unsigned pos) {
							Genode::String<12> pos_name { hub_trace, "-", pos };
							if (Genode::String<12>(button) == pos_name) {
								state.hovered = true;
							}
						});
					} else {
						for_each_online_cpu([&] (Location const &loc) {
							_cpu_number(loc).for_each([&](Button_state &state, unsigned pos) {
								Genode::String<18> cpu("hub-", loc.xpos(), ".", loc.ypos(), "-", pos);
								if (button == cpu) {
									state.hovered = true;
									_button_cpu_num = loc;
								}
							});
						});
					}
				}
			}

			if (Genode::String<7>(button) == "number") {
				if (button == "number<")
					_button_numbers.prev = true;
				else if (button == "number>")
					_button_numbers.next = true;
				else {
					for (unsigned i = _button_numbers.first;
					     i <= _button_numbers.last; i++)
					{
						if (Genode::String<10>("number", i) == button) {
							_button_numbers.hovered = true;
							_button_number = i;
							break;
						}
					}
				}
				return { _button_numbers.active(), false };
			} else
			if (Genode::String<5>(button) == "most") {
				for_each_online_cpu([&] (Location const &loc) {
					Genode::String<18> cpu_name("mostcpu", loc.xpos(), ".", loc.ypos());
					if (button == cpu_name) {
						_button_g_top_all_hovered = true;
						_button_top_most = loc;
					}
				});
			} else
			if (Genode::String<5>(button) == "idle") {
				for_each_online_cpu([&] (Location const &loc) {
					Genode::String<18> cpu_name("idlecpu", loc.xpos(), ".", loc.ypos());
					if (button == cpu_name) {
						_button_g_top_idle_hovered = true;
						_button_top_most_no_idle = loc;
					}
				});
			}


			if (button == "<") {
				if (_detailed_view.id)
					_detailed_view_back = true;
				else
					_button_cpus.prev = true;
			} else if (button == ">") {
				_button_cpus.next    = true;
			} else {
				for_each_online_cpu([&] (Location const &loc) {
					Genode::String<12> cpu_name("cpu", loc.xpos(), ".", loc.ypos());
					if (button == cpu_name) {
						_button_cpus.hovered = true;
						_button_cpu = loc;
					}
				});
			}
			return { button_hovered_before ||
			         _button_cpus.active() ||
			         _button_numbers.active(),
			         false };
		}

		void top(Genode::Generator &, SORT_TIME const, bool const,
		         Genode::String<12> const &, Genode::String<12> const &);

		void graph(Genode::Generator &g, SORT_TIME const sort)
		{
			if (_trace_top_most || _trace_top_no_idle) {
				for_each([&] (Top::Thread const &thread, uint64_t t) {
					if (!_graph_top_most(thread.affinity())) return;
					if (_graph_top_most_no_idle(thread.affinity()) &&
					    (thread.thread_name() == "idle")) return;

					g.node("entry", [&]{
						int const xpos = thread.affinity().xpos();
						int const ypos = thread.affinity().ypos();

						Genode::String<12> cpu_name(xpos, ".", ypos, _show_second_time ? (sort == EC_TIME ? " ec" : " sc") : "");
						g.attribute("cpu", cpu_name);
						g.attribute("label", thread.session_label());
						g.attribute("thread", thread.thread_name());
						g.attribute("id", thread.id().id);
						g.attribute("tsc", _timestamp);

						g.attribute("value", t ? (thread.recent_time(sort == EC_TIME) * 10000 / t) : 0 );
					});
				});
				return;
			}

			for_each_thread([&] (Top::Thread &thread) {
				if (thread.track_ec()) {
					g.node("entry", [&]{
						int const xpos = thread.affinity().xpos();
						int const ypos = thread.affinity().ypos();

						Genode::String<12> cpu_name(xpos, ".", ypos, _show_second_time ? " ec" : "");
						g.attribute("cpu",    cpu_name);
						g.attribute("label",  thread.session_label());
						g.attribute("thread", thread.thread_name());
						g.attribute("id",     thread.id().id);
						g.attribute("tsc",    _timestamp);

						Genode::uint64_t t = total_cpu_first(thread.affinity());

						g.attribute("value", t ? (thread.recent_time(true /* EC */) * 10000 / t) : 0 );
					});
				}
				if (thread.track_sc()) {
					g.node("entry", [&]{
						int const xpos = thread.affinity().xpos();
						int const ypos = thread.affinity().ypos();

						Genode::String<12> cpu_name(xpos, ".", ypos, _show_second_time ? " sc" : "");
						g.attribute("cpu",    cpu_name);
						g.attribute("label",  thread.session_label());
						g.attribute("thread", thread.thread_name());
						/* HACK, graph can't handle same ID for SC and EC atm */
						g.attribute("id",     ~0U - thread.id().id);
						g.attribute("tsc",    _timestamp);

						Genode::uint64_t t = total_cpu_second(thread.affinity());

						g.attribute("value", t ? (thread.recent_time(false /* SC */) * 10000 / t) : 0 );
					});
				}
			});
		}

		void detail_view_tool(Genode::Generator &g,
		                      Top::Thread const &entry,
		                      Genode::String<16> name,
		                      unsigned const id,
		                      auto const &fn,
		                      Genode::String<12> align = "left")
		{
			g.node("vbox", [&] () {
				g.attribute("name", Genode::String<20>(name, id));

				g.node("hbox", [&] () {
					g.attribute("name", name);
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(name); });
						g.attribute("color", "#ffffff");
						g.attribute("align", align);
					});
				});

				entry.for_each_thread_of_pd([&] (Top::Thread &thread) {
					bool left = true;
					Genode::String<64> text(fn(thread, left));

					g.node("hbox", [&] () {
						g.attribute("name", thread.id().id * DIV + id);
						g.attribute("west", "yes");
						g.node("label", [&] () {
							g.node("text", [&] { g.append_quoted(text); });
							g.attribute("color", "#ffffff");
							g.attribute("align", left ? "left" : "right");
						});
					});
				});
			});
		}

		void detail_view_tool_track(Genode::Generator       &g,
		                            Top::Thread           const &thread,
		                            unsigned              const  id,
		                            SORT_TIME             const  sort,
		                            bool                  const  first)
		{
			Genode::String<4> ec_sc(_show_second_time ? "EC" : "");
			bool ec = true;

			if ( first && sort == SC_TIME) { ec_sc = "SC"; ec = false; }
			if (!first && sort == EC_TIME) { ec_sc = "SC"; ec = false; }

			g.node("vbox", [&] () {
				g.attribute("name", Genode::String<16>("track_", ec_sc));

				g.node("button", [&] () {
					g.attribute("name", "inv");
					g.attribute("style", "invisible");

					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(ec_sc); });
						g.attribute("color", "#ffffff");
						g.attribute("align", "left");
					});
				});

				thread.for_each_thread_of_pd([&] (Top::Thread &check) {
					g.node("button", [&] () {
						g.attribute("name", check.id().id * DIV + id);
						g.attribute("style", "checkbox");
						if (check.track(ec))
							g.attribute("selected","yes");
						g.node("hbox", [&] () { });
					});
				});
			});
		}

		void detail_view(Genode::Generator         &g,
		                 Top::Thread         const &thread,
		                 SORT_TIME           const  sort,
		                 Genode::String<12>  const &name_prio,
		                 Genode::String<12>  const &name_quantum)
		{
			g.node("vbox", [&] () {
				g.attribute("name", "detail_view");

				g.node("hbox", [&] () {
					g.attribute("name", "header");
					g.node("button", [&] () {
						g.attribute("name", "<");
						g.node("label", [&] () {
							g.node("text", [&] { g.append_quoted("<"); });
						});
					});
					g.node("float", [&] () {
						g.attribute("name", thread.id().id * DIV);
						g.node("label", [&] () {
							g.node("text", [&] { g.append_quoted(thread.session_label()); });
							g.attribute("color", "#ffffff");
							g.attribute("align", "left");
						});
					});
				});

				g.node("hbox", [&] () {
					g.attribute("name", thread.id().id * DIV + 1);
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(Genode::String<64>("kernel memory: X/Y 4k pages")); });
						g.attribute("color", "#ffffff");
						g.attribute("align", "left");
					});
				});

				g.node("hbox", [&] () {
					g.attribute("name", "list");

					detail_view_tool(g, thread, Genode::String<16>("cpu "), 2,
						[&] (Top::Thread const &e, bool &) {
							return Genode::String<8>(e.affinity().xpos(), ".", e.affinity().ypos(), " ");
						});

					detail_view_tool(g, thread, Genode::String<16>("load"), 3,
						[&] (Top::Thread const &e, bool &left) {
							unsigned long long t = total_first[e.affinity().xpos()][e.affinity().ypos()];
							auto const percent = t ? (e.recent_time(sort == EC_TIME) * 100   / t) : 0ull;
							auto const rest    = t ? (e.recent_time(sort == EC_TIME) * 10000 / t - (percent * 100)) : 0ull;

							left = false;
							return Genode::String<9>(string(percent, rest), " ");
						}, "right");

					detail_view_tool_track(g, thread, CHECKBOX_ID_FIRST,
					                       sort, true);

					detail_view_tool(g, thread, Genode::String<16>("thread "), 4,
						[&] (Top::Thread const &e, bool &) {
							return Genode::String<64>(e.thread_name(), " ");
						});

					detail_view_tool(g, thread, Genode::String<16>(name_prio, " "), 5,
						[&] (Top::Thread const &e, bool &) {
							return Genode::String<4>(e.execution_time().priority);
						});

					detail_view_tool(g, thread, Genode::String<16>(name_quantum, " "), 6,
						[&] (Top::Thread const &e, bool &) {
							return Genode::String<10>(e.execution_time().quantum, "us");
						});

					if (_show_second_time) {
						detail_view_tool(g, thread, Genode::String<16>("load"), 8,
							[&] (Top::Thread const &e, bool &left) {
								unsigned long long t = total_second[e.affinity().xpos()][e.affinity().ypos()];
								auto const percent = t ? (e.recent_time(sort == SC_TIME) * 100   / t) : 0ull;
								auto const rest    = t ? (e.recent_time(sort == SC_TIME) * 10000 / t - (percent * 100)) : 0ull;

								left = false;
								return Genode::String<9>(string(percent, rest), " ");
							}, "right");

						detail_view_tool_track(g, thread, CHECKBOX_ID_SECOND,
						                       sort, false);
					}
				});
			});
		}

		template <typename FN>
		void list_view_tool(Genode::Generator &g,
		                    Genode::String<16> name,
		                    unsigned id,
		                    FN const &fn)
		{
			g.node("vbox", [&] () {
				g.attribute("name", Genode::String<20>(name, id));

				g.node("hbox", [&] () {
					g.attribute("name", name);
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(name); });
						g.attribute("color", "#ffffff");
						g.attribute("align", "left");
					});
				});

				for_each([&] (Top::Thread const &thread, unsigned long long const) {

					if (!cpu_show(thread.affinity()))
						return;

					bool left = true;
					Genode::String<64> text(fn(thread, left));

					g.node("hbox", [&] () {
						g.attribute("name", thread.id().id * DIV + id);
						g.node("label", [&] () {
							g.node("text", [&] { g.append_quoted(text); });
							g.attribute("color", "#ffffff");
							g.attribute("align", left ? "left" : "right");
						});
					});
				});
			});
		}

		void list_view_bar(Genode::Generator &g,
		                   Top::Thread const &thread,
		                   unsigned long long percent, unsigned long long rest)
		{
			g.node("float", [&] () {
				g.attribute("name", thread.id().id * DIV);
				g.attribute("west", "yes");
				g.node("hbox", [&] () {
					g.attribute("name", thread.id().id * DIV + 1);
					g.node("float", [&] () {
						g.attribute("name", thread.id().id * DIV + 2);
						g.attribute("west", "yes");
						g.node("bar", [&] () {
							if (thread.session_label("kernel")) {
								g.attribute("color", "#00ff000");
								g.attribute("textcolor", "#f000f0");
							} else {
								g.attribute("color", "#ff0000");
								g.attribute("textcolor", "#ffffff");
							}

							g.attribute("percent", percent);
							g.attribute("width", 128);
							g.node("text", [&] { g.append_quoted(string(percent, rest)); });
						});
					});
				});
			});
		}

		void short_view(Genode::Generator &, SORT_TIME const);

		void list_view(Genode::Generator &g, SORT_TIME const sort)
		{
			g.node("vbox", [&] () {
				g.attribute("name", "list_view_load");

				g.node("hbox", [&] () {
					Genode::String<16> name("load ", _show_second_time ? (sort == EC_TIME ? "EC " : "SC ") : "");
					g.attribute("name", "load");
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(name); });
						g.attribute("color", "#ffffff");
						g.attribute("align", "left");
					});
				});

				for_each([&] (Top::Thread const &thread, unsigned long long const total) {

					if (!cpu_show(thread.affinity()))
						return;

					Genode::uint64_t time = thread.recent_time(sort == EC_TIME);
					auto percent = total ? time * 100 / total : 0ull;
					auto rest    = total ? time * 10000 / total - (percent * 100) : 0ull;

					list_view_bar(g, thread, percent, rest);
				});
			});

			if (_show_second_time)
			{
				list_view_tool(g, Genode::String<16>("load ", sort == SC_TIME ? "ec " : "sc "), 2,
					[&] (Top::Thread const &e, bool &left) {
						left = false;

						Genode::uint64_t time = e.recent_time(sort == SC_TIME);
						Genode::uint64_t total = total_second[e.affinity().xpos()][e.affinity().ypos()];
						auto percent = total ? (time * 100 / total) : 0ull;
						auto rest    = total ? (time * 10000 / total - (percent * 100)) : 0ull;

						return string(percent, rest);
					});
			}

			list_view_tool(g, Genode::String<16>("cpu "), 3,
				[&] (Top::Thread const &e, bool &left) {
					left = false;
					return Genode::String<8>(e.affinity().xpos(), ".", e.affinity().ypos(), " ");
				});

			list_view_tool(g, Genode::String<16>("thread "), 4,
				[&] (Top::Thread const &e, bool &) {
					return Genode::String<64>(e.thread_name(), " ");
				});

			list_view_tool(g, Genode::String<16>("label"), 5,
				[&] (Top::Thread const &e, bool &) {
					return Genode::String<64>(e.session_label());
				});
		}

		void list_view_pd(Genode::Generator &g, SORT_TIME const sort)
		{
			Genode::String<18> label("load cpu", _last_cpu.xpos(), ".", _last_cpu.ypos(), _show_second_time ? (sort == EC_TIME ? " EC " : " SC ") : " ");
			list_view_pd_tool(g, "list_view_load", "load", label.string(),
			                  [&] (Top::Component const &, Top::Thread const &thread)
			{
				Genode::uint64_t   time   = 0;

				thread.for_each_thread_of_pd([&] (Top::Thread &t) {
					if (_same(t.affinity(), _last_cpu))
						time += t.recent_time(sort == EC_TIME);
				});

				Genode::uint64_t max = total_first[_last_cpu.xpos()][_last_cpu.ypos()];

				auto percent = max ? (time * 100 / max) : 0ull;
				auto rest    = max ? (time * 10000 / max - (percent * 100)) : 0ull;

				list_view_bar(g, thread, percent, rest);
			});

			if (_show_second_time) {

				Genode::String<18> label("load cpu", _last_cpu.xpos(), ".", _last_cpu.ypos(), " ", sort == SC_TIME ? "ec " : "sc ");
				list_view_pd_tool(g, "list_view_load_sc", "load", label.string(),
				                  [&] (Top::Component const &, Top::Thread const &thread)
				{
					Genode::uint64_t   time   = 0;

					thread.for_each_thread_of_pd([&] (Top::Thread &t) {
						if (_same(t.affinity(), _last_cpu))
							time += t.recent_time(sort == SC_TIME);
					});

					Genode::uint64_t max = total_second[_last_cpu.xpos()][_last_cpu.ypos()];

					auto percent = max ? (time * 100 / max) : 0ull;
					auto rest    = max ? (time * 10000 / max - (percent * 100)) : 0ull;

					list_view_bar(g, thread, percent, rest);
				});
			}

			list_view_pd_tool(g, "components", "components", "components ",
			                  [&] (Top::Component const &component,
			                       Top::Thread const &thread)
			{
				g.node("hbox", [&] () {
					g.attribute("name", thread.id().id * DIV + 3);
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(component.name); });
						g.attribute("color", "#ffffff");
						g.attribute("align", "left");
					});
				});
			});
		}

		void list_view_pd_tool(Genode::Generator &g,
		                       char const * const name,
		                       char const * const attribute,
		                       char const * const attribute_label,
		                       auto const &fn)
		{
			unsigned const max_pds = _config_pds_per_cpu;

			g.node("vbox", [&] () {
				g.attribute("name", name);

				g.node("hbox", [&] () {
					g.attribute("name", attribute);
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(attribute_label); });
						g.attribute("color", "#ffffff");
					});
				});

				unsigned pd_count = 0;

				if (pd_count < _pd_scroll.current) {
					g.node("hbox", [&] () {
						g.attribute("name", PD_SCROLL_UP * DIV);
						g.node("label", [&] () {
							g.node("text", [&] { g.append_quoted("..."); });
							g.attribute("color", "#ffffff");
						});
					});
				}

				for_each_pd([&] (auto const &base) {

					pd_count ++;
					if (pd_count - 1 < _pd_scroll.current || pd_count > _pd_scroll.current + max_pds)
						return;

					Top::Component const &component = static_cast<Top::Component const &>(base);
					Top::Thread const *thread = component._threads.first();
					if (!thread) {
						Genode::warning("component without any thread ?");
						return;
					}

					fn(component, *thread);
				});

				if (pd_count > _pd_scroll.current + max_pds) {
					g.node("hbox", [&] () {
						g.attribute("name", PD_SCROLL_DOWN * DIV);
						g.node("label", [&] () {
							g.node("text", [&] { g.append_quoted("..."); });
							g.attribute("color", "#ffffff");
						});
					});
				}
			});
		}
};

void Subjects::top(Genode::Generator         &g,
                   SORT_TIME           const  sort,
                   bool                const  trace_ms,
                   Genode::String<12>  const &name_prio,
                   Genode::String<12>  const &name_quantum)
{
	if (_detailed_view.id) {
		Top::Thread const * thread = _lookup_thread(_detailed_view);
		if (thread) {
			g.node("frame", [&] () {
				detail_view(g, *thread, sort, name_prio, name_quantum);
			});
			return;
		}
		_detailed_view.id = 0;
	}

	g.node("frame", [&] () {
		g.node("hbox", [&] () {
			g.node("button", [&] () {
				g.attribute("name", "settings");
				if (_button_setting_hovered) {
					g.attribute("hovered","yes");
				}
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("|||"); });
				});
			});

			g.node("vbox", [&] () {
				if (_button_setting) {
					g.node("hbox", [&] () {
						g.attribute("name", "aa");

						g.node("label", [&] () {
							g.attribute("name", "label_view");
							g.node("text", [&] { g.append_quoted("view period ms:"); });
						});
						hub(g, _button_view_period, "view");

					});

					g.node("hbox", [&] () {
						g.attribute("name", "bb");

						if (trace_ms) {
							g.node("label", [&] () {
								g.attribute("name", "label_trace");
								g.node("text", [&] { g.append_quoted("trace period ms:"); });
							});
							hub(g, _button_trace_period, "trace");
						}
					});

					g.node("hbox", [&] () {
						g.attribute("name", "cc");

						g.node("label", [&] () {
							g.attribute("name", "label2");
							g.node("text", [&] { g.append_quoted("list:"); });
						});
						g.node("button", [&] () {
							g.attribute("name", "enable_view");
							g.attribute("style", "checkbox");
							if (_button_enable_view_hovered)
								g.attribute("hovered","yes");
							if (_enable_view)
								g.attribute("selected","yes");
							g.node("label", [&] () {
								g.node("text", [&] { g.append_quoted("enable"); });
							});
						});

						g.node("label", [&] () {
							g.attribute("name", "label_g");
							g.node("text", [&] { g.append_quoted("graph:"); });
						});

						g.node("button", [&] () {
							g.attribute("name", "graph_reset");
							g.attribute("style", "checkbox");
							if (_button_reset_graph_hovered)
								g.attribute("hovered","yes");
							g.node("label", [&] () {
								g.node("text", [&] { g.append_quoted("reset"); });
							});
						});
					});
				}

				if (_enable_view) {
					g.node("hbox", [&] () {
						g.attribute("name", "dd");
						g.node("button", [&] () {
							g.attribute("name", "threads");
							if (_sort == THREAD)
								g.attribute("selected","yes");
							if (_button_thread_hovered)
								g.attribute("hovered","yes");
							g.node("label", [&] () {
								g.node("text", [&] { g.append_quoted(Genode::String<16>("threads (", _num_subjects,")")); });
							});
						});
						g.node("button", [&] () {
							g.attribute("name", "components");
							if (_sort == COMPONENT)
								g.attribute("selected","yes");
							if (_button_component_hovered)
								g.attribute("hovered","yes");
							g.node("label", [&] () {
								g.node("text", [&] { g.append_quoted(Genode::String<20>("components (", _num_pds,")")); });
							});
						});

						if (_show_second_time) {
							g.node("label", [&] () {
								g.attribute("name", "sort");
								g.node("text", [&] { g.append_quoted("sort:"); });
							});
							g.node("button", [&] () {
								g.attribute("name", "ec");
								if (sort == EC_TIME)
									g.attribute("selected","yes");
								if (_button_ec_hovered)
									g.attribute("hovered","yes");
								g.node("label", [&] () {
									g.node("text", [&] { g.append_quoted("EC"); });
								});
							});
							g.node("button", [&] () {
								g.attribute("name", "sc");
								if (sort == SC_TIME)
									g.attribute("selected","yes");
								if (_button_sc_hovered)
									g.attribute("hovered","yes");
								g.node("label", [&] () {
									g.node("text", [&] { g.append_quoted("SC"); });
								});
							});
						}
					});

					g.node("hbox", [&] () {
						g.attribute("name", "ee");
						if (_button_setting) {
							g.node("vbox", [&] () {
								buttons(g, _button_cpus);
							});
							g.node("vbox", [&] () {
								numbers(g, _button_numbers);
							});
						}

						if (_sort == THREAD) list_view(g, sort);
						if (_sort == COMPONENT) list_view_pd(g, sort);
					});
				} else {
					short_view(g, sort);
				}
			});
		});
	});
}

void Subjects::short_view(Genode::Generator &g, SORT_TIME const)
{
	unsigned cpus_online = 0;
	for_each_online_cpu([&] (Location const &) {
		cpus_online ++;
	});

	unsigned start = 0;
	unsigned step  = cpus_online / 2;
	if (cpus_online < 3) step = cpus_online;
	if (cpus_online > 6) step = 4;

	unsigned next  = step;
	unsigned i = 0;

	while (i != cpus_online) {

		g.node("hbox", [&] () {
			g.attribute("name", Genode::String<8>("ff", i));

			unsigned r = 0;

			for_each_online_cpu([&] (Location const &loc) {

//				Genode::log(start, "<", r, ">=", next);
				if (r < start || r >= next) {
					r ++;
					return;
				}

				r ++;
				i ++;

				Genode::String <16> name(loc.xpos(), ".", loc.ypos());

				g.node("vbox", [&] () {
					g.attribute("name", Genode::String<16>("v", name));

					auto total   = total_first[loc.xpos()][loc.ypos()];
					auto idle    = total_idle [loc.xpos()][loc.ypos()];
					auto percent = (total && idle <= total)
					             ? 100 - (idle * 100 / total) : 101;

					g.node("graph", [&] () {
						g.attribute("color", "#ff0000");
						g.attribute("textcolor", "#ffffff");
						g.attribute("percent", percent);
						g.attribute("width", 100);
						g.attribute("height", 100);
						g.node("text", [&] { g.append_quoted(name); });
						g.attribute("id", _timestamp);
					});
				});
			});
		});
		start += step;
		next += step;
	}
}

void Subjects::read_config(Genode::Node const & node)
{
	{
		Genode::String<8> view(node.attribute_value("view", Genode::String<8>("diagram")));
		if (view == "diagram")
			_enable_view = false;
		else
			_enable_view = true;
	}

	{
		Genode::String<12> view(node.attribute_value("list", Genode::String<12>("threads")));
		if (view == "components")
			_sort = COMPONENT;
		else
			_sort = THREAD;
	}

	node.for_each_sub_node("cpu", [&](auto const & cpu){
		unsigned xpos = cpu.attribute_value("xpos", unsigned(MAX_CPUS_X));
		unsigned ypos = cpu.attribute_value("ypos", unsigned(MAX_CPUS_Y));
		if (xpos >= MAX_CPUS_X || ypos >= MAX_CPUS_Y) return;

		Location const loc(xpos, ypos);
		cpu_show(loc) = cpu.attribute_value("show", true);
		_cpu_number(loc).set(cpu.attribute_value("threads", 2U));
		cpu_online(loc) = true;
	});
}

void Subjects::write_config(Genode::Generator &g) const
{
	g.attribute("period_ms", _button_view_period.value());
	g.attribute("trace_ms", _button_trace_period.value());

	if (!_enable_view)
		g.attribute("view", "diagram");
	else
		g.attribute("view", "list");

	if (_sort == THREAD)
		g.attribute("list", "threads");
	if (_sort == COMPONENT)
		g.attribute("list", "components");

	for_each_online_cpu([&] (Location const &loc) {
		g.node("cpu", [&] () {
			g.attribute("xpos", loc.xpos());
			g.attribute("ypos", loc.ypos());
			g.attribute("show", cpu_show(loc));
			g.attribute("threads", _cpu_number(loc).value());
		});
	});
}

namespace App {

	struct Main;
	using namespace Genode;
}


struct App::Main
{
	static unsigned _default_period_ms() { return 5000; }

	Env &_env;

	size_t arg_buffer_ram  { 64 * 4096 }; /* sufficient for 1000 trace ids */
	size_t trace_ram_quota { arg_buffer_ram + 4 * 4096 };

	Reconstructible<Trace::Connection> _trace { _env,
	                                            trace_ram_quota,
	                                            arg_buffer_ram };

	unsigned               _period_trace  { _default_period_ms() };
	unsigned               _period_view   { _default_period_ms() };
	bool                   _use_log       { true };
	bool                   _empty_graph   { true };
	bool                   _updated_trace { false };
	bool                   _flush_config  { false };
	SORT_TIME              _sort          { EC_TIME };
	Attached_rom_dataspace _config        { _env, "config" };
	Timer::Connection      _timer         { _env };
	Heap                   _heap          { _env.ram(), _env.rm() };
	Subjects               _subjects      { };
	unsigned               _dialog_size   { 2 * 4096 };
	unsigned               _graph_size    { 4096 };
	Attached_rom_dataspace _info          { _env, "platform_info" };
	Genode::String<12>     _name_prio     { "prio" };
	Genode::String<12>     _name_quantum  { "quantum" };

	void _handle_config();
	void _handle_trace(Duration);
	void _handle_view(Duration);
	void _handle_hover();
	void _generate_report();

	void _read_config();
	void _write_config();
	void _detect_kernel();

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config};

	Constructible<Timer::Periodic_timeout<Main>> _timeout_trace { };
	Constructible<Timer::Periodic_timeout<Main>> _timeout_view { };

	Signal_handler<Main> _hover_handler = {
		_env.ep(), *this, &Main::_handle_hover};

	Constructible<Reporter>               _reporter { };
	Constructible<Reporter>               _reporter_graph { };
	Constructible<Reporter>               _reporter_config { };
	Constructible<Attached_rom_dataspace> _hover { };
	Constructible<Top::Storage>           _storage { };

	Main(Env &env) : _env(env)
	{
		_subjects.init(env.cpu().affinity_space());

		_config.sigh(_config_handler);

		_handle_config();

		/* trigger to get immediate GUI content before first timeout */
		_handle_trace(Duration(Microseconds(1000)));
	}
};

template <typename T>
static T _attribute_value(Genode::Node const & node, char const *attr_name)
{
	return node.attribute_value(attr_name, T{});
}

template <typename T, typename... ARGS>
static T _attribute_value(Genode::Node const & node, char const *sub_node_type, ARGS... args)
{
	return node.with_sub_node(sub_node_type, [&](auto const &node) {
		return _attribute_value<T>(node, args...);
	}, []() { return T{}; });
}

/**
 * Query attribute value from g sub nodd
 *
 * The list of arguments except for the last one refer to g path into the
 * g structure. The last argument denotes the queried attribute name.
 */
template <typename T, typename... ARGS>
static T query_attribute(Genode::Node const & node, ARGS &&... args)
{
	return _attribute_value<T>(node, args...);
}

void App::Main::_handle_hover()
{
	/* reconfigure trace period time */
	unsigned period_trace = _subjects.period_trace();
	unsigned period_view = _subjects.period_view();

	if ((period_trace != _period_trace) || (period_view != _period_view)) {
		if (period_trace == 0)
			period_trace = 1;
		if (period_view < 50)
			period_view = 50;

		if (period_trace >= period_view) {
			if (period_view != _period_view)
				period_trace = period_view;
			else
				period_view = period_trace;
		}

		if (_period_view != period_view) {
			_period_view = period_view;

			/* if storage is off we did not construct the additional timer */
			if (_timeout_view.constructed()) {
				_timeout_view.destruct();
				_timeout_view.construct(_timer, *this, &Main::_handle_view, Microseconds(_period_view * 1000));
			} else
				period_trace = _period_view;
		}
		if (_period_trace != period_trace) {
			_period_trace = period_trace;
			_timeout_trace.destruct();
			_timeout_trace.construct(_timer, *this, &Main::_handle_trace, Microseconds(_period_trace * 1000));
		}

		_subjects.period(period_trace, period_view);
		_flush_config = true;
	}

	_hover->update();

	if (!_hover->valid())
		return;

	Node const & hover = _hover->node();

	typedef String<12> Button;
	Button button = query_attribute<Button>(hover, "dialog", "frame",
	                                        "hbox", "button", "name");
	if (button == "") {
		button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
		                                 "vbox", "hbox", "button", "name");
	}
	if (button == "") {
		button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
		                                 "vbox", "hbox", "vbox", "hbox", "button", "name");
	}
	if (button == "") {
		button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
		                                 "vbox", "hbox", "vbox", "button", "name");
	}

	/* detailed view: to detect "<" button */
	if (button == "") {
		button = query_attribute<Button>(hover, "dialog", "frame", "vbox",
		                                 "hbox", "button", "name");
	}

	bool click_valid = false;
	Button click = query_attribute<Button>(hover, "button", "left");
	if (click == "yes") {
		click = "left";
		click_valid = true;
	} else {
		click = query_attribute<Button>(hover, "button", "right");
		if (click == "yes") {
			click = "right";
			click_valid = true;
		} else {
			long y = query_attribute<long>(hover, "button", "wheel");
			click_valid = y;
			if (y < 0) click = "wheel_down";
			if (y > 0) click = "wheel_up";
		}
	}

	Genode::Trace::Subject_id id {
		query_attribute<unsigned>(hover, "dialog", "frame", "hbox", "vbox",
		                          "hbox", "vbox", "hbox", "name") / DIV};
	unsigned sub_id = 0;

	if (!id.id) {
		sub_id = query_attribute<unsigned>(hover, "dialog", "frame", "vbox",
		                                  "hbox", "vbox", "button", "name");
		id.id = sub_id / 10;
		sub_id = sub_id % 10;
	}

	auto res = _subjects.hover(button, click, click_valid, id, sub_id, _sort);
	if (res.flush_config)
		_flush_config = true;
	if (res.report_menu)
		_generate_report();
}

void App::Main::_handle_config()
{
	_config.update();

	if (!_config.valid()) return;

	_detect_kernel();

	unsigned long const period_view = _period_view;
	_period_view = _config.node().attribute_value("view_ms", _default_period_ms());

	unsigned long const period_trace = _period_trace;
	_period_trace = _config.node().attribute_value("trace_ms", _period_view);

	_use_log = _config.node().attribute_value("log", false);

	bool const store = _config.node().attribute_value("store", false);

	String<8> ec_sc(_config.node().attribute_value("sort_time", String<8>("ec")));
	if (ec_sc == "ec")
		_sort = EC_TIME;
	else
		_sort = SC_TIME;

	if (store && !_storage.constructed())
		_storage.construct(_env);
	if (!store && _storage.constructed())
		_storage.destruct();

	if (period_trace != _period_trace)
		if (_timeout_trace.constructed())
			_timeout_trace.destruct();

	if (period_view != _period_view)
		if (_timeout_view.constructed())
			_timeout_view.destruct();

	if (!_timeout_trace.constructed())
		_timeout_trace.construct(_timer, *this, &Main::_handle_trace, Microseconds(_period_trace * 1000));

	if (_storage.constructed())
		_timeout_view.construct(_timer, *this, &Main::_handle_view, Microseconds(_period_view * 1000));
	else
		_period_view = _period_trace;

	_subjects.period(_period_trace, _period_view);

	if (_config.node().attribute_value("report", true)) {
		if (!_reporter.constructed()) {
			_reporter.construct(_env, "dialog", "dialog", _dialog_size);
			_reporter->enabled(true);
		}
		if (!_hover.constructed()) {
			_hover.construct(_env, "hover");
			_hover->sigh(_hover_handler);
		}
		if (!_reporter_graph.constructed()) {
			_reporter_graph.construct(_env, "graph", "graph", _graph_size);
			_reporter_graph->enabled(true);
		}
	} else {
		if (_reporter.constructed())
			_reporter.destruct();
	}

	if (_config.node().attribute_value("report_config", true)) {
		if (!_reporter_config.constructed()) {
			_reporter_config.construct(_env, "config", "config", 4096);
			_reporter_config->enabled(true);
		}
	} else {
		if (_reporter_config.constructed())
			_reporter_config.destruct();
	}

	_read_config();
}

void App::Main::_read_config()
{
	try {
		_subjects.read_config(_config.node());
	} catch (...) {
		error("view config invalid - ignored");
	}
}

void App::Main::_write_config()
{
	if (!_reporter_config.constructed())
		return;

	auto result = _reporter_config->generate([&] (auto &g) {
		g.attribute("report", _reporter.constructed());
		g.attribute("report_config" , _reporter_config.constructed());

		if (_storage.constructed())
			g.attribute("store" , _storage.constructed());

		g.attribute("log", _use_log);

		if (_sort == EC_TIME)
			g.attribute("sort_time", "ec");
		else
		if (_sort == SC_TIME)
			g.attribute("sort_time", "sc");

		_subjects.write_config(g);
	});

	if (result.failed())
		error(__func__, " failed");
}

void App::Main::_handle_view(Duration)
{
	if (!_updated_trace) return;

	_updated_trace = false;

	if (_flush_config) {
		_write_config();
		_flush_config = false;
	}

	/* show most significant consumers */
	if (_use_log)
		_subjects.top(_sort);

	if (_storage.constructed())
		_storage->force_data_flush();

	_generate_report();
}

void App::Main::_handle_trace(Duration time)
{
	/* update subject information */
	bool const arg_buffer_sufficient = _subjects.update(*_trace, _heap,
	                                                    _sort, _storage);

	if (arg_buffer_sufficient) {
		_updated_trace = true;

		if (_period_trace == _period_view)
			_handle_view(time);

		return;
	}

	arg_buffer_ram  += 4 * 4096;
	trace_ram_quota += 4 * 4096;

	/* by destructing the session we free up the allocated memory in core */
	Genode::warning("re-construct trace session");

	_subjects.flush(*_trace, _heap);

	_trace.destruct();
	_trace.construct(_env, trace_ram_quota, arg_buffer_ram);
}

void App::Main::_generate_report()
{
	if (_reporter.constructed()) {
		unsigned retry = 0;

		do {
			auto result = _reporter->generate([&] (auto &g) {
				_subjects.top(g, _sort, _storage.constructed(), _name_prio, _name_quantum);
			});

			result.with_result([&](auto) {
				retry = 0;
			}, [&](auto const &e) {
				switch (e) {
				case Buffer_error::EXCEEDED:
					if (++retry % 5 == 0)
						Genode::warning(retry, ". attempt to extend dialog report size");

					_dialog_size += 4096;
					_reporter.destruct();
					_reporter.construct(_env, "dialog", "dialog", _dialog_size);
					_reporter->enabled(true);
				}
			});
		} while (retry);
	}

	bool const show_graph = !_empty_graph || _subjects.tracked_threads() || _subjects.trace_top_most();
	if (_reporter_graph.constructed() && show_graph) {
		unsigned retry = 0;

		do {
			auto result = _reporter_graph->generate([&] (auto &g) {
				_subjects.graph(g, _sort);
			});

			result.with_result([&](auto) {
				retry = 0;
			}, [&](auto const &e) {
				switch (e) {
				case Buffer_error::EXCEEDED:

					if (++retry % 5 == 0)
						Genode::warning(retry, ". attempt to extend graph report size");

					_graph_size += 4096;
					_reporter_graph.destruct();
					_reporter_graph.construct(_env, "graph", "graph", _graph_size);
					_reporter_graph->enabled(true);
				}
			});
		} while (retry);
	}

	_empty_graph = !_subjects.tracked_threads() && !_subjects.trace_top_most();
}


void App::Main::_detect_kernel()
{
	_info.update();

	if (!_info.valid() || !_config.valid())
		return;

	_info.node().with_optional_sub_node("kernel", [&](auto const &node) {
		auto const kernel = node.attribute_value("name", String<16>("unknown"));

		if (kernel == "hw") {
			_name_prio    = "weight";
			_name_quantum = "warp";
		}
	});
}

void Component::construct(Genode::Env &env) { static App::Main main(env); }
