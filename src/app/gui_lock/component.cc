/*
 * \brief  Very simple fullscreen GUI client exiting on right passphrase
 * \author Alexander Boettcher
 * \date   2018-08-07
 */

/*
 * Copyright (C) 2018-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <gui_session/connection.h>

#include <nitpicker_gfx/tff_font.h>
#include <nitpicker_gfx/box_painter.h>

#include "icon/user.h"

extern char _binary_mono_tff_start[];

struct Lock
{
	typedef Gui::Session::Command     Command;
	typedef Gui::View_id              View_id;

	typedef Genode::Signal_handler<Lock>                      Signal_handler;
	typedef Genode::Constructible<Gui::Connection>            Gui_con;
	typedef Genode::Constructible<Genode::Attached_dataspace> Fb;
	typedef Genode::Attached_rom_dataspace                    Rom;
	typedef Genode::Color                                     Color;
	typedef Genode::Pixel_rgb888                              Pixel_rgb888;
	typedef Tff_font::Static_glyph_buffer<4096>               Glyph_buffer;
	typedef Genode::String<32>                                String;

	Genode::Env    &_env;
	View_id         _handle         { 1 };
	Gui_con         _gui            { };
	Fb              _fb             { };
	Signal_handler  _input_handler  { _env.ep(), *this, &Lock::_handle_input };
	Signal_handler  _mode_handler   { _env.ep(), *this, &Lock::_handle_mode };
	Rom             _config_rom     { _env, "config" };
	Rom             _info_rom       { _env, "platform_info" };
	Signal_handler  _config_handler { _env.ep(), *this, &Lock::_update_config };

	Glyph_buffer    _glyph_buffer   { };
	Tff_font        _default_font   { _binary_mono_tff_start, _glyph_buffer };

	String          _kernel         { };
	String          _user           { "User" };

	Input::Keycode  _lock_key       { };
	short int const _hs             { 10 }; /* half size of square box */
	bool            _transparent    { };
	bool            _cmp_valid      { };

	/* pwd state */
	enum State {
		WAIT_CLICK,
		RECORD_PWD,
		BEFORE_COMPARE,
		COMPARE_PWD,
		WAIT_FOR_LOCK_KEY
	} state { WAIT_CLICK };

	struct {
		unsigned chars [128];
		unsigned i;
		unsigned max;
	} _pwd { };

	void _update_view(auto const &bg_color, auto const &fg_color,
	                  unsigned const offset,
	                  char const * const text, unsigned const text_len)
	{
		unsigned   * const pixels = _fb->local_addr<unsigned>();
		auto         const size   = _fb->size() / sizeof(*pixels);

		for (unsigned i = 0; i < size; i++)
			pixels[i] = *(unsigned *)(&bg_color);

		auto const mode = _mode();

		Genode::Surface<Pixel_rgb888> surface(reinterpret_cast<Pixel_rgb888 *>(pixels),
		                                      mode.area);

		if (text && text_len) {
			auto const size  = _default_font.bounding_box();
			auto const sub_w = size.w * text_len / 2;
			auto const sub_h = size.h / 2;

			Text_painter::Position where(int(mode.area.w / 2 - sub_w),
			                             int(mode.area.h / 2 - sub_h + offset));
			Text_painter::paint(surface, where, _default_font,
			                    fg_color, text);
		}

		_gui->view(_handle, { .title = { }, .rect = mode, .front = true });

		_gui->enqueue<Command::Geometry>(_handle, mode);
		_gui->enqueue<Command::Front   >(_handle);
		_gui->execute();
	}

	void _default_view(auto const buffer, unsigned const buffer_size)
	{
		auto const bg_black = Color::rgb(0, 0, 0);
		auto const bg_trans = Color::clamped_rgba(16, 16, 16, 16);
		auto const fg_white = Color::rgb(255, 255, 255);

		auto const   offset_y = _hs * 2;
		auto const   mode     = _mode();
		auto       & fb       = _gui->framebuffer;

		_update_view(_transparent ? bg_trans : bg_black, fg_white,
		             offset_y, buffer, buffer_size);

		_user_bubble(mode, fb);
	}

	void _switch_view_record_pwd()
	{
		char const buffer[] = "Recording password ...";

		_default_view(buffer, sizeof(buffer) - 1);

		state = RECORD_PWD;
		_cmp_valid = false;
	}

	void _switch_view_before_compare()
	{
		auto const bg_trans = Color::clamped_rgba(16, 16, 16, 16);

		_update_view(_transparent ? bg_trans : Color::black(), Color::black(),
		             0, nullptr, 0);

		state = BEFORE_COMPARE;
	}

	void _switch_view_compare_pwd()
	{
		char const buffer[] = "Password to unlock screen ...";

		_default_view(buffer, sizeof(buffer) - 1);

		state = COMPARE_PWD;

		_cmp_valid = true;
		_pwd.i = 0;
	}

	void _switch_view_initial()
	{
		char const buffer[] = "No password set, recording starts with next input ...";

		_default_view(buffer, sizeof(buffer) - 1);

		state = WAIT_CLICK;
		_cmp_valid = false;
	}

	void _inc_pwd_i()
	{
		_pwd.i ++;
		if (_pwd.i >= sizeof(_pwd.chars))
			_pwd.i = 0;
	}

	Gui::Rect _mode()
	{
		return _gui->window().convert<Gui::Rect>([&] (Gui::Rect rect) { return rect; },
		                                         [&] (Gui::Undefined) { return Gui::Rect { { }, { 1, 1 } }; });
	}

	void _handle_mode()
	{
		if (state == WAIT_FOR_LOCK_KEY)
			return;

		if (!_gui.constructed())
			return;

		_gui->buffer({ .area = _mode().area, .alpha = _transparent });

		_fb.construct(_env.rm(), _gui->framebuffer.dataspace());

		switch (state) {
		case BEFORE_COMPARE    : _switch_view_before_compare(); break;
		case COMPARE_PWD       : _switch_view_compare_pwd();    break;
		case RECORD_PWD        : _switch_view_record_pwd();     break;
		case WAIT_CLICK        : _switch_view_initial();        break;
		case WAIT_FOR_LOCK_KEY : break;
		}
	}

	void _update_config();
	void _handle_input();
	void _show_box(unsigned, Genode::uint8_t, unsigned, short int);

	Gui::Rect _avatar   (Gui::Rect const &, Gui::Rect const &);
	Gui::Rect _circle   (Gui::Rect const &, Gui::Rect const &, Color const &);
	Gui::Rect _rectangle(Gui::Rect const &, Gui::Rect const &, Color const &, bool);
	Gui::Rect _text     (Gui::Rect const &, char const *, unsigned,
	                     Color const &, Gui::Rect const &);
	void      _user_bubble(Gui::Rect const &, Framebuffer::Session_client &);

	Gui::Area _avatar_size() const { return { avatar_width, avatar_height }; }

	void reinit_gui()
	{
		_gui.construct(_env, "screen");

		_gui->info_sigh (_mode_handler);
		_gui->input.sigh(_input_handler);
	}

	Lock(Genode::Env &env) : _env(env)
	{
		_config_rom.sigh(_config_handler);
		_info_rom  .sigh(_config_handler);

		reinit_gui();

		_update_config();

		_handle_mode();
	}

};


