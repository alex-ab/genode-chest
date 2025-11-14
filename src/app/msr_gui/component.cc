/*
 * \brief  GUI for managing power states for AMD & Intel
 * \author Alexander Boettcher
 * \date   2022-10-15
 */

/*
 * Copyright (C) 2022-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/duration.h>
#include <base/signal.h>

#include <os/reporter.h>

#include "node_tools.h"
#include "button.h"

using namespace Genode;


struct State
{
	unsigned value { ~0U };

	bool valid()      const { return value != ~0U; }
	void invalidate()       { value = ~0U; }

	bool operator == (State const &o) const { return value == o.value; }
	bool operator != (State const &o) const { return value != o.value; }
};


class Power
{
	private:

		enum { CPU_MUL = 10000 };

		Env                    &_env;
		Attached_rom_dataspace  _info     { _env, "info" };
		Signal_handler<Power>   _info_sig { _env.ep(), *this, &Power::_info_update };

		Attached_rom_dataspace  _hover     { _env, "hover" };
		Signal_handler<Power>   _hover_sig { _env.ep(), *this, &Power::_hover_update};

		Expanding_reporter      _dialog          { _env, "dialog", "dialog" };
		Expanding_reporter      _msr_config      { _env, "config", "config" };

		State                   _setting_cpu       { };
		State                   _setting_hovered   { };
		unsigned                _last_cpu          { ~0U };

		String<16>              _mwait_button_hovered  { };
		String<16>              _mwait_button_selected { "mwait_hlt" };
		uint8_t                 _mwait_c_state         { };
		uint8_t                 _mwait_c_sub_state     { };

		struct Seho { bool hover; bool select; };

		bool                    _initial_hwp_cap   { false };
		bool                    _none_hovered      { false };
		bool                    _apply_period      { false };
		bool                    _apply_hovered     { false };
		bool                    _apply_all_hovered { false };
		bool                    _hwp_epp_perf      { false };
		bool                    _hwp_epp_bala      { false };
		bool                    _hwp_epp_ener      { false };
		bool                    _hwp_epp_custom    { false };
		bool                    _epb_perf          { false };
		bool                    _epb_bala          { false };
		bool                    _epb_ener          { false };
		bool                    _epb_custom        { false };
		bool                    _hwp_on_selected   { false };
		bool                    _hwp_on_hovered    { false };
		bool                    _epb_custom_select { false };
		bool                    _epp_custom_select { false };
		bool                    _hwp_req_custom    { false };
		bool                    _hwp_req_cus_sel   { false };
		bool                    _hwp_req_auto      { false };
		bool                    _hwp_req_auto_sel  { false };
		bool                    _apply_select      { false };
		bool                    _apply_all_select  { false };
		bool                    _apply_select_per  { false };
		bool                    _pstate_max        { false };
		bool                    _pstate_mid        { false };
		bool                    _pstate_min        { false };
		bool                    _pstate_custom     { false };
		bool                    _pstate_custom_sel { false };
		bool                    _hwp_enabled_once  { false };
		bool                    _hover_normal      { false };
		bool                    _hover_advanced    { false };
		bool                    _select_normal     { true  };
		bool                    _select_advanced   { false };
		bool                    _hover_rapl_detail  { false };
		bool                    _select_rapl_detail { false };
		bool _hover_mwait { false };
		Seho _residency   { .hover = false, .select = true };

		Button_hub<5, 0, 9, 0>  _timer_period { };

		/* ranges are set by read out hardware features */
		Button_hub<1, 0, 10, 0> _amd_pstate { };

		/* PERFORMANCE = 0, BALANCED = 7, POWER_SAVING = 15 */
		enum { EPB_PERF = 0, EPB_BALANCED = 7, EPB_POWER_SAVE = 15 };
		Button_hub<1, 0, 15, 7>    _intel_epb  { };

		/* ranges are set by read out hardware features */
		Button_hub<1, 0, 255, 128> _intel_hwp_min { };
		Button_hub<1, 0, 255, 128> _intel_hwp_max { };
		Button_hub<1, 0, 255, 128> _intel_hwp_des { };

		Button_hub<1, 0, 255, 128> _intel_hwp_pck_min { };
		Button_hub<1, 0, 255, 128> _intel_hwp_pck_max { };
		Button_hub<1, 0, 255, 128> _intel_hwp_pck_des { };

		/* PERFORMANCE = 0, BALANCED = 128, ENERGY = 255 */
		enum { EPP_PERF = 0, EPP_BALANCED = 128, EPP_ENERGY = 255 };
		Button_hub<1, 0, 255, 128> _intel_hwp_epp { };

		void _generate_msr_config(bool, bool = false);
		void _generate_msr_cpu(Generator &, unsigned, unsigned);
		void _info_update();
		void _hover_update();
		void _settings_period(Generator &);
		void _settings_mode  (Generator &);
		void _cpu_energy(Generator &, Node const &, unsigned &);
		void _cpu_energy_detail(Generator &, Node const &, unsigned &,
		                        char const *);
		void _cpu_power_info(Generator &, Node const &, unsigned &);
		void _cpu_power_info_detail(Generator &, Node const &,
		                            unsigned &, char const *);
		void _cpu_power_limit(Generator &, Node const &, unsigned &);
		void _cpu_power_limit_dram_pp0_pp1(Generator &, Node const &,
		                                   unsigned &, char const *);
		void _cpu_power_limit_common(Generator &, Node const &,
		                             unsigned &, char const *);
		void _cpu_power_limit_headline(Generator &, unsigned &, char const *);
		void _cpu_perf_status(Generator &, Node const &, unsigned &);
		void _cpu_perf_status_detail(Generator &, Node const &,
		                             char const *, unsigned &);
		void _cpu_residency(Generator &, Node const &, unsigned &);
		bool _cpu_residency_detail(Generator &, Node const &,
		                           char const *, unsigned &);
		void _cpu_mwait(Generator &, Node const &, unsigned &);
		void _cpu_mwait_detail(Generator &, String<4> const &, uint8_t, uint8_t);
		void _cpu_temp(Generator &, Node const &);
		void _cpu_freq(Generator &, Node const &);
		void _cpu_setting(Generator &, Node const &);
		void _settings_view(Generator &, Node const &,
		                    String<12> const &, unsigned, bool);
		void _settings_amd(Generator &, Node const &, bool);
		void _settings_intel_epb(Generator &, Node const &, bool);
		void _settings_intel_hwp(Generator &, Node const &, bool);
		void _settings_intel_hwp_req(Generator &, Node const &,
		                             unsigned, unsigned, uint64_t, bool, bool,
		                             unsigned &);

		unsigned _cpu_name(Generator &, Node const &, unsigned);

		template <typename T>
		void hub(Generator &g, T &hub, char const *name)
		{
			hub.for_each([&](Button_state &state, unsigned pos) {
				g.attribute("name", Genode::String<20>("hub-", name, "-", pos));

				Genode::String<12> number(state.current);

				g.node("button", [&] () {
					g.attribute("name", Genode::String<20>("hub-", name, "-", pos));
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted(number); });
					});

					if (state.active())
						g.attribute("hovered", true);
				});
			});
		}

		unsigned cpu_id(Genode::Node const &cpu) const
		{
			auto const affinity_x = cpu.attribute_value("x", 0U);
			auto const affinity_y = cpu.attribute_value("y", 0U);
			return affinity_x * CPU_MUL + affinity_y;
		}

	public:

		Power(Env &env) : _env(env)
		{
			_info.sigh(_info_sig);
			_hover.sigh(_hover_sig);

			_timer_period.set(unsigned(Milliseconds(4000).value));

			_info_update();
		}
};


