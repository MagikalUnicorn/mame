// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

// Zoran ZR36110 mpeg video decoder

#ifndef MAME_VIDEO_ZR36110_H
#define MAME_VIDEO_ZR36110_H

#pragma once

#include "machine/mpeg_system.h"
#include "mpeg_video.h"

class zr36110_device : public device_t
{
public:
	zr36110_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);

	// The device clock is GCLK.  MB4 selects the internal PCLK PLL ratio.
	void set_mb4(bool state) { m_mb4 = state; }

	auto drq_w()     { return m_drq_w.bind(); }
	auto sp1_frm_w() { return m_sp_frm_w[0].bind();   }
	auto sp1_dat_w() { return m_sp_dat_w[0].bind();   }
	auto sp1_clk_w() { return m_sp_clk_w[0].bind();   }
	auto sp2_frm_w() { return m_sp_frm_w[1].bind();   }
	auto sp2_dat_w() { return m_sp_dat_w[1].bind();   }
	auto sp2_clk_w() { return m_sp_clk_w[1].bind();   }

	void setup8_w(u8 data); // a = 0 (also mpeg data in pio mode)
	void mc18_w  (u8 data); // a = 1
	void cmd8_w  (u8 data); // a = 2
	void mc238_w (u8 data); // a = 3

	void setup_w(u16 data); // a = 0 (also mpeg data in pio mode)
	void mc1_w  (u16 data); // a = 1
	void cmd_w  (u16 data); // a = 2
	void mc23_w (u16 data); // a = 3

	// For when d0-d7 and d8-d15 are deliberately inverted
	void setupx_w(u16 data); // a = 0 (also mpeg data in pio mode)
	void mc1x_w  (u16 data); // a = 1
	void cmdx_w  (u16 data); // a = 2
	void mc23x_w (u16 data); // a = 3

	void dma8_w  (u8  data); // mpeg data write with dma
	void dma_w   (u16 data);
	void dmax_w  (u16 data);

	u8   stat08_r(); // a = 0
	u8   stat18_r(); // a = 1
	u8   stat28_r(); // a = 2
	u8   user8_r();  // a = 3

	u16  stat0_r();  // a = 0
	u16  stat1_r();  // a = 1
	u16  stat2_r();  // a = 2
	u16  user_r();   // a = 3

	u16  stat0x_r(); // a = 0
	u16  stat1x_r(); // a = 1
	u16  stat2x_r(); // a = 2
	u16  userx_r();  // a = 3

	void vblank_w(int state);
	bool video_valid() const { return m_display_width && m_display_height; }
	const bitmap_rgb32 &video_bitmap() const { return m_video_bitmap; }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_post_load() override;

private:
	// Host staging holds at least one maximum DMA burst.  This is not a model
	// of an additional physical FIFO.  Code-buffer limits are applied below it.
	static constexpr unsigned HOST_BUFFER_BYTES = 64;
	static constexpr unsigned VIDEO_BUFFER_BYTES = 0x40000;
	static constexpr unsigned SERIAL_BUFFER_BYTES = 0x1000;
	static constexpr unsigned MAX_VIDEO_WIDTH = 704;
	static constexpr unsigned MAX_VIDEO_HEIGHT = 576;
	static constexpr unsigned PICTURE_BYTES = MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT * 3 / 2;
	static constexpr u8 NO_PICTURE = 0xff;
	static constexpr unsigned SERIAL_BITRATES[2][16] =
	{
		{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 },
		{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }
	};
	static_assert(!(HOST_BUFFER_BYTES & (HOST_BUFFER_BYTES - 1)));
	static_assert(!(VIDEO_BUFFER_BYTES & (VIDEO_BUFFER_BYTES - 1)));
	static_assert(!(SERIAL_BUFFER_BYTES & (SERIAL_BUFFER_BYTES - 1)));

	enum {
		S_INIT   = 0x0,
		S_IDLE   = 0x1,
		S_NORMAL = 0x2,
		S_PAUSE  = 0x3,
		S_STEP   = 0x4,
		S_DFIRST = 0x6,
		S_DNEXT  = 0x7,
		S_END    = 0x8
	};

	devcb_write_line m_drq_w;
	devcb_write_line m_sp_frm_w[2];
	devcb_write_line m_sp_dat_w[2];
	devcb_write_line m_sp_clk_w[2];
	bool m_mb4 = false;

	u8 m_setup[0x80];
	u32 m_mc1_adr;
	u32 m_mc23_adr;
	u32 m_setup_adr;
	u16 m_cmd;
	u8 m_state, m_bus_control;
	bool m_cmd_phase;

	std::unique_ptr<mpeg_system> m_demux;
	std::unique_ptr<mpeg_video> m_decoder;
	std::unique_ptr<u8[]> m_video_data;
	std::unique_ptr<u8[]> m_serial_data[2];
	std::unique_ptr<u8[]> m_picture_data[3];
	std::unique_ptr<u8[]> m_display_data;
	bitmap_rgb32 m_video_bitmap;
	emu_timer *m_process_timer;
	emu_timer *m_bus_timer;
	emu_timer *m_serial_timer;

	u8 m_host_data[HOST_BUFFER_BYTES];
	u32 m_host_read, m_host_count;
	u32 m_video_read, m_video_count;
	u32 m_serial_read[2], m_serial_count[2], m_serial_rate[2], m_serial_fraction[2];
	u32 m_serial_clock_fraction[2];
	u16 m_serial_word[2];
	u8 m_serial_bits[2], m_serial_frame_clocks[2];
	bool m_serial_frm[2], m_serial_dat[2], m_serial_clk[2];
	u16 m_picture_width[3], m_picture_height[3];
	u8 m_picture_type[3];
	u16 m_display_width, m_display_height;
	u8 m_selected_video, m_selected_serial[2];
	u8 m_older_reference, m_newer_reference, m_target_picture;
	u8 m_reference_count;
	u8 m_pending_reference, m_queued_picture, m_current_type;
	u8 m_last_coded_picture;
	u16 m_last_coded_width, m_last_coded_height;
	bool m_last_coded_displayed;
	u8 m_burst_left, m_slow_factor, m_end_mode;
	bool m_drq, m_bus_wait, m_buffer_full;
	bool m_system_end, m_video_end, m_command_end, m_freeze;
	double m_picture_rate, m_picture_fraction;

	static double u6_10_to_f(u16 val);
	static double u5_19_to_f(u32 val);
	void setup_show() const;

	void go();
	void end_decoding(u8 mode);
	void clear_streams();
	bool input_ready() const;
	void update_drq();
	void start_bus_off();
	unsigned video_capacity() const;
	void input_byte(u8 data);
	std::size_t stream_payload(const mpeg_system::packet_info &packet, std::span<const u8> data);
	std::size_t video_payload(std::span<const u8> data);
	std::size_t serial_payload(unsigned port, std::span<const u8> data);
	bool decode_video();
	void finish_picture(int width, int height);
	void finish_stream();
	void update_video_bitmap();
	u32 serial_rate(unsigned port) const;
	bool serial_pending(unsigned port) const { return m_serial_count[port] || m_serial_bits[port]; }
	void restore_serial_lines();
	u8 status1();
	u8 status2() const;
	TIMER_CALLBACK_MEMBER(process_tick);
	TIMER_CALLBACK_MEMBER(bus_tick);
	TIMER_CALLBACK_MEMBER(serial_tick);
};

DECLARE_DEVICE_TYPE(ZR36110, zr36110_device)

#endif // MAME_VIDEO_ZR36110_H
