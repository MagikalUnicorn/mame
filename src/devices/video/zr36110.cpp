// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

// Zoran ZR36110 mpeg video decoder

#include "emu.h"
#include "zr36110.h"

#include <algorithm>

#define LOG_MICROCODE (1U << 1)
#define LOG_SETUP     (1U << 2)
#define LOG_STREAM    (1U << 3)
#define LOG_COMMAND   (1U << 4)

#define VERBOSE       (0)
#include "logmacro.h"

#define LOGMICROCODE(...) LOGMASKED(LOG_MICROCODE, __VA_ARGS__)

DEFINE_DEVICE_TYPE(ZR36110, zr36110_device, "zr36110", "Zoran ZR36110 mpeg decoder")

zr36110_device::zr36110_device(const machine_config &mconfig, char const *tag, device_t *owner, u32 clock) :
	device_t(mconfig, ZR36110, tag, owner, clock),
	m_drq_w(*this),
	m_sp_frm_w{*this, *this},
	m_sp_dat_w{*this, *this},
	m_sp_clk_w{*this, *this}
{
}

void zr36110_device::device_start()
{
	m_demux = std::make_unique<mpeg_system>();
	m_decoder = std::make_unique<mpeg_video>(MAX_VIDEO_WIDTH, MAX_VIDEO_HEIGHT);
	m_video_data = std::make_unique<u8[]>(VIDEO_BUFFER_BYTES);
	m_display_data = std::make_unique<u8[]>(PICTURE_BYTES);
	for (auto &data : m_serial_data)
		data = std::make_unique<u8[]>(SERIAL_BUFFER_BYTES);
	for (auto &data : m_picture_data)
		data = std::make_unique<u8[]>(PICTURE_BYTES);
	m_process_timer = timer_alloc(FUNC(zr36110_device::process_tick), this);
	m_bus_timer = timer_alloc(FUNC(zr36110_device::bus_tick), this);
	m_serial_timer = timer_alloc(FUNC(zr36110_device::serial_tick), this);
	m_demux->register_save_state(*this);
	m_decoder->register_save_state(*this);

	save_pointer(NAME(m_video_data), VIDEO_BUFFER_BYTES);
	save_pointer(NAME(m_display_data), PICTURE_BYTES);
	for (unsigned i = 0; i < 2; ++i)
		save_pointer(NAME(m_serial_data[i]), SERIAL_BUFFER_BYTES, i);
	for (unsigned i = 0; i < 3; ++i)
		save_pointer(NAME(m_picture_data[i]), PICTURE_BYTES, i);
	save_item(NAME(m_host_data));
	save_item(NAME(m_host_read));
	save_item(NAME(m_host_count));
	save_item(NAME(m_video_read));
	save_item(NAME(m_video_count));
	save_item(NAME(m_serial_read));
	save_item(NAME(m_serial_count));
	save_item(NAME(m_serial_rate));
	save_item(NAME(m_serial_fraction));
	save_item(NAME(m_serial_clock_fraction));
	save_item(NAME(m_serial_word));
	save_item(NAME(m_serial_bits));
	save_item(NAME(m_serial_frame_clocks));
	save_item(NAME(m_serial_frm));
	save_item(NAME(m_serial_dat));
	save_item(NAME(m_serial_clk));
	save_item(NAME(m_picture_width));
	save_item(NAME(m_picture_height));
	save_item(NAME(m_picture_type));
	save_item(NAME(m_display_width));
	save_item(NAME(m_display_height));
	save_item(NAME(m_selected_video));
	save_item(NAME(m_selected_serial));
	save_item(NAME(m_older_reference));
	save_item(NAME(m_newer_reference));
	save_item(NAME(m_target_picture));
	save_item(NAME(m_reference_count));
	save_item(NAME(m_pending_reference));
	save_item(NAME(m_queued_picture));
	save_item(NAME(m_current_type));
	save_item(NAME(m_last_coded_picture));
	save_item(NAME(m_last_coded_width));
	save_item(NAME(m_last_coded_height));
	save_item(NAME(m_last_coded_displayed));
	save_item(NAME(m_burst_left));
	save_item(NAME(m_slow_factor));
	save_item(NAME(m_end_mode));
	save_item(NAME(m_drq));
	save_item(NAME(m_bus_wait));
	save_item(NAME(m_buffer_full));
	save_item(NAME(m_system_end));
	save_item(NAME(m_video_end));
	save_item(NAME(m_command_end));
	save_item(NAME(m_freeze));
	save_item(NAME(m_picture_rate));
	save_item(NAME(m_picture_fraction));
	save_item(NAME(m_mc1_adr));
	save_item(NAME(m_mc23_adr));
	save_item(NAME(m_setup_adr));
	save_item(NAME(m_state));
	save_item(NAME(m_setup));
	save_item(NAME(m_cmd_phase));
	save_item(NAME(m_cmd));
	save_item(NAME(m_bus_control));
}