void Power::_hover_update()
{
	_hover.update();

	if (!_hover.valid())
		return;

	Genode::Node hover = _hover.node();

	/* settings and apply button */
	typedef Genode::String<20> Button;
	Button button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
	                                        "vbox", "hbox", "button", "name");
	if (button == "") /* mwait, intel hwp, epb, epp & AMD pstate buttons */
		button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
		                                 "vbox", "frame", "hbox", "button", "name");

	if (button == "") /* intel rapl button */
		button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
		                                 "vbox", "frame", "hbox", "vbox", "hbox", "button", "name");

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
/*
			long y = query_attribute<long>(hover, "button", "wheel");
			click_valid = y;
			if (y < 0) click = "wheel_down";
			if (y > 0) click = "wheel_up";
*/
		}
	}

	if (_apply_select)      _apply_select     = false;
	if (_apply_all_select)  _apply_all_select = false;
	if (_apply_select_per)  _apply_select_per = false;

	bool refresh = false;

	if (click_valid && _setting_hovered.valid()) {
		if (_setting_cpu == _setting_hovered) {
			_setting_cpu.invalidate();
		} else
			_setting_cpu = _setting_hovered;

		refresh = true;
	}

	if (click_valid && (_hover_normal || _hover_advanced)) {

		if (_hover_normal)   { _select_normal = true; _select_advanced = false; }
		if (_hover_advanced) { _select_advanced = true; _select_normal = false; }

		refresh = true;
	}

	if (click_valid && (_hover_rapl_detail)) {
		_select_rapl_detail = !_select_rapl_detail;
		refresh = true;
	} else
	if (click_valid && (_residency.hover)) {
		_residency.select = !_residency.select;
		refresh = true;
	}

	if (click_valid && (_apply_hovered || _apply_all_hovered)) {

		_generate_msr_config(_apply_all_hovered);

		if (_apply_hovered)     _apply_select     = true;
		if (_apply_all_hovered) _apply_all_select = true;

		refresh = true;
	}

	if (click_valid && (_apply_period)) {

		_generate_msr_config(_apply_all_hovered, _apply_period);

		_apply_select_per = true;

		refresh = true;
	}

	if (click_valid && _setting_cpu.valid()) {

		if (_timer_period.any_active()) {
			if (click == "left")
				refresh = refresh || _timer_period.update_inc();
			else
			if (click == "right")
				refresh = refresh || _timer_period.update_dec();

			if (_timer_period.value() < 100)
				_timer_period.set(100);
		}

		if (_amd_pstate.any_active()) {
			if (click == "left")
				refresh = refresh || _amd_pstate.update_inc();
			else
			if (click == "right")
				refresh = refresh || _amd_pstate.update_dec();
		}

		if (_intel_epb.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_epb.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_epb.update_dec();
		}

		if (_intel_hwp_min.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_hwp_min.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_hwp_min.update_dec();
		}

		if (_intel_hwp_max.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_hwp_max.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_hwp_max.update_dec();
		}

		if (_intel_hwp_des.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_hwp_des.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_hwp_des.update_dec();
		}

		if (_intel_hwp_epp.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_hwp_epp.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_hwp_epp.update_dec();
		}

		if (_hwp_on_hovered) {
			_hwp_on_selected = true;
			refresh = true;
		}

		if (_hover_mwait) {
			_mwait_button_selected = _mwait_button_hovered;
			refresh = true;
		}

		if (_hwp_epp_perf) {
			_intel_hwp_epp.set(EPP_PERF);
			refresh = true;
		}

		if (_hwp_epp_bala) {
			_intel_hwp_epp.set(EPP_BALANCED);
			refresh = true;
		}

		if (_hwp_epp_ener) {
			_intel_hwp_epp.set(EPP_ENERGY);
			refresh = true;
		}

		if (_hwp_epp_custom) {
			_epp_custom_select = !_epp_custom_select;
			refresh = true;
		}

		if (_hwp_req_custom) {
			_hwp_req_cus_sel = !_hwp_req_cus_sel;
			refresh = true;
		}

		if (_hwp_req_auto) {
			_hwp_req_auto_sel = !_hwp_req_auto_sel;
			refresh = true;
		}

		if (_epb_perf) {
			_intel_epb.set(EPB_PERF);
			refresh = true;
		}

		if (_epb_bala) {
			_intel_epb.set(EPB_BALANCED);
			refresh = true;
		}

		if (_epb_ener) {
			_intel_epb.set(EPB_POWER_SAVE);
			refresh = true;
		}

		if (_epb_custom) {
			_epb_custom_select = !_epb_custom_select;
			refresh = true;
		}

		if (_pstate_max) {
			_amd_pstate.set(_amd_pstate.min());
			refresh = true;
		}

		if (_pstate_mid) {
			_amd_pstate.set((_amd_pstate.max() - _amd_pstate.min() + 1) / 2);
			refresh = true;
		}

		if (_pstate_min) {
			_amd_pstate.set(_amd_pstate.max());
			refresh = true;
		}

		if (_pstate_custom) {
			_pstate_custom_sel = !_pstate_custom_sel;
			refresh = true;
		}
	}

	if (click_valid) {
		if (refresh)
			_info_update();
		return;
	}

	auto const before_hovered        = _setting_hovered;
	auto const before_cpu            = _setting_cpu;
	auto const before_period         = _timer_period.any_active();
	auto const before_pstate         = _amd_pstate.any_active();
	auto const before_epb            = _intel_epb.any_active();
	auto const before_hwp_min        = _intel_hwp_min.any_active();
	auto const before_hwp_max        = _intel_hwp_max.any_active();
	auto const before_hwp_des        = _intel_hwp_des.any_active();
	auto const before_hwp_epp        = _intel_hwp_epp.any_active();
	auto const before_none           = _none_hovered;
	auto const before_apply_period   = _apply_period;
	auto const before_apply          = _apply_hovered;
	auto const before_all_apply      = _apply_all_hovered;
	auto const before_hwp_epp_perf   = _hwp_epp_perf;
	auto const before_hwp_epp_bala   = _hwp_epp_bala;
	auto const before_hwp_epp_ener   = _hwp_epp_ener;
	auto const before_hwp_epp_custom = _hwp_epp_custom;
	auto const before_hwp_req_custom = _hwp_req_custom;
	auto const before_hwp_req_auto   = _hwp_req_auto;
	auto const before_epb_perf       = _epb_perf;
	auto const before_epb_bala       = _epb_bala;
	auto const before_epb_ener       = _epb_ener;
	auto const before_epb_custom     = _epb_custom;
	auto const before_hwp_on         = _hwp_on_hovered;
	auto const before_pstate_max     = _pstate_max;
	auto const before_pstate_mid     = _pstate_mid;
	auto const before_pstate_min     = _pstate_min;
	auto const before_pstate_custom  = _pstate_custom;
	auto const before_normal         = _hover_normal;
	auto const before_advanced       = _hover_advanced;
	auto const before_rapl_detail    = _hover_rapl_detail;
	auto const before_mwait          = _hover_mwait;
	auto const before_residency      = _residency.hover;

	bool any = button != "";

	bool const hovered_setting = any && (button == "settings")                && (!(any = false));
	bool const hovered_period  = any && (String<11>(button) == "hub-period")  && (!(any = false));
	bool const hovered_pstate  = any && (String<11>(button) == "hub-pstate")  && (!(any = false));
	bool const hovered_epb     = any && (String< 8>(button) == "hub-epb")     && (!(any = false));
	bool const hovered_hwp_min = any && (String<12>(button) == "hub-hwp_min") && (!(any = false));
	bool const hovered_hwp_max = any && (String<12>(button) == "hub-hwp_max") && (!(any = false));
	bool const hovered_hwp_des = any && (String<12>(button) == "hub-hwp_des") && (!(any = false));
	bool const hovered_hwp_epp = any && (String<12>(button) == "hub-hwp_epp") && (!(any = false));

	_none_hovered      = any && (button == "none")         && (!(any = false));
	_apply_hovered     = any && (button == "apply")        && (!(any = false));
	_apply_all_hovered = any && (button == "applyall")     && (!(any = false));
	_apply_period      = any && (button == "apply_period") && (!(any = false));

	_hwp_on_hovered    = any && (button == "hwp_on")  && (!(any = false));

	_hwp_epp_perf      = any && (button == "hwp_epp-perf")   && (!(any = false));
	_hwp_epp_bala      = any && (button == "hwp_epp-bala")   && (!(any = false));
	_hwp_epp_ener      = any && (button == "hwp_epp-ener")   && (!(any = false));
	_hwp_epp_custom    = any && (button == "hwp_epp-custom") && (!(any = false));

	_hwp_req_custom    = any && (button == "hwp_req-custom") && (!(any = false));
	_hwp_req_auto      = any && (button == "hwp_req-auto")   && (!(any = false));

	_epb_perf   = any && (button == "epb-perf")   && (!(any = false));
	_epb_bala   = any && (button == "epb-bala")   && (!(any = false));
	_epb_ener   = any && (button == "epb-ener")   && (!(any = false));
	_epb_custom = any && (button == "epb-custom") && (!(any = false));

	_pstate_max    = any && (button == "pstate-max")    && (!(any = false));
	_pstate_mid    = any && (button == "pstate-mid")    && (!(any = false));
	_pstate_min    = any && (button == "pstate-min")    && (!(any = false));
	_pstate_custom = any && (button == "pstate-custom") && (!(any = false));

	_hover_normal   = any && (button == "normal") && (!(any = false));
	_hover_advanced = any && (button == "advanced") && (!(any = false));

	_hover_rapl_detail = any && (button == "info") && (!(any = false));
	_residency.hover = any && (button == "info_res") && (!(any = false));

	_hover_mwait = any && (String<7>(button) == "mwait_") && (!(any = false));
	if (_hover_mwait) _mwait_button_hovered = button;

	if (hovered_setting) {
		_setting_hovered.value = query_attribute<unsigned>(hover, "dialog", "frame",
		                                                   "hbox", "vbox", "hbox", "name");
	} else if (_setting_hovered.valid())
		_setting_hovered.invalidate();

	if (hovered_period || before_period) {
		_timer_period.for_each([&](Button_state &state, unsigned pos) {
			Genode::String<20> pos_name { "hub-period-", pos };
			if (Genode::String<20>(button) == pos_name) {
				state.hovered = hovered_period;
			} else
				state.hovered = false;
		});
	}

	_amd_pstate.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_pstate;
	});

	_intel_epb.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_epb;
	});

	_intel_hwp_min.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_hwp_min;
	});

	_intel_hwp_max.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_hwp_max;
	});

	_intel_hwp_des.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_hwp_des;
	});

	_intel_hwp_epp.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_hwp_epp;
	});

	if ((before_hovered        != _setting_hovered)   ||
	    (before_cpu            != _setting_cpu)       ||
	    (before_period         != hovered_period)     ||
	    (before_pstate         != hovered_pstate)     ||
	    (before_epb            != hovered_epb)        ||
	    (before_hwp_min        != hovered_hwp_min)    ||
	    (before_hwp_max        != hovered_hwp_max)    ||
	    (before_hwp_des        != hovered_hwp_des)    ||
	    (before_hwp_epp        != hovered_hwp_epp)    ||
	    (before_none           != _none_hovered)      ||
	    (before_apply_period   != _apply_period)      ||
	    (before_apply          != _apply_hovered)     ||
	    (before_all_apply      != _apply_all_hovered) ||
	    (before_hwp_epp_perf   != _hwp_epp_perf)      ||
	    (before_hwp_epp_bala   != _hwp_epp_bala)      ||
	    (before_hwp_epp_ener   != _hwp_epp_ener)      ||
	    (before_hwp_epp_custom != _hwp_epp_custom)    ||
	    (before_hwp_req_custom != _hwp_req_custom)    ||
	    (before_hwp_req_auto   != _hwp_req_auto)      ||
	    (before_epb_perf       != _epb_perf)          ||
	    (before_epb_bala       != _epb_bala)          ||
	    (before_epb_ener       != _epb_ener)          ||
	    (before_epb_custom     != _epb_custom)        ||
	    (before_hwp_on         != _hwp_on_hovered)    ||
	    (before_normal         != _hover_normal)      ||
	    (before_advanced       != _hover_advanced)    ||
	    (before_rapl_detail    != _hover_rapl_detail) ||
	    (before_residency      != _residency.hover)   ||
	    (before_pstate_max     != _pstate_max)        ||
	    (before_pstate_mid     != _pstate_mid)        ||
	    (before_pstate_min     != _pstate_min)        ||
	    (before_pstate_custom  != _pstate_custom)     ||
	    (before_mwait          != _hover_mwait))
		refresh = true;

	if (refresh)
		_info_update();
}


