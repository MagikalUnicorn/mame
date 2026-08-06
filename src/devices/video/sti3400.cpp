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
	decoder_create();
	m_decode_timer = timer_alloc(FUNC(sti3400_device::decode_tick), this);

	save_item(NAME(m_registers));
	save_item(NAME(m_fifo));
	save_item(NAME(m_decode_staging));
	save_item(NAME(m_event_position));
	save_pointer(NAME(m_video_frame), MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT);
	save_item(NAME(m_fifo_write));
	save_item(NAME(m_fifo_read));
	save_item(NAME(m_start_code_shift));
	save_item(NAME(m_decode_staging_count));
	save_item(NAME(m_video_width));
	save_item(NAME(m_video_height));
	save_item(NAME(m_event_head));
	save_item(NAME(m_event_tail));
	save_item(NAME(m_event_count));
	save_item(NAME(m_event_active));
	save_item(NAME(m_picture_complete));
	save_item(NAME(m_picture_completion_armed));
	save_item(NAME(m_irq_suppressed));
	save_item(NAME(m_irq_state));
	save_item(NAME(m_video_valid));
}

void sti3400_device::device_reset()
{
	decoder_destroy();
	decoder_create();

	std::fill(std::begin(m_registers), std::end(m_registers), 0);
	std::fill(std::begin(m_fifo), std::end(m_fifo), 0);
	std::fill(std::begin(m_decode_staging), std::end(m_decode_staging), 0);
	std::fill(std::begin(m_event_position), std::end(m_event_position), 0);
	std::fill_n(m_video_frame.get(), MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT, 0);

	m_fifo_write = 0;
	m_fifo_read = 0;
	m_start_code_shift = 0xffffffff;
	m_decode_staging_count = 0;
	m_video_width = 0;
	m_video_height = 0;
	m_event_head = 0;
	m_event_tail = 0;
	m_event_count = 0;
	m_event_active = false;
	m_picture_complete = false;
	m_picture_completion_armed = true;
	m_irq_suppressed = false;
	m_irq_state = false;
	m_video_valid = false;
	m_irq_cb(CLEAR_LINE);
	m_decode_timer->adjust(attotime::from_hz(25));
}

void sti3400_device::device_stop()
{
	decoder_destroy();
}

void sti3400_device::decoder_create()
{
	m_decode_buffer = plm_buffer_create_with_capacity(0x10000);
	m_video_decoder = plm_video_create_with_buffer(m_decode_buffer, 1);
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
	m_decode_staging_count = 0;
}

TIMER_CALLBACK_MEMBER(sti3400_device::decode_tick)
{
	decoder_flush();

	if (plm_frame_t *const frame = plm_video_decode(m_video_decoder))
	{
		m_video_width = std::min<unsigned>(frame->width, MAX_VIDEO_WIDTH);
		m_video_height = std::min<unsigned>(frame->height, MAX_VIDEO_HEIGHT);
		plm_frame_to_bgra(frame, reinterpret_cast<u8 *>(m_video_frame.get()), MAX_VIDEO_WIDTH * sizeof(u32));
		m_video_valid = true;
	}

	const double frame_rate = plm_video_get_framerate(m_video_decoder);
	m_decode_timer->adjust(frame_rate > 0.0
		? attotime::from_double(1.0 / frame_rate)
		: attotime::from_hz(25));
}

void sti3400_device::stream_byte_w(u8 data)
{
	m_decode_staging[m_decode_staging_count++] = data;
	if (m_decode_staging_count == DECODE_STAGING_SIZE)
		decoder_flush();

	m_fifo[m_fifo_write & (FIFO_SIZE - 1)] = data;
	m_fifo_write++;
	m_start_code_shift = (m_start_code_shift << 8) | data;

	if ((m_start_code_shift & 0xffffff00) == 0x00000100)
	{
		const u8 code = m_start_code_shift;

		// The start-code interrupt is used for MPEG headers, not slice data.
		if ((code == 0x00) || (code == 0xb2) || (code == 0xb3) ||
			(code == 0xb5) || (code == 0xb7) || (code == 0xb8))
		{
			LOGMASKED(LOG_START_CODES, "start code %02x at %08x\n", code, u32(m_fifo_write - 1));
			queue_start_code(m_fifo_write - 1);
		}
	}

	activate_event();
}

