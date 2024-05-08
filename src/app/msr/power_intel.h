/*
 * \author Alexander Boettcher
 * \date   2021-10-25
 */

/*
 * Copyright (C) 2021-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include "cpuid.h"

namespace Msr {
	using Genode::uint64_t;
	using Genode::uint8_t;
	struct Power_intel;
}

struct Msr::Power_intel
{
	Cpuid cpuid { };

	uint64_t hwp_cap     { };
	uint64_t hwp_req_pkg { };
	uint64_t hwp_req     { };
	uint64_t epb         { };

	uint64_t misc_enable { };

	uint64_t perf_status { };
	uint64_t perf_ctl    { };

	uint64_t msr_rapl_units  { };
	uint64_t msr_pkg_energy  { };
	uint64_t msr_dram_energy { };
	uint64_t msr_pp0_energy  { };
	uint64_t msr_pp1_energy  { };

	uint64_t msr_pkg_energy_prev  { };
	uint64_t msr_dram_energy_prev { };
	uint64_t msr_pp0_energy_prev  { };
	uint64_t msr_pp1_energy_prev  { };

	uint64_t msr_pkg_perf  { };
	uint64_t msr_pp0_perf  { };
	uint64_t msr_dram_perf { };

	uint64_t msr_pkg_perf_prev  { };
	uint64_t msr_pp0_perf_prev  { };
	uint64_t msr_dram_perf_prev { };

	uint64_t msr_pkg_limits  { };
	uint64_t msr_dram_limits { };
	uint64_t msr_pp0_limits  { };
	uint64_t msr_pp1_limits  { };

	uint64_t msr_pkg_power_info  { };
	uint64_t msr_dram_power_info { };

	uint64_t msr_pp0_policy { };
	uint64_t msr_pp1_policy { };

	uint64_t msr_core_c1 { };
	uint64_t msr_core_c3 { };
	uint64_t msr_core_c6 { };
	uint64_t msr_core_c7 { };

	uint64_t msr_core_c1_prev { };
	uint64_t msr_core_c3_prev { };
	uint64_t msr_core_c6_prev { };
	uint64_t msr_core_c7_prev { };

	uint64_t msr_pkg_c2  { };
	uint64_t msr_pkg_c3  { };
	uint64_t msr_pkg_c6  { };
	uint64_t msr_pkg_c7  { };
	uint64_t msr_pkg_c8  { };
	uint64_t msr_pkg_c9  { };
	uint64_t msr_pkg_c10 { };

	uint64_t msr_pkg_c2_prev  { };
	uint64_t msr_pkg_c3_prev  { };
	uint64_t msr_pkg_c6_prev  { };
	uint64_t msr_pkg_c7_prev  { };
	uint64_t msr_pkg_c8_prev  { };
	uint64_t msr_pkg_c9_prev  { };
	uint64_t msr_pkg_c10_prev { };

	bool valid_hwp_cap     { };
	bool valid_hwp_req_pkg { };
	bool valid_hwp_req     { };
	bool valid_epb         { };

	bool valid_perf_status { };
	bool valid_perf_ctl    { };
	bool valid_misc_enable { };

	bool enabled_hwp { };
	bool init_done   { };

	bool valid_msr_rapl_units  { };

	bool valid_msr_pkg_energy  { };
	bool valid_msr_dram_energy { };
	bool valid_msr_pp0_energy  { };
	bool valid_msr_pp1_energy  { };

	bool valid_msr_pkg_perf    { };
	bool valid_msr_dram_perf   { };
	bool valid_msr_pp0_perf    { };

	bool valid_msr_pkg_limits  { };
	bool valid_msr_dram_limits { };
	bool valid_msr_pp0_limits  { };
	bool valid_msr_pp1_limits  { };

	bool valid_msr_pkg_power_info  { };
	bool valid_msr_dram_power_info { };

	bool valid_msr_pp0_policy  { };
	bool valid_msr_pp1_policy  { };

	bool features_perf_server     { true };
	bool features_rapl            { true };
	bool features_perf_status     { true };
	bool features_dram            { true };
	bool features_dram_power_info { true };

	bool valid_core_c1 { };
	bool valid_core_c3 { };
	bool valid_core_c6 { };
	bool valid_core_c7 { };

	bool features_core_c1 { true };
	bool features_core_c3 { true };
	bool features_core_c6 { true };
	bool features_core_c7 { true };

	bool valid_pkg_c2  { };
	bool valid_pkg_c3  { };
	bool valid_pkg_c6  { };
	bool valid_pkg_c7  { };
	bool valid_pkg_c8  { };
	bool valid_pkg_c9  { };
	bool valid_pkg_c10 { };

	bool features_pkg_c2  { true };
	bool features_pkg_c3  { true };
	bool features_pkg_c6  { true };
	bool features_pkg_c7  { true };
	bool features_pkg_c8  { true };
	bool features_pkg_c9  { true };
	bool features_pkg_c10 { true };

	Trace::Timestamp energy_timestamp      { };
	Trace::Timestamp energy_timestamp_prev { };

	Trace::Timestamp perf_timestamp      { };
	Trace::Timestamp perf_timestamp_prev { };

	struct Hwp_cap : Genode::Register<64> {
		struct Perf_highest   : Bitfield< 0, 8> { };
		struct Perf_guaranted : Bitfield< 8, 8> { };
		struct Perf_most_eff  : Bitfield<16, 8> { };
		struct Perf_lowest    : Bitfield<24, 8> { };
	};

	struct Hwp_request : Genode::Register<64> {
		struct Perf_min     : Bitfield< 0, 8> { };
		struct Perf_max     : Bitfield< 8, 8> { };
		struct Perf_desired : Bitfield<16, 8> { };
		struct Perf_epp     : Bitfield<24, 8> {
			enum { PERFORMANCE = 0, BALANCED = 128, ENERGY = 255 };
		};
		struct Activity_wnd  : Bitfield<32,10> { };
		struct Pkg_ctrl      : Bitfield<42, 1> { };
		struct Act_wnd_valid : Bitfield<59, 1> { };
		struct Epp_valid     : Bitfield<60, 1> { };
		struct Desired_valid : Bitfield<61, 1> { };
		struct Max_valid     : Bitfield<62, 1> { };
		struct Min_valid     : Bitfield<63, 1> { };
	};

	struct Epb : Genode::Register<64> {
		struct Hint : Bitfield<0, 4> {
			enum { PERFORMANCE = 0, BALANCED = 7, POWER_SAVING = 15 };
		};
	};

	struct Msr_rapl_units : Genode::Register<64> {
		struct Power  : Bitfield< 0, 4> { };
		struct Energy : Bitfield< 8, 5> { };
		struct Time   : Bitfield<16, 4> { };
	};

	struct Msr_pkg_power_info : Genode::Register<64> {
		struct Thermal_spec_power : Bitfield< 0, 15> { };
		struct Minimum_power      : Bitfield<16, 15> { };
		struct Maximum_power      : Bitfield<32, 15> { };
		struct Max_time_window    : Bitfield<48,  6> { };
	};

	struct Msr_pkg_power_limit : Genode::Register<64> {
		struct Power_1      : Bitfield< 0, 15> { };
		struct Enable_1     : Bitfield<15,  1> { };
		struct Clamp_1      : Bitfield<16,  1> { };
		struct Time_wnd_y_1 : Bitfield<17,  5> { };
		struct Time_wnd_z_1 : Bitfield<22,  2> { };
		struct Power_2      : Bitfield<32, 15> { };
		struct Enable_2     : Bitfield<47,  1> { };
		struct Clamp_2      : Bitfield<48,  1> { };
		struct Time_wnd_y_2 : Bitfield<49,  5> { };
		struct Time_wnd_z_2 : Bitfield<54,  2> { };
		struct Lock         : Bitfield<63,  1> { };
	};

	/* PP0, PP1, DRAM */
	struct Msr_power_limit : Genode::Register<64> {
		struct Power      : Bitfield< 0, 15> { };
		struct Enable     : Bitfield<15,  1> { };
		struct Clamp      : Bitfield<16,  1> { };
		struct Time_wnd_y : Bitfield<17,  5> { };
		struct Time_wnd_f : Bitfield<22,  2> { };
		struct Lock       : Bitfield<31,  1> { };
	};

	enum {
		SET_MWAIT_HINT = 0,

		/*
		 * Intel Speed Step - chapter 14.1
		 *
		 * - IA32_PERF_CTL = 0x199
		 *
		 * gets disabled, as soon as Intel HWP is enabled
		 * - see 14.4.2 Enabling HWP
		 */

		IA32_MISC_ENABLE       = 0x1a0,

		IA32_ENERGY_PERF_BIAS  = 0x1b0,

		IA32_PERF_STATUS       = 0x198,
		IA32_PERF_CTL          = 0x199,

		MSR_RAPL_POWER_UNIT    = 0x606,

		/* 14.10.3 RAPL */
		MSR_PKG_POWER_LIMIT    = 0x610,
		MSR_PKG_ENERGY_STATUS  = MSR_PKG_POWER_LIMIT + 1,
		MSR_PKG_PERF_STATUS    = MSR_PKG_POWER_LIMIT + 3,
		MSR_PKG_POWER_INFO     = MSR_PKG_POWER_LIMIT + 4,

		/* 14.10.5 RAPL - solely server platforms */
		MSR_DRAM_POWER_LIMIT   = 0x618,
		MSR_DRAM_ENERGY_STATUS = MSR_DRAM_POWER_LIMIT + 1,
		MSR_DRAM_PERF_STATUS   = MSR_DRAM_POWER_LIMIT + 3,
		MSR_DRAM_POWER_INFO    = MSR_DRAM_POWER_LIMIT + 4,

		/* 14.10.4 RAPL - on client platform it refers in general to processor cores */
		MSR_PP0_POWER_LIMIT    = 0x638,
		MSR_PP0_ENERGY_STATUS  = MSR_PP0_POWER_LIMIT + 1,
		MSR_PP0_POLICY         = MSR_PP0_POWER_LIMIT + 2,
		MSR_PP0_PERF_STATUS    = MSR_PP0_POWER_LIMIT + 3,

		/* 14.10.4 RAPL - on client platforms, some specific device in uncore area */
		MSR_PP1_POWER_LIMIT    = 0x640,
		MSR_PP1_ENERGY_STATUS  = MSR_PP1_POWER_LIMIT + 1,
		MSR_PP1_POLICY         = MSR_PP1_POWER_LIMIT + 2,

		IA32_PM_ENABLE        = 0x770, 
		IA32_HWP_CAPABILITIES = 0x771, 
		IA32_HWP_REQUEST_PKG  = 0x772, 
		IA32_HWP_REQUEST      = 0x774,

		/*
		 * Intel spec
		 * - IA32_POWER_CTL = 0x1fc -> http://biosbits.org
		 *
		 * C1E Enable (R/W)
		 * When set to ‘1’, will enable the CPU to switch to the
		 * Minimum Enhanced Intel SpeedStep Technology
		 * operating point when all execution cores enter MWAIT.
		 */

		MSR_CORE_C1_RESIDENCY   = 0x660,
		MSR_CORE_C3_RESIDENCY   = 0x3fc,
		MSR_CORE_C6_RESIDENCY   = 0x3fd,
		MSR_CORE_C7_RESIDENCY   = 0x3fe,

		MSR_PKG_C2_RESIDENCY    = 0x60d,
		MSR_PKG_C3_RESIDENCY    = 0x3f8,
		MSR_PKG_C6_RESIDENCY    = 0x3f9,
		MSR_PKG_C7_RESIDENCY    = 0x3fa,
		MSR_PKG_C8_RESIDENCY    = 0x630,
		MSR_PKG_C9_RESIDENCY    = 0x631,
		MSR_PKG_C10_RESIDENCY   = 0x632,
	};

	bool hwp_enabled(System_control &system)
	{
		System_control::State state { };

		system.add_rdmsr(state, IA32_PM_ENABLE);

		state = system.system_control(state);

		uint64_t pm_enable = 0;
		addr_t   success   = 0;
		bool     result    = system.get_state(state, success, &pm_enable);

		return result && (success & 1) && (pm_enable & 1);
	}

	void read_enhanced_speedstep(System_control &system)
	{
		System_control::State state { };

		system.add_rdmsr(state, IA32_PERF_STATUS);
		system.add_rdmsr(state, IA32_PERF_CTL);
		system.add_rdmsr(state, IA32_MISC_ENABLE);

		state = system.system_control(state);

		addr_t success = 0;
		bool   result  = system.get_state(state, success, &perf_status,
		                                  &perf_ctl, &misc_enable);

		valid_perf_status = result && (success & 1);
		valid_perf_ctl    = result && (success & 2);
		valid_misc_enable = result && (success & 4);
	}

	void read_epb(System_control &system)
	{
		System_control::State state { };

		system.add_rdmsr(state, IA32_ENERGY_PERF_BIAS);

		state = system.system_control(state);

		addr_t success = 0;
		bool   result  = system.get_state(state, success, &epb);

		valid_epb = result && (success & 1);
	}

	bool write_epb(System_control &system, uint64_t const &value) const
	{
		System_control::State state { };

		system.add_wrmsr(state, IA32_ENERGY_PERF_BIAS, value);

		state = system.system_control(state);

		addr_t success = 0;
		bool   result  = system.get_state(state, success);

		return result && (success & 1);
	}

	bool enable_hwp(System_control &system) const
	{
		System_control::State state { };

		system.add_wrmsr(state, IA32_PM_ENABLE, 1ull);

		state = system.system_control(state);

		addr_t success = 0;
		bool   result  = system.get_state(state, success);

		return result && (success == 1);
	}

	bool write_hwp_request(System_control &system, uint64_t const &value) const
	{
		System_control::State state { };

		system.add_wrmsr(state, IA32_HWP_REQUEST, value);

		state = system.system_control(state);

		addr_t success = 0;
		bool   result  = system.get_state(state, success);

		return result && (success == 1);
	}

	void read_hwp(System_control &system)
	{
		System_control::State state { };

		system.add_rdmsr(state, IA32_HWP_CAPABILITIES);
		system.add_rdmsr(state, IA32_HWP_REQUEST_PKG);
		system.add_rdmsr(state, IA32_HWP_REQUEST);

		state = system.system_control(state);

		addr_t success = 0;
		bool    result = system.get_state(state, success, &hwp_cap,
		                                  &hwp_req_pkg, &hwp_req);

		valid_hwp_cap     = result && (success & 1);
		valid_hwp_req_pkg = result && (success & 2);
		valid_hwp_req     = result && (success & 4);
	}

	void read_energy_status(System_control &system)
	{
		if (!features_rapl)
			return;

		System_control::State state { };

		system.add_rdmsr(state, MSR_RAPL_POWER_UNIT);
		system.add_rdmsr(state, MSR_PKG_ENERGY_STATUS);
		system.add_rdmsr(state, MSR_PP0_ENERGY_STATUS);
		system.add_rdmsr(state, MSR_PP1_ENERGY_STATUS);
		if (features_dram)
			system.add_rdmsr(state, MSR_DRAM_ENERGY_STATUS);

		state = system.system_control(state);

		msr_pkg_energy_prev  = msr_pkg_energy;
		msr_dram_energy_prev = msr_dram_energy;
		msr_pp0_energy_prev  = msr_pp0_energy;
		msr_pp1_energy_prev  = msr_pp1_energy;

		energy_timestamp_prev = energy_timestamp;
		energy_timestamp      = Trace::timestamp();

		addr_t success = 0;
		bool        ok = system.get_state(state, success, &msr_rapl_units,
		                                  &msr_pkg_energy, &msr_pp0_energy,
		                                  &msr_pp1_energy, &msr_dram_energy);

		valid_msr_rapl_units  = ok && (success & (1u << 0));
		valid_msr_pkg_energy  = ok && (success & (1u << 1));
		valid_msr_pp0_energy  = ok && (success & (1u << 2));
		valid_msr_pp1_energy  = ok && (success & (1u << 3));
		valid_msr_dram_energy = ok && (success & (1u << 4));

		if (!valid_msr_rapl_units)
			features_rapl = false;

		if (features_dram && !valid_msr_dram_energy)
			features_dram = false;
	}

	void read_perf_status(System_control &system)
	{
		if (!features_rapl)
			return;

		if (!features_perf_status && !features_perf_server && !features_dram)
			return;

		System_control::State state { };

		if (features_perf_status)
			system.add_rdmsr(state, MSR_PKG_PERF_STATUS);
		if (features_perf_server)
			system.add_rdmsr(state, MSR_PP0_PERF_STATUS);
		if (features_dram)
			system.add_rdmsr(state, MSR_DRAM_PERF_STATUS);

		state = system.system_control(state);

		msr_pkg_perf_prev  = msr_pkg_perf;
		msr_pp0_perf_prev  = msr_pp0_perf;
		msr_dram_perf_prev = msr_dram_perf;

		perf_timestamp_prev = perf_timestamp;
		perf_timestamp      = Trace::timestamp();

		uint64_t msr_values[3] { };

		addr_t success = 0;
		bool const  ok = system.get_state(state, success, &msr_values[0],
		                                  &msr_values[1], &msr_values[2]);

		unsigned bit = 0;

		if (features_perf_status) {
			msr_pkg_perf       = msr_values[bit];
			valid_msr_pkg_perf = ok && (success & (1u << bit++));
		}

		if (features_perf_server) {
			msr_pp0_perf       = msr_values[bit];
			valid_msr_pp0_perf = ok && (success & (1u << bit++));
		}

		if (features_dram) {
			msr_dram_perf       = msr_values[bit];
			valid_msr_dram_perf = ok && (success & (1u << bit++));
		}

		if (features_dram && !valid_msr_dram_perf)
			features_dram = false;

		if (features_perf_server && !valid_msr_pp0_perf)
			features_perf_server = false;

		if (features_perf_status && !valid_msr_pkg_perf)
			features_perf_status = false;
	}

	void read_power_limits(System_control &system)
	{
		if (!features_rapl)
			return;

		System_control::State state { };

		system.add_rdmsr(state, MSR_PKG_POWER_LIMIT);
		system.add_rdmsr(state, MSR_PP0_POWER_LIMIT);
		system.add_rdmsr(state, MSR_PP1_POWER_LIMIT);
		if (features_dram)
			system.add_rdmsr(state, MSR_DRAM_POWER_LIMIT);

		state = system.system_control(state);

		addr_t success = 0;
		bool        ok = system.get_state(state, success, &msr_pkg_limits,
		                                  &msr_pp0_limits, &msr_pp1_limits,
		                                  &msr_dram_limits);

		valid_msr_pkg_limits      = ok && (success & (1u << 0));
		valid_msr_pp0_limits      = ok && (success & (1u << 1));
		valid_msr_pp1_limits      = ok && (success & (1u << 2));
		if (features_dram)
			valid_msr_dram_limits = ok && (success & (1u << 3));

		if (features_dram && !valid_msr_dram_limits)
			features_dram = false;
	}

	void read_power_info(System_control &system)
	{
		if (!features_rapl)
			return;

		System_control::State state { };

		system.add_rdmsr(state, MSR_PKG_POWER_INFO);
		if (features_dram_power_info)
			system.add_rdmsr(state, MSR_DRAM_POWER_INFO);

		state = system.system_control(state);

		addr_t success = 0;
		bool        ok = system.get_state(state, success, &msr_pkg_power_info,
		                                  &msr_dram_power_info);

		valid_msr_pkg_power_info      = ok && (success & (1u << 0));
		if (features_dram_power_info)
			valid_msr_dram_power_info = ok && (success & (1u << 1));

		if (features_dram_power_info && !valid_msr_dram_power_info)
			features_dram_power_info = false;
	}

	void read_policy(System_control &system)
	{
		if (!features_rapl)
			return;

		System_control::State state { };

		system.add_rdmsr(state, MSR_PP0_POLICY);
		system.add_rdmsr(state, MSR_PP1_POLICY);

		state = system.system_control(state);

		addr_t success = 0;
		bool        ok = system.get_state(state, success, &msr_pp0_policy,
		                                  &msr_pp1_policy);

		valid_msr_pp0_policy = ok && (success & (1u << 0));
		valid_msr_pp1_policy = ok && (success & (1u << 1));
	}

	void read_mwait_core(System_control &system)
	{
		if (!features_core_c1 && !features_core_c3 &&
		    !features_core_c6 && !features_core_c7)
			return;

		System_control::State state { };

		if (features_core_c1) system.add_rdmsr(state, MSR_CORE_C1_RESIDENCY);
		if (features_core_c3) system.add_rdmsr(state, MSR_CORE_C3_RESIDENCY);
		if (features_core_c6) system.add_rdmsr(state, MSR_CORE_C6_RESIDENCY);
		if (features_core_c7) system.add_rdmsr(state, MSR_CORE_C7_RESIDENCY);

		state = system.system_control(state);

		uint64_t msr_core_c[4] {};

		addr_t success = 0;
		bool   result  = system.get_state(state, success,
		                                  &msr_core_c[0], &msr_core_c[1],
		                                  &msr_core_c[2], &msr_core_c[3]);

		unsigned bit = 0;

		if (features_core_c1) {
			msr_core_c1_prev = msr_core_c1;
			msr_core_c1      = msr_core_c[bit];
			valid_core_c1    = result && (success & (1 << bit++));
		}
		if (features_core_c3) {
			msr_core_c3_prev = msr_core_c3;
			msr_core_c3      = msr_core_c[bit];
			valid_core_c3    = result && (success & (1 << bit++));
		}
		if (features_core_c6) {
			msr_core_c6_prev = msr_core_c6;
			msr_core_c6      = msr_core_c[bit];
			valid_core_c6    = result && (success & (1 << bit++));
		}
		if (features_core_c7) {
			msr_core_c7_prev = msr_core_c7;
			msr_core_c7      = msr_core_c[bit];
			valid_core_c7    = result && (success & (1 << bit++));
		}

		if (features_core_c1 && !valid_core_c1) features_core_c1 = false;
		if (features_core_c3 && !valid_core_c3) features_core_c3 = false;
		if (features_core_c6 && !valid_core_c6) features_core_c6 = false;
		if (features_core_c7 && !valid_core_c7) features_core_c7 = false;
	}

	void read_mwait_pkg(System_control &system)
	{
		if (!features_pkg_c2 && !features_pkg_c3 && !features_pkg_c6 &&
		    !features_pkg_c7 && !features_pkg_c8 && !features_pkg_c9 &&
		    !features_pkg_c10)
			return;

		System_control::State state { };

		if (features_pkg_c2)  system.add_rdmsr(state, MSR_PKG_C2_RESIDENCY);
		if (features_pkg_c3)  system.add_rdmsr(state, MSR_PKG_C3_RESIDENCY);
		if (features_pkg_c6)  system.add_rdmsr(state, MSR_PKG_C6_RESIDENCY);
		if (features_pkg_c7)  system.add_rdmsr(state, MSR_PKG_C7_RESIDENCY);
		if (features_pkg_c8)  system.add_rdmsr(state, MSR_PKG_C8_RESIDENCY);
		if (features_pkg_c9)  system.add_rdmsr(state, MSR_PKG_C9_RESIDENCY);
		if (features_pkg_c10) system.add_rdmsr(state, MSR_PKG_C10_RESIDENCY);

		state = system.system_control(state);

		uint64_t msr_pkg_c[7] {};

		addr_t success = 0;
		bool   result  = system.get_state(state, success,
		                                  &msr_pkg_c[0], &msr_pkg_c[1],
		                                  &msr_pkg_c[2], &msr_pkg_c[3],
		                                  &msr_pkg_c[4], &msr_pkg_c[5],
		                                  &msr_pkg_c[6]);

		unsigned bit = 0;

		if (features_pkg_c2) {
			msr_pkg_c2_prev = msr_pkg_c2;
			msr_pkg_c2      = msr_pkg_c[bit];
			valid_pkg_c2    = result && (success & (1 << bit++));
		}
		if (features_pkg_c3) {
			msr_pkg_c3_prev = msr_pkg_c3;
			msr_pkg_c3      = msr_pkg_c[bit];
			valid_pkg_c3    = result && (success & (1 << bit++));
		}
		if (features_pkg_c6) {
			msr_pkg_c6_prev = msr_pkg_c6;
			msr_pkg_c6      = msr_pkg_c[bit];
			valid_pkg_c6    = result && (success & (1 << bit++));
		}
		if (features_pkg_c7) {
			msr_pkg_c7_prev = msr_pkg_c7;
			msr_pkg_c7      = msr_pkg_c[bit];
			valid_pkg_c7    = result && (success & (1 << bit++));
		}
		if (features_pkg_c8) {
			msr_pkg_c8_prev = msr_pkg_c8;
			msr_pkg_c8      = msr_pkg_c[bit];
			valid_pkg_c8    = result && (success & (1 << bit++));
		}
		if (features_pkg_c9) {
			msr_pkg_c9_prev = msr_pkg_c9;
			msr_pkg_c9      = msr_pkg_c[bit];
			valid_pkg_c9    = result && (success & (1 << bit++));
		}
		if (features_pkg_c10) {
			msr_pkg_c10_prev = msr_pkg_c10;
			msr_pkg_c10      = msr_pkg_c[bit];
			valid_pkg_c10    = result && (success & (1 << bit++));
		}

		if (features_pkg_c2  && !valid_pkg_c2)  features_pkg_c2  = false;
		if (features_pkg_c3  && !valid_pkg_c3)  features_pkg_c3  = false;
		if (features_pkg_c6  && !valid_pkg_c6)  features_pkg_c6  = false;
		if (features_pkg_c7  && !valid_pkg_c7)  features_pkg_c7  = false;
		if (features_pkg_c8  && !valid_pkg_c8)  features_pkg_c8  = false;
		if (features_pkg_c9  && !valid_pkg_c9)  features_pkg_c9  = false;
		if (features_pkg_c10 && !valid_pkg_c10) features_pkg_c10 = false;
	}

	bool write_mwait_hint(System_control &system, uint64_t const &value) const
	{
		System_control::State state { };

		system.add_wrmsr(state, SET_MWAIT_HINT, value);

		state = system.system_control(state);

		addr_t success = 0;
		bool   result  = system.get_state(state, success);

		return result && (success == 1);
	}

	void update(System_control &system)
	{
		if (cpuid.hwp()) {
			if (!init_done) {
				enabled_hwp = hwp_enabled(system);
				init_done   = true;
			}

			if (enabled_hwp)
				read_hwp(system);
		}

#if 0
		/* todo: if one wants to support pre HWP machines, start here */
		if (!enabled_hwp)
			read_enhanced_speedstep(system);
#endif

		if (cpuid.hwp_energy_perf_bias())
			read_epb(system);
	}

	void update_package(System_control &system)
	{
#if 0
		Genode::log("family ", Genode::Hex(cpuid.family_id()));
		Genode::log("family ", Genode::Hex(cpuid.model_id()));
#endif

		read_energy_status (system);
		read_perf_status   (system);
		read_power_info    (system);
		read_power_limits  (system);
		read_policy        (system);
		read_mwait_core    (system);
		read_mwait_pkg     (system);
	}

	void update(System_control &system, Genode::Xml_node const &config, Genode::Affinity::Location const &cpu)
	{
		bool const verbose = config.attribute_value("verbose", false);

		config.with_optional_sub_node("energy_perf_bias", [&] (Genode::Xml_node const &node) {
			if (!cpuid.hwp_energy_perf_bias())
				return;

			unsigned epb_set = node.attribute_value("raw", ~0U);

			if (Epb::Hint::PERFORMANCE <= epb_set &&
			    epb_set <= Epb::Hint::POWER_SAVING) {

				uint64_t raw_epb = epb;
				Epb::Hint::set(raw_epb, epb_set);

				if (write_epb(system, raw_epb))
					read_epb(system);
				else
					Genode::warning(cpu, " epb not updated");
			} else
				if (verbose && epb_set != ~0U)
					Genode::warning(cpu, " epb out of range [",
					                int(Epb::Hint::PERFORMANCE), "-",
					                int(Epb::Hint::POWER_SAVING), "]");
		});

		config.with_optional_sub_node("hwp", [&] (auto const &node) {
			if (!cpuid.hwp())
				return;

			if (!node.has_attribute("enable"))
				return;

			bool on = node.attribute_value("enable", false);

			if (on && !enabled_hwp) {
				bool ok = enable_hwp(system);
				Genode::log(cpu, " enabling HWP ", ok ? " succeeded" : " failed");
			} else
			if (!on && enabled_hwp)
				Genode::log(cpu, " disabling HWP not supported - see Intel spec");

			enabled_hwp = hwp_enabled(system);
		});

		config.with_optional_sub_node("hwp_request", [&] (auto const &node) {
			if (!enabled_hwp)
				return;

			if (!valid_hwp_req)
				return;

			if (!cpuid.hwp_energy_perf_pref())
				return;

			using Genode::warning;
			using Genode::Hex;

			uint8_t const low  = uint8_t(Hwp_cap::Perf_lowest::get(hwp_cap));
			uint8_t const high = uint8_t(Hwp_cap::Perf_highest::get(hwp_cap));

			uint64_t raw_hwp = hwp_req;

			if (node.has_attribute("min")) {
				unsigned value = node.attribute_value("min", low);
				if ((low <= value) && (value <= high))
					Hwp_request::Perf_min::set(raw_hwp, value);
				else
					if (verbose)
						warning(cpu, " min - out of range - ", value, " [",
						        low, "-", high, "]");
			}
			if (node.has_attribute("max")) {
				unsigned value = node.attribute_value("max", high);
				if ((low <= value) && (value <= high))
					Hwp_request::Perf_max::set(raw_hwp, value);
				else
					if (verbose)
						warning(cpu, " max - out of range - ", value, " [",
						        low, "-", high, "]");
			}
			if (node.has_attribute("desired")) {
				unsigned value = node.attribute_value("desired", 0u /* disable */);
				if (!value || ((low <= value) && (value <= high)))
					Hwp_request::Perf_desired::set(raw_hwp, value);
				else
					if (verbose)
						warning(cpu, " desired - out of range - ", value, " [",
						        low, "-", high, "]");
			}
			if (node.has_attribute("epp")) {
				unsigned value = node.attribute_value("epp", unsigned(Hwp_request::Perf_epp::BALANCED));
				if (value <= Hwp_request::Perf_epp::ENERGY)
					Hwp_request::Perf_epp::set(raw_hwp, value);
				else
					if (verbose)
						warning(cpu, " epp - out of range - ", value, " [",
						        low, "-", high, "]");
			}

			if (raw_hwp != hwp_req) {
				if (write_hwp_request(system, raw_hwp))
					read_hwp(system);
				else
					warning(cpu, " hwp_request failed, ",
					        Hex(hwp_req), " -> ", Hex(raw_hwp));
			}
		});

		config.with_optional_sub_node("mwait", [&] (auto const &node) {

			bool off = node.attribute_value("off", false);
			if (off) {
				if (!write_mwait_hint(system, ~0U))
					error ("changing to hlt failed");
				return;
			}

			if (!node.has_attribute("c_state"))
				return;

			unsigned c_state = node.attribute_value("c_state", 0u);
			unsigned c_sub   = node.attribute_value("c_sub_state", 0u);

			if (c_state >= 8) {
				error("mwait hint ", c_state, " ", c_sub, " is invalid");
				return;
			}

			cpuid.intel_mwait_ext([&](auto const & c_sub_states) {
				auto const sub_state_cnt = (c_sub_states >> (c_state * 4)) & 0xfu;

				/* Table 4-11. MWAIT Hints Register (EAX) */
				if (!sub_state_cnt || c_sub >= sub_state_cnt) {
					error("mwait hint ", c_state, " ", c_sub, " is invalid");
					return;
				}

				/*
				 * Intel spec - Table 4-11. MWAIT Hints Register (EAX)
				 *
				 * C0 -> 1111b, C1 = 0, C2 = 1, ...
				 */
				c_state = (c_state == 0) ? 0xf : c_state - 1;

				unsigned mwait_eax = (c_sub & 0xfu) | (c_state << 4);

				if (!write_mwait_hint(system, mwait_eax))
					error ("setting mwait hint failed");
			});
		});
	}

	auto time_diff(auto const now, auto const prev) const {
		return (now > prev) ? now - prev : prev - now; }

	double _pow(double value, unsigned long rounds) const
	{
		if (rounds == 0) return 1;
		if (rounds == 1) return value;
		return value * _pow(value, rounds - 1);
	}

	void report_energy(Genode::Xml_generator &xml, char const * const name,
	                   uint64_t const msr, uint64_t const msr_prev,
	                   uint64_t const tsc_freq_khz) const
	{
		if (!valid_msr_rapl_units)
			return;

		auto const time_ms = time_diff(energy_timestamp, energy_timestamp_prev)
		                   / tsc_freq_khz;

		auto const pow = _pow(0.5d, Msr_rapl_units::Energy::get(msr_rapl_units));

		xml.node(name, [&] () {
			auto const t   = double(msr      & ((1ull << 32) - 1));
			auto const t_p = double(msr_prev & ((1ull << 32) - 1));

			xml.attribute("raw"  , msr);
			xml.attribute("Joule", t * pow); /* J = W * s */
			xml.attribute("Watt" , (time_ms > 0) ? time_diff(t, t_p) * pow * 1000.0d / double(time_ms) : 0);
		});
	}

	void report_power(Genode::Xml_generator &xml, char const * const name,
	                  uint64_t const msr) const
	{
		if (!valid_msr_rapl_units)
			return;

		xml.node(name, [&] () {
			auto const pow_power = _pow(0.5d, Msr_rapl_units::Power::get(msr_rapl_units));
			auto const pow_time  = _pow(0.5d, Msr_rapl_units::Time ::get(msr_rapl_units));

			auto const therm = Msr_pkg_power_info::Thermal_spec_power::get(msr);
			auto const min   = Msr_pkg_power_info::Minimum_power     ::get(msr);
			auto const max   = Msr_pkg_power_info::Maximum_power     ::get(msr);
			auto const time  = Msr_pkg_power_info::Max_time_window   ::get(msr);

			xml.attribute("raw", msr);
			xml.attribute("ThermalSpecPower",  double(therm) * pow_power);
			xml.attribute("MinimumPower",      double(min)   * pow_power);
			xml.attribute("MaximumPower",      double(max)   * pow_power);
			xml.attribute("MaximumTimeWindow", double(time)  * pow_time);
		});
	}

	void report_limits_package(Genode::Xml_generator &xml,
	                           char const * const name,
	                           uint64_t const msr) const
	{
		if (!valid_msr_rapl_units)
			return;

		typedef Msr_pkg_power_limit Limit;
		typedef Msr_rapl_units      Units;

		xml.node(name, [&] () {

			auto const pow_power = _pow(0.5d, Units::Power::get(msr_rapl_units));
			auto const pow_time  = _pow(0.5d, Units::Time ::get(msr_rapl_units));

			auto const pkg_1    =   Limit::Power_1     ::get(msr);
			auto const enable_1 = !!Limit::Enable_1    ::get(msr);
			auto const clamp_1  = !!Limit::Clamp_1     ::get(msr);
			auto const wnd_y_1  =   Limit::Time_wnd_y_1::get(msr);
			auto const wnd_z_1  =   Limit::Time_wnd_z_1::get(msr);

			auto const pkg_2    =   Limit::Power_2     ::get(msr);
			auto const enable_2 = !!Limit::Enable_2    ::get(msr);
			auto const clamp_2  = !!Limit::Clamp_2     ::get(msr);
			auto const wnd_y_2  =   Limit::Time_wnd_y_2::get(msr);
			auto const wnd_z_2  =   Limit::Time_wnd_z_2::get(msr);

			auto const lock           = !!Limit::Lock::get(msr);

			auto const pow_window_1 = _pow(2.0d, wnd_y_1)
			                        * (1.0d + (wnd_z_1 / 4.0d))
			                        * pow_time;

			auto const pow_window_2 = _pow(2.0d, wnd_y_2)
			                        * (1.0d + (wnd_z_2 / 4.0d))
			                        * pow_time;

			xml.attribute("raw"  , String<19>(Hex(msr)));
			xml.attribute("lock" , lock);

			xml.node("limit_1", [&] () {
				xml.attribute("power"       , double(pkg_1) * pow_power);
				xml.attribute("enable"      , enable_1);
				xml.attribute("clamp"       , clamp_1);
				xml.attribute("time_window" , pow_window_1);
			});

			xml.node("limit_2", [&] () {
				xml.attribute("power"       , double(pkg_2) * pow_power);
				xml.attribute("enable"      , enable_2);
				xml.attribute("clamp"       , clamp_2);
				xml.attribute("time_window" , pow_window_2);
			});
		});
	}

	void report_limits_dram_pp0_pp1(Genode::Xml_generator &xml,
	                                char const * const name,
	                                uint64_t const msr) const
	{
		if (!valid_msr_rapl_units)
			return;

		typedef Msr_power_limit Limit;
		typedef Msr_rapl_units  Units;

		xml.node(name, [&] () {

			auto const pow_power = _pow(0.5d, Units::Power::get(msr_rapl_units));
			auto const pow_time  = _pow(0.5d, Units::Time ::get(msr_rapl_units));

			auto const power      =   Limit::Power     ::get(msr);
			auto const enable     = !!Limit::Enable    ::get(msr);
			auto const clamp      = !!Limit::Clamp     ::get(msr);
			auto const time_wnd_y =   Limit::Time_wnd_y::get(msr);
			auto const time_wnd_f =   Limit::Time_wnd_f::get(msr);
			auto const lock       =   Limit::Lock      ::get(msr);

			auto const pow_window = _pow(2.0d, time_wnd_y)
			                      * (1.0d + (double(time_wnd_f) / 10))
			                      * pow_time;

			xml.attribute("raw"         , String<19>(Hex(msr)));
			xml.attribute("lock"        , lock);
			xml.attribute("power"       , double(power) * pow_power);
			xml.attribute("enable"      , enable);
			xml.attribute("clamp"       , clamp);
			xml.attribute("time_window" , pow_window);
		});
	}

	void report_perf_status(Genode::Xml_generator &xml, char const * const name,
	                        uint64_t const msr, uint64_t const msr_prev,
	                        uint64_t const tsc_freq_khz) const
	{
		if (!valid_msr_rapl_units)
			return;

		auto const time_ms = time_diff(perf_timestamp, perf_timestamp_prev)
		                   / tsc_freq_khz;

		xml.node(name, [&] () {
			typedef Msr_rapl_units Units;

			auto const pow = _pow(0.5d, Units::Time::get(msr_rapl_units));
			auto const t   = double(msr      & ((1ull << 32) - 1));
			auto const t_p = double(msr_prev & ((1ull << 32) - 1));

			xml.attribute("raw" , String<19>(Hex(msr)));

			xml.attribute("throttle_abs" , double(t) * pow);
			xml.attribute("throttle_diff", (time_ms > 0) ? time_diff(t, t_p) * pow * 1000.0d / double(time_ms) : 0);
		});
	}

	void _report_enhanced_speedstep(Genode::Xml_generator &xml) const
	{
		xml.node("intel_speedstep", [&] () {
			xml.attribute("enhanced", cpuid.enhanced_speedstep());
#if 0
			/* reporting missing in kernel -> see dev debug branch */
			xml.attribute("enabled", !!(valid_misc_enable && (misc_enable & (1u << 16))));

			if (valid_perf_status)
				xml.attribute("ia32_perf_status", String<19>(Hex(perf_status)));
			if (valid_perf_ctl)
				xml.attribute("ia32_perf_ctl", String<19>(Hex(perf_ctl)));
#endif
		});
	}

	void _report_residency(Genode::Xml_generator & xml,
	                       uint64_t const tsc_freq_khz,
	                       char const * const name,
	                       uint64_t const value,
	                       uint64_t const value_prev,
	                       bool     const valid) const
	{
		if (!valid)
			return;

		auto const  abs_ms = time_diff(value,       0ull) / tsc_freq_khz;
		auto const diff_ms = time_diff(value, value_prev) / tsc_freq_khz;

		xml.node(name, [&] () {
			xml.attribute("raw",     value);
			xml.attribute("abs_ms",  abs_ms);
			xml.attribute("diff_ms", diff_ms);
		});
	}

	void report(Genode::Xml_generator &xml, uint64_t const tsc_freq_khz) const
	{
		using Genode::String;


		if (cpuid.hwp()) {
			xml.node("hwp", [&] () {
				xml.attribute("enable", enabled_hwp);
			});
		}

		if (valid_hwp_cap) {
			xml.node("hwp_cap", [&] () {
				xml.attribute("high", Hwp_cap::Perf_highest::get(hwp_cap));
				xml.attribute("guar", Hwp_cap::Perf_guaranted::get(hwp_cap));
				xml.attribute("effi", Hwp_cap::Perf_most_eff::get(hwp_cap));
				xml.attribute("low",  Hwp_cap::Perf_lowest::get(hwp_cap));
				xml.attribute("raw",  String<19>(Genode::Hex(hwp_cap)));
			});
		}

		if (valid_hwp_req_pkg) {
			xml.node("hwp_request_package", [&] () {
				xml.attribute("raw", String<19>(Genode::Hex(hwp_req_pkg)));
			});
		}

		if (valid_hwp_req) {
			xml.node("hwp_request", [&] () {
				xml.attribute("min", Hwp_request::Perf_min::get(hwp_req));
				xml.attribute("max", Hwp_request::Perf_max::get(hwp_req));
				xml.attribute("desired", Hwp_request::Perf_desired::get(hwp_req));
				xml.attribute("epp", Hwp_request::Perf_epp::get(hwp_req));
				xml.attribute("raw", String<19>(Genode::Hex(hwp_req)));
			});
		}

		if (valid_epb) {
			xml.node("energy_perf_bias", [&] () {
				xml.attribute("raw", epb);
			});
		}

		if (cpuid.enhanced_speedstep())
			_report_enhanced_speedstep(xml);

		/* msr mperf and aperf availability */
		if (cpuid.hardware_coordination_feedback_cap())
			xml.node("hwp_coord_feed_cap", [&] () { });

		if (valid_msr_rapl_units  || valid_msr_pkg_energy ||
		    valid_msr_dram_energy || valid_msr_pp0_energy ||
		    valid_msr_pp1_energy) {

			xml.node("energy", [&] () {
				auto const time_ms = energy_timestamp / tsc_freq_khz;

				xml.attribute("timestamp_ms", time_ms);

				if (valid_msr_rapl_units) {
					xml.node("units", [&] () {
						xml.attribute("raw"   , msr_rapl_units);
						xml.attribute("power" , Msr_rapl_units::Power::get(msr_rapl_units));
						xml.attribute("energy", Msr_rapl_units::Energy::get(msr_rapl_units));
						xml.attribute("time"  , Msr_rapl_units::Time::get(msr_rapl_units));
					});
				}
				if (valid_msr_pkg_energy)
					report_energy(xml, "package", msr_pkg_energy,
					              msr_pkg_energy_prev, tsc_freq_khz);
				if (valid_msr_dram_energy)
					report_energy(xml, "dram", msr_dram_energy,
					              msr_dram_energy_prev, tsc_freq_khz);
				if (valid_msr_pp0_energy)
					report_energy(xml, "pp0", msr_pp0_energy,
					              msr_pp0_energy_prev, tsc_freq_khz);
				if (valid_msr_pp1_energy)
					report_energy(xml, "pp1", msr_pp1_energy,
					              msr_pp1_energy_prev, tsc_freq_khz);
			});
		}

		if (valid_msr_pkg_power_info || valid_msr_dram_power_info) {

			xml.node("power_info", [&] () {
				if (valid_msr_pkg_power_info)
					report_power(xml, "package", msr_pkg_power_info);
				if (valid_msr_dram_power_info)
					report_power(xml, "dram",    msr_dram_power_info);
			});
		}

		if (valid_msr_pkg_limits || valid_msr_dram_limits ||
		    valid_msr_pp0_limits || valid_msr_pp1_limits) {

			xml.node("power_limit", [&] () {
				if (valid_msr_pkg_limits && msr_pkg_limits)
					report_limits_package     (xml, "package", msr_pkg_limits);
				if (valid_msr_dram_limits && msr_dram_limits)
					report_limits_dram_pp0_pp1(xml, "dram"   , msr_dram_limits);
				if (valid_msr_pp0_limits && msr_pp0_limits)
					report_limits_dram_pp0_pp1(xml, "pp0"    , msr_pp0_limits);
				if (valid_msr_pp1_limits && msr_pp1_limits)
					report_limits_dram_pp0_pp1(xml, "pp1"    , msr_pp1_limits);
			});
		}

		if (valid_msr_pp0_policy || valid_msr_pp1_policy) {

			xml.node("policy", [&] () {
				if (valid_msr_pp0_policy)
					xml.attribute("pp0", String<19>(Hex(msr_pp0_policy)));
				if (valid_msr_pp1_policy)
					xml.attribute("pp1", String<19>(Hex(msr_pp1_policy)));
			});
		}

		if (valid_msr_pkg_perf || valid_msr_dram_perf || valid_msr_pp0_perf) {

			xml.node("perf_status", [&] () {
				if (valid_msr_pkg_perf)
					report_perf_status(xml, "package", msr_pkg_perf,
					                   msr_pkg_perf_prev, tsc_freq_khz);
				if (valid_msr_pp0_perf)
					report_perf_status(xml, "pp0", msr_pp0_perf,
					                   msr_pp0_perf_prev, tsc_freq_khz);
				if (valid_msr_dram_perf)
					report_perf_status(xml, "dram", msr_dram_perf,
					                   msr_dram_perf_prev, tsc_freq_khz);
			});
		}

		xml.node("mwait_support", [&] () {
			cpuid.intel_mwait_ext([&](auto const &value) {
				auto const c_sub_states = value;

				for (unsigned i = 0; i < 8; i++) {
					auto const sub_state_cnt = (c_sub_states >> (i * 4)) & 0xf;

					if (!sub_state_cnt)
						continue;

					xml.node(Genode::String<4>("c", i).string(), [&] () {
						xml.attribute("sub_state_count", sub_state_cnt);
					});
				}
			});
		});

		xml.node("msr_residency", [&] () {
			_report_residency(xml, tsc_freq_khz, "core_c1",
			                  msr_core_c1, msr_core_c1_prev, valid_core_c1);
			_report_residency(xml, tsc_freq_khz, "core_c3",
			                  msr_core_c3, msr_core_c3_prev, valid_core_c3);
			_report_residency(xml, tsc_freq_khz, "core_c6",
			                  msr_core_c6, msr_core_c6_prev, valid_core_c6);
			_report_residency(xml, tsc_freq_khz, "core_c7",
			                  msr_core_c7, msr_core_c7_prev, valid_core_c7);
			_report_residency(xml, tsc_freq_khz, "pkg_c2",
			                  msr_pkg_c2, msr_pkg_c2_prev, valid_pkg_c2);
			_report_residency(xml, tsc_freq_khz, "pkg_c3",
			                  msr_pkg_c3, msr_pkg_c3_prev, valid_pkg_c3);
			_report_residency(xml, tsc_freq_khz, "pkg_c6",
			                  msr_pkg_c6, msr_pkg_c6_prev, valid_pkg_c6);
			_report_residency(xml, tsc_freq_khz, "pkg_c7",
			                  msr_pkg_c7, msr_pkg_c7_prev, valid_pkg_c7);
			_report_residency(xml, tsc_freq_khz, "pkg_c8",
			                  msr_pkg_c8, msr_pkg_c8_prev, valid_pkg_c8);
			_report_residency(xml, tsc_freq_khz, "pkg_c9",
			                  msr_pkg_c9, msr_pkg_c9_prev, valid_pkg_c9);
			_report_residency(xml, tsc_freq_khz, "pkg_c10",
			                  msr_pkg_c9, msr_pkg_c10_prev, valid_pkg_c10);
		});
	}
};