void Power::_info_update ()
{
	_info.update();

	if (!_info.valid())
		return;

	_dialog.generate([&] (Generator &g) {

		g.node("frame", [&] {

			g.node("hbox", [&] {

				unsigned cpu_count = 0;

				g.node("vbox", [&] {
					g.attribute("name", 1);

					unsigned loc_x_last = ~0U;

					_info.node().for_each_sub_node("cpu", [&](Genode::Node const &cpu) {
						loc_x_last = _cpu_name(g, cpu, loc_x_last);
						cpu_count ++;
					});
				});

				g.node("vbox", [&] {
					g.attribute("name", 2);
					_info.node().for_each_sub_node("cpu", [&](Genode::Node const &cpu) {
						_cpu_temp(g, cpu);
					});
				});

				g.node("vbox", [&] {
					g.attribute("name", 3);
					_info.node().for_each_sub_node("cpu", [&](Genode::Node const &cpu) {
						_cpu_freq(g, cpu);
					});
				});

				g.node("vbox", [&] {
					g.attribute("name", 4);
					_info.node().for_each_sub_node("cpu", [&](Genode::Node const &cpu) {
						_cpu_setting(g, cpu);
					});
				});

				bool const re_eval = _setting_cpu.value != _last_cpu;

				_info.node().for_each_sub_node("cpu", [&](Genode::Node const &cpu) {
					if (cpu_id(cpu) != _setting_cpu.value)
						return;

					auto const affinity_x = cpu.attribute_value("x", 0U);
					auto const affinity_y = cpu.attribute_value("y", 0U);

					g.node("vbox", [&] {
						g.attribute("name", 5);

						auto const name = String<12>("CPU ", affinity_x, "x", affinity_y);
						_settings_view(g, cpu, name, cpu_count, re_eval);
					});

					_last_cpu = cpu_id(cpu);
				});
			});
		});
	});
}


void Power::_generate_msr_cpu(Generator &g, unsigned affinity_x, unsigned affinity_y)
{
	g.node("cpu", [&] {
		g.attribute("x", affinity_x);
		g.attribute("y", affinity_y);

		g.node("pstate", [&] {
			g.attribute("rw_command", _amd_pstate.value());
		});

		g.node("hwp_request", [&] {
			g.attribute("min",     _intel_hwp_min.value());
			g.attribute("max",     _intel_hwp_max.value());
			if (_hwp_req_auto_sel)
				g.attribute("desired", 0);
			else
				g.attribute("desired", _intel_hwp_des.value());
			g.attribute("epp",     _intel_hwp_epp.value());
		});

		g.node("energy_perf_bias", [&] {
			g.attribute("raw", _intel_epb.value());
		});

		if (_hwp_on_selected && !_hwp_enabled_once) {
			g.node("hwp", [&] {
				g.attribute("enable", _hwp_on_selected);
			});
		}

		if (_mwait_button_selected != "") {

			g.node("mwait", [&] {
				if (_mwait_button_selected == "mwait_hlt")
					g.attribute("off", "yes");
				else {
					g.attribute("c_state",     _mwait_c_state);
					g.attribute("c_sub_state", _mwait_c_sub_state);
				}
			});
		}
	});
}


void Power::_generate_msr_config(bool all_cpus, bool const apply_period)
{
	if (!_setting_cpu.valid())
		return;

	_msr_config.generate([&] (Generator &g) {

/*
		g.attribute("verbose", false);
*/
		g.attribute("update_rate_us", _timer_period.value() * 1000);

		/* if soley period changed, don't rewrite HWP parameters */
		if (apply_period)
			return;

		if (all_cpus) {
			_info.node().for_each_sub_node("cpu", [&](Genode::Node const &cpu) {

				auto const affinity_x = cpu.attribute_value("x", 0U);
				auto const affinity_y = cpu.attribute_value("y", 0U);

				_generate_msr_cpu(g, affinity_x, affinity_y);
			});
		} else {
			auto const affinity_x = _setting_cpu.value / CPU_MUL;
			auto const affinity_y = _setting_cpu.value % CPU_MUL;

			_generate_msr_cpu(g, affinity_x, affinity_y);
		}
	});
}


