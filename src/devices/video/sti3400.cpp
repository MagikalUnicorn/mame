// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn

/*
 * SGS-Thomson STi3400 MPEG-1 video decoder (preliminary)
 *
 * The host interface, start-code detector and MPEG-1 picture reconstruction
 * are implemented sufficiently for Cobra 3 software.
 */

#include "emu.h"
#include "sti3400.h"

#include "mpeg_video.h"

#define LOG_START_CODES (1U << 1)
#define LOG_INVALID_DATA (1U << 2)

#define VERBOSE (0)
#include "logmacro.h"

namespace {

constexpr u32 MPEG_START_CODE_SHIFT_RESET = ~u32(0);
constexpr u32 MPEG_START_CODE_MASK = 0xffffff00U;
constexpr u32 MPEG_START_CODE_PREFIX = 0x00000100U;
constexpr u8 MPEG_PICTURE_START_CODE = 0x00;
constexpr u8 MPEG_NON_SLICE_CODE_MIN = 0xb0;
constexpr u8 MPEG_SEQUENCE_END_CODE = 0xb7;

// Maximum dimensions for a constrained-parameters MPEG-1 sequence.
constexpr unsigned MAX_VIDEO_WIDTH = 768;
constexpr unsigned MAX_VIDEO_HEIGHT = 576;

// Used until the MPEG sequence header supplies a picture rate.
constexpr unsigned FALLBACK_FRAME_RATE = 25;

} // anonymous namespace


DEFINE_DEVICE_TYPE(STI3400, sti3400_device, "sti3400", "SGS-Thomson STi3400 MPEG-1 Video Decoder")

struct sti3400_device::decoder_state
{
	std::array<u8, COMPRESSED_DATA_BUFFER_BYTES> input{};
	std::vector<u8> dram;
	std::vector<u8> picture_valid;
	std::unique_ptr<mpeg_video> decoder;
	unsigned input_bytes = 0;
	unsigned input_bit_position = 0;
	u16 display_pointer = 0;
	u16 reconstructed_pointer = 0;
	u16 forward_pointer = 0;
	u16 backward_pointer = 0;
	u16 width = 0;
	u16 height = 0;
	u16 decoded_width = 0;
	u16 decoded_height = 0;
	double frame_rate = FALLBACK_FRAME_RATE;
	bool task_active = false;
	bool repeat_pending = false;
};

sti3400_device::sti3400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, STI3400, tag, owner, clock),
	m_irq_cb(*this),
	m_decoder(std::make_unique<decoder_state>()),
	m_dram_size(0),
	m_decode_timer(nullptr)
{
}

sti3400_device::~sti3400_device() = default;

void sti3400_device::device_start()
{
	if (!m_dram_size || !std::has_single_bit(m_dram_size) || (m_dram_size % BIT_BUFFER_LEVEL_UNIT_BYTES))
		fatalerror("%s: picture DRAM size must be a power-of-two multiple of 256 bytes\n", tag());

	m_decode_timer = timer_alloc(FUNC(sti3400_device::decode_tick), this);
	m_decoder->dram.resize(m_dram_size);
	m_decoder->picture_valid.resize(m_dram_size / BIT_BUFFER_LEVEL_UNIT_BYTES);
	m_decoder->decoder = std::make_unique<mpeg_video>(m_decoder->input.data(), MAX_VIDEO_WIDTH, MAX_VIDEO_HEIGHT);
	m_decoder->decoder->register_save_state(*this);

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
	save_item(NAME(m_decoder->input));
	save_item(NAME(m_decoder->dram));
	save_item(NAME(m_decoder->picture_valid));
	save_item(NAME(m_decoder->input_bytes));
	save_item(NAME(m_decoder->input_bit_position));
	save_item(NAME(m_decoder->display_pointer));
	save_item(NAME(m_decoder->reconstructed_pointer));
	save_item(NAME(m_decoder->forward_pointer));
	save_item(NAME(m_decoder->backward_pointer));
	save_item(NAME(m_decoder->width));
	save_item(NAME(m_decoder->height));
	save_item(NAME(m_decoder->decoded_width));
	save_item(NAME(m_decoder->decoded_height));
	save_item(NAME(m_decoder->frame_rate));
	save_item(NAME(m_decoder->task_active));
	save_item(NAME(m_decoder->repeat_pending));
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
	m_decoder->input.fill(0);
	std::fill(m_decoder->picture_valid.begin(), m_decoder->picture_valid.end(), 0);
	m_decoder->input_bytes = 0;
	m_decoder->input_bit_position = 0;
	m_decoder->display_pointer = 0;
	m_decoder->reconstructed_pointer = 0;
	m_decoder->forward_pointer = 0;
	m_decoder->backward_pointer = 0;
	m_decoder->width = 0;
	m_decoder->height = 0;
	m_decoder->decoded_width = 0;
	m_decoder->decoded_height = 0;
	m_decoder->frame_rate = FALLBACK_FRAME_RATE;
	m_decoder->task_active = false;
	m_decoder->repeat_pending = false;
	m_decoder->decoder->clear();
	m_irq_cb(CLEAR_LINE);
	m_decode_timer->adjust(attotime::never);
}

