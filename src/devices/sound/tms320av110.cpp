// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn

/*
 * Texas Instruments TMS320AV110 MPEG-1 audio decoder (preliminary)
 *
 * The host register interface and compressed-data input are implemented for
 * Bell-Fruit Cobra 3.  MPEG-1 Audio Layer II reconstruction uses PL_MPEG.
 */

#include "emu.h"
#include "tms320av110.h"

#define PLM_NO_STDIO
#include "pl_mpeg/pl_mpeg.h"

DEFINE_DEVICE_TYPE(TMS320AV110, tms320av110_device, "tms320av110", "Texas Instruments TMS320AV110 MPEG Audio Decoder")

tms320av110_device::tms320av110_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, TMS320AV110, tag, owner, clock),
	device_sound_interface(mconfig, *this),
	m_stream(nullptr),
	m_drq_cb(*this),
	m_decode_buffer(nullptr),
	m_audio_decoder(nullptr),
	m_decode_staging_count(0),
	m_pcm_read(0),
	m_pcm_write(0),
	m_control(0)
{
}

void tms320av110_device::device_start()
{
	m_pcm = make_unique_clear<float[]>(PCM_FRAME_COUNT * 2);
	m_stream = stream_alloc(0, 2, 44'100);
	decoder_create();

	save_item(NAME(m_registers));
	save_item(NAME(m_decode_staging));
	save_item(NAME(m_decode_staging_count));
	save_pointer(NAME(m_pcm), PCM_FRAME_COUNT * 2);
	save_item(NAME(m_pcm_read));
	save_item(NAME(m_pcm_write));
	save_item(NAME(m_control));
}

void tms320av110_device::device_reset()
{
	decoder_reset();
	std::fill(std::begin(m_registers), std::end(m_registers), 0);
	m_control = 0;
	m_drq_cb(CLEAR_LINE);
}

void tms320av110_device::device_stop()
{
	decoder_destroy();
}

void tms320av110_device::decoder_create()
{
	m_decode_buffer = plm_buffer_create_with_capacity(0x10000);
	m_audio_decoder = plm_audio_create_with_buffer(m_decode_buffer, 1);
}

void tms320av110_device::decoder_destroy()
{
	if (m_audio_decoder)
		plm_audio_destroy(m_audio_decoder);

	m_audio_decoder = nullptr;
	m_decode_buffer = nullptr;
}

void tms320av110_device::decoder_reset()
{
	if (m_stream)
		m_stream->update();

	decoder_destroy();
	decoder_create();
	std::fill(std::begin(m_decode_staging), std::end(m_decode_staging), 0);
	std::fill_n(m_pcm.get(), PCM_FRAME_COUNT * 2, 0.0F);
	m_decode_staging_count = 0;
	m_pcm_read = 0;
	m_pcm_write = 0;
}

void tms320av110_device::decoder_flush()
{
	if (!m_decode_staging_count)
		return;

	plm_buffer_write(m_decode_buffer, m_decode_staging, m_decode_staging_count);
	m_decode_staging_count = 0;
}

bool tms320av110_device::decode_frame()
{
	plm_samples_t *const samples = plm_audio_decode(m_audio_decoder);
	if (!samples)
		return false;

	for (unsigned sample = 0; sample < samples->count; sample++)
	{
		const unsigned position = unsigned(m_pcm_write & (PCM_FRAME_COUNT - 1)) * 2;
		m_pcm[position + 0] = samples->interleaved[(sample * 2) + 0];
		m_pcm[position + 1] = samples->interleaved[(sample * 2) + 1];
		m_pcm_write++;
	}

	return true;
}

void tms320av110_device::fifo_w(u8 data)
{
	m_decode_staging[m_decode_staging_count++] = data;
	if (m_decode_staging_count == DECODE_STAGING_SIZE)
		decoder_flush();
}

void tms320av110_device::reset_w(u8 data)
{
	if (!data && m_control)
		decoder_reset();
	m_control = data;
	m_drq_cb(data ? ASSERT_LINE : CLEAR_LINE);
}

u8 tms320av110_device::read(offs_t offset)
{
	decoder_flush();
	return m_registers[offset & 0x7f];
}

void tms320av110_device::write(offs_t offset, u8 data)
{
	offset &= 0x7f;
	if (offset == 0x18)
	{
		fifo_w(data);
		return;
	}

	decoder_flush();
	m_registers[offset] = data;
}

void tms320av110_device::sound_stream_update(sound_stream &stream)
{
	decoder_flush();

	for (int sample = 0; sample < stream.samples(); sample++)
	{
		if ((m_pcm_read == m_pcm_write) && !decode_frame())
		{
			stream.put(0, sample, 0.0F);
			stream.put(1, sample, 0.0F);
			continue;
		}

		if (m_pcm_read != m_pcm_write)
		{
			const unsigned position = unsigned(m_pcm_read & (PCM_FRAME_COUNT - 1)) * 2;
			stream.put(0, sample, m_pcm[position + 0]);
			stream.put(1, sample, m_pcm[position + 1]);
			m_pcm_read++;
		}
	}
}