unsigned Power::_cpu_name(Generator &g, Node const &cpu, unsigned last_x)
{
	auto const affinity_x = cpu.attribute_value("x", 0U);
	auto const affinity_y = cpu.attribute_value("y", 0U);
	auto const core_type  = cpu.attribute_value("type", String<2> (""));
	bool const same_x     = (affinity_x == last_x) &&
	                        (core_type != "E");

	g.node("hbox", [&] {
		auto const name = String<12>(same_x ? "" : "CPU ", affinity_x, "x",
		                             affinity_y, " ", core_type, " |");

		g.attribute("name", cpu_id(cpu));

		g.node("label", [&] {
			g.attribute("name", 1);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(name); });
		});
	});

	return affinity_x;
}


static String<12> align_string(auto const value)
{
	String<12> string { };

	string = String<12>(uint64_t(value));

	auto const rest = uint64_t(value * 100) % 100;

	string = String<12>(string, ".", (rest < 10) ? "0" : "", rest);

	/* align right */
	for (auto i = string.length(); i < string.capacity() - 1; i++) {
		string = String<12>(" ", string);
	}

	return string;
}


void Power::_cpu_energy_detail(Generator &g, Node const &node,
                               unsigned  &id , char const *text)
{
	auto const raw = node.attribute_value("raw", 0ULL);
	if (!raw)
		return;

	g.node("hbox", [&] {
		g.attribute("name", id++);

		double joule = 0, watt = 0;

		watt  = node.attribute_value("Watt",  watt);
		joule = node.attribute_value("Joule", joule);

		g.node("label", [&] {
			g.attribute("name", id++);
			g.attribute("align", "left");
			g.node("text", [&] { g.append_quoted(text); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(String<40>(align_string(watt) , " Watt | ",
			                                 align_string(joule), " Joule")); });
		});
	});
}


void Power::_cpu_energy(Generator &g, Node const &energy, unsigned &frames)
{
	unsigned id = 0;

	g.node("vbox", [&] {

		g.node("hbox", [&] {
			g.attribute("name", id++);

			g.node("label", [&] {
				g.attribute("name", id++);
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(String<30>(" Running Average Power Limit - energy:")); });
			});

			g.node("button", [&] () {
				g.attribute("align", "right");
				g.attribute("name", "info");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("info"); });
				});

				if (_hover_rapl_detail)
					g.attribute("hovered", true);
				if (_select_rapl_detail)
					g.attribute("selected", true);
			});
		});

		energy.with_optional_sub_node("package", [&](auto const &node) {
			frames++;
			_cpu_energy_detail(g, node, id, " Domain package:");
		});

		energy.with_optional_sub_node("dram", [&](auto const &node) {
			frames++;
			_cpu_energy_detail(g, node, id, " Domain DRAM:");
		});

		energy.with_optional_sub_node("pp0", [&](auto const &node) {
			frames++;
			_cpu_energy_detail(g, node, id, " Domain PP0: (CPUs)");
		});

		energy.with_optional_sub_node("pp1", [&](auto const &node) {
			frames++;
			_cpu_energy_detail(g, node, id, " Domain PP1: (GPU)");
		});
	});
}


void Power::_cpu_power_info_detail(Generator &g, Node const &node,
                                   unsigned  &id , char const *text)
{
	g.node("vbox", [&] {
		g.attribute("name", id++);

		double spec = 0, min = 0, max = 0, wnd = 0;

		spec = node.attribute_value("ThermalSpecPower" , spec);
		min  = node.attribute_value("MinimumPower"     , min);
		max  = node.attribute_value("MaximumPower"     , max);
		wnd  = node.attribute_value("MaximumTimeWindow", wnd);

		g.node("hbox", [&] {
			g.attribute("name", id++);

			g.node("label", [&] {
				g.attribute("name", id++);
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(text); });
			});
		});

		g.node("hbox", [&] {
			g.attribute("name", id++);

			g.node("label", [&] {
				g.attribute("font", "monospace/regular");
				g.attribute("name", id++);
				g.attribute("align", "right");
				g.node("text", [&] { g.append_quoted(String<40>(" Thermal spec. power ", align_string(spec), " Watt")); });
			});
		});

		g.node("hbox", [&] {
			g.attribute("name", id++);

			g.node("label", [&] {
				g.attribute("font", "monospace/regular");
				g.attribute("name", id++);
				g.attribute("align", "right");
				g.node("text", [&] { g.append_quoted(String<40>(" Minimal power ", align_string(min), " Watt")); });
			});
		});

		g.node("hbox", [&] {
			g.attribute("name", id++);

			g.node("label", [&] {
				g.attribute("font", "monospace/regular");
				g.attribute("name", id++);
				g.attribute("align", "right");
				g.node("text", [&] { g.append_quoted(String<40>(" Maximum power ", align_string(max), " Watt")); });
			});
		});

		g.node("hbox", [&] {
			g.attribute("name", id++);

			g.node("label", [&] {
				g.attribute("font", "monospace/regular");
				g.attribute("name", id++);
				g.attribute("align", "right");
				g.node("text", [&] { g.append_quoted(String<40>(" Maximum time window ", align_string(wnd), " s   ")); });
			});
		});
	});
}


void Power::_cpu_power_info(Generator &g, Node const &info, unsigned &frames)
{
	unsigned id = 0;

	info.with_optional_sub_node("package", [&](auto const &node) {
		frames ++;
		_cpu_power_info_detail(g, node, id, " Package power info:");
	});
	info.with_optional_sub_node("dram", [&](auto const &node) {
		frames ++;
		_cpu_power_info_detail(g, node, id, " DRAM power info:");
	});
}


void Power::_cpu_power_limit_common(Generator &g, Node const &node,
                                    unsigned  &id , char const *text)
{
	g.node("hbox", [&] {
		g.attribute("name", id++);

		double power = 0, window = 0;
		bool   enable = false, clamp = false;

		power  = node.attribute_value("power"       , power);
		enable = node.attribute_value("enable"      , enable);
		clamp  = node.attribute_value("clamp"       , clamp);
		window = node.attribute_value("time_window" , window);

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "left");
			g.node("text", [&] { g.append_quoted(text); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(String<19>(" ", align_string(power), " Watt")); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(enable ? String<10>(" true    ") : String<10>("false    ")); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(clamp ? String<10>(" true    ") : String<10>("false    ")); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(String<16>(" ", align_string(window), " s")); });
		});
	});
}


void Power::_cpu_power_limit_dram_pp0_pp1(Generator          &g,
                                          Node         const &node,
                                          unsigned           &id,
                                          char const * const  text)
{
		bool lock = false;

		lock = node.attribute_value("lock", lock);

		g.node("hbox", [&] {
			g.attribute("name", id++);

			g.node("label", [&] {
				g.attribute("name", id++);
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(String<32>(text, lock ? " - LOCKED" : "")); });
			});
		});

		_cpu_power_limit_common(g, node, id, " -  ");
}


void Power::_cpu_power_limit_headline(Generator          &g,
                                      unsigned           &id,
                                      char const * const  text)
{
	g.node("hbox", [&] {
		g.attribute("name", id++);

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "left");
			g.node("text", [&] { g.append_quoted(text); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(String<19>("         power")); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted("enable"); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted("clamp"); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(String<16>("time window  ")); });
		});
	});
}


void Power::_cpu_power_limit(Generator &g, Node const &limit, unsigned &)
{
	unsigned id = 0;

	g.node("vbox", [&] {
		g.attribute("name", id++);

		limit.with_optional_sub_node("package", [&](auto const &node) {

			bool lock = false;

			lock = node.attribute_value("lock", lock);

			g.node("hbox", [&] {
				g.attribute("name", id++);

				g.node("label", [&] {
					g.attribute("name", id++);
					g.attribute("align", "left");
					g.node("text", [&] {
						g.append_quoted(String<32>(" Package power limit",
						                           lock ? " LOCKED" : ""));
					});
				});
			});

			_cpu_power_limit_headline(g, id, "");

			node.with_optional_sub_node("limit_1", [&](auto const &node) {
				_cpu_power_limit_common(g, node, id, " - 1");
			});

			node.with_optional_sub_node("limit_2", [&](auto const &node) {
				_cpu_power_limit_common(g, node, id, " - 2");
			});
		});

		limit.with_optional_sub_node("dram", [&](auto const &node) {
			_cpu_power_limit_dram_pp0_pp1(g, node, id, " DRAM power limit");
		});

		limit.with_optional_sub_node("pp0", [&](auto const &node) {
			_cpu_power_limit_dram_pp0_pp1(g, node, id, " PP0 power limit");
		});

		limit.with_optional_sub_node("pp1", [&](auto const &node) {
			_cpu_power_limit_dram_pp0_pp1(g, node, id, " PP1 power limit");
		});
	});
}


