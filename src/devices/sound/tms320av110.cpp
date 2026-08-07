// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn

/*
 * Texas Instruments TMS320AV110 MPEG-1 audio decoder (preliminary)
 *
 * The implemented subset consists of the host register window, memory-mapped
 * compressed-data input, reset-controlled REQ signalling, and MPEG-1 Audio
 * Layer II reconstruction using PL_MPEG.  Input-buffer-full backpressure and
 * other control/status register behaviour are not yet implemented.
 *
 * Reference: Texas Instruments data sheet SCSS013C, revised August 1995.
 */

#include "emu.h"
#include "tms320av110.h"

#define PLM_NO_STDIO
#include "pl_mpeg/pl_mpeg.h"

namespace {

// The host interface has seven address pins, SADDR6 through SADDR0.
constexpr unsigned HOST_ADDRESS_BITS = 7;
constexpr offs_t HOST_ADDRESS_MASK = (1U << HOST_ADDRESS_BITS) - 1;

// DATAIN is the data sheet's memory-mapped compressed-audio input register.
constexpr offs_t REG_DATAIN = 0x18;

// MPEG audio is presented as left and right PCM output slots, including mono streams.
constexpr unsigned LEFT_CHANNEL = 0;
constexpr unsigned RIGHT_CHANNEL = 1;
constexpr unsigned OUTPUT_CHANNELS = 2;

// Used for sound scheduling until PL_MPEG finds a header and reports the stream rate.
constexpr u32 INITIAL_SAMPLE_RATE = 44'100;

// Match the AV110's documented 256-byte internal input buffer when batching host writes.
constexpr unsigned INPUT_WRITE_BATCH_BYTES = 1U << 8;

} // anonymous namespace

DEFINE_DEVICE_TYPE(TMS320AV110, tms320av110_device, "tms320av110", "Texas Instruments TMS320AV110 MPEG Audio Decoder")

struct tms320av110_device::decoder_state
{
	plm_buffer_t *buffer = nullptr;
	plm_audio_t *audio = nullptr;
	std::array<u8, INPUT_WRITE_BATCH_BYTES> input_staging{};
	std::array<float, PLM_AUDIO_SAMPLES_PER_FRAME * OUTPUT_CHANNELS> pcm{};
	unsigned input_staging_count = 0;
	unsigned pcm_position = 0;
	unsigned pcm_count = 0;
};

tms320av110_device::tms320av110_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, TMS320AV110, tag, owner, clock),
	device_sound_interface(mconfig, *this),
	m_stream(nullptr),
	m_req_cb(*this),
	m_decoder(std::make_unique<decoder_state>()),
	m_reset_asserted(true)
{
}

tms320av110_device::~tms320av110_device() = default;

void tms320av110_device::device_start()
{
	m_stream = stream_alloc(0, OUTPUT_CHANNELS, INITIAL_SAMPLE_RATE);
	decoder_create();

	save_item(NAME(m_decoder->input_staging));
	save_item(NAME(m_decoder->pcm));
	save_item(NAME(m_decoder->input_staging_count));
	save_item(NAME(m_decoder->pcm_position));
	save_item(NAME(m_decoder->pcm_count));
	save_item(NAME(m_reset_asserted));
}

void tms320av110_device::device_reset()
{
	decoder_reset();
	m_reset_asserted = true;
	m_req_cb(ASSERT_LINE); // REQ is high (not requesting data) throughout reset.
}

void tms320av110_device::device_stop()
{
	decoder_destroy();
}

void tms320av110_device::decoder_create()
{
	// PL_MPEG's ring buffer grows as required and discards bytes after decoding.
	m_decoder->buffer = plm_buffer_create_with_capacity(PLM_BUFFER_DEFAULT_SIZE);
	m_decoder->audio = plm_audio_create_with_buffer(m_decoder->buffer, true);
}

void tms320av110_device::decoder_destroy()
{
	if (m_decoder->audio)
		plm_audio_destroy(m_decoder->audio);

	m_decoder->audio = nullptr;
	m_decoder->buffer = nullptr;
}

void tms320av110_device::decoder_reset()
{
	if (m_stream)
	{
		m_stream->update();
		m_stream->set_sample_rate(INITIAL_SAMPLE_RATE);
	}

	decoder_destroy();
	decoder_create();
	m_decoder->input_staging.fill(0);
	m_decoder->pcm.fill(0.0F);
	m_decoder->input_staging_count = 0;
	m_decoder->pcm_position = 0;
	m_decoder->pcm_count = 0;
}

void tms320av110_device::decoder_flush()
{
	if (!m_decoder->input_staging_count)
		return;

	plm_buffer_write(m_decoder->buffer, m_decoder->input_staging.data(), m_decoder->input_staging_count);
	m_decoder->input_staging_count = 0;
}

bool tms320av110_device::decode_frame()
{
	plm_samples_t *const samples = plm_audio_decode(m_decoder->audio);
	if (!samples)
		return false;

	// PL_MPEG always returns one 1,152-sample MPEG-1 Layer II frame here.
	assert(samples->count <= PLM_AUDIO_SAMPLES_PER_FRAME);
	std::copy_n(samples->interleaved, samples->count * OUTPUT_CHANNELS, m_decoder->pcm.begin());
	m_decoder->pcm_position = 0;
	m_decoder->pcm_count = samples->count;

	u32 const sample_rate = plm_audio_get_samplerate(m_decoder->audio);
	if (sample_rate && (sample_rate != m_stream->sample_rate()))
		m_stream->set_sample_rate(sample_rate);

	return true;
}

void tms320av110_device::fifo_w(u8 data)
{
	m_decoder->input_staging[m_decoder->input_staging_count++] = data;
	if (m_decoder->input_staging_count == m_decoder->input_staging.size())
		decoder_flush();
}

void tms320av110_device::reset_w(int state)
{
	bool const asserted = !state;
	if (asserted && !m_reset_asserted)
		decoder_reset();

	m_reset_asserted = asserted;
	// Reset timing is not yet modelled; REQ becomes active as soon as RESET is released.
	m_req_cb(m_reset_asserted ? ASSERT_LINE : CLEAR_LINE);
}

u8 tms320av110_device::read(offs_t offset)
{
	if (m_reset_asserted)
		return 0; // The data sheet specifies that RESET low disables host accesses.

	offset &= HOST_ADDRESS_MASK;
	logerror("%s: unimplemented register read %02x\n", machine().describe_context(), offset);
	return 0;
}

void tms320av110_device::write(offs_t offset, u8 data)
{
	if (m_reset_asserted)
		return; // The data sheet specifies that RESET low disables host accesses.

	offset &= HOST_ADDRESS_MASK;
	if (offset == REG_DATAIN)
	{
		fifo_w(data);
		return;
	}

	logerror("%s: unimplemented register write %02x = %02x\n", machine().describe_context(), offset, data);
}

void tms320av110_device::sound_stream_update(sound_stream &stream)
{
	decoder_flush();

	for (int sample = 0; sample < stream.samples(); sample++)
	{
		if ((m_decoder->pcm_position == m_decoder->pcm_count) && !decode_frame())
		{
			stream.put(LEFT_CHANNEL, sample, 0.0F);
			stream.put(RIGHT_CHANNEL, sample, 0.0F);
			continue;
		}

		unsigned const position = m_decoder->pcm_position * OUTPUT_CHANNELS;
		stream.put(LEFT_CHANNEL, sample, m_decoder->pcm[position + LEFT_CHANNEL]);
		stream.put(RIGHT_CHANNEL, sample, m_decoder->pcm[position + RIGHT_CHANNEL]);
		m_decoder->pcm_position++;
	}
}
