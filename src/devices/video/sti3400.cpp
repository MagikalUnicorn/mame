// license:BSD-3-Clause
// copyright-holders:David Haywood, James Wallace, blueonesarefaster

/*
 * SGS-Thomson STi3400 MPEG-1 video decoder (preliminary)
 *
 * The host interface and start-code detector are implemented sufficiently for
 * Cobra 3 software.  MPEG-1 picture reconstruction uses the PL_MPEG decoder.
 */

#include "emu.h"
#include "sti3400.h"

#include "util/multibyte.h"

#define PLM_NO_STDIO
#include "pl_mpeg/pl_mpeg.h"

#define LOG_START_CODES (1U << 1)

#define VERBOSE (0)
#include "logmacro.h"


DEFINE_DEVICE_TYPE(STI3400, sti3400_device, "sti3400", "SGS-Thomson STi3400 MPEG-1 Video Decoder")

sti3400_device::sti3400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, STI3400, tag, owner, clock),
	m_irq_cb(*this),
	m_decode_buffer(nullptr),
	m_video_decoder(nullptr),
	m_decode_timer(nullptr)
{
}

void sti3400_device::device_start()
{
	m_video_frame = make_unique_clear<u32[]>(MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT);
	m_decoded_frame = make_unique_clear<u32[]>(MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT);
	decoder_create();
	m_decode_timer = timer_alloc(FUNC(sti3400_device::decode_tick), this);

	save_item(NAME(m_registers));
	save_item(NAME(m_fifo));
	save_item(NAME(m_decode_staging));
	save_item(NAME(m_event_position));
	save_item(NAME(m_event_code));
	save_pointer(NAME(m_video_frame), MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT);
	save_pointer(NAME(m_decoded_frame), MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT);
	save_item(NAME(m_fifo_write));
	save_item(NAME(m_fifo_read));
	save_item(NAME(m_decode_stream_base));
	save_item(NAME(m_decode_stream_written));
	save_item(NAME(m_start_code_shift));
	save_item(NAME(m_decode_staging_count));
	save_item(NAME(m_video_width));
	save_item(NAME(m_video_height));
	save_item(NAME(m_decoded_width));
	save_item(NAME(m_decoded_height));
	save_item(NAME(m_event_head));
	save_item(NAME(m_event_tail));
	save_item(NAME(m_event_count));
	save_item(NAME(m_interrupt_status));
	save_item(NAME(m_status));
	save_item(NAME(m_event_active));
	save_item(NAME(m_irq_state));
	save_item(NAME(m_video_valid));
	save_item(NAME(m_decoded_frame_pending));
	save_item(NAME(m_decode_has_sequence_header));
	save_item(NAME(m_decode_sequence_ended));
}

void sti3400_device::device_reset()
{
	decoder_destroy();
	decoder_create();

	std::fill(std::begin(m_registers), std::end(m_registers), 0);
	std::fill(std::begin(m_fifo), std::end(m_fifo), 0);
	std::fill(std::begin(m_decode_staging), std::end(m_decode_staging), 0);
	std::fill(std::begin(m_event_position), std::end(m_event_position), 0);
	std::fill(std::begin(m_event_code), std::end(m_event_code), 0);
	std::fill_n(m_video_frame.get(), MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT, 0);
	std::fill_n(m_decoded_frame.get(), MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT, 0);

	m_fifo_write = 0;
	m_fifo_read = 0;
	m_decode_stream_base = 0;
	m_decode_stream_written = 0;
	m_start_code_shift = MPEG_START_CODE_SHIFT_RESET;
	m_decode_staging_count = 0;
	m_video_width = 0;
	m_video_height = 0;
	m_decoded_width = 0;
	m_decoded_height = 0;
	m_event_head = 0;
	m_event_tail = 0;
	m_event_count = 0;
	m_interrupt_status = 0;
	m_status = STA_RESET;
	m_event_active = false;
	m_irq_state = false;
	m_video_valid = false;
	m_decoded_frame_pending = false;
	m_decode_has_sequence_header = false;
	m_decode_sequence_ended = false;
	m_irq_cb(CLEAR_LINE);
	m_decode_timer->adjust(attotime::from_hz(DEFAULT_FRAME_RATE));
}

