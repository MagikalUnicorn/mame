// license:BSD-3-Clause
// copyright-holders:David Haywood, MagikalUnicorn
/***************************************************************************

    XaviX CRT light gun. The receiver detects light as the beam passes
    through the sight. The gun's trigger is a separate switch read via GPIO.

    TODO: Measure the optical aperture, luminance threshold and pulse timing.

***************************************************************************/

#include "emu.h"
#include "xavix_io_lightgun.h"

#include "sound/beep.h"

#include "speaker.h"

#include <algorithm>

DEFINE_DEVICE_TYPE(XAVIX_LIGHTGUN, xavix_lightgun_device, "xavix_lightgun", "XaviX CRT Light Gun")

static INPUT_PORTS_START( xavix_gun )
	auto const &gun = downcast<xavix_lightgun_device const &>(owner);
	PORT_START("X")
	PORT_BIT( 0xff, 0x80, IPT_LIGHTGUN_X ) PORT_CROSSHAIR(X, 1.0, 0.0, 0) PORT_SENSITIVITY(35) PORT_KEYDELTA(15)
	if (gun.removable())
		PORT_CONDITION("CONNECTION", 0x01, EQUALS, 0x01)

	PORT_START("Y")
	PORT_BIT( 0xff, 0x80, IPT_LIGHTGUN_Y ) PORT_CROSSHAIR(Y, 1.0, 0.0, 0) PORT_SENSITIVITY(35) PORT_KEYDELTA(15)
	if (gun.secondary())
		PORT_CODE_DEC(KEYCODE_E)
	if (gun.removable())
		PORT_CONDITION("CONNECTION", 0x01, EQUALS, 0x01)

	PORT_START("BUTTONS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Trigger")
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_gun_reset )
	PORT_START("RESET")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_NAME("Reset") PORT_CODE(input_seq::end_code)
		PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(xavix_lightgun_device::reset_changed), 0)
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_gun_power )
	PORT_START("POWER")
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_NAME("Power")
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_gun_power_switch )
	PORT_START("POWER")
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_NAME("Power Switch")
		PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(xavix_lightgun_device::power_changed), 0)
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_support_gun )
	PORT_INCLUDE(xavix_gun)
	PORT_MODIFY("BUTTONS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Support Gun Trigger") PORT_CONDITION("CONNECTION", 0x01, EQUALS, 0x01)
	PORT_START("CONNECTION")
	PORT_CONFNAME( 0x01, 0x00, "P2 Gun" )
	PORT_CONFSETTING(    0x00, "Disconnected" )
	PORT_CONFSETTING(    0x01, "Connected" )
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_adventure_gun )
	PORT_INCLUDE(xavix_gun)
	PORT_INCLUDE(xavix_gun_power)
	PORT_INCLUDE(xavix_gun_reset)
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_revolution_gun )
	PORT_INCLUDE(xavix_gun)
	PORT_INCLUDE(xavix_gun_power_switch)
	PORT_INCLUDE(xavix_gun_reset)
	PORT_MODIFY("BUTTONS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Trigger / Confirm")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Weapon Select (Machine Gun / Grenade)") PORT_TOGGLE PORT_CODE(KEYCODE_SPACE)
	// The three-position power switch combines Off with vibration disabled/enabled.
	PORT_START("VIBRATION")
	PORT_CONFNAME( 0x01, 0x01, "Vibration" )
	PORT_CONFSETTING(    0x00, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x01, DEF_STR( On ) )
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_buckmasters_rifle )
	PORT_INCLUDE(xavix_gun)
	PORT_INCLUDE(xavix_gun_power)
	PORT_INCLUDE(xavix_gun_reset)
	PORT_MODIFY("BUTTONS")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON6 ) PORT_NAME("Safety") PORT_TOGGLE
	PORT_START("PANEL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("Menu Next") PORT_CODE(KEYCODE_PGDN)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("Menu Previous") PORT_CODE(KEYCODE_PGUP)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_NAME("Menu Select")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_NAME("Pause / Menu")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_4WAY
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_4WAY
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_4WAY
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_4WAY
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_shateking_gun )
	PORT_INCLUDE(xavix_gun)
	PORT_MODIFY("BUTTONS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Push / Shoot")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("Pull / Charge")
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_air_cannon )
	PORT_INCLUDE(xavix_gun)
	PORT_MODIFY("BUTTONS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Air Cannon Trigger")
	auto const &gun = downcast<xavix_lightgun_device const &>(owner);
	if (!gun.secondary())
	{
		PORT_INCLUDE(xavix_gun_power_switch)
	}
	// Usability hack for users without a host microphone.
	PORT_START("VOICE")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("Simulate Voice") PORT_CODE(gun.secondary() ? KEYCODE_M : KEYCODE_N)
		PORT_WRITE_LINE_DEVICE_MEMBER("voice_tone", FUNC(beep_device::set_state))
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_challenger_gun )
	PORT_INCLUDE(xavix_gun)
	PORT_MODIFY("BUTTONS")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, downcast<xavix_lightgun_device const &>(owner).secondary() ? IPT_START2 : IPT_START1 )
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_dragon_cannon )
	PORT_INCLUDE(xavix_gun)
	PORT_MODIFY("BUTTONS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Cannon Attack / Shot")
INPUT_PORTS_END

xavix_lightgun_device::xavix_lightgun_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: xavix_lightgun_device(mconfig, tag, owner, model::STANDARD, false, clock)
{
}

