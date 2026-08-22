/*
 * \brief  Selective ROM forwarding
 * \author Alexander Boettcher
 * \date   2026-07-01
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/session_state.h>


namespace Rom_select {
	using namespace Genode;
	struct Service;
}


struct Rom_select::Service
{
	Genode::Env              &env;
	Heap                      heap     { env.ram(), env.rm() };
	Attached_rom_dataspace    requests { env, "session_requests" };
	Attached_rom_dataspace    platform { env, "platform_info" };
	Attached_rom_dataspace    config   { env, "config" };
	Id_space<Parent::Server>  sp_id    { };

	/**
	 * Object to bind ids between parent and client space.
	 */
	struct Session : Parent::Server
	{
		Parent::Client parent_client { };

		Id_space<Parent::Client>::Element client_id;
		Id_space<Parent::Server>::Element server_id;

		Session(Id_space<Parent::Client> &client_space,
		        Id_space<Parent::Server> &server_space,
		        Parent::Server::Id server_id)
		:
			client_id(parent_client, client_space),
			server_id(*this, server_space, server_id) { }
	};

	void handle_session_request(Node const &request);

	void handle_session_requests()
	{
		requests.update();

		if (!requests.valid())
			return;

		requests.node().for_each_sub_node([&] (Node const &request) {
			handle_session_request(request);
		});
	}

	void handle_config_change()
	{
		config.update();

		if (!config.valid())
			return;

		log("config changed - established session are not re-validated ...");
	}

	Signal_handler<Service> session_request_handler {
		env.ep(), *this, &Service::handle_session_requests };

	Signal_handler<Service> config_handler {
		env.ep(), *this, &Service::handle_config_change };

	Service(Genode::Env &env) : env(env)
	{
		requests.sigh(session_request_handler);
		config  .sigh(config_handler);

		handle_session_requests();
		handle_config_change();
	}

	Session_capability request_session(Parent::Client::Id  const &id,
	                                   Session_state::Args const &args,
	                                   Affinity            const  affinity)
	{
		enum { ARGS_MAX_LEN = Parent::Session_args::MAX_SIZE };
		char new_args[ARGS_MAX_LEN];

		static constexpr unsigned str_len = 16;

		String<str_len> kernel("unknown");

		platform.node().with_optional_sub_node("kernel", [&](auto const &node) {
			kernel = node.attribute_value("name", kernel); });

		copy_cstring(new_args, args.string(), ARGS_MAX_LEN);

		auto arg_label = Arg_string::find_arg(new_args, "label");
		bool done      = false;

		config.node().for_each_sub_node("policy", [&](auto const &policy) {

			if (done)
				return;

			auto target_kernel = policy.attribute_value("kernel" , String<str_len>("any"));
			auto target_rom    = policy.attribute_value("target" , String<str_len>(""));
			auto replace_rom   = policy.attribute_value("replace", String<str_len>(""));

			char arg_label_c[str_len] { };
			arg_label.string(arg_label_c, sizeof(arg_label_c), "");

			if (target_kernel != "any" && kernel != target_kernel)
				return;

			if (target_rom != arg_label_c)
				return;

			/* sacrifice the label to make space for the root argument */
			Arg_string::remove_arg(new_args, "label");

			Arg_string::set_arg(new_args, ARGS_MAX_LEN, "label",
			                    String<str_len + 2>("\"", replace_rom, "\"").string());

			log("re-route '", target_rom, "' -> '", replace_rom, "'");

			done = true;
		});

		return env.session("ROM", id, new_args, affinity);
	}
};


void Rom_select::Service::handle_session_request(Node const &request)
{
	if (!request.has_attribute("id"))
		return;

	Parent::Server::Id const server_id { request.attribute_value("id", 0UL) };

	if (request.has_type("create")) {

		if (!request.has_sub_node("args"))
			return;

		using Args = Session_state::Args;
		Args const args = request.with_sub_node("args",
			[] (auto const &node) { return Args(Node::Quoted_content(node)); },
			[]                    { return Args(); });

		auto &session = *new (heap) Session(env.id_space(), sp_id, server_id);
		auto cap = request_session(session.client_id.id(), args,
		                           Affinity::from_node(request));
		env.parent().deliver_session_cap(server_id, cap);
	}

	if (request.has_type("upgrade")) {

		sp_id.apply<Session>(server_id, [&] (Session &session) {

			auto const ram_quota = request.attribute_value("ram_quota", 0UL);
			auto const cap_quota = request.attribute_value("cap_quota", 0UL);

			String<128> args("ram_quota=", ram_quota, ", cap_quota=", cap_quota);

			env.upgrade(session.client_id.id(), args.string());
			env.parent().session_response(server_id, Parent::Session_response::OK);
		});
	}

	if (request.has_type("close")) {
		sp_id.apply<Session>(server_id, [&] (Session &session) {
			env.close(session.client_id.id());
			destroy(heap, &session);
			env.parent().session_response(server_id, Parent::Session_response::CLOSED);
		});
	}
}


void Component::construct(Genode::Env &env)
{
	static Rom_select::Service service(env);

	env.parent().announce("ROM");
}