void sti3400_device::device_stop()
{
	decoder_destroy();
}

void sti3400_device::decoder_create()
{
	m_decode_buffer = plm_buffer_create_with_capacity(DECODE_BUFFER_INITIAL_BYTES);
	m_video_decoder = plm_video_create_with_buffer(m_decode_buffer, true);
}

void sti3400_device::decoder_destroy()
{
	if (m_video_decoder)
		plm_video_destroy(m_video_decoder);

	m_video_decoder = nullptr;
	m_decode_buffer = nullptr;
}

void sti3400_device::decoder_flush()
{
	if (!m_decode_staging_count)
		return;

	plm_buffer_write(m_decode_buffer, m_decode_staging, m_decode_staging_count);
	m_decode_stream_written += m_decode_staging_count;
	m_decode_staging_count = 0;
}

void sti3400_device::decoder_soft_reset()
{
	decoder_destroy();
	decoder_create();

	std::fill(std::begin(m_fifo), std::end(m_fifo), 0);
	std::fill(std::begin(m_decode_staging), std::end(m_decode_staging), 0);
	std::fill(std::begin(m_event_position), std::end(m_event_position), 0);
	std::fill(std::begin(m_event_code), std::end(m_event_code), 0);

	m_fifo_write = 0;
	m_fifo_read = 0;
	m_decode_stream_base = 0;
	m_decode_stream_written = 0;
	m_start_code_shift = MPEG_START_CODE_SHIFT_RESET;
	m_decode_staging_count = 0;
	m_event_head = 0;
	m_event_tail = 0;
	m_event_count = 0;
	m_event_active = false;
	m_decoded_frame_pending = false;
	m_decode_has_sequence_header = false;
	m_decode_sequence_ended = false;
	update_status();
}

TIMER_CALLBACK_MEMBER(sti3400_device::decode_tick)
{
	decoder_flush();

	if (m_registers[REG_CTL] & CTL_EDC)
	{
		// Keep a completed dynamic stream marked as ended while PL_MPEG drains and compacts its buffer.
		if (m_decode_sequence_ended)
			plm_buffer_signal_end(m_decode_buffer);

		if (plm_frame_t *const frame = plm_video_decode(m_video_decoder))
		{
			m_decoded_width = std::min<unsigned>(frame->width, MAX_VIDEO_WIDTH);
			m_decoded_height = std::min<unsigned>(frame->height, MAX_VIDEO_HEIGHT);
			plm_frame_to_bgra(frame, reinterpret_cast<u8 *>(m_decoded_frame.get()), MAX_VIDEO_WIDTH * sizeof(u32));
			m_decoded_frame_pending = true;
		}
	}

	update_status();
	activate_event();

	const double frame_rate = plm_video_get_framerate(m_video_decoder);
	m_decode_timer->adjust(frame_rate > 0.0
		? attotime::from_double(1.0 / frame_rate)
		: attotime::from_hz(DEFAULT_FRAME_RATE));
}

void sti3400_device::vblank_w(int state)
{
	if (!state)
		return;

	// A VSYNC-generated DSYNC starts the next task and restarts automatic start-code detection.
	if ((m_registers[REG_CTL] & CTL_EDC) && !(m_registers[REG_CTL] & CTL_DVS) &&
		!(m_registers[REG_INS] & INS_WAIT) && m_event_active)
	{
		finish_event();
	}

	if (m_decoded_frame_pending)
	{
		std::copy_n(m_decoded_frame.get(), MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT, m_video_frame.get());
		m_video_width = m_decoded_width;
		m_video_height = m_decoded_height;
		m_video_valid = true;
		m_decoded_frame_pending = false;
	}
}

