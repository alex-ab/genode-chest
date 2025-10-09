/*
 * \author Alexander Boettcher
 * \date   2021-10-23
 */

/*
 * Copyright (C) 2021-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/register.h>

#include "system_control.h"
#include "temp_freq.h"
#include "power_amd.h"
#include "power_intel.h"

namespace Genode {

	static inline void print(Output &out, Affinity::Location location)
	{
		print(out, location.xpos(), ",", location.ypos());
	}
}


using namespace Genode;

struct Core_thread : Thread, Msr::Monitoring
{
	/*
	 * Noncopyable
	 */
	Core_thread(Core_thread const &);
	Core_thread &operator = (Core_thread const &);

	Location const location;
	Blockade       barrier { };
	Blockade       done    { };

	uint64_t const tsc_freq_khz;

	Constructible<Msr::Power_intel> power_intel { };
	Constructible<Msr::Power_amd>   power_amd   { };

	Capability<Pd_session::System_control> const control_cap;
	Msr::System_control                          system { control_cap };

	bool intel;
	bool amd;
	bool main { };

	Node const * config_node { };

	Core_thread(Env &env, Location const &location, uint64_t tsc_freq_khz,
	            bool intel, bool amd)
	:
		Thread(env, Name("msr", location), { 4 * 4096 }, location),
		location(location), tsc_freq_khz(tsc_freq_khz),
		control_cap(env.pd().system_control_cap(location)),
		intel(intel), amd(amd)
	{ }

	void entry() override
	{
		/* create object by thread per CPU */
		if (intel)
			power_intel.construct();
		if (amd)
			power_amd.construct();

		if (intel && main)
			Monitoring::target_temperature(system);

		while (true) {
			barrier.block();

			if (intel)
				Monitoring::update_cpu_temperature(system);

			Monitoring::cpu_frequency(system, tsc_freq_khz);

			if (intel && main)
				Monitoring::update_package_temperature(system);

			if (power_intel.constructed()) {
				power_intel->update(system);
				if (config_node)
					power_intel->update(system, *config_node, location);

				/* features are homogen across all CPUs, e.g. P/E ? */
				if (main)
					power_intel->update_package(system);
			}

			if (power_amd.constructed()) {
				power_amd->update(system);

				if (config_node)
					power_amd->update(system, *config_node);
			}

			config_node = nullptr;

			done.wakeup();
		}
	}

};

struct Msr::Msr {

	/*
	 * Noncopyable
	 */
	Msr(Msr const &);
	Msr &operator = (Msr const &);

	Env                &env;
	Heap                heap     { env.ram(), env.rm() };
	Timer::Connection   timer    { env };
	Signal_handler<Msr> handler  { env.ep(), *this, &Msr::handle_timeout };
	Expanding_reporter  reporter { env, "info", "info" };

	Affinity::Space     cpus     { env.cpu().affinity_space() };
	Core_thread **      threads  { new (heap) Core_thread*[cpus.total()] };

	Microseconds        timer_rate { 5000 * 1000 };

	Attached_rom_dataspace config { env, "config" };

	Signal_handler<Msr> signal_config { env.ep(), *this, &Msr::handle_config };

	bool _cpu_name(char const * name)
	{
		uint32_t cpuid = 0, edx = 0, ebx = 0, ecx = 0;
		asm volatile ("cpuid" : "+a" (cpuid), "=d" (edx), "=b"(ebx), "=c"(ecx));

		return ebx == *reinterpret_cast<uint32_t const *>(name) &&
		       edx == *reinterpret_cast<uint32_t const *>(name + 4) &&
		       ecx == *reinterpret_cast<uint32_t const *>(name + 8);
	}

	bool _amd()   { return _cpu_name("AuthenticAMD"); }
	bool _intel() { return _cpu_name("GenuineIntel"); }

