// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn

#ifndef MAME_VIDEO_STI3400_H
#define MAME_VIDEO_STI3400_H

#pragma once

class sti3400_device : public device_t
{
public:
	sti3400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);
	virtual ~sti3400_device();

	void set_dram_size(u32 bytes) { m_dram_size = bytes; }

	auto irq() { return m_irq_cb.bind(); }

	u16 read(offs_t offset, u16 mem_mask = ~0);
	void write(offs_t offset, u16 data, u16 mem_mask = ~0);
	void vblank_w(int state);

	bool video_valid() const;
	u16 video_width() const;
	u16 video_height() const;
	u32 video_pixel(unsigned x, unsigned y) const;

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_post_load() override ATTR_COLD;

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
	static constexpr unsigned REG_DFP = 0x18; // displayed picture pointer
	static constexpr unsigned REG_RFP = 0x1a; // reconstructed picture pointer
	static constexpr unsigned REG_FFP = 0x1c; // forward prediction picture pointer
	static constexpr unsigned REG_BFP = 0x1e; // backward prediction picture pointer
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
	static constexpr u16 INS_RPT = 0x0002; // repeat picture for a second VSYNC period
	static constexpr u16 INS_WAIT = 0x0004; // inhibit DSYNC, decoding and header search

	// Bit-buffer levels and thresholds are 14-bit counts of 256-byte units.
	static constexpr u16 BIT_BUFFER_LEVEL_MASK = 0x3fff;
	static constexpr unsigned BIT_BUFFER_LEVEL_UNIT_BYTES = 0x100;
	static constexpr unsigned BIT_BUFFER_LEVEL_BIAS_BYTES = 0x40;
	static constexpr u16 PICTURE_POINTER_MASK = 0x3fff;

	// The hardware header FIFO is 256 bits wide.
	static constexpr unsigned HEADER_FIFO_BYTES = 0x20;
	// Software buffers cover the maximum amount representable by BBL.
	static constexpr unsigned COMPRESSED_DATA_BUFFER_BYTES = 4U * 1024 * 1024;
	static constexpr unsigned START_CODE_EVENT_COUNT = 256;
	static_assert(std::has_single_bit(COMPRESSED_DATA_BUFFER_BYTES));
	static_assert(std::has_single_bit(START_CODE_EVENT_COUNT));

	void stream_byte_w(u8 data);
	void reset_decoder();
	void decoder_soft_reset();
	bool execute_task();
	TIMER_CALLBACK_MEMBER(decode_tick);
	void queue_start_code(u64 position, u8 code);
	void activate_event();
	void finish_event();
	u16 bit_buffer_level() const;
	u16 decoder_status() const;
	void update_status();
	void update_irq();
	u8 stream_byte(u64 position) const;

	struct decoder_state;

	devcb_write_line m_irq_cb;
	std::unique_ptr<decoder_state> m_decoder;
	u32 m_dram_size;

	u16 m_registers[0x40];
	u8 m_fifo[COMPRESSED_DATA_BUFFER_BYTES];
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
