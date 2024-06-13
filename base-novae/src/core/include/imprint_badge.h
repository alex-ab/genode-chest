/*
 * \brief  Utility to imprint a badge into a NOVAe portal
 * \author Norman Feske
 * \date   2016-03-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

/* NOVAe includes */
#include <novae/syscalls.h>

static inline bool imprint_badge(unsigned long pt_sel, unsigned long badge)
{
	using namespace Novae;

	/* assign badge to portal */
	if (pt_ctrl(pt_sel, badge) != NOVA_OK)
		return false;

	/* disable PT_CTRL permission to prevent subsequent imprint attempts */
	revoke(Obj_crd(pt_sel, 0, Obj_crd::RIGHT_PT_CTRL));
	return true;
}