void Power::_cpu_perf_status_detail(Generator &g, Node const &node,
                                    char const * text, unsigned &id)
{
	double abs = 0, diff = 0;

	abs  = node.attribute_value("throttle_abs",  abs);
	diff = node.attribute_value("throttle_diff", diff);

	g.node("hbox", [&] {
		g.attribute("name", id++);

		g.node("label", [&] {
			g.attribute("name", id++);
			g.attribute("align", "left");
			g.node("text", [&] { g.append_quoted(text); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(String<48>("throttle current ", align_string(diff), "s")); });
		});
	});

	g.node("hbox", [&] {
		g.attribute("name", id++);
		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(String<48>("throttle absolut ", align_string(abs), "s")); });
		});
	});
}


void Power::_cpu_perf_status(Generator &g, Node const &status, unsigned &)
{
	unsigned id = 0;

	g.node("vbox", [&] {
		g.attribute("name", id++);

		status.with_optional_sub_node("package", [&](auto const &node) {
			_cpu_perf_status_detail(g, node, " Package perf status", id);
		});

		status.with_optional_sub_node("dram", [&](auto const &node) {
			_cpu_perf_status_detail(g, node, " DRAM perf status", id);
		});

		status.with_optional_sub_node("pp0", [&](auto const &node) {
			_cpu_perf_status_detail(g, node, " PP0 perf status", id);
		});
	});
}


bool Power::_cpu_residency_detail(Generator &g, Node const &node,
                                  char const * const text, unsigned &id)
{
	uint64_t tsc_raw = 0, ms_abs = 0, ms_diff = 0;

	tsc_raw = node.attribute_value("raw"    , tsc_raw);
	ms_abs  = node.attribute_value("abs_ms" , ms_abs);
	ms_diff = node.attribute_value("diff_ms", ms_diff);

	if (ms_abs == 0)
		return false;

	g.node("hbox", [&] {
		g.attribute("name", id++);

		g.node("label", [&] {
			g.attribute("name", id++);
			g.attribute("align", "left");
			g.node("text", [&] { g.append_quoted(text); });
		});

		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", id++);
			g.attribute("align", "right");

			String<4> unity_abs (ms_abs  >= 60ul*60*1000 ? "  h" :
			                     ms_abs  >=  2ul*60*1000 ? "  m" :
			                     ms_abs  >=  10'000      ? "  s" : " ms");
			String<4> unity_diff(ms_diff >=  10'000      ? "  s" : " ms");

			auto abs  = (ms_abs  >= 60ul * 60 * 1000) ? ms_abs / 1000 / 60 / 60 :
			            (ms_abs  >=  2ul * 60 * 1000) ? ms_abs  / 1000 / 60 :
			            (ms_abs  >=  10'000)          ? ms_abs  / 1000 : ms_abs;
			auto diff = (ms_diff >=  10'000) ? ms_diff / 1000 : ms_diff;

			g.node("text", [&] { g.append_quoted(String<60>("abs=",
			                                 align_string(abs), unity_abs,
			                                 " diff=",
			                                 align_string(diff), unity_diff)); });
		});
	});

	return true;
}


void Power::_cpu_residency(Generator &g, Node const &status, unsigned &)
{
	unsigned id = 0;

	unsigned const core[] = { 1, 3, 6, 7 };
	unsigned const pkg [] = { 2, 3, 6, 7, 8, 9, 10 };

	g.node("vbox", [&] {
		g.attribute("name", id++);

		g.node("hbox", [&] {
			g.attribute("name", id++);

			g.node("label", [&] {
				g.attribute("name", id++);
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(String<60>(" Package/Core C-state residency counters (try mwait!):")); });
			});

			g.node("button", [&] () {
				g.attribute("align", "right");
				g.attribute("name", "info_res");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("info"); });
				});

				if (_residency.hover)
					g.attribute("hovered", true);
				if (_residency.select)
					g.attribute("selected", true);
			});
		});

		if (!_residency.select)
			return;

		unsigned count = 0;
		auto id_before = id;

		for (auto const &entry : core) {
			status.with_optional_sub_node(String<8>("core_c", entry).string(), [&](auto const &node) {
				if (_cpu_residency_detail(g, node, String<16>(" Core C", entry).string(), id))
					count ++;
			});
		}

		for (auto const &entry : pkg) {
			status.with_optional_sub_node(String<8>("pkg_c", entry).string(), [&](auto const &node) {
				if (_cpu_residency_detail(g, node, String<16>(" Package C", entry).string(), id))
					count ++;
			});
		}

		if (!count || id == id_before) {
			g.node("hbox", [&] {
				g.attribute("name", id++);

				g.node("label", [&] {
					g.attribute("name", id++);
					g.attribute("align", "left");
					g.node("text", [&] { g.append_quoted(String<40>(" no counters available")); });
				});
			});
		}
	});
}


void Power::_cpu_mwait_detail(Generator &g, String<4> const &text,
                              uint8_t const c_state, uint8_t const sub_state)
{
		g.node("label", [&] {
			g.attribute("font", "monospace/regular");
			g.attribute("name", "mwait");
			g.attribute("align", "left");
			g.node("text", [&] { g.append_quoted(String<16>(" MWAIT hint ")); });
		});

		g.node("button", [&] () {
			g.attribute("name", "mwait_hlt");
			g.node("label", [&] () {
				g.node("text", [&] { g.append_quoted("hlt"); });
			});

			if (_hover_mwait && _mwait_button_hovered == "mwait_hlt")
				g.attribute("hovered", true);
			if (_mwait_button_selected == "mwait_hlt")
				g.attribute("selected", true);
		});

		for (uint8_t i = 0; i < sub_state; i++) {
			g.node("button", [&] () {
				auto name = sub_state > 1 ? String<16>("mwait_", text, "_", i)
				                          : String<16>("mwait_", text);
				g.attribute("name", name);
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted(					              sub_state > 1 ? String<16>(text, "_", i)
					                            : String<16>(text)); });
				});

				if (_hover_mwait && _mwait_button_hovered == name)
					g.attribute("hovered", true);
				if (_mwait_button_selected == name) {
					g.attribute("selected", true);
					_mwait_c_state     = c_state;
					_mwait_c_sub_state = i;
				}
			});
		}
}


void Power::_cpu_mwait(Generator &g, Node const &status, unsigned &)
{
	for (uint8_t c = 0; c < 8; c++) {
		String<4> mwait_c_state("c", c);
		status.with_optional_sub_node(mwait_c_state.string(), [&](auto const &node) {
			auto sub_count = node.attribute_value("sub_state_count", uint8_t(0u));
			if (!sub_count)
				return;

			_cpu_mwait_detail(g, mwait_c_state, c, sub_count);
		});
	}
}


void Power::_cpu_temp(Generator &g, Node const &cpu)
{
	auto const temp_c = cpu.attribute_value("temp_c", 0U);
	auto const cpuid  = cpu_id(cpu);

	g.node("hbox", [&] {
		g.attribute("name", cpuid);

		g.node("label", [&] {
			g.attribute("name", cpuid);
			g.attribute("align", "right");
			g.node("text", [&] { g.append_quoted(String<12>(" ", temp_c, " °C |")); });
		});
	});
}


void Power::_cpu_freq(Generator &g, Node const &cpu)
{
	auto const freq_khz = cpu.attribute_value("freq_khz", 0ULL);
	auto const cpuid    = cpu_id(cpu);

	g.node("hbox", [&] {
		g.attribute("name", cpuid);

		g.node("label", [&] {
			g.attribute("name", cpuid);
			g.attribute("align", "right");

			auto const rest = (freq_khz % 1000) / 10;
			g.node("text", [&] { g.append_quoted(String<16>(" ", freq_khz / 1000, ".",
			                                 rest < 10 ? "0" : "", rest, " MHz")); });
		});
	});
}


void Power::_cpu_setting(Generator &g, Node const &cpu)
{
	auto const cpuid = cpu_id(cpu);

	g.node("hbox", [&] {
		g.attribute("name", cpuid);
		g.node("button", [&] () {
			g.attribute("name", "settings");
			g.node("label", [&] () {
				g.node("text", [&] { g.append_quoted(""); });
			});

			if (_setting_hovered.value == cpuid)
				g.attribute("hovered", true);
			if (_setting_cpu.value == cpuid)
				g.attribute("selected", true);
		});
	});
}