void sti3400_device::stream_byte_w(u8 data)
{
	m_decode_staging[m_decode_staging_count++] = data;
	if (m_decode_staging_count == DECODE_STAGING_BYTES)
		decoder_flush();

	m_fifo[m_fifo_write & (STREAM_HISTORY_SIZE - 1)] = data;
	m_fifo_write++;
	m_start_code_shift = (m_start_code_shift << 8) | data;

	if ((m_start_code_shift & MPEG_START_CODE_MASK) == MPEG_START_CODE_PREFIX)
	{
		const u8 code = m_start_code_shift;
		if (code == MPEG_SEQUENCE_END_CODE)
			m_decode_sequence_ended = true;
		if (code == MPEG_SEQUENCE_HEADER_CODE)
		{
			if (m_decode_has_sequence_header && m_decode_sequence_ended)
			{
				// PL_MPEG does not apply a new sequence header to an existing video decoder.
				// Repeated headers within one sequence must retain their reference pictures.
				decoder_destroy();
				decoder_create();
				m_decode_stream_base = m_fifo_write - MPEG_START_CODE_BYTES;
				m_decode_stream_written = 0;
				// Re-seed PL_MPEG with the 00 00 01 B3 sequence-header start code.
				put_u32be(m_decode_staging, MPEG_START_CODE_PREFIX | MPEG_SEQUENCE_HEADER_CODE);
				m_decode_staging_count = MPEG_START_CODE_BYTES;
			}
			else
			{
				m_decode_has_sequence_header = true;
			}
			m_decode_sequence_ended = false;
		}

		// In MPEG mode the detector recognises every start code except slice codes 01-AF.
		if ((code == MPEG_PICTURE_START_CODE) || (code >= MPEG_NON_SLICE_CODE_MIN))
		{
			LOGMASKED(LOG_START_CODES, "start code %02x at %08x\n", code, u32(m_fifo_write - 1));
			queue_start_code(m_fifo_write - 1, code);
		}
	}

	update_status();
	activate_event();
}

void sti3400_device::queue_start_code(u64 position, u8 code)
{
	if (m_event_count == START_CODE_EVENT_COUNT)
	{
		logerror("%s: start-code event queue overflow\n", machine().describe_context());
		return;
	}

	m_event_position[m_event_tail] = position;
	m_event_code[m_event_tail] = code;
	m_event_tail = (m_event_tail + 1) & (START_CODE_EVENT_COUNT - 1);
	m_event_count++;
	activate_event();
}

void sti3400_device::activate_event()
{
	if (!m_event_active && m_event_count)
	{
		const u64 position = m_event_position[m_event_head];
		const u8 code = m_event_code[m_event_head];
		if ((code != MPEG_SEQUENCE_END_CODE) && ((m_fifo_write - position) < HEADER_LOOKAHEAD_BYTES))
			return;
		if (code == MPEG_SEQUENCE_END_CODE)
		{
			// Sequence end is detected as the compressed stream is consumed, not as the FIFO is filled.
			const u64 decode_position = m_decode_stream_base + m_decode_stream_written
				- plm_buffer_get_remaining(m_decode_buffer);
			if (decode_position <= position)
				return;
		}

		m_event_active = true;
		m_fifo_read = position;
		update_status();
	}
}

void sti3400_device::finish_event()
{
	if (m_event_active)
	{
		m_event_head = (m_event_head + 1) & (START_CODE_EVENT_COUNT - 1);
		m_event_count--;
		m_event_active = false;
	}

	update_status();
	activate_event();
}