	Msr(Genode::Env &env) : env(env)
	{
		Attached_rom_dataspace info { env, "platform_info"};

		uint64_t   freq_khz { };
		String<16> kernel   { };

		info.node().with_optional_sub_node("hardware", [&] (auto const &n) {
			n.with_optional_sub_node("tsc", [&] (auto const &node) {
				freq_khz = node.attribute_value("freq_khz", freq_khz);
			});
		});

		info.node().with_optional_sub_node("kernel", [&] (auto const &node) {
			kernel = node.attribute_value("name", kernel); });

		bool const amd   = _amd();
		bool const intel = _intel();

		if (!amd && !intel) {
			error("no supported CPU detected");
			return;
		}

		{
			auto cap = env.pd().system_control_cap(Affinity::Location());

			if (cap.valid()) {
				System_control system { cap };

				/* check for working system control cap */
				if (!Monitoring::supported(system, amd, intel))
					cap = { };
			}

			if (!cap.valid()) {
				error("- CPU or used kernel misses MSR access support");
				error("- and/or missing 'managing_system' configuration");
				return;
			}
		}

		log("Detected: ", kernel, " kernel, ", cpus.width(), "x",
		    cpus.height(), " CPU", cpus.total() > 1 ? "s" : "",
		    ", TSC ", freq_khz, " kHz");

		/* construct the thread objects */
		for (unsigned x = 0; x < cpus.width(); x++) {
			for (unsigned y = 0; y < cpus.height(); y++) {
				unsigned const i = y + x * cpus.height();
				threads[i] = new (heap) Core_thread(env, Affinity::Location(x, y),
				                                    freq_khz, intel, amd);

				/* the first thread will read out TCC && package temperature */
				if (x == 0 && y == 0)
					threads[i]->main = true;

				threads[i]->start();
			}
		}

		timer.sigh(handler);
		timer.trigger_periodic(timer_rate.value);

		config.sigh(signal_config);
		handle_config();
	}

	void handle_timeout()
	{
		for (unsigned i = 0; i < cpus.total(); i++) {
			threads[i]->barrier.wakeup();
		}

		for (unsigned i = 0; i < cpus.total(); i++) {
			threads[i]->done.block();
		}

		reporter.generate([&] (Generator &g) {

			g.attribute("update_rate_us", timer_rate.value);

			/* XXX per package value handling
			 * target temperature is identical over a package
			 */
			unsigned tcc = 0;

			Core_thread const &package = *threads[0];
			if (package.temp_tcc_valid) {
				tcc = package.temp_tcc;
				g.attribute("tcc_temp_c", tcc);
			}

			if (tcc && package.temp_package_valid)
				g.attribute("pkg_temp_c", tcc - package.temp_package);

			for (unsigned i = 0; i < cpus.total(); i++) {
				Core_thread const &cpu = *threads[i];

				g.node("cpu", [&] () {
					g.attribute("x", cpu.location.xpos());
					g.attribute("y", cpu.location.ypos());

					if (cpu.intel) {
						if (cpu.power_intel->cpuid.core_type == Cpuid::INTEL_ATOM)
							g.attribute("type", "E");
						else
						if (cpu.power_intel->cpuid.core_type == Cpuid::INTEL_CORE)
							g.attribute("type", "P");
					}

					cpu.report(g, tcc);

					if (cpu.power_intel.constructed())
						cpu.power_intel->report(g, cpu.tsc_freq_khz);
					if (cpu.power_amd.constructed())
						cpu.power_amd->report(g);
				});
			}
		});
	}

	void handle_config()
	{
		config.update();

		if (!config.valid())
			return;

		if (config.node().has_attribute("update_rate_us")) {
			auto const new_rate = config.node().attribute_value("update_rate_us",
			                                                   timer_rate.value);

			if ((new_rate != timer_rate.value) && (new_rate >= Microseconds(100'000).value)) {
				timer_rate.value = new_rate;
				timer.trigger_periodic(timer_rate.value);
			}
		}

		config.node().for_each_sub_node("cpu", [&](Node const &node) {

			if (!node.has_attribute("x") || !node.has_attribute("y"))
				return;

			unsigned const xpos  = node.attribute_value("x", 0u);
			unsigned const ypos  = node.attribute_value("y", 0u);
			unsigned const index = ypos + xpos * cpus.height();

			if (index >= cpus.total())
				return;

			Core_thread &cpu = *threads[index];

			if (!cpu.power_intel.constructed() && !cpu.power_amd.constructed())
				return;

			cpu.config_node = &node;

			cpu.barrier.wakeup();
			cpu.done.block();
		});
	}
};

void Component::construct(Env &env) { static Msr::Msr component(env); }