void sti3400_device::device_post_load()
{
	m_irq_cb(m_irq_state ? ASSERT_LINE : CLEAR_LINE);
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
	m_decoder->input.fill(0);
	std::fill(m_decoder->picture_valid.begin(), m_decoder->picture_valid.end(), 0);
	m_decoder->input_bytes = 0;
	m_decoder->input_bit_position = 0;
	m_decoder->display_pointer = 0;
	m_decoder->reconstructed_pointer = 0;
	m_decoder->forward_pointer = 0;
	m_decoder->backward_pointer = 0;
	m_decoder->width = 0;
	m_decoder->height = 0;
	m_decoder->decoded_width = 0;
	m_decoder->decoded_height = 0;
	m_decoder->frame_rate = FALLBACK_FRAME_RATE;
	m_decoder->task_active = false;
	m_decoder->repeat_pending = false;
	m_decoder->decoder->clear();
	m_decode_timer->adjust(attotime::never);
	update_status();
}

sti3400_device::frame_decode_result sti3400_device::decode_frame(bool &frame_valid)
{
	int position = m_decoder->input_bit_position;
	int width = m_decoder->decoded_width;
	int height = m_decoder->decoded_height;
	double frame_rate = m_decoder->frame_rate;
	frame_valid = false;
	auto const picture_buffer = [this] (u16 pointer)
	{
		const unsigned base = (unsigned(pointer) * BIT_BUFFER_LEVEL_UNIT_BYTES) & (m_dram_size - 1);
		return mpeg_video::picture_buffer{ m_decoder->dram.data() + base, m_dram_size - base };
	};
	const mpeg_video::picture_buffers buffers
	{
		picture_buffer(m_decoder->reconstructed_pointer),
		picture_buffer(m_decoder->forward_pointer),
		picture_buffer(m_decoder->backward_pointer)
	};
	const mpeg_video::decode_result result = m_decoder->decoder->decode_buffer(
			position,
			m_decoder->input_bytes * 8,
			buffers,
			width,
			height,
			frame_rate,
			frame_valid);

	if (result == mpeg_video::decode_result::DECODED)
	{
		m_decoder->decoded_width = width;
		m_decoder->decoded_height = height;
		m_decoder->frame_rate = frame_rate;
		if (frame_valid)
		{
			const unsigned picture = m_decoder->reconstructed_pointer & (m_decoder->picture_valid.size() - 1);
			m_decoder->picture_valid[picture] = 1;
		}
	}
	else if (result == mpeg_video::decode_result::INVALID_DATA)
	{
		LOGMASKED(LOG_INVALID_DATA, "%s: skipped invalid MPEG video data at byte %08x\n",
			machine().describe_context(), u32(m_decode_position + (position / 8)));
	}

	const unsigned bytes_consumed = position / 8;
	if (bytes_consumed)
	{
		std::move(
			m_decoder->input.begin() + bytes_consumed,
			m_decoder->input.begin() + m_decoder->input_bytes,
			m_decoder->input.begin());
		m_decoder->input_bytes -= bytes_consumed;
		m_decode_position += bytes_consumed;
	}
	m_decoder->input_bit_position = position & 7;

	switch (result)
	{
	case mpeg_video::decode_result::DECODED:
		return frame_decode_result::DECODED;
	case mpeg_video::decode_result::INVALID_DATA:
		return frame_decode_result::INVALID_DATA;
	default:
		return frame_decode_result::NEED_DATA;
	}
}

TIMER_CALLBACK_MEMBER(sti3400_device::decode_tick)
{
	if (m_decoder->task_active)
	{
		frame_decode_result result;
		do
		{
			bool frame_valid = false;
			result = decode_frame(frame_valid);
			if (frame_valid)
				break;
		}
		while (result != frame_decode_result::NEED_DATA);

		if (result != frame_decode_result::NEED_DATA)
			m_decoder->task_active = false;
		else
			m_decode_timer->adjust(attotime::from_hz(m_decoder->frame_rate));
	}

	update_status();
	activate_event();
}

void sti3400_device::vblank_w(int state)
{
	if (!state)
		return;

	// DFP is double-buffered by the hardware and becomes active at VSYNC.
	m_decoder->display_pointer = m_registers[REG_DFP] & PICTURE_POINTER_MASK;
	m_decoder->width = m_decoder->decoded_width;
	m_decoder->height = m_decoder->decoded_height;

	// A VSYNC-generated DSYNC starts the next task and restarts automatic start-code detection.
	if (m_decoder->repeat_pending)
	{
		m_decoder->repeat_pending = false;
	}
	else if ((m_registers[REG_CTL] & CTL_EDC) && !(m_registers[REG_CTL] & CTL_DVS) &&
		!(m_registers[REG_INS] & INS_WAIT) && m_event_active && !m_decoder->task_active)
	{
		m_decoder->reconstructed_pointer = m_registers[REG_RFP] & PICTURE_POINTER_MASK;
		m_decoder->forward_pointer = m_registers[REG_FFP] & PICTURE_POINTER_MASK;
		m_decoder->backward_pointer = m_registers[REG_BFP] & PICTURE_POINTER_MASK;
		m_decoder->task_active = true;
		m_decoder->repeat_pending = bool(m_registers[REG_INS] & INS_RPT);
		finish_event();
		m_decode_timer->adjust(attotime::zero);
	}
}

