// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn

/*
 * SGS-Thomson STi3400 MPEG-1 video decoder (preliminary)
 *
 * The host interface and start-code detector are implemented sufficiently for
 * Cobra 3 software.  MPEG-1 picture reconstruction is not yet implemented.
 */

#include "emu.h"
#include "sti3400.h"

#define LOG_START_CODES (1U << 1)

#define VERBOSE (0)
#include "logmacro.h"


DEFINE_DEVICE_TYPE(STI3400, sti3400_device, "sti3400", "SGS-Thomson STi3400 MPEG-1 Video Decoder")

sti3400_device::sti3400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, STI3400, tag, owner, clock),
	m_irq_cb(*this),
	m_decode_timer(nullptr)
{
}

void sti3400_device::device_start()
{
	m_decode_timer = timer_alloc(FUNC(sti3400_device::decode_tick), this);

	save_item(NAME(m_registers));
	save_item(NAME(m_fifo));
	save_item(NAME(m_event_position));
	save_item(NAME(m_event_code));
	save_item(NAME(m_fifo_write));
	save_item(NAME(m_fifo_read));
	save_item(NAME(m_decode_position));
	save_item(NAME(m_start_code_shift));
	save_item(NAME(m_event_head));
	save_item(NAME(m_event_tail));
	save_item(NAME(m_event_count));
	save_item(NAME(m_interrupt_status));
	save_item(NAME(m_status));
	save_item(NAME(m_event_active));
	save_item(NAME(m_irq_state));
}

void sti3400_device::device_reset()
{
	std::fill(std::begin(m_registers), std::end(m_registers), 0);
	std::fill(std::begin(m_fifo), std::end(m_fifo), 0);
	std::fill(std::begin(m_event_position), std::end(m_event_position), 0);
	std::fill(std::begin(m_event_code), std::end(m_event_code), 0);

	m_fifo_write = 0;
	m_fifo_read = 0;
	m_decode_position = 0;
	m_start_code_shift = MPEG_START_CODE_SHIFT_RESET;
	m_event_head = 0;
	m_event_tail = 0;
	m_event_count = 0;
	m_interrupt_status = 0;
	m_status = STA_RESET;
	m_event_active = false;
	m_irq_state = false;
	m_irq_cb(CLEAR_LINE);
	m_decode_timer->adjust(attotime::from_hz(DEFAULT_FRAME_RATE));
}

void sti3400_device::decoder_soft_reset()
{
	std::fill(std::begin(m_fifo), std::end(m_fifo), 0);
	std::fill(std::begin(m_event_position), std::end(m_event_position), 0);
	std::fill(std::begin(m_event_code), std::end(m_event_code), 0);

	m_fifo_write = 0;
	m_fifo_read = 0;
	m_decode_position = 0;
	m_start_code_shift = MPEG_START_CODE_SHIFT_RESET;
	m_event_head = 0;
	m_event_tail = 0;
	m_event_count = 0;
	m_event_active = false;
	update_status();
}

TIMER_CALLBACK_MEMBER(sti3400_device::decode_tick)
{
	// Until picture reconstruction is implemented, consume all compressed data at each picture period.
	if (m_registers[REG_CTL] & CTL_EDC)
		m_decode_position = m_fifo_write;

	update_status();
	activate_event();
	m_decode_timer->adjust(attotime::from_hz(DEFAULT_FRAME_RATE));
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
}

void sti3400_device::stream_byte_w(u8 data)
{
	m_fifo[m_fifo_write & (STREAM_HISTORY_SIZE - 1)] = data;
	m_fifo_write++;
	m_start_code_shift = (m_start_code_shift << 8) | data;

	if ((m_start_code_shift & MPEG_START_CODE_MASK) == MPEG_START_CODE_PREFIX)
	{
		const u8 code = m_start_code_shift;

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
		if (code == MPEG_SEQUENCE_END_CODE)
		{
			if (m_decode_position <= position)
				return;
		}
		else if ((m_fifo_write - position) < HEADER_LOOKAHEAD_BYTES)
		{
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
	const u64 bytes_available = (m_fifo_write > m_decode_position) ? (m_fifo_write - m_decode_position) : 0;

	// BBL excludes the first 64 bytes and reports the remainder in 256-byte units.
	return std::min<u64>((bytes_available > BIT_BUFFER_LEVEL_BIAS_BYTES)
		? ((bytes_available - BIT_BUFFER_LEVEL_BIAS_BYTES) / BIT_BUFFER_LEVEL_UNIT_BYTES)
		: 0, BIT_BUFFER_LEVEL_MASK);
}

u16 sti3400_device::decoder_status() const
{
	const u16 level = bit_buffer_level();
	u16 status = 0;

	// The stub does not model picture-task execution state.
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