void Lock::_user_bubble(Gui::Rect const             & mode,
                        Framebuffer::Session_client & fb)
{
	auto const color_txt  = Color::rgb(255, 255, 255);

	Gui::Rect r_mid   = { int(mode.area.w / 2), int(mode.area.h / 2), 0, 0 };
	Gui::Rect r_start = { int(mode.area.w / 2 - _avatar_size().w / 2),
	                      int(mode.area.h / 2 - _avatar_size().h),
	                      0, 0 };

#if 0
//	unsigned const circle_d = mode.area.h / 2 / 3;

	Gui::Rect circle_r = { int(mode.area.w / 2),
	                       int(mode.area.h / 2 - circle_d / 2 - 50),
	                       circle_d, circle_d};

	if (circle_r.at.x <= 0 || circle_r.at.y <= 0)
		return;

	auto dirty = _circle(circle_r, mode, circle);
	fb.refresh(dirty);

	int  offset_head   = circle_r.area.h / 2 - 15;
#endif

	/* XXX clipping missing */

	if (r_start.at.x <= 0 || r_start.at.y <= 0)
		return;

	/* avatar ^^ */
	auto dirty = _avatar({ .at = r_start.at, .area = { 0, 0 }}, mode);
	fb.refresh(dirty);

	/* name of user */
	auto const user_txt = String(_user);

	Gui::Rect rect  = { r_mid.at.x, dirty.at.y + _hs * 3, 0, 0 };
	rect.at.y += dirty.area.h;
	rect.at.y += int(_default_font.bounding_box().h);
	rect.at.x -= int(_default_font.bounding_box().w * (user_txt.length() - 1) / 2);

	dirty = _text(rect, user_txt.string(), unsigned(user_txt.length()),
	              color_txt, mode);
	fb.refresh(dirty);

	/* in use kernel */
	auto const kern_txt = String("Genode@", _kernel);

	Gui::Rect kern_r = { r_mid.at.x, dirty.at.y, 0, 0 };
	kern_r.at.y += dirty.area.h;
	kern_r.at.y += int(_default_font.bounding_box().h);
	kern_r.at.x -= int(_default_font.bounding_box().w * (kern_txt.length() - 1) / 2);

	dirty = _text(kern_r, kern_txt.string(),
	              unsigned(kern_txt.length()), color_txt, mode);
	fb.refresh(dirty);
}


