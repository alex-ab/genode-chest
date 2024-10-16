/*
 * \brief  Very simple fullscreen GUI client exiting on right passphrase
 * \author Alexander Boettcher
 * \date   2018-08-07
 */

/*
 * Copyright (C) 2018-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <gui_session/connection.h>

#include <nitpicker_gfx/tff_font.h>
#include <nitpicker_gfx/box_painter.h>

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

	Genode::Env    &_env;
	View_id         _handle         { 1 };
	Gui_con         _gui            { };
	Fb              _fb             { };
	Signal_handler  _input_handler  { _env.ep(), *this, &Lock::_handle_input };
	Signal_handler  _mode_handler   { _env.ep(), *this, &Lock::_handle_mode };
	Rom             _config_rom     { _env, "config" };
	Signal_handler  _config_handler { _env.ep(), *this, &Lock::_update_config };

	Tff_font::Static_glyph_buffer<4096> _glyph_buffer { };
	Tff_font                            _default_font;

	short int const hs              { 10 };
	bool            _transparent    { };
	bool            _cmp_valid      { };

	/* pwd state */
	enum State { WAIT_CLICK, RECORD_PWD, COMPARE_PWD } state { WAIT_CLICK };

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

		{
			auto const size  = _default_font.bounding_box();
			auto const sub_w = size.w * text_len / 2;
			auto const sub_h = size.h / 2;

			Text_painter::Position where(int(mode.area.w / 2 - sub_w),
			                             int(mode.area.h / 2 - sub_h - offset));
			Text_painter::paint(surface, where, _default_font,
			                    fg_color, text);
		}

		_gui->view(_handle, { .title = { }, .rect = mode, .front = true });

		_gui->enqueue<Command::Geometry>(_handle, mode);
		_gui->enqueue<Command::Front   >(_handle);
		_gui->execute();
	}

	void _switch_view_record_pwd()
	{
		char const buffer[] = "Recording password ...";
		auto const bg_black = Color::rgb(255, 255, 255);
		auto const fg_white = Color::rgb(  0,   0,   0);

		_update_view(bg_black, fg_white, hs * 2, buffer, sizeof(buffer) - 1);

		state = RECORD_PWD;
		_cmp_valid = false;
	}

	void _switch_view_compare_pwd()
	{
		char const buffer[] = "Provide password to unlock screen ...";
		auto const bg_white = Color::rgb(0, 0, 0);
		auto const bg_trans = Color::clamped_rgba(16, 16, 16, 16);
		auto const fg_black = Color::rgb(255, 255, 255);

		_update_view(_transparent ? bg_trans : bg_white, fg_black,
		             hs * 2, buffer, sizeof(buffer) - 1);

		state = COMPARE_PWD;

		_cmp_valid = true;
		_pwd.i = 0;
	}

	void _switch_view_initial()
	{
		char const buffer[] = "After the next text input,"
		                      " password recording starts ...";
		auto const bg_red   = Color::rgb(0, 0, 0xac);
		auto const fg_black = Color::rgb(255, 255, 255);

		_update_view(bg_red, fg_black, 0, buffer, sizeof(buffer) - 1);

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
		if (!_gui.constructed())
			return;

		_gui->buffer({ .area = _mode().area, .alpha = _transparent });

		_fb.construct(_env.rm(), _gui->framebuffer.dataspace());

		switch (state) {
		case COMPARE_PWD : _switch_view_compare_pwd(); break;
		case RECORD_PWD  : _switch_view_record_pwd();  break;
		case WAIT_CLICK  : _switch_view_initial();     break;
		}
	}

	void _update_config();
	void _handle_input();
	void _show_box(unsigned, Genode::uint8_t, unsigned);

	Lock(Genode::Env &env)
	:
		_env(env), _default_font(_binary_mono_tff_start, _glyph_buffer)
	{
		_gui.construct(_env, "screen");

		_config_rom.sigh(_config_handler);

		_gui->info_sigh (_mode_handler);
		_gui->input.sigh(_input_handler);

		_update_config();

		_handle_mode();
	}

};


void Lock::_show_box(unsigned const chars, Genode::uint8_t const cc, unsigned const sc)
{
	if (!chars) return;

	unsigned * const pixels  = _fb->local_addr<unsigned>();
	auto       const fb_size = _fb->size();

	auto const mode = _mode();
	int  const hsa  = hs + 2;

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
				}
				if (_pwd.i > 0 && state == RECORD_PWD) {
					_pwd.max = _pwd.i;
					_switch_view_compare_pwd();
				}

				_pwd.i = 0;
			} else {
				if (state == RECORD_PWD) {
					_pwd.chars[_pwd.i] = cp.value;
					_inc_pwd_i();
					_show_box(_pwd.i, ~0, 0);
				}
				if (state == COMPARE_PWD) {
					if (_cmp_valid)
						_cmp_valid = (_pwd.chars[_pwd.i] == cp.value);

					_inc_pwd_i();
					_show_box(_pwd.i, 0, ~0);
				}
			}
			if (reset) {
				if (state == RECORD_PWD)
					_switch_view_record_pwd();
				if (state == COMPARE_PWD)
					_switch_view_compare_pwd();
			}
		});
	});

	if (!unlock || _handle.value == 0)
		return;

	_gui->destroy_view(_handle);
	_handle = {};

	_gui->info_sigh ({});
	_gui->input.sigh({});

	_fb .destruct();
	_gui.destruct();

	Genode::memset(&_pwd, 0, sizeof(_pwd));

	_env.parent().exit(0);
}


void Lock::_update_config()
{
	if (!_gui.constructed())
		return;

	_config_rom.update();

	if (!_config_rom.valid())
		return;

	Genode::String<sizeof(_pwd.chars)> passwd;

	Genode::Xml_node node = _config_rom.xml();
	passwd = node.attribute_value("password", passwd);

	bool transparent = _transparent;
	_transparent = node.attribute_value("transparent", _transparent);

	bool switch_view = transparent != _transparent;

	if (passwd.length() > 1) {
		for (unsigned i = 0; i < passwd.length(); i++)
			_pwd.chars[i] = passwd.string()[i];

		_pwd.max = unsigned(passwd.length() - 1);
		_pwd.i   = 0;

		if (!switch_view)
			switch_view = state != COMPARE_PWD;

		state = COMPARE_PWD;
	}

	if (_handle.value > 0 && switch_view)
		_handle_mode();
}


void Component::construct(Genode::Env &env) { static Lock lock(env); }