void zr36110_device::device_reset()
{
	memset(m_setup, 0, sizeof(m_setup));
	m_setup[0] = 0x10;
	m_mc1_adr = 0;
	m_mc23_adr = 0;
	m_setup_adr = 0;
	m_state = S_IDLE;
	m_cmd_phase = false;
	m_cmd = 0;
	m_bus_control = 0x10;
	m_drq = false;
	m_display_width = m_display_height = 0;
	std::fill_n(m_display_data.get(), PICTURE_BYTES, 0);
	clear_streams();
	update_video_bitmap();
	m_drq_w(0);
}

void zr36110_device::device_post_load()
{
	update_video_bitmap();
	m_drq_w(m_drq);
	restore_serial_lines();
}

void zr36110_device::mc18_w(u8 data)
{
	m_mc23_adr = 0;
	m_setup_adr = 0;
	LOGMICROCODE("mc1[%03x] = %02x\n", m_mc1_adr, data);
	m_mc1_adr = m_mc1_adr + 1;
}

void zr36110_device::mc1x_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		mc18_w(data >> 8);
	else if(m_bus_control & 0x40) {
		mc18_w(data >> 8);
		mc18_w(data);
	} else {
		mc18_w(data);
		mc18_w(data >> 8);
	}
}

void zr36110_device::mc1_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		mc18_w(data);
	else if(m_bus_control & 0x40) {
		mc18_w(data);
		mc18_w(data >> 8);
	} else {
		mc18_w(data >> 8);
		mc18_w(data);
	}
}

void zr36110_device::mc238_w(u8 data)
{
	m_setup_adr = 0;
	m_mc1_adr = 0;
	LOGMICROCODE("mc23[%03x] = %02x\n", m_mc23_adr, data);
	m_mc23_adr = m_mc23_adr + 1;
	if(m_mc23_adr >= 0x2000 && m_state == S_INIT)
		m_state = S_IDLE;
}

void zr36110_device::mc23_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		mc238_w(data);
	else if(m_bus_control & 0x40) {
		mc238_w(data);
		mc238_w(data >> 8);
	} else {
		mc238_w(data >> 8);
		mc238_w(data);
	}
}

void zr36110_device::mc23x_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		mc238_w(data >> 8);
	else if(m_bus_control & 0x40) {
		mc238_w(data >> 8);
		mc238_w(data);
	} else {
		mc238_w(data);
		mc238_w(data >> 8);
	}
}

double zr36110_device::u6_10_to_f(u16 val)
{
	if(val & 0x8000)
		return (val - 0x10000) / 1024.0;
	else
		return val / 1024.0;
}

double zr36110_device::u5_19_to_f(u32 val)
{
	return val / 524288.0;
}