Gui::Rect Lock::_text(Gui::Rect const & pos,
                      char      const * txt,
                      unsigned  const   txt_len,
                      Color     const & color,
                      Gui::Rect const & mode)
{
	if (!txt || !txt_len)
		return { };

	unsigned * const pixels = _fb->local_addr<unsigned>();

	Genode::Surface<Pixel_rgb888> surface(reinterpret_cast<Pixel_rgb888 *>(pixels),
	                                      mode.area);

	auto const size  = _default_font.bounding_box();

	Text_painter::Position const where(pos.at.x, pos.at.y);

	Text_painter::paint(surface, where, _default_font, color, txt);

	return { .at { pos.at.x, pos.at.y }, .area { size.w * txt_len, size.h } };
}


Gui::Rect Lock::_circle(Gui::Rect const & rect,
                        Gui::Rect const & mode,
                        Color     const & sc)
{
	unsigned * const pixels = _fb->local_addr<unsigned>();

	int      const r    = rect.area.h / 2;
	unsigned const xpos = rect.at.x;
	unsigned const ypos = rect.at.y;

	for (int y = -r; y < r; y++) {
		for (int x = -r; x < r; x++) {
			/* r² = x² + y²; */
			bool skip = (r * r) < (x*x) + (y*y);

			if (skip)
				continue;

			/* Gui::Color stores data in RGBA order, but fb is in ARGB */
			auto color = unsigned(sc.r) << 16 |
			             unsigned(sc.g) <<  8 |
			             unsigned(sc.b) <<  0 |
			             unsigned(sc.a) << 24;

			pixels[mode.area.w * (ypos + y) + xpos + x] = color;
		}
	}

	return { int(xpos) - r, int(ypos) - r, rect.area.h, rect.area.h };
}


