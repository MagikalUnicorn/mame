// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn

#ifndef MAME_VIDEO_STI3400_H
#define MAME_VIDEO_STI3400_H

#pragma once

class sti3400_device : public device_t
{
public:
	sti3400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto irq() { return m_irq_cb.bind(); }

	u16 read(offs_t offset, u16 mem_mask = ~0);
	void write(offs_t offset, u16 data, u16 mem_mask = ~0);
	void vblank_w(int state);

	// Picture reconstruction is not yet implemented.
	bool video_valid() const { return false; }
	u16 video_width() const { return 0; }
	u16 video_height() const { return 0; }
	u32 video_pixel(unsigned, unsigned) const { return 0; }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	// STi3400 host-interface register addresses from the data sheet.
	static constexpr unsigned REG_CDF = 0x00; // compressed data FIFO
	static constexpr unsigned REG_HDF = 0x02; // header data FIFO
	static constexpr unsigned REG_HDP = 0x04; // header data position
	static constexpr unsigned REG_STA = 0x08; // decoder status
	static constexpr unsigned REG_CTL = 0x0a; // decoder control
	static constexpr unsigned REG_ITM = 0x0e; // interrupt mask
	static constexpr unsigned REG_ITS = 0x10; // interrupt status
	static constexpr unsigned REG_HDS = 0x12; // header data search command
	static constexpr unsigned REG_INS = 0x14; // next decoding instruction
	static constexpr unsigned REG_BBB = 0x16; // bit-buffer bottom threshold
	static constexpr unsigned REG_BBL = 0x22; // current bit-buffer level
	static constexpr unsigned REG_BBT = 0x32; // bit-buffer top threshold

	static constexpr u16 STA_SCH = 0x0001; // start-code hit
	static constexpr u16 STA_HFE = 0x0004; // header FIFO empty
	static constexpr u16 STA_BBF = 0x0008; // bit buffer nearly full
	static constexpr u16 STA_BBE = 0x0010; // bit buffer nearly empty
	static constexpr u16 STA_PID = 0x0200; // decoding pipeline idle
	static constexpr u16 STA_HFF = 0x1000; // header FIFO full
	static constexpr u16 STA_RESET = STA_PID | STA_BBE | STA_HFE; // hard-reset status

	static constexpr u16 CTL_EDC = 0x0001; // enable decoding
	static constexpr u16 CTL_SRS = 0x0002; // soft reset
	static constexpr u16 CTL_DVS = 0x0080; // disable VSYNC-triggered task start
	static constexpr u16 INS_WAIT = 0x0004; // inhibit DSYNC, decoding and header search

	// The currently implemented portion of the host interface uses six address bits.
	static constexpr unsigned REGISTER_ADDRESS_BITS = 6;
	static constexpr unsigned REGISTER_COUNT = 1U << REGISTER_ADDRESS_BITS;
	static constexpr unsigned REGISTER_ADDRESS_MASK = REGISTER_COUNT - 1;

	// Bit-buffer levels and thresholds are 14-bit counts of 256-byte units.
	static constexpr unsigned BIT_BUFFER_LEVEL_BITS = 14;
	static constexpr unsigned BIT_BUFFER_LEVEL_UNIT_BYTES = 256;
	static constexpr unsigned BIT_BUFFER_LEVEL_BIAS_BYTES = 64;
	static constexpr u16 BIT_BUFFER_LEVEL_MASK = (1U << BIT_BUFFER_LEVEL_BITS) - 1;
	// This is stream history for header searches, not the STi3400's 512-bit CD FIFO.
	static constexpr unsigned STREAM_HISTORY_SIZE = (BIT_BUFFER_LEVEL_MASK + 1U) * BIT_BUFFER_LEVEL_UNIT_BYTES;
	// The header FIFO is 256 bits wide.
	static constexpr unsigned HEADER_FIFO_BYTES = 256 / 8;

	static constexpr u32 MPEG_START_CODE_SHIFT_RESET = ~u32(0);
	static constexpr u32 MPEG_START_CODE_MASK = 0xffffff00U;
	static constexpr u32 MPEG_START_CODE_PREFIX = 0x00000100U;
	static constexpr u8 MPEG_PICTURE_START_CODE = 0x00;
	static constexpr u8 MPEG_NON_SLICE_CODE_MIN = 0xb0;
	static constexpr u8 MPEG_SEQUENCE_END_CODE = 0xb7;

	// Event and lookahead capacity allows the host to parse headers while input continues to arrive.
	static constexpr unsigned START_CODE_EVENT_COUNT = 256;
	static constexpr unsigned HEADER_LOOKAHEAD_BYTES = 256;
	// Use the PAL frame rate for the preliminary compressed-data consumption model.
	static constexpr unsigned DEFAULT_FRAME_RATE = 25;

	static_assert(!(STREAM_HISTORY_SIZE & (STREAM_HISTORY_SIZE - 1)), "stream history size must be a power of two");
	static_assert(!(START_CODE_EVENT_COUNT & (START_CODE_EVENT_COUNT - 1)), "event count must be a power of two");

	void stream_byte_w(u8 data);
	void decoder_soft_reset();
	TIMER_CALLBACK_MEMBER(decode_tick);
	void queue_start_code(u64 position, u8 code);
	void activate_event();
	void finish_event();
	u16 bit_buffer_level() const;
	u16 decoder_status() const;
	void update_status();
	void update_irq();
	u8 stream_byte_r();

	devcb_write_line m_irq_cb;

	u16 m_registers[REGISTER_COUNT];
	u8 m_fifo[STREAM_HISTORY_SIZE];
	u64 m_event_position[START_CODE_EVENT_COUNT];
	u8 m_event_code[START_CODE_EVENT_COUNT];
	emu_timer *m_decode_timer;

	u64 m_fifo_write;
	u64 m_fifo_read;
	u64 m_decode_position;
	u32 m_start_code_shift;
	u16 m_event_head;
	u16 m_event_tail;
	u16 m_event_count;
	u16 m_interrupt_status;
	u16 m_status;
	bool m_event_active;
	bool m_irq_state;
};

DECLARE_DEVICE_TYPE(STI3400, sti3400_device)

#endif // MAME_VIDEO_STI3400_H