xavix_lightgun_device::xavix_lightgun_device(const machine_config &mconfig, const char *tag, device_t *owner, model controls, bool secondary, u32 clock)
	: device_t(mconfig, XAVIX_LIGHTGUN, tag, owner, clock)
	, device_video_interface(mconfig, *this)
	, m_model(controls)
	, m_secondary(secondary)
	, m_sample_lines((controls == model::ADVENTURE || controls == model::REVOLUTION ||
			controls == model::VIDEO_CHALLENGER || controls == model::DRAGON_CANNON) ? 8 : 1)
	, m_x(*this, "X")
	, m_y(*this, "Y")
	, m_buttons(*this, "BUTTONS")
	, m_connection(*this, "CONNECTION")
	, m_power(*this, "POWER")
	, m_vibration(*this, "VIBRATION")
	, m_panel(*this, "PANEL")
	, m_microphone(*this, "microphone")
	, m_voice_tone(*this, "voice_tone")
	, m_reset_cb(*this)
	, m_power_off_cb(*this)
	, m_enable_cb(*this, 1)
	, m_video_cb(*this)
	, m_event_cb(*this)
	, m_aim_x(0)
	, m_aim_y(0)
	, m_capture_x(0)
	, m_capture_y(0)
{
}

ioport_constructor xavix_lightgun_device::device_input_ports() const
{
	if (removable())
		return INPUT_PORTS_NAME(xavix_support_gun);
	switch (m_model)
	{
	case model::ADVENTURE:       return INPUT_PORTS_NAME(xavix_adventure_gun);
	case model::REVOLUTION:      return INPUT_PORTS_NAME(xavix_revolution_gun);
	case model::BUCKMASTERS:     return INPUT_PORTS_NAME(xavix_buckmasters_rifle);
	case model::SHATEKING:       return INPUT_PORTS_NAME(xavix_shateking_gun);
	case model::AIR_CANNON:      return INPUT_PORTS_NAME(xavix_air_cannon);
	case model::VIDEO_CHALLENGER: return INPUT_PORTS_NAME(xavix_challenger_gun);
	case model::DRAGON_CANNON:   return INPUT_PORTS_NAME(xavix_dragon_cannon);
	default:                     return INPUT_PORTS_NAME(xavix_gun);
	}
}

void xavix_lightgun_device::device_add_mconfig(machine_config &config)
{
	if (m_model == model::AIR_CANNON)
	{
		MICROPHONE(config, m_microphone, 1).front_center();
		BEEP(config, m_voice_tone, 1000);
	}
}

void xavix_lightgun_device::set_recoil_output(const char *name)
{
	// Publish the motor level under the configuring board's output name.
	m_recoil.emplace(*this, name).set_name(name);
}

device_sound_interface &xavix_lightgun_device::microphone() const
{
	return *m_microphone.lookup();
}

device_sound_interface &xavix_lightgun_device::voice_tone() const
{
	return *m_voice_tone.lookup();
}

void xavix_lightgun_device::recoil_w(int state)
{
	if (m_recoil)
		*m_recoil = state;
}

INPUT_CHANGED_MEMBER(xavix_lightgun_device::reset_changed)
{
	m_reset_cb(newval);
}

INPUT_CHANGED_MEMBER(xavix_lightgun_device::power_changed)
{
	if (newval && !machine().side_effects_disabled())
		m_power_off_cb();
}

void xavix_lightgun_device::device_start()
{
	m_video_cb.resolve();
	m_event_cb.resolve_safe();
	m_power_off_cb.resolve_safe();
	screen().register_screen_bitmap(m_sample_bitmap);
	m_timer = timer_alloc(FUNC(xavix_lightgun_device::sample), this);

	save_item(NAME(m_aim_x));
	save_item(NAME(m_aim_y));
	save_item(NAME(m_capture_x));
	save_item(NAME(m_capture_y));
}

rectangle xavix_lightgun_device::aperture() const
{
	rectangle const &visible = screen().visible_area();
	int const x = visible.left() + ((m_aim_x * visible.width()) >> 8);
	int const y = visible.top() + ((m_aim_y * visible.height()) >> 8);
	int const height = std::min(m_sample_lines, visible.height());
	int const top = std::clamp(y - height / 2, visible.top(), visible.bottom() + 1 - height);
	return rectangle(std::max(x - APERTURE_HALF_WIDTH, visible.left()), std::min(x + APERTURE_HALF_WIDTH, visible.right()), top, top + height - 1);
}

void xavix_lightgun_device::vblank_w(int state)
{
	if (state)
	{
		m_aim_x = m_x->read();
		m_aim_y = m_y->read();
		rectangle const area = aperture();
		m_timer->adjust(screen().time_until_pos(area.top(), area.left()));
	}
}

TIMER_CALLBACK_MEMBER(xavix_lightgun_device::sample)
{
	rectangle const area = aperture();
	int const x = screen().hpos();
	int const y = screen().vpos();
	if (!area.contains(x, y))
		return;

	// Sample pixels as the beam emits them. A later palette write cannot take light back.
	bool lit = bool(param);
	if (!lit)
	{
		m_video_cb(screen(), m_sample_bitmap, rectangle(x, x, y, y));
		lit = rgb_t(m_sample_bitmap.pix(y, x)).brightness() != 0;
	}
	if (x < area.right())
	{
		// Once lit, the timer carries the detection to the end of this aperture row.
		m_timer->adjust(screen().time_until_pos(y, lit ? area.right() : x + 1), lit);
		return;
	}

	if (lit && connected_r() && m_enable_cb())
	{
		m_capture_x = m_aim_x;
		m_capture_y = m_aim_y;
		m_event_cb();
	}
	if (y < area.bottom())
		m_timer->adjust(screen().time_until_pos(y + 1, area.left()));
}