u16 sti3400_device::bit_buffer_level() const
{
	const u64 decoder_remaining = m_decode_buffer ? plm_buffer_get_remaining(m_decode_buffer) : 0;
	const u64 decoder_consumed = (m_decode_stream_written > decoder_remaining)
		? (m_decode_stream_written - decoder_remaining)
		: 0;
	const u64 decoder_position = m_decode_stream_base + decoder_consumed;
	const u64 bytes_available = (m_fifo_write > decoder_position) ? (m_fifo_write - decoder_position) : 0;

	// BBL excludes the first 64 bytes and reports the remainder in 256-byte units.
	return std::min<u64>((bytes_available > BIT_BUFFER_LEVEL_BIAS_BYTES)
		? ((bytes_available - BIT_BUFFER_LEVEL_BIAS_BYTES) / BIT_BUFFER_LEVEL_UNIT_BYTES)
		: 0, BIT_BUFFER_LEVEL_MASK);
}

u16 sti3400_device::decoder_status() const
{
	const u16 level = bit_buffer_level();
	u16 status = 0;

	// The high-level decoder does not expose picture-task execution state.
	if (!(m_registers[REG_CTL] & CTL_EDC))
		status |= STA_PID;
	if (!level || (level < (m_registers[REG_BBB] & BIT_BUFFER_LEVEL_MASK)))
		status |= STA_BBE;
	if (level > (m_registers[REG_BBT] & BIT_BUFFER_LEVEL_MASK))
		status |= STA_BBF;
	if (!m_event_active || (m_fifo_read >= m_fifo_write))
		status |= STA_HFE;
	if (m_event_active && ((m_fifo_write - m_fifo_read) >= HEADER_FIFO_BYTES))
		status |= STA_HFF;
	if (m_event_active && (m_fifo_read == m_event_position[m_event_head]))
		status |= STA_SCH;

	return status;
}

void sti3400_device::update_status()
{
	const u16 status = decoder_status();
	m_interrupt_status |= status & ~m_status;
	m_status = status;
	update_irq();
}

void sti3400_device::update_irq()
{
	const u16 mask = m_registers[REG_ITM];
	const bool state = bool(m_interrupt_status & mask);
	if (state != m_irq_state)
	{
		m_irq_state = state;
		m_irq_cb(state ? ASSERT_LINE : CLEAR_LINE);
	}
}

u8 sti3400_device::stream_byte_r()
{
	if (m_fifo_read >= m_fifo_write)
		return 0;

	return m_fifo[m_fifo_read++ & (STREAM_HISTORY_SIZE - 1)];
}

u16 sti3400_device::read(offs_t offset, u16 mem_mask)
{
	const unsigned address = offset & REGISTER_ADDRESS_MASK;
	if (address != REG_CDF)
		decoder_flush();

	switch (address)
	{
	case REG_HDF:
	{
		const u16 result = (stream_byte_r() << 8) | stream_byte_r();
		update_status();
		return result;
	}

	case REG_HDP:
		return 0;

	case REG_STA:
		return m_status;

	case REG_BBL:
		return bit_buffer_level();

	case REG_ITS:
	{
		const u16 result = m_interrupt_status;
		if (ACCESSING_BITS_8_15)
			m_interrupt_status &= 0x00ff;
		if (ACCESSING_BITS_0_7)
			m_interrupt_status &= 0xff00;
		update_irq();
		return result;
	}

	default:
		return m_registers[address];
	}
}

void sti3400_device::write(offs_t offset, u16 data, u16 mem_mask)
{
	const unsigned address = offset & REGISTER_ADDRESS_MASK;

	if (address == REG_CDF)
	{
		if (ACCESSING_BITS_8_15)
			stream_byte_w(data >> 8);
		if (ACCESSING_BITS_0_7)
			stream_byte_w(data);
		return;
	}

	decoder_flush();

	const u16 old_data = m_registers[address];
	COMBINE_DATA(&m_registers[address]);

	switch (address)
	{
	case REG_CTL:
		if ((old_data & CTL_SRS) && !(m_registers[address] & CTL_SRS))
			decoder_soft_reset();
		else
			update_status();
		break;

	case REG_ITM:
		update_irq();
		break;

	case REG_HDS:
		finish_event();
		break;

	case REG_BBB:
	case REG_BBT:
		update_status();
		break;
	}
}