void zr36110_device::setup_show() const
{
	LOG_OUTPUT_FUNC("Chip setup:\n");
	LOG_OUTPUT_FUNC("  00    - buswidth=%d order=%s stream=%s burst_length=%d\n",
					m_setup[0x00] & 0x80 ? 16 : 8,
					m_setup[0x00] & 0x40 ? "lsb" : "msb",
					m_setup[0x00] & 0x20 ? "dma" : "pio",
					m_setup[0x00] & 0x1f);
	LOG_OUTPUT_FUNC("  01    - busoff=%d\n",
					m_setup[0x01] << 4);
	LOG_OUTPUT_FUNC("  02    - type=%s rate=%s interface=%s half=%s dram=%s start=%s last=%s\n",
					m_setup[0x02] & 0x80 ? m_setup[0x02] & 0x04 ? "serial" : "video" : m_setup[0x02] & 0x04 ? "sectors" : "stream",
					m_setup[0x02] & 0x40 ? "pal" : "ntsc",
					m_setup[0x02] & 0x20 ? "enabled" : "normal",
					m_setup[0x02] & 0x10 ? "as-needed" : "always",
					m_setup[0x02] & 0x08 ? "8M" : "4M",
					m_setup[0x02] & 0x02 ? "sequence" : "any",
					m_setup[0x02] & 0x01 ? "image" : "background");
	LOG_OUTPUT_FUNC("  03    - size=%s hinterpolate=%s color=%s bias=%s fi=%s\n",
					m_setup[0x03] & 0x80 ? "sif-prog" : "ccir-int",
					m_setup[0x03] & 0x40 ? "off" : "on",
					m_setup[0x03] & 0x20 ? (m_setup[0x03] & 0x18) == 0x00 ? "rgb888" : (m_setup[0x03] & 0x18) == 0x08 ? "rgb565" : (m_setup[0x03] & 0x18) == 0x10 ? "rgb555" : "rgb?" : m_setup[0x03] & 0x04 ? "yuv411" : "yuv422",
					m_setup[0x03] & 0x02 ? "on" : "off",
					m_setup[0x03] & 0x01 ? "blank" : "field");
	LOG_OUTPUT_FUNC("  04    - signals=%s hsync=%s,%s vsync=%s,%s field=%s,%s clk=%s\n",
					m_setup[0x04] & 0x80 ? "output" : "input",
					m_setup[0x04] & 0x40 ? "high" : "low",
					m_setup[0x04] & 0x08 ? "blank" : "sync",
					m_setup[0x04] & 0x20 ? "high" : "low",
					m_setup[0x04] & 0x04 ? "blank" : "sync",
					m_setup[0x04] & 0x10 ? "high" : "low",
					m_setup[0x04] & 0x02 ? "II" : "I",
					m_setup[0x04] & 0x01 ? "qclk_v" : "vclk");
	LOG_OUTPUT_FUNC("  05    - sp2frm=%s sp2clk=%s sp2=%s sp1frm=%s sp1clk=%s sp1=%s\n",
					m_setup[0x05] & 0x80 ? m_setup[0x05] & 0x20 ? "window" : "transition" : "pulse",
					m_setup[0x05] & 0x40 ? "output" : "input",
					m_setup[0x05] & 0x10 ? "on" : "off",
					m_setup[0x05] & 0x08 ? m_setup[0x05] & 0x02 ? "window" : "transition" : "pulse",
					m_setup[0x05] & 0x04 ? "output" : "input",
					m_setup[0x05] & 0x01 ? "on" : "off");
	LOG_OUTPUT_FUNC("  06    - serial_audio_mp1=%s fi=%s\n",
					m_setup[0x06] & 0x02 ? "yes" : "no",
					m_setup[0x06] & 0x01 ? "normal" : "resolution");
	LOG_OUTPUT_FUNC("  08-21 - active=%dx%d offset=%d,%d total=%dx%d blank=%d,%d front_blank=%d,%d delay=%d,%d, sync=%d,%d\n",
					(m_setup[0x08] << 8) | m_setup[0x09],
					(m_setup[0x14] << 8) | m_setup[0x15],
					(m_setup[0x0a] << 8) | m_setup[0x0b],
					(m_setup[0x16] << 8) | m_setup[0x17],
					(m_setup[0x0c] << 8) | m_setup[0x0d],
					(m_setup[0x18] << 8) | m_setup[0x19],
					(m_setup[0x0e] << 8) | m_setup[0x0f],
					(m_setup[0x1a] << 8) | m_setup[0x1b],
					(m_setup[0x10] << 8) | m_setup[0x11],
					(m_setup[0x1c] << 8) | m_setup[0x1d],
					(m_setup[0x12] << 8) | m_setup[0x13],
					(m_setup[0x1e] << 8) | m_setup[0x1f],
					m_setup[0x20],
					m_setup[0x21]);
	LOG_OUTPUT_FUNC("  22-29 - crv=%f cbu=%f cgv=%f cgu=%f\n",
					u6_10_to_f((m_setup[0x22] << 8) | m_setup[0x23]),
					u6_10_to_f((m_setup[0x24] << 8) | m_setup[0x25]),
					u6_10_to_f((m_setup[0x26] << 8) | m_setup[0x27]),
					u6_10_to_f((m_setup[0x28] << 8) | m_setup[0x29]));
	LOG_OUTPUT_FUNC("  2a-2c - background=%d,%d,%d\n",
					m_setup[0x2a],
					m_setup[0x2b],
					m_setup[0x2c]);
	LOG_OUTPUT_FUNC("  2d    - high_byte=%02x\n",
					m_setup[0x2d]);
	LOG_OUTPUT_FUNC("  2e-40 - spclk=1/%d,1/%d spbr=%f,%f spdelay=%d,%d vdelay=%d vclk=%d\n",
					(m_setup[0x34] << 8) | m_setup[0x35],
					(m_setup[0x36] << 8) | m_setup[0x37],
					u5_19_to_f((m_setup[0x2e] << 16) | (m_setup[0x2f] << 8) | m_setup[0x30]),
					u5_19_to_f((m_setup[0x31] << 16) | (m_setup[0x32] << 8) | m_setup[0x33]),
					(m_setup[0x38] << 8) | m_setup[0x39],
					(m_setup[0x3a] << 8) | m_setup[0x3b],
					(m_setup[0x3c] << 8) | m_setup[0x3d],
					(m_setup[0x3e] << 16) | (m_setup[0x3f] << 8) | m_setup[0x40]);
	LOG_OUTPUT_FUNC("  42-44 - sp2id=%02x sp1id=%02x vidid=%02x\n",
					m_setup[0x42],
					m_setup[0x43],
					m_setup[0x44]);
}

void zr36110_device::setup8_w(u8 data)
{
	if (m_state != S_IDLE && m_state != S_INIT)
	{
		if (!(m_bus_control & 0x20))
			input_byte(data);
		return;
	}
	m_mc1_adr = 0;
	m_mc23_adr = 0;
	if(m_setup_adr < 0x80)
		m_setup[m_setup_adr] = data;
	if(m_setup_adr == 0x80 && (VERBOSE & LOG_SETUP))
		setup_show();
	m_setup_adr = m_setup_adr + 1;
}

void zr36110_device::setup_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		setup8_w(data);
	else if(m_bus_control & 0x40) {
		setup8_w(data);
		setup8_w(data >> 8);
	} else {
		setup8_w(data >> 8);
		setup8_w(data);
	}
}

void zr36110_device::setupx_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		setup8_w(data >> 8);
	else if(m_bus_control & 0x40) {
		setup8_w(data >> 8);
		setup8_w(data);
	} else {
		setup8_w(data);
		setup8_w(data >> 8);
	}
}