void Power::_settings_mode(Generator &g)
{
	g.node("frame", [&] () {
		g.attribute("name", "frame_mode");

		g.node("hbox", [&] () {
			g.attribute("name", "mode");

			auto text = String<64>(" Settings:");

			g.node("label", [&] () {
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(text); });
			});

#if 1
			g.node("button", [&] () {
				g.attribute("align", "right");
				g.attribute("name", "normal");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("normal"); });
				});

				if (_hover_normal)
					g.attribute("hovered", true);
				if (_select_normal)
					g.attribute("selected", true);
			});
#endif

			g.node("button", [&] () {
				g.attribute("align", "right");
				g.attribute("name", "advanced");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("advanced"); });
				});

				if (_hover_advanced)
					g.attribute("hovered", true);
				if (_select_advanced)
					g.attribute("selected", true);
			});
		});
	});
}


void Power::_settings_period(Generator &g)
{
	g.node("frame", [&] () {
		g.attribute("name", "frame_period");

		g.node("hbox", [&] () {
			g.attribute("name", "period");

			auto text = String<64>(" Update period in ms:");

			g.node("label", [&] () {
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(text); });
			});

			hub(g, _timer_period, "period");

			g.node("label", [&] () {
				g.attribute("name", "b");
				g.attribute("align", "right");
				g.node("text", [&] { g.append_quoted(""); });
			});

			g.node("button", [&] () {
				g.attribute("align", "right");
				g.attribute("name", "apply_period");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("apply"); });
				});

				if (_apply_period)
					g.attribute("hovered", true);
				if (_apply_select_per)
					g.attribute("selected", true);
			});
		});
	});
}


void Power::_settings_amd(Generator &g, Node const &node,
                          bool const re_eval)
{
	unsigned min_value = node.attribute_value("ro_limit_cur", 0u);
	unsigned max_value = node.attribute_value("ro_max_value", 0u);
	unsigned cur_value = node.attribute_value("ro_status", 0u);

	_amd_pstate.set_min_max(min_value, max_value);

	if (re_eval)
		_amd_pstate.set(cur_value);

	g.node("frame", [&] () {
		g.attribute("name", "frame_pstate");

		g.node("hbox", [&] () {
			g.attribute("name", "pstate");

			auto text = String<64>("Hardware Performance-State: ");

			g.node("label", [&] () {
				g.attribute("name", "left");
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(text); });
			});

			g.node("button", [&] () {
				g.attribute("name", "pstate-max");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("max"); });
				});
				if (_pstate_max)
					g.attribute("hovered", true);
				if (_amd_pstate.value() == _amd_pstate.min())
					g.attribute("selected", true);
			});

			g.node("button", [&] () {
				g.attribute("name", "pstate-mid");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("mid"); });
				});
				if (_pstate_mid)
					g.attribute("hovered", true);
				if (_amd_pstate.value() == (_amd_pstate.max() - _amd_pstate.min() + 1) / 2)
					g.attribute("selected", true);
			});

			g.node("button", [&] () {
				g.attribute("name", "pstate-min");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("min"); });
				});
				if (_pstate_min)
					g.attribute("hovered", true);
				if (_amd_pstate.value() == _amd_pstate.max())
					g.attribute("selected", true);
			});

			if (_select_advanced) {
				if (_pstate_custom_sel) {
					auto text = String<64>(" range max-min [", min_value, "-",
					                       max_value, "] current=", cur_value);

					g.node("label", [&] () {
						g.attribute("name", "right");
						g.attribute("align", "right");
						g.node("text", [&] { g.append_quoted(text); });
					});

					hub(g, _amd_pstate, "pstate");
				}

				g.node("button", [&] () {
					g.attribute("name", "pstate-custom");
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted("custom"); });
					});
					if (_pstate_custom)
						g.attribute("hovered", true);
					if (_pstate_custom_sel)
						g.attribute("selected", true);
				});
			}
		});
	});
}


void Power::_settings_intel_epb(Generator  &g,
                                Node const &node,
                                bool const  re_read)
{
	unsigned const epb = node.attribute_value("raw", 0);

	g.node("frame", [&] () {
		g.attribute("name", "frame_speed_step");

		g.node("hbox", [&] () {
			g.attribute("name", "epb");

			auto text = String<64>(" Energy Performance Bias hint: ");

			g.node("label", [&] () {
				g.attribute("name", "left");
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(text); });
			});

			if (re_read)
				_intel_epb.set(epb);

			g.node("button", [&] () {
				g.attribute("name", "epb-perf");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("performance"); });
				});
				if (_epb_perf)
					g.attribute("hovered", true);
				if (_intel_epb.value() == EPB_PERF)
					g.attribute("selected", true);
			});

			g.node("button", [&] () {
				g.attribute("name", "epb-bala");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("balanced"); });
				});
				if (_epb_bala)
					g.attribute("hovered", true);
				if (_intel_epb.value() == EPB_BALANCED ||
				    _intel_epb.value() == EPB_BALANCED - 1)
					g.attribute("selected", true);
			});

			g.node("button", [&] () {
				g.attribute("name", "epb-ener");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("energy"); });
				});
				if (_epb_ener)
					g.attribute("hovered", true);
				if (_intel_epb.value() == EPB_POWER_SAVE)
					g.attribute("selected", true);
			});

			if (!_select_advanced)
				return;

			bool const extra_info = _epb_custom_select;

			if (extra_info) {
				auto text = String<64>(" range [", _intel_epb.min(), "-",
				                       _intel_epb.max(), "] current=", epb);

				g.node("label", [&] () {
					g.attribute("name", "right");
					g.attribute("align", "right");
					g.node("text", [&] { g.append_quoted(text); });
				});

				hub(g, _intel_epb, "epb");
			}

			g.node("button", [&] () {
				g.attribute("align", "right");
				g.attribute("name", "epb-custom");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("custom"); });
				});
				if (_epb_custom)
					g.attribute("hovered", true);
				if (extra_info || ((_intel_epb.value() != EPB_PERF) &&
				                   (_intel_epb.value() != EPB_POWER_SAVE) &&
				                   (_intel_epb.value() != EPB_BALANCED)))
					g.attribute("selected", true);
			});

		});
	});
}


void Power::_settings_intel_hwp(Generator &g, Node const &node, bool)
{
	bool enabled = node.attribute_value("enable", false);

	g.node("frame", [&] () {
		g.attribute("name", "frame_hwp");

		g.node("hbox", [&] () {
			g.attribute("name", "hwp");

			auto text = String<72>(" Intel HWP state: ",
			                       enabled ? "on" : "off",
			                       " - Once enabled stays until reset (Intel spec)");
			g.node("label", [&] () {
				g.attribute("align", "left");
				g.node("text", [&] { g.append_quoted(text); });
			});

			if (enabled)
				return;

			g.node("button", [&] () {
				g.attribute("name", "hwp_on");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("on"); });
				});

				if (_hwp_on_hovered)
					g.attribute("hovered", true);
				if (_hwp_on_selected)
					g.attribute("selected", true);
			});
		});
	});

	if (enabled && !_hwp_enabled_once)
		_hwp_enabled_once = true;
}


struct Hwp_request : Genode::Register<64> {
	struct Perf_min : Bitfield< 0, 8> { };
	struct Perf_max : Bitfield< 8, 8> { };
	struct Perf_des : Bitfield<16, 8> { };
	struct Perf_epp : Bitfield<24, 8> { };

	struct Activity_wnd  : Bitfield<32,10> { };
	struct Pkg_ctrl      : Bitfield<42, 1> { };
	struct Act_wnd_valid : Bitfield<59, 1> { };
	struct Epp_valid     : Bitfield<60, 1> { };
	struct Desired_valid : Bitfield<61, 1> { };
	struct Max_valid     : Bitfield<62, 1> { };
	struct Min_valid     : Bitfield<63, 1> { };
};