Gui::Rect Lock::_avatar(Gui::Rect const & rect, Gui::Rect const & mode)
{
	unsigned * const pixels = _fb->local_addr<unsigned>();

	auto data = header_data;

	unsigned const xpos = rect.at.x;
	unsigned const ypos = rect.at.y;

	for (auto y = 0u; y < avatar_height; y++) {
		auto const pixel_pos = mode.area.w * (ypos + y);
		for (auto x = 0u; x < avatar_width; x++) {
			char * const _p = reinterpret_cast<char *>(&pixels[pixel_pos + xpos + x]);

			HEADER_PIXEL(data, _p);

			/* invert colors */
			*(unsigned *)_p = ~*(unsigned *)_p;
		}
	}

	return { .at = rect.at, .area = { avatar_width, avatar_height } };
}


Gui::Rect Lock::_rectangle(Gui::Rect const &rect, Gui::Rect const &mode,
                           Color     const &sc  , bool      const  fill)
{
	if (!rect.area.w || !rect.area.h)
		return rect;

	unsigned * const pixels = _fb->local_addr<unsigned>();

	unsigned const xpos = rect.at.x;
	unsigned const ypos = rect.at.y;

	/* Gui::Color stores data in RGBA order, but fb is in ARGB */
	auto color = unsigned(sc.r) << 16 |
	             unsigned(sc.g) <<  8 |
	             unsigned(sc.b) <<  0 |
	             unsigned(sc.a) << 24;

	for (auto y = 0u; y < rect.area.h; y++) {
		auto const pixel_pos = mode.area.w * (ypos + y);

		if (y == 0 || y == rect.area.h - 1) {
			for (auto x = 0u; x < rect.area.w; x++) {
				pixels[pixel_pos + xpos + x ] = color;
			}
			continue;
		}

		pixels[pixel_pos + xpos + 0] = color;

		if (fill && rect.area.w > 1)
			for (auto x = 1u; x < rect.area.w - 1; x++)
				pixels[pixel_pos + xpos + x] = color;

		pixels[pixel_pos + xpos + rect.area.w - 1 ] = color;
	}

	return rect;
}


void Lock::_show_box(unsigned        const chars,
                     Genode::uint8_t const cc, /* clear character */
                     unsigned        const sc  /* set   character */,
                     int       short const hs)
{
	if (!chars) return;

	unsigned * const pixels  = _fb->local_addr<unsigned>();
	auto       const fb_size = _fb->size();

	auto const mode = _mode();
	int  const hsa  = _hs + 2;

	unsigned const offset = (chars - 1) * hsa;
	if (offset + hsa * 2 >= mode.area.w / 2)
		return;

	unsigned const xpos = mode.area.w / 2 - offset;
	unsigned const ypos = mode.area.h / 2;

	for (int y = -hs; y < hs; y++) {
		auto buf  = pixels + mode.area.w * (ypos + y) + xpos - hs;
		auto size = (chars * hsa * 2 + hs) * sizeof(*pixels);

		if (buf < pixels || size > fb_size)
			continue;
		if ((char *)buf > (char *)pixels + fb_size - size)
			continue;

		Genode::memset(buf, cc, size);
	}

	for (int y = -hs; y < hs; y++) {
		for (unsigned c = 0; c < chars; c++) {
			unsigned x = xpos + c * hsa * 2;
			for (int i = -hs; i < hs; i++) {
				pixels[mode.area.w * (ypos + y) + x + i ] = sc;
			}
		}
	}

	_gui->framebuffer.refresh(xpos - hs, ypos - hs,
	                          chars * hsa * 2 + hs * 2, hs * 2);
}