void zr36110_device::cmd8_w(u8 data)
{
	//  LOG("cmd_w %02x\n", data);

	if(m_cmd_phase) {
		m_cmd |= data;
		switch(m_cmd & 0xff) {
		case 0x00: go(); break;
		case 0x80: case 0x81: case 0x82: case 0x83: end_decoding(m_cmd & 3); break;
		case 0x10: m_state = S_PAUSE; break;
		case 0x20: m_state = S_DFIRST; break;
		case 0x30: m_state = S_STEP; break;
		case 0x40: m_state = S_DNEXT; break;
		case 0x60: m_freeze = true; break;
		case 0x90:
			m_state = S_NORMAL;
			m_freeze = false;
			m_slow_factor = 1;
			break;
		default:
			if ((m_cmd & 0xf8) == 0x50 && (m_cmd & 7) >= 2)
			{
				m_slow_factor = m_cmd & 7;
				m_state = S_NORMAL;
			}
			else
				LOGMASKED(LOG_COMMAND, "Unimplemented command %02x.%02x\n", m_cmd >> 8, m_cmd & 0xff);
		}
		m_process_timer->adjust(attotime::zero);
		update_drq();
	} else
		m_cmd = data << 8;
	m_cmd_phase = !m_cmd_phase;
}

void zr36110_device::cmd_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		cmd8_w(data);
	else if(m_bus_control & 0x40) {
		cmd8_w(data);
		cmd8_w(data >> 8);
	} else {
		cmd8_w(data >> 8);
		cmd8_w(data);
	}
}

void zr36110_device::cmdx_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		cmd8_w(data >> 8);
	else if(m_bus_control & 0x40) {
		cmd8_w(data >> 8);
		cmd8_w(data);
	} else {
		cmd8_w(data);
		cmd8_w(data >> 8);
	}
}

void zr36110_device::dma8_w(u8 data)
{
	input_byte(data);
	if (m_burst_left && !--m_burst_left)
	{
		// A block DMA can finish its burst after DREQ has already dropped for
		// buffer pressure.  Its final byte must still arm the off-time timer.
		start_bus_off();
		update_drq();
	}
}

void zr36110_device::dma_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		dma8_w(data);
	else if(m_bus_control & 0x40) {
		dma8_w(data);
		dma8_w(data >> 8);
	} else {
		dma8_w(data >> 8);
		dma8_w(data);
	}
}

void zr36110_device::dmax_w(u16 data)
{
	if(!(m_bus_control & 0x80))
		dma8_w(data >> 8);
	else if(m_bus_control & 0x40) {
		dma8_w(data >> 8);
		dma8_w(data);
	} else {
		dma8_w(data);
		dma8_w(data >> 8);
	}
}

u8 zr36110_device::stat08_r()
{
	return 0x20 | m_state;
}

u8 zr36110_device::stat18_r()
{
	return status1();
}

u8 zr36110_device::stat28_r()
{
	return status2();
}

u16 zr36110_device::stat0_r()
{
	return 0x20 | m_state;
}

u16 zr36110_device::stat1_r()
{
	return status1();
}

u16 zr36110_device::stat2_r()
{
	return status2();
}

u16 zr36110_device::stat0x_r()
{
	return (0x20 | m_state) << 8;
}

u16 zr36110_device::stat1x_r()
{
	return status1() << 8;
}

u16 zr36110_device::stat2x_r()
{
	return status2() << 8;
}

u8 zr36110_device::user8_r()
{
	return 0;
}

u16 zr36110_device::user_r()
{
	return 0;
}

u16 zr36110_device::userx_r()
{
	return 0;
}

void zr36110_device::go()
{
	if(m_state != S_IDLE) {
		logerror("command GO on wrong state, ignoring\n");
		return;
	}
	LOGMASKED(LOG_COMMAND, "command GO\n");
	clear_streams();
	m_bus_control = m_setup[0];
	m_selected_video = m_setup[0x44];
	m_selected_serial[0] = m_setup[0x43];
	m_selected_serial[1] = m_setup[0x42];
	m_state = S_NORMAL;
	m_serial_timer->adjust(attotime::from_msec(1), 0, attotime::from_msec(1));
	update_drq();
}

void zr36110_device::end_decoding(u8 mode)
{
	if(m_state == S_IDLE || m_state == S_INIT) {
		logerror("command END DECODING on wrong state, ignoring\n");
		return;
	}
	LOGMASKED(LOG_COMMAND, "command END DECODING %d\n", mode);
	m_state = S_END;
	m_end_mode = mode;
	m_command_end = true;
	m_system_end = m_video_end = true;
	m_host_count = m_video_count = 0;
	// Unlike a natural stream end, this command stops serial output rather
	// than draining the buffered audio/private data (datasheet figure 29).
	m_serial_timer->adjust(attotime::never);
	for (unsigned port = 0; port < 2; ++port)
	{
		m_serial_count[port] = m_serial_fraction[port] = m_serial_clock_fraction[port] = 0;
		m_serial_word[port] = m_serial_bits[port] = m_serial_frame_clocks[port] = 0;
		m_serial_frm[port] = m_serial_dat[port] = m_serial_clk[port] = false;
	}
	restore_serial_lines();
	m_freeze = false;
	m_slow_factor = 1;
	if (mode == 2)
	{
		// A following reference may be later in display order than the last
		// coded B picture.  END02 selects that B picture, not the held reference.
		m_pending_reference = NO_PICTURE;
		m_queued_picture = m_last_coded_displayed ? NO_PICTURE : m_last_coded_picture;
		if (m_queued_picture != NO_PICTURE)
		{
			m_picture_width[m_queued_picture] = m_last_coded_width;
			m_picture_height[m_queued_picture] = m_last_coded_height;
			// Latch the final picture at the next display boundary, even if a
			// prior slow-motion or freeze command delayed normal presentation.
			m_picture_fraction = 1.0;
		}
	}
	if (m_queued_picture == NO_PICTURE)
		finish_stream();
	update_drq();
}