void Power::_settings_intel_hwp_req(Generator      &g,
                                    Node     const &node,
                                    unsigned const hwp_low,
                                    unsigned const hwp_high,
                                    uint64_t const hwp_req_pkg,
                                    bool     const hwp_req_pkg_valid,
                                    bool     const re_read,
                                    unsigned     & frames_count)
{
	auto const hwp_req = node.attribute_value("raw", 0ull);

	auto const hwp_min = unsigned(Hwp_request::Perf_min::get(hwp_req));
	auto const hwp_max = unsigned(Hwp_request::Perf_max::get(hwp_req));
	auto const hwp_des = unsigned(Hwp_request::Perf_des::get(hwp_req));
	auto const hwp_epp = unsigned(Hwp_request::Perf_epp::get(hwp_req));
	auto const act_wnd = unsigned(Hwp_request::Activity_wnd::get(hwp_req));

	auto const hwp_pkg_min = unsigned(Hwp_request::Perf_min::get(hwp_req_pkg));
	auto const hwp_pkg_max = unsigned(Hwp_request::Perf_max::get(hwp_req_pkg));
	auto const hwp_pkg_des = unsigned(Hwp_request::Perf_des::get(hwp_req_pkg));

	if (re_read)
	{
		_intel_hwp_min.set_min_max(hwp_low, hwp_high);
		_intel_hwp_max.set_min_max(hwp_low, hwp_high);
		_intel_hwp_des.set_min_max(hwp_low, hwp_high);

		/* read out features sometimes are not within hw range .oO */
		if (hwp_low <= hwp_min && hwp_min <= hwp_high)
			_intel_hwp_min.set(hwp_min);
		if (hwp_low <= hwp_max && hwp_max <= hwp_high)
			_intel_hwp_max.set(hwp_max);
		if (hwp_des <= hwp_high) {
			_intel_hwp_des.set(hwp_des);

			_hwp_req_auto_sel = hwp_des == 0;
		}

		_intel_hwp_epp.set(hwp_epp);

		_intel_hwp_pck_min.set_min_max(hwp_low, hwp_high);
		_intel_hwp_pck_max.set_min_max(hwp_low, hwp_high);
		_intel_hwp_pck_des.set_min_max(hwp_low, hwp_high);
	}

	if (_select_advanced) {
		frames_count ++;

		g.node("frame", [&] () {
			g.attribute("name", "frame_hwpreq");

			g.node("hbox", [&] () {
				g.attribute("name", "hwpreq");

				auto text = String<72>(" HWP CPU: [", hwp_min, "-", hwp_max, "] desired=",
				                       hwp_des, (hwp_des == 0) ? " (AUTO)" : "",
				                       (hwp_req >> 32) ? " IMPLEMENT ME:" : "");

				/* only relevant if HWP_REQ_PACKAGE is supported */
				if (Hwp_request::Pkg_ctrl::get(hwp_req))
					text = String<72>(text, "P");
				if (Hwp_request::Act_wnd_valid::get(hwp_req))
					text = String<72>(text, "A");
				if (Hwp_request::Epp_valid::get(hwp_req))
					text = String<72>(text, "E");
				if (Hwp_request::Desired_valid::get(hwp_req))
					text = String<72>(text, "D");
				if (Hwp_request::Max_valid::get(hwp_req))
					text = String<72>(text, "X");
				if (Hwp_request::Min_valid::get(hwp_req))
					text = String<72>(text, "N");

				g.node("label", [&] () {
					g.attribute("align", "left");
					g.attribute("name", 1);
					g.node("text", [&] { g.append_quoted(text); });
				});

				if (_hwp_req_cus_sel) {
					g.node("label", [&] () {
						g.attribute("align", "right");
						g.attribute("name", 2);
						g.node("text", [&] { g.append_quoted(String<16>(" min:")); });
					});
					hub(g, _intel_hwp_min, "hwp_min");

					g.node("label", [&] () {
						g.attribute("align", "right");
						g.attribute("name", 3);
						g.node("text", [&] { g.append_quoted(String<16>(" max:")); });
					});
					hub(g, _intel_hwp_max, "hwp_max");

					g.node("label", [&] () {
						g.attribute("align", "right");
						g.attribute("name", 4);
						g.node("text", [&] { g.append_quoted(String<16>(" desired:")); });
					});

					/* if auto on, hide button for individual values */
					if (!_hwp_req_auto_sel) {
						hub(g, _intel_hwp_des, "hwp_des");
					}

					g.node("button", [&] () {
						g.attribute("name", "hwp_req-auto");
						g.node("label", [&] () {
							g.node("text", [&] { g.append_quoted("auto"); });
						});
						if (_hwp_req_auto)
							g.attribute("hovered", true);
						if (_hwp_req_auto_sel)
							g.attribute("selected", true);
					});
				}

				g.node("button", [&] () {
				g.attribute("align", "right");
					g.attribute("name", "hwp_req-custom");
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted("custom"); });
					});
					if (_hwp_req_custom)
						g.attribute("hovered", true);

					if (_hwp_req_cus_sel)
						g.attribute("selected", true);
				});

			});
		});

		/* just show the values if hwp req package is available XXX */
		/* re-use code from  "hwp request" XXX */
		if (hwp_req_pkg_valid) {
			frames_count ++;
			g.node("frame", [&] () {
				g.attribute("name", "frame_hwpreq_pck");

				g.node("hbox", [&] () {
					g.attribute("name", "hwpreq_pck");

					auto text = String<72>(" Package: [", hwp_pkg_min, "-", hwp_pkg_max,
					                       "] desired=", hwp_pkg_des,
					                       (hwp_pkg_des == 0) ? " (AUTO)" : "");
					g.node("label", [&] () {
						g.attribute("align", "left");
						g.attribute("name", 1);
						g.node("text", [&] { g.append_quoted(text); });
					});

					if (_hwp_req_cus_sel) {
						g.node("label", [&] () {
							g.attribute("align", "right");
							g.attribute("name", 2);
							g.node("text", [&] { g.append_quoted(String<16>(" min:")); });
						});
						hub(g, _intel_hwp_pck_min, "hwp_pck_min");

						g.node("label", [&] () {
							g.attribute("align", "right");
							g.attribute("name", 3);
							g.node("text", [&] { g.append_quoted(String<16>(" max:")); });
						});
						hub(g, _intel_hwp_pck_max, "hwp_pck_max");

						g.node("label", [&] () {
							g.attribute("align", "right");
							g.attribute("name", 4);
							g.node("text", [&] { g.append_quoted(String<16>(" desired:")); });
						});
						hub(g, _intel_hwp_pck_des, "hwp_pck_des");
					}
				});
			});
		}
	}

	g.node("frame", [&] () {
		frames_count ++;
		g.attribute("name", "frame_hwpepp");

		g.node("hbox", [&] () {
			g.attribute("name", "hwpepp");

			g.node("label", [&] () {
				g.attribute("align", "left");
				g.attribute("name", "a");
				g.node("text", [&] { g.append_quoted(" Energy-Performance-Preference:"); });
			});

			g.node("button", [&] () {
				g.attribute("name", "hwp_epp-perf");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("performance"); });
				});
				if (_hwp_epp_perf)
					g.attribute("hovered", true);
				if (_intel_hwp_epp.value() == EPP_PERF)
					g.attribute("selected", true);
			});

			g.node("button", [&] () {
				g.attribute("name", "hwp_epp-bala");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("balanced"); });
				});
				if (_hwp_epp_bala)
					g.attribute("hovered", true);
				if (_intel_hwp_epp.value() == EPP_BALANCED ||
				    _intel_hwp_epp.value() == EPP_BALANCED - 1)
					g.attribute("selected", true);
			});

			g.node("button", [&] () {
				g.attribute("name", "hwp_epp-ener");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted("energy"); });
				});
				if (_hwp_epp_ener)
					g.attribute("hovered", true);
				if (_intel_hwp_epp.value() == EPP_ENERGY)
					g.attribute("selected", true);
			});

			bool const extra_info = _epp_custom_select && _select_advanced;

			if (extra_info) {
				g.node("vbox", [&] () {
					auto text = String<64>(" range [", _intel_hwp_epp.min(),
					                       "-", _intel_hwp_epp.max(),
					                       "] current=", hwp_epp);
					g.node("label", [&] () {
						g.attribute("align", "left");
						g.attribute("name", "a");
						g.node("text", [&] { g.append_quoted(text); });
					});
					g.node("label", [&] () {
						g.attribute("align", "left");
						g.attribute("name", "b");
						g.node("text", [&] { g.append_quoted(" (EPP - Energy-Performance-Preference)"); });
					});
					g.node("label", [&] () {
						g.attribute("align", "left");
						g.attribute("name", "c");
						g.node("text", [&] { g.append_quoted(String<22>(" Activity window=", act_wnd)); });
					});
				});

				hub(g, _intel_hwp_epp, "hwp_epp");
			}

			if (_select_advanced) {
				g.node("button", [&] () {
					g.attribute("align", "right");
					g.attribute("name", "hwp_epp-custom");
					g.node("label", [&] () {
						g.node("text", [&] { g.append_quoted("custom"); });
					});
					if (_hwp_epp_custom)
						g.attribute("hovered", true);
					if (extra_info || ((_intel_hwp_epp.value() != EPP_PERF)     &&
					                   (_intel_hwp_epp.value() != EPP_BALANCED) &&
					                   (_intel_hwp_epp.value() != EPP_ENERGY)))
						g.attribute("selected", true);
				});
			}

		});
	});
}


