// license:BSD-3-Clause
// copyright-holders:David Haywood, MagikalUnicorn

#ifndef MAME_TVGAMES_XAVIX_IO_LIGHTGUN_H
#define MAME_TVGAMES_XAVIX_IO_LIGHTGUN_H

#pragma once

#include "screen.h"

#include <optional>
#include <utility>

class beep_device;
class microphone_device;

class xavix_lightgun_device : public device_t, public device_video_interface
{
public:
	enum class model { STANDARD, ADVENTURE, REVOLUTION, BUCKMASTERS, SHATEKING, AIR_CANNON, VIDEO_CHALLENGER, DRAGON_CANNON };

	xavix_lightgun_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);
	xavix_lightgun_device(const machine_config &mconfig, const char *tag, device_t *owner, model controls, bool secondary, u32 clock = 0);

	auto enable_callback() { return m_enable_cb.bind(); }
	auto reset_callback() { return m_reset_cb.bind(); }
	template <typename... T> void set_power_off_callback(T &&... args) { m_power_off_cb.set(std::forward<T>(args)...); }
	template <typename... T> void set_video_callback(T &&... args) { m_video_cb.set(std::forward<T>(args)...); }
	template <typename... T> void set_event_callback(T &&... args) { m_event_cb.set(std::forward<T>(args)...); }
	void set_recoil_output(const char *name) ATTR_COLD;
	device_sound_interface &microphone() const ATTR_COLD;
	device_sound_interface &voice_tone() const ATTR_COLD;

	void recoil_w(int state);
	void vblank_w(int state);
	u8 capture_x() const { return m_capture_x; }
	u8 capture_y() const { return m_capture_y; }
	bool secondary() const { return m_secondary; }
	bool removable() const { return m_secondary && ((m_model == model::ADVENTURE) || (m_model == model::REVOLUTION)); }
	int connected_r() { return m_connection.read_safe(1); }
	int trigger_r() { return BIT(m_buttons->read(), 0); }
	int secondary_button_r() { return BIT(m_buttons->read(), 1); }
	int power_r() { return BIT(m_power->read(), 7); }
	int vibration_r() { return m_vibration->read(); }
	ioport_value panel_r() { return m_panel->read(); }
	DECLARE_INPUT_CHANGED_MEMBER(power_changed);
	DECLARE_INPUT_CHANGED_MEMBER(reset_changed);

protected:
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;

private:
	static constexpr int APERTURE_HALF_WIDTH = 4;
	rectangle aperture() const;
	TIMER_CALLBACK_MEMBER(sample);

	model const m_model;
	bool const m_secondary;
	int const m_sample_lines;
	required_ioport m_x, m_y, m_buttons;
	optional_ioport m_connection, m_power, m_vibration, m_panel;
	optional_device<microphone_device> m_microphone;
	optional_device<beep_device> m_voice_tone;
	std::optional<output_finder<>> m_recoil;
	devcb_write_line m_reset_cb;
	device_delegate<void ()> m_power_off_cb;
	devcb_read_line m_enable_cb;
	screen_update_rgb32_delegate m_video_cb;
	device_delegate<void ()> m_event_cb;
	bitmap_rgb32 m_sample_bitmap;
	emu_timer *m_timer;
	u8 m_aim_x;
	u8 m_aim_y;
	u8 m_capture_x;
	u8 m_capture_y;
};

DECLARE_DEVICE_TYPE(XAVIX_LIGHTGUN, xavix_lightgun_device)

#endif // MAME_TVGAMES_XAVIX_IO_LIGHTGUN_H