void sti3400_device::queue_start_code(u64 position)
{
	if (m_event_count == EVENT_COUNT)
	{
		logerror("%s: start-code event queue overflow\n", machine().describe_context());
		return;
	}

	m_event_position[m_event_tail] = position;
	m_event_tail = (m_event_tail + 1) & (EVENT_COUNT - 1);
	m_event_count++;
	activate_event();
}

void sti3400_device::activate_event()
{
	if (!m_event_active && m_event_count)
	{
		const u64 position = m_event_position[m_event_head];
		const u8 code = m_fifo[position & (FIFO_SIZE - 1)];
		if ((code != 0xb7) && ((m_fifo_write - position) < HEADER_LOOKAHEAD))
			return;

		m_event_active = true;
		m_irq_suppressed = false;
		m_fifo_read = position;
		update_irq();
	}
}

void sti3400_device::finish_event()
{
	if (m_event_active)
	{
		if (m_picture_completion_armed && (m_fifo[m_event_position[m_event_head] & (FIFO_SIZE - 1)] == 0x00))
		{
			m_picture_complete = true;
			m_picture_completion_armed = false;
		}

		m_event_head = (m_event_head + 1) & (EVENT_COUNT - 1);
		m_event_count--;
		m_event_active = false;
		m_irq_suppressed = false;
	}

	update_irq();
	activate_event();
}

void sti3400_device::update_irq()
{
	const u16 mask = m_registers[0x1c / 2];
	const bool state =
		(m_event_active && !m_irq_suppressed && BIT(mask, 0)) ||
		(m_picture_complete && BIT(mask, 3));
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

	return m_fifo[m_fifo_read++ & (FIFO_SIZE - 1)];
}

u16 sti3400_device::read(offs_t offset, u16 mem_mask)
{
	const unsigned address = (offset << 1) & 0x7e;
	if (address != 0x00)
		decoder_flush();

	switch (address)
	{
	case 0x04: // header data FIFO
		return (stream_byte_r() << 8) | stream_byte_r();

	case 0x08: // header FIFO bit alignment
	case 0x10: // decoder status
		return 0;

	case 0x44: // compressed-data FIFO level
		// A non-empty level causes the host to enable start-code interrupts
		// while it streams data, rather than preloading the complete file.
		return m_fifo_write ? 0x0100 : 0x0000;

	case 0x20: // interrupt status
		return (m_event_active ? 0x0001 : 0x0000) | (m_picture_complete ? 0x0008 : 0x0000);

	default:
		return m_registers[address >> 1];
	}
}

void sti3400_device::write(offs_t offset, u16 data, u16 mem_mask)
{
	const unsigned address = (offset << 1) & 0x7e;

	if (address == 0x00)
	{
		if (ACCESSING_BITS_8_15)
			stream_byte_w(data >> 8);
		if (ACCESSING_BITS_0_7)
			stream_byte_w(data);
		return;
	}

	decoder_flush();

	COMBINE_DATA(&m_registers[address >> 1]);

	switch (address)
	{
	case 0x1c: // interrupt mask
		if (!m_registers[address >> 1])
			m_picture_completion_armed = true;
		update_irq();
		break;

	case 0x24: // release start-code detector
		if (m_registers[address >> 1] == 0)
			finish_event();
		break;

	case 0x28: // decoder/interrupt control
		if (m_event_active && (m_registers[address >> 1] == 0x0004))
		{
			m_irq_suppressed = true;
			update_irq();
		}
		else
		{
			const bool clear_picture_complete = m_picture_complete;

			if (m_event_active && m_irq_suppressed)
				finish_event();
			if (clear_picture_complete)
			{
				m_picture_complete = false;
				update_irq();
			}
		}
		break;
	}
}
