/*
 * \brief  Utilities for Node handling
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2018-01-11
 */

/*
 * Copyright (C) 2017-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <base/node.h>

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
 * Query attribute value from sub node
 *
 * The list of arguments except for the last one refer to Node path into the
 * Node structure. The last argument denotes the queried attribute name.
 */
template <typename T, typename... ARGS>
static T query_attribute(Genode::Node const & node, ARGS &&... args)
{
	return _attribute_value<T>(node, args...);
}