void zr36110_device::clear_streams()
{
	m_process_timer->adjust(attotime::never);
	m_bus_timer->adjust(attotime::never);
	m_serial_timer->adjust(attotime::never);
	m_demux->clear();
	m_decoder->clear();
	std::fill(std::begin(m_host_data), std::end(m_host_data), 0);
	m_host_read = m_host_count = m_video_read = m_video_count = 0;
	std::fill_n(m_video_data.get(), VIDEO_BUFFER_BYTES, 0);
	for (unsigned i = 0; i < 2; ++i)
	{
		m_serial_read[i] = m_serial_count[i] = m_serial_rate[i] = m_serial_fraction[i] = 0;
		m_serial_clock_fraction[i] = 0;
		m_serial_word[i] = m_serial_bits[i] = m_serial_frame_clocks[i] = 0;
		m_serial_frm[i] = m_serial_dat[i] = m_serial_clk[i] = false;
		m_selected_serial[i] = 0xff;
		std::fill_n(m_serial_data[i].get(), SERIAL_BUFFER_BYTES, 0);
	}
	for (unsigned i = 0; i < 3; ++i)
	{
		m_picture_width[i] = m_picture_height[i] = m_picture_type[i] = 0;
		std::fill_n(m_picture_data[i].get(), PICTURE_BYTES, 0);
	}
	m_selected_video = 0xff;
	m_older_reference = m_target_picture = 0;
	m_newer_reference = 1;
	m_reference_count = 0;
	m_pending_reference = m_queued_picture = NO_PICTURE;
	m_last_coded_picture = NO_PICTURE;
	m_last_coded_width = m_last_coded_height = 0;
	m_last_coded_displayed = false;
	m_current_type = m_burst_left = m_end_mode = 0;
	m_slow_factor = 1;
	m_bus_wait = m_buffer_full = m_system_end = m_video_end = m_command_end = m_freeze = false;
	m_picture_rate = 30000.0 / 1001.0;
	m_picture_fraction = 0;
	restore_serial_lines();
}

unsigned zr36110_device::video_capacity() const
{
	// MPEG systems STD capacity is 46 KiB; the elementary video VBV limit
	// is 40 KiB.  High-resolution stills use a 224 KiB code buffer.
	const bool high_resolution = m_picture_width[m_target_picture] > 384 || m_picture_height[m_target_picture] > 288;
	return high_resolution ? 224 * 1024 : (m_setup[2] & 0x80) ? 40 * 1024 : 46 * 1024;
}

bool zr36110_device::input_ready() const
{
	return m_state != S_IDLE && m_state != S_INIT && !m_system_end
		&& m_host_count <= HOST_BUFFER_BYTES - 32
		&& m_video_count < video_capacity()
		&& m_serial_count[0] < SERIAL_BUFFER_BYTES && m_serial_count[1] < SERIAL_BUFFER_BYTES;
}

void zr36110_device::update_drq()
{
	// Starting a request reserves enough host staging space for the complete
	// BSLN burst.  Keep DREQ asserted through that burst even if a code buffer
	// fills; reasserting it mid-burst can lose an edge at a block DMA host.
	const bool ready = m_drq
		? m_burst_left && m_state != S_IDLE && m_state != S_INIT && !m_system_end
		: input_ready();
	const bool request = (m_bus_control & 0x20) && !m_bus_wait && ready;
	if (request == m_drq)
		return;
	m_drq = request;
	if (request)
		m_burst_left = std::max<unsigned>(1, m_bus_control & 0x1f) * ((m_bus_control & 0x80) ? 2 : 1);
	else
		start_bus_off();
	m_drq_w(request);
}

void zr36110_device::start_bus_off()
{
	m_bus_wait = true;
	m_bus_timer->adjust(clocks_to_attotime(32 * std::max<unsigned>(1, m_setup[1])) / (m_mb4 ? 9 : 8));
}

TIMER_CALLBACK_MEMBER(zr36110_device::bus_tick)
{
	m_bus_wait = false;
	update_drq();
}

void zr36110_device::input_byte(u8 data)
{
	if (m_state == S_IDLE || m_state == S_INIT || m_system_end)
		return;
	if (m_host_count == HOST_BUFFER_BYTES)
	{
		LOGMASKED(LOG_STREAM, "Host write while input is full\n");
		return;
	}
	m_host_data[(m_host_read + m_host_count++) & (HOST_BUFFER_BYTES - 1)] = data;
	m_process_timer->adjust(attotime::zero);
	update_drq();
}