void Lock::_handle_input()
{
	if (!_gui.constructed())
		return;

	bool unlock = false;

	_gui->input.for_each_event([&] (Input::Event const &ev) {
		ev.handle_press([&] (Input::Keycode const &key, Input::Codepoint const &cp) {
			if (!ev.key_press(key) || !cp.valid())
				return;

			if (state == WAIT_FOR_LOCK_KEY &&
				key   != Input::Keycode::KEY_UNKNOWN &&
				key   == _lock_key)
			{
				Genode::log("Lock key detected - lock screen");
				state = BEFORE_COMPARE;
				_handle_mode();
				return;
			}

			if (!_fb.constructed())
				return;

			if (state == WAIT_CLICK)
				_switch_view_record_pwd();

			if (key == Input::Keycode::BTN_LEFT ||
			    key == Input::Keycode::BTN_RIGHT ||
			    key == Input::Keycode::BTN_MIDDLE)
				return;

			if (state == WAIT_CLICK)
				return;

			bool reset = false;

			if (key == Input::Keycode::KEY_ESC) {
				reset = _pwd.i > 0;
				_pwd.i = 0;

			} else
			if (key == Input::Keycode::KEY_ENTER) {
				if (state == COMPARE_PWD) {
					unlock = _cmp_valid && (_pwd.i == _pwd.max);
					reset = true;

					_switch_view_before_compare();
				}
				if (_pwd.i > 0 && state == RECORD_PWD) {
					_pwd.max = _pwd.i;
					_switch_view_before_compare();
				}

				_pwd.i = 0;
			} else {
				if (state == BEFORE_COMPARE) {
					_switch_view_compare_pwd();
				}
				if (state == RECORD_PWD) {
					_pwd.chars[_pwd.i] = cp.value;
					_inc_pwd_i();
					_show_box(_pwd.i, 0, ~0, _hs);
				}
				if (state == COMPARE_PWD) {
					if (_cmp_valid)
						_cmp_valid = (_pwd.chars[_pwd.i] == cp.value);

					_inc_pwd_i();
					_show_box(_pwd.i, 0, ~0, _hs);
				}
			}

			if (reset) {
				if (state == COMPARE_PWD) _switch_view_before_compare();
				if (state == RECORD_PWD)  _switch_view_record_pwd();
			}
		});
	});

	if (!unlock || _handle.value == 0)
		return;

	_gui->destroy_view(_handle);

	_gui->info_sigh ({});
	_gui->input.sigh({});

	_fb .destruct();
	_gui.destruct();

	if (_lock_key == Input::Keycode::KEY_UNKNOWN) {

		Genode::log("Exiting ... no lock key configured.");

		_handle = {};

		Genode::memset(&_pwd, 0, sizeof(_pwd));

		_env.parent().exit(0);
	} else {
		Genode::log("Unlocked. Waiting for lock key press.");

		reinit_gui();

		state = WAIT_FOR_LOCK_KEY;
	}
}


void Lock::_update_config()
{
	if (!_gui.constructed())
		return;

	_info_rom  .update();
	_config_rom.update();

	if (_info_rom.valid())
		_info_rom.xml().with_optional_sub_node("kernel", [&] (auto const &xml) {
			_kernel = xml.attribute_value("name", _kernel);
		});

	if (!_config_rom.valid())
		return;

	Genode::String<sizeof(_pwd.chars)> passwd { };

	auto const & config      = _config_rom.xml();
	bool const   transparent = _transparent;

	passwd       = config.attribute_value("password", passwd);
	_user        = config.attribute_value("name", _user);
	_transparent = config.attribute_value("transparent", _transparent);
	_lock_key    = Input::key_code(config.attribute_value("lock_key",
	                                                      Input::Key_name()));

	bool switch_view = transparent != _transparent;

	if (passwd.length() > 1) {
		for (unsigned i = 0; i < passwd.length(); i++)
			_pwd.chars[i] = passwd.string()[i];

		_pwd.max = unsigned(passwd.length() - 1);
		_pwd.i   = 0;

		if (!switch_view)
			switch_view = state != BEFORE_COMPARE;

		state = BEFORE_COMPARE;

		memset(&passwd, 0, sizeof(passwd));
	}

	if (_handle.value > 0 && switch_view)
		_handle_mode();
}


void Component::construct(Genode::Env &env) { static Lock lock(env); }