void sti3400_device::stream_byte_w(u8 data)
{
	if (m_decoder->input_bytes == m_decoder->input.size())
	{
		logerror("%s: compressed-video input overflow\n", machine().describe_context());
		return;
	}
	m_decoder->input[m_decoder->input_bytes++] = data;

	m_fifo[m_fifo_write & (COMPRESSED_DATA_BUFFER_BYTES - 1)] = data;
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

bool sti3400_device::video_valid() const
{
	const unsigned picture = m_decoder->display_pointer & (m_decoder->picture_valid.size() - 1);
	return m_decoder->picture_valid[picture];
}

u16 sti3400_device::video_width() const
{
	return m_decoder->width;
}

u16 sti3400_device::video_height() const
{
	return m_decoder->height;
}

u32 sti3400_device::video_pixel(unsigned x, unsigned y) const
{
	if (!video_valid() || (x >= m_decoder->width) || (y >= m_decoder->height))
		return 0;

	const unsigned mask = m_dram_size - 1;
	const unsigned base = (unsigned(m_decoder->display_pointer) * BIT_BUFFER_LEVEL_UNIT_BYTES) & mask;
	const unsigned luma_pitch = (m_decoder->width + 15) & ~15;
	const unsigned luma_rows = (m_decoder->height + 15) & ~15;
	const unsigned luma_bytes = luma_pitch * luma_rows;
	const unsigned chroma_pitch = luma_pitch / 2;
	const unsigned chroma_bytes = chroma_pitch * luma_rows / 2;
	const int luminance = m_decoder->dram[(base + y * luma_pitch + x) & mask];
	const int cb = m_decoder->dram[(base + luma_bytes + (y / 2) * chroma_pitch + x / 2) & mask] - 128;
	const int cr = m_decoder->dram[(base + luma_bytes + chroma_bytes + (y / 2) * chroma_pitch + x / 2) & mask] - 128;
	const int scaled_luminance = 298 * (luminance - 16);
	const u8 red = std::clamp((scaled_luminance + 409 * cr + 128) >> 8, 0, 255);
	const u8 green = std::clamp((scaled_luminance - 100 * cb - 208 * cr + 128) >> 8, 0, 255);
	const u8 blue = std::clamp((scaled_luminance + 516 * cb + 128) >> 8, 0, 255);
	return rgb_t(red, green, blue);
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
		else if ((m_fifo_write - position) < HEADER_FIFO_BYTES)
		{
			// The software detector runs on CDF writes; defer the hit until HDF can supply a hardware-sized window.
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

	if (!m_decoder->task_active)
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

u8 sti3400_device::stream_byte(u64 position) const
{
	if (position >= m_fifo_write)
		return 0;

	return m_fifo[position & (COMPRESSED_DATA_BUFFER_BYTES - 1)];
}

u16 sti3400_device::read(offs_t offset, u16 mem_mask)
{
	switch (offset)
	{
	case REG_HDF:
	{
		const u16 result = (stream_byte(m_fifo_read) << 8) | stream_byte(m_fifo_read + 1);
		if (!machine().side_effects_disabled())
		{
			m_fifo_read = std::min(m_fifo_read + 2, m_fifo_write);
			update_status();
		}
		return result;
	}

	case REG_HDP:
		return 0;

	case REG_STA:
		return m_status;

	case REG_BBL:
		return bit_buffer_level();

	case REG_DFP:
		return m_decoder->display_pointer;

	case REG_RFP:
		return m_decoder->reconstructed_pointer;

	case REG_FFP:
		return m_decoder->forward_pointer;

	case REG_BFP:
		return m_decoder->backward_pointer;

	case REG_ITS:
	{
		const u16 result = m_interrupt_status;
		if (!machine().side_effects_disabled())
		{
			if (ACCESSING_BITS_8_15)
				m_interrupt_status &= 0x00ff;
			if (ACCESSING_BITS_0_7)
				m_interrupt_status &= 0xff00;
			update_irq();
		}
		return result;
	}

	default:
		return m_registers[offset];
	}
}

void sti3400_device::write(offs_t offset, u16 data, u16 mem_mask)
{
	if (offset == REG_CDF)
	{
		if (ACCESSING_BITS_8_15)
			stream_byte_w(data >> 8);
		if (ACCESSING_BITS_0_7)
			stream_byte_w(data);
		return;
	}

	const u16 old_data = m_registers[offset];
	COMBINE_DATA(&m_registers[offset]);

	switch (offset)
	{
	case REG_CTL:
		if ((old_data & CTL_SRS) && !(m_registers[offset] & CTL_SRS))
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