std::size_t zr36110_device::video_payload(std::span<const u8> data)
{
	const unsigned capacity = video_capacity();
	const unsigned count = std::min<std::size_t>(data.size(), capacity - std::min(capacity, m_video_count));
	for (unsigned i = 0; i < count; ++i)
		m_video_data[(m_video_read + m_video_count + i) & (VIDEO_BUFFER_BYTES - 1)] = data[i];
	m_video_count += count;
	if (count < data.size() || m_video_count == capacity)
		m_buffer_full = true;
	return count;
}

std::size_t zr36110_device::serial_payload(unsigned port, std::span<const u8> data)
{
	const unsigned count = std::min<std::size_t>(data.size(), SERIAL_BUFFER_BYTES - m_serial_count[port]);
	for (unsigned i = 0; i < count; ++i)
		m_serial_data[port][(m_serial_read[port] + m_serial_count[port] + i) & (SERIAL_BUFFER_BYTES - 1)] = data[i];
	m_serial_count[port] += count;
	if (count < data.size() || m_serial_count[port] == SERIAL_BUFFER_BYTES)
		m_buffer_full = true;
	return count;
}

std::size_t zr36110_device::stream_payload(const mpeg_system::packet_info &packet, std::span<const u8> data)
{
	if (!m_selected_video && (packet.stream_id & 0xf0) == 0xe0)
		m_selected_video = packet.stream_id;
	if (m_selected_video != 0xff && packet.stream_id == m_selected_video)
		return video_payload(data);
	for (unsigned port = 0; port < 2; ++port)
	{
		if (!(m_setup[5] & (1 << (4 * port))))
			continue;
		if (!m_selected_serial[port] && (packet.stream_id & 0xe0) == 0xc0)
			m_selected_serial[port] = packet.stream_id;
		if (m_selected_serial[port] != 0xff && packet.stream_id == m_selected_serial[port])
			return serial_payload(port, data);
	}
	return data.size();
}

bool zr36110_device::decode_video()
{
	if (m_state == S_IDLE || m_state == S_INIT || m_state == S_PAUSE || m_video_end || m_queued_picture != NO_PICTURE)
		return false;
	const unsigned count = std::min(m_video_count, VIDEO_BUFFER_BYTES - m_video_read);
	const mpeg_video::picture_buffers buffers =
	{
		{ m_picture_data[m_target_picture].get(), PICTURE_BYTES },
		{ m_picture_data[m_current_type == 3 ? m_older_reference : m_newer_reference].get(), PICTURE_BYTES },
		{ m_picture_data[m_newer_reference].get(), PICTURE_BYTES }
	};
	int width = 0, height = 0;
	std::size_t consumed = 0;
	const auto result = m_decoder->decode(std::span<const u8>(m_video_data.get() + m_video_read, count), consumed,
		buffers, width, height, m_picture_rate);
	m_video_read = (m_video_read + consumed) & (VIDEO_BUFFER_BYTES - 1);
	m_video_count -= consumed;
	switch (result)
	{
	case mpeg_video::decode_result::PICTURE_HEADER:
		m_current_type = u8(m_decoder->coding_type());
		m_target_picture = m_current_type == 3 ? 2 : m_older_reference;
		m_picture_width[m_target_picture] = width;
		m_picture_height[m_target_picture] = height;
		m_picture_type[m_target_picture] = m_current_type;
		// The chip does not support MPEG D pictures.  Other profile restrictions,
		// high-resolution half-size output and timing-register effects remain TODO.
		if (m_current_type == 4)
			m_decoder->clear();
		return true;
	case mpeg_video::decode_result::PICTURE:
		finish_picture(width, height);
		m_video_end = m_decoder->picture_ends_sequence();
		if (m_video_end && (m_setup[2] & 0x80))
			m_system_end = true;
		return true;
	case mpeg_video::decode_result::SEQUENCE_END:
		m_video_end = true;
		if (m_setup[2] & 0x80)
			m_system_end = true;
		return true;
	case mpeg_video::decode_result::INVALID_DATA:
		return true;
	case mpeg_video::decode_result::NEED_DATA:
		return consumed != 0;
	}
	return false;
}

void zr36110_device::finish_picture(int width, int height)
{
	// An entry point can be an open GOP.  Its leading B pictures need an
	// earlier reference that was never received; consume them without display.
	if((!m_reference_count && m_current_type != 1) || (m_current_type == 3 && m_reference_count < 2))
		return;
	m_picture_width[m_target_picture] = width;
	m_picture_height[m_target_picture] = height;
	m_last_coded_picture = m_target_picture;
	m_last_coded_width = width;
	m_last_coded_height = height;
	m_last_coded_displayed = false;
	if (m_current_type == 3)
		m_queued_picture = m_target_picture;
	else
	{
		m_reference_count = std::min<unsigned>(2, m_reference_count + 1);
		m_queued_picture = m_pending_reference;
		m_pending_reference = m_target_picture;
		m_older_reference = m_newer_reference;
		m_newer_reference = m_target_picture;
	}
}

