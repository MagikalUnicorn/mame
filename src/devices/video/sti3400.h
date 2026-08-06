// license:BSD-3-Clause
// copyright-holders:David Haywood, James Wallace, blueonesarefaster

#ifndef MAME_VIDEO_STI3400_H
#define MAME_VIDEO_STI3400_H

#pragma once

struct plm_buffer_t;
struct plm_video_t;


class sti3400_device : public device_t
{
public:
	sti3400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto irq() { return m_irq_cb.bind(); }

	u16 read(offs_t offset, u16 mem_mask = ~0);
	void write(offs_t offset, u16 data, u16 mem_mask = ~0);

	bool video_valid() const { return m_video_valid; }
	u16 video_width() const { return m_video_width; }
	u16 video_height() const { return m_video_height; }
	u32 video_pixel(unsigned x, unsigned y) const { return m_video_frame[(y * MAX_VIDEO_WIDTH) + x]; }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

private:
	static constexpr unsigned FIFO_SIZE = 0x10000;
	static constexpr unsigned EVENT_COUNT = 0x100;
	static constexpr unsigned HEADER_LOOKAHEAD = 0x100;
	static constexpr unsigned DECODE_STAGING_SIZE = 0x800;
	static constexpr unsigned MAX_VIDEO_WIDTH = 720;
	static constexpr unsigned MAX_VIDEO_HEIGHT = 576;

	void stream_byte_w(u8 data);
	void decoder_create();
	void decoder_destroy();
	void decoder_flush();
	TIMER_CALLBACK_MEMBER(decode_tick);
	void queue_start_code(u64 position);
	void activate_event();
	void finish_event();
	void update_irq();
	u8 stream_byte_r();

	devcb_write_line m_irq_cb;

	u16 m_registers[0x40];
	u8 m_fifo[FIFO_SIZE];
	u8 m_decode_staging[DECODE_STAGING_SIZE];
	u64 m_event_position[EVENT_COUNT];
	std::unique_ptr<u32[]> m_video_frame;
	plm_buffer_t *m_decode_buffer;
	plm_video_t *m_video_decoder;
	emu_timer *m_decode_timer;

	u64 m_fifo_write;
	u64 m_fifo_read;
	u32 m_start_code_shift;
	u16 m_decode_staging_count;
	u16 m_video_width;
	u16 m_video_height;
	u16 m_event_head;
	u16 m_event_tail;
	u16 m_event_count;
	bool m_event_active;
	bool m_picture_complete;
	bool m_picture_completion_armed;
	bool m_irq_suppressed;
	bool m_irq_state;
	bool m_video_valid;
};

DECLARE_DEVICE_TYPE(STI3400, sti3400_device)

#endif // MAME_VIDEO_STI3400_H