void Power::_settings_view(Generator &g, Node const &cpu,
                           String<12> const &cpuid, unsigned const cpu_count,
                           bool re_eval)
{
	bool     hwp_extension = false;
	unsigned frames   = 1; /* none - apply - all apply frame */
	unsigned hwp_high = 0;
	unsigned hwp_low  = 0;
	uint64_t hwp_req_pkg = 0;
	bool     hwp_req_pkg_valid = false;

	g.attribute("name", "settings");

	_settings_period(g);
	frames ++;

	_settings_mode(g);
	frames ++;

	cpu.for_each_sub_node([&](Genode::Node const &node) {

		if (node.type() == "pstate") {
			frames ++;
			_settings_amd(g, node, re_eval);
			return;
		}

		if (node.type() == "energy_perf_bias" && node.has_attribute("raw")) {
			frames ++;
			_settings_intel_epb(g, node, re_eval);
			return;
		}

		if (node.type() == "hwp") {
			frames ++;
			_settings_intel_hwp(g, node, re_eval);
			return;
		}

		if (node.type() == "hwp_cap") {

			hwp_extension = true;

			if (!_hwp_enabled_once)
				return;

			bool const extra_info = _select_advanced && _hwp_req_cus_sel;

			unsigned effi = node.attribute_value("effi" , 1);
			unsigned guar = node.attribute_value("guar" , 1);

			hwp_high = node.attribute_value("high" , 0);
			hwp_low  = node.attribute_value("low"  , 0);

			if (!_initial_hwp_cap) {
				re_eval = true;
				_initial_hwp_cap = true;
			}

			if (extra_info) {
				frames ++;

				g.node("frame", [&] () {
					g.attribute("name", "frame_hwpcap");

					g.node("hbox", [&] () {
						g.attribute("name", "hwpcap");

						g.node("vbox", [&] () {
							auto text = String<72>(" Intel HWP features: [", hwp_low,
							                       "-", hwp_high, "] efficient=", effi,
							                       " guaranty=", guar, " desired=0 (AUTO)");

							g.node("label", [&] () {
								g.attribute("align", "left");
								g.attribute("name", "a");
								g.node("text", [&] { g.append_quoted(text); });
							});
							g.node("label", [&] () {
								g.attribute("align", "left");
								g.attribute("name", "b");
								g.node("text", [&] { g.append_quoted(" performance & frequency range steering"); });
							});
						});
					});
				});
			}
			return;
		}

		if (node.type() == "hwp_request_package") {
			hwp_req_pkg_valid = true;
			hwp_req_pkg       = node.attribute_value("raw", 0ull);
		}

		if (node.type() == "hwp_request") {

			hwp_extension = true;

			if (!_hwp_enabled_once)
				return;

			_settings_intel_hwp_req(g, node, hwp_low, hwp_high, hwp_req_pkg,
			                        hwp_req_pkg_valid, re_eval, frames);
			return;
		}
	});

	if (_hwp_on_selected && !hwp_extension) {
		g.node("frame", [&] () {
			g.attribute("name", "frame_missing_hwp");

			g.node("hbox", [&] () {
				g.attribute("name", "hwp_extension");

				auto text = String<72>(" Intel HWP features available but HWP is off (not applied yet?)");

				g.node("label", [&] () {
					g.attribute("align", "left");
					g.attribute("name", "a");
					g.node("text", [&] { g.append_quoted(text); });
				});
			});
		});
	}

	cpu.with_optional_sub_node("energy", [&](Genode::Node const &energy) {
		frames ++;
		g.node("frame", [&] () {
			g.attribute("name", "rafl");

			g.node("hbox", [&] {
				g.attribute("name", "energy");

				_cpu_energy(g, energy, frames);
			});
		});
	});

	if (_select_rapl_detail) {
		cpu.with_optional_sub_node("power_info", [&](Genode::Node const &info) {
			frames ++;
			g.node("frame", [&] () {
				g.attribute("name", "info");

				g.node("hbox", [&] {
					g.attribute("name", "info");

					_cpu_power_info(g, info, frames);
				});
			});
		});

		cpu.with_optional_sub_node("power_limit", [&](Genode::Node const &info) {
			frames ++;
			g.node("frame", [&] () {
				g.attribute("name", "limit");

				g.node("hbox", [&] {
					g.attribute("name", "limit");

					_cpu_power_limit(g, info, frames);
				});
			});
		});
	}

/*
	<policy pp0="0x10" pp1="0x10"/>

	cpu.with_optional_sub_node("policy", [&](Genode::Node const &info) {
		frames ++;
		g.node("frame", [&] () {
			g.attribute("name", "policy");

			g.node("hbox", [&] {
				g.attribute("name", "policy");

			});
		});
	});
*/

	cpu.with_optional_sub_node("msr_residency", [&](Genode::Node const &info) {
		frames ++;
		g.node("frame", [&] () {
			g.attribute("name", "residency");

			g.node("hbox", [&] {
				g.attribute("name", "residency");

				_cpu_residency(g, info, frames);
			});
		});
	});

	cpu.with_optional_sub_node("mwait_support", [&](Genode::Node const &info) {
		frames ++;
		g.node("frame", [&] () {
			g.attribute("name", "mwait");

			g.node("hbox", [&] {
				g.attribute("name", "mwait");

				_cpu_mwait(g, info, frames);
			});
		});
	});

	cpu.with_optional_sub_node("perf_status", [&](Genode::Node const &info) {
		frames ++;
		g.node("frame", [&] () {
			g.attribute("name", "perf");

			g.node("hbox", [&] {
				g.attribute("name", "perf");

				_cpu_perf_status(g, info, frames);
			});
		});
	});

	for (unsigned i = 0; i < 1 + ((cpu_count > frames) ? cpu_count - frames : 0); i++) {
		g.node("frame", [&] () {
			g.attribute("style", "invisible");
			g.attribute("name", String<15>("frame_space_", i));

			g.node("hbox", [&] () {
				g.attribute("name", "space");

				g.node("label", [&] () {
					g.attribute("align", "left");
					g.node("text", [&] { g.append_quoted(""); });
				});
			});
		});
	}

	g.node("hbox", [&] () {
		g.node("label", [&] () {
			g.node("text", [&] { g.append_quoted("Apply to:"); });
		});

		g.node("button", [&] () {
			g.attribute("name", "none");
			g.node("label", [&] () {
				g.node("text", [&] { g.append_quoted("none"); });
			});

			if (_none_hovered)
				g.attribute("hovered", true);
			if (!_apply_select && !_apply_all_select)
				g.attribute("selected", true);
		});

		if (_select_advanced) {
			g.node("button", [&] () {
				g.attribute("name", "apply");
				g.node("label", [&] () {
					g.node("text", [&] { g.append_quoted(cpuid); });
				});

				if (_apply_hovered)
					g.attribute("hovered", true);
				if (_apply_select)
					g.attribute("selected", true);
			});
		}

		g.node("button", [&] () {
			g.attribute("name", "applyall");
			g.node("label", [&] () {
				g.node("text", [&] { g.append_quoted("all CPUs"); });
			});

			if (_apply_all_hovered)
				g.attribute("hovered", true);
			if (_apply_all_select)
				g.attribute("selected", true);
		});
	});
}

void Component::construct(Genode::Env &env) { static Power state(env); }