void zr36110_device::finish_stream()
{
	if (m_queued_picture != NO_PICTURE)
		return;
	if (m_pending_reference != NO_PICTURE)
	{
		m_queued_picture = m_pending_reference;
		m_pending_reference = NO_PICTURE;
		return;
	}
	// Program end stops host input, but later video sequences can already be
	// buffered behind the sequence whose display pictures just drained.
	if (m_system_end && (!m_video_count || (m_setup[2] & 0x80)))
	{
		if (serial_pending(0) || serial_pending(1))
			return;
		m_state = S_IDLE;
		m_serial_timer->adjust(attotime::never);
		if (m_command_end ? (m_end_mode == 1 || m_end_mode == 3) : !(m_setup[2] & 1))
		{
			m_display_width = m_display_height = 0;
			update_video_bitmap();
		}
	}
	else
	{
		m_video_end = false;
		m_older_reference = m_target_picture = 0;
		m_newer_reference = 1;
		m_reference_count = 0;
	}
}

TIMER_CALLBACK_MEMBER(zr36110_device::process_tick)
{
	bool progress;
	do
	{
		progress = decode_video();
		if (m_host_count && !m_system_end)
		{
			const std::span<const u8> input(m_host_data + m_host_read, std::min(m_host_count, HOST_BUFFER_BYTES - m_host_read));
			std::size_t consumed = 0;
			if (m_setup[2] & 0x80)
			{
				if (!(m_setup[2] & 4))
					consumed = video_payload(input);
				else
				{
					consumed = input.size();
					for (unsigned port = 0; port < 2; ++port)
						if ((m_setup[5] & (1 << (4 * port))) && m_selected_serial[port] == 0xbf)
						{
							consumed = serial_payload(port, input);
							break;
						}
				}
			}
			else
			{
				const auto result = m_demux->parse(input, consumed, [this](const auto &packet, auto data) { return stream_payload(packet, data); });
				if (result == mpeg_system::parse_result::SYSTEM_END)
				{
					m_system_end = true;
				}
			}
			m_host_read = (m_host_read + consumed) & (HOST_BUFFER_BYTES - 1);
			m_host_count -= consumed;
			progress = progress || consumed;
		}
		if (m_state != S_IDLE && (m_video_end || (m_system_end && !m_video_count && !progress)) && m_queued_picture == NO_PICTURE)
		{
			const bool sequence_end = m_video_end;
			finish_stream();
			progress = progress || (sequence_end && !m_video_end);
		}
	} while (progress);
	update_drq();
}

void zr36110_device::vblank_w(int state)
{
	if (!state || m_state == S_IDLE || m_state == S_INIT || m_state == S_PAUSE)
		return;
	// Display-order scheduling is independent of host transfers.  A complete
	// output picture holds the decoder until its display slot, allowing code
	// buffers to fill and the real DMA request to drop.  PTS/STC synchronization
	// and sub-picture hardware timing are not yet implemented.
	const double field_rate = (m_setup[2] & 0x40) ? 50.0 : 60000.0 / 1001.0;
	m_picture_fraction = std::min(1.0, m_picture_fraction) + m_picture_rate / (field_rate * m_slow_factor);
	if (m_queued_picture != NO_PICTURE && m_picture_fraction >= 1.0)
	{
		m_picture_fraction -= 1.0;
		const u8 picture = m_queued_picture;
		if (!m_freeze)
		{
			m_display_width = m_picture_width[picture];
			m_display_height = m_picture_height[picture];
			std::copy_n(m_picture_data[picture].get(), PICTURE_BYTES, m_display_data.get());
			m_last_coded_displayed = picture == m_last_coded_picture;
			update_video_bitmap();
		}
		m_queued_picture = NO_PICTURE;
		if (m_state == S_STEP || ((m_state == S_DFIRST || m_state == S_DNEXT) && m_picture_type[picture] == 1))
			m_state = S_PAUSE;
		m_process_timer->adjust(attotime::zero);
	}
}

void zr36110_device::update_video_bitmap()
{
	if (!video_valid())
	{
		m_video_bitmap.reset();
		return;
	}
	m_video_bitmap.resize(m_display_width, m_display_height);
	const unsigned pitch = (m_display_width + 15) & ~15;
	const unsigned rows = (m_display_height + 15) & ~15;
	const u8 *const luma = m_display_data.get();
	const u8 *const cb = luma + pitch * rows;
	const u8 *const cr = cb + pitch * rows / 4;
	for (unsigned y = 0; y < m_display_height; ++y)
		for (unsigned x = 0; x < m_display_width; ++x)
		{
			const int c = int(luma[y * pitch + x]) - 16;
			const int d = int(cb[(y / 2) * (pitch / 2) + x / 2]) - 128;
			const int e = int(cr[(y / 2) * (pitch / 2) + x / 2]) - 128;
			m_video_bitmap.pix(y, x) = rgb_t(std::clamp((298 * c + 409 * e + 128) >> 8, 0, 255),
				std::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255),
				std::clamp((298 * c + 516 * d + 128) >> 8, 0, 255));
		}
}

u8 zr36110_device::status1()
{
	const u8 result = ((m_current_type & 3) << 4) | (input_ready() ? 0 : 8)
		| (m_state == S_IDLE ? 4 : 0) | (m_buffer_full ? 2 : 0);
	if (!machine().side_effects_disabled())
		m_buffer_full = false;
	return result;
}

u8 zr36110_device::status2() const
{
	return 1 | ((m_display_width > 384 || m_display_height > 288) ? 2 : 0);
}

u32 zr36110_device::serial_rate(unsigned port) const
{
	const unsigned base = 0x2e + 3 * port;
	const u32 ratio = (m_setup[base] << 16) | (m_setup[base + 1] << 8) | m_setup[base + 2];
	const unsigned divisor = (m_setup[0x34 + 2 * port] << 8) | m_setup[0x35 + 2 * port];
	const u32 vclk = (m_setup[0x3e] << 16) | (m_setup[0x3f] << 8) | m_setup[0x40];
	if (ratio && divisor)
		return u64(vclk) * ratio / (u64(divisor) << 19);
	// A random-access packet can begin inside an audio frame.  Locate the next
	// header without discarding the preceding bytes sent to the audio decoder.
	u32 header = 0;
	for (unsigned i = 0; i < m_serial_count[port]; ++i)
	{
		header = (header << 8) | m_serial_data[port][(m_serial_read[port] + i) & (SERIAL_BUFFER_BYTES - 1)];
		const unsigned layer = (header >> 17) & 3;
		if (i >= 3 && (header & 0xfff80000) == 0xfff80000 && layer >= 2 && ((header >> 10) & 3) != 3)
		{
			const u32 rate = SERIAL_BITRATES[layer & 1][(header >> 12) & 15] * 1000;
			if (rate)
				return rate;
		}
	}
	return 0;
}

void zr36110_device::restore_serial_lines()
{
	for (unsigned port = 0; port < 2; ++port)
	{
		m_sp_frm_w[port](m_serial_frm[port]);
		m_sp_dat_w[port](m_serial_dat[port]);
		m_sp_clk_w[port](m_serial_clk[port]);
	}
}

TIMER_CALLBACK_MEMBER(zr36110_device::serial_tick)
{
	if (m_state == S_PAUSE)
		return;
	bool consumed = false;
	for (unsigned port = 0; port < 2; ++port)
	{
		const u8 mode = m_setup[5] >> (4 * port);
		// External serial clocks require a clock-input interface.  Do not drive
		// an input pin or consume its stream as if it were an internal clock.
		if ((mode & 5) != 5)
			continue;
		if (!m_serial_rate[port])
			m_serial_rate[port] = serial_rate(port);
		const u32 vclk = (m_setup[0x3e] << 16) | (m_setup[0x3f] << 8) | m_setup[0x40];
		if (!vclk || !m_serial_rate[port])
			continue;
		unsigned divisor = (m_setup[0x34 + 2 * port] << 8) | m_setup[0x35 + 2 * port];
		if (!divisor)
			divisor = std::max<u32>(4, vclk / m_serial_rate[port]);
		m_serial_clock_fraction[port] += vclk;
		unsigned clocks = m_serial_clock_fraction[port] / (1000 * divisor);
		m_serial_clock_fraction[port] %= 1000 * divisor;
		m_serial_fraction[port] += m_serial_rate[port];
		unsigned words = m_serial_fraction[port] / 16000;
		m_serial_fraction[port] %= 16000;
		// Delivery is still batched at millisecond resolution.  Within a batch,
		// each frame announces sixteen bits one clock before its first data bit.
		// Consecutive frames can share that clock with the preceding final bit.
		while (clocks--)
		{
			const bool data = m_serial_bits[port] ? BIT(m_serial_word[port], --m_serial_bits[port]) : false;
			const bool first = !m_serial_bits[port] && words && m_serial_count[port]
				&& (m_serial_count[port] >= 2 || m_system_end);
			if (first)
			{
				--words;
				const unsigned count = std::min<unsigned>(2, m_serial_count[port]);
				m_serial_word[port] = u16(m_serial_data[port][m_serial_read[port]]) << 8;
				if (count == 2)
					m_serial_word[port] |= m_serial_data[port][(m_serial_read[port] + 1) & (SERIAL_BUFFER_BYTES - 1)];
				// Inferred termination behaviour: pad only the unused serial half
				// word with zero after EOF; no extra coded-input byte is accepted.
				m_serial_read[port] = (m_serial_read[port] + count) & (SERIAL_BUFFER_BYTES - 1);
				m_serial_count[port] -= count;
				m_serial_bits[port] = 16;
				m_serial_frame_clocks[port] = 16;
				consumed = true;
			}
			m_serial_clk[port] = false;
			m_sp_clk_w[port](0);
			if (mode & 8)
			{
				if (mode & 2)
					m_serial_frm[port] = m_serial_frame_clocks[port] != 0;
				else if (first)
					m_serial_frm[port] = !m_serial_frm[port];
				m_sp_frm_w[port](m_serial_frm[port]);
			}
			m_serial_dat[port] = data;
			m_sp_dat_w[port](data);
			if (!(mode & 8))
			{
				m_serial_frm[port] = first;
				m_sp_frm_w[port](first);
			}
			m_serial_clk[port] = true;
			m_sp_clk_w[port](1);
			if (m_serial_frame_clocks[port])
				--m_serial_frame_clocks[port];
		}
	}
	if (consumed || m_system_end)
		m_process_timer->adjust(attotime::zero);
}
