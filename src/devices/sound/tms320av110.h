// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn

#ifndef MAME_SOUND_TMS320AV110_H
#define MAME_SOUND_TMS320AV110_H

#pragma once

struct plm_audio_t;
struct plm_buffer_t;


class tms320av110_device : public device_t, public device_sound_interface
{
public:
	tms320av110_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto drq() { return m_drq_cb.bind(); }

	void reset_w(u8 data);
	u8 read(offs_t offset);
	void write(offs_t offset, u8 data);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;
	virtual void sound_stream_update(sound_stream &stream) override;

private:
	static constexpr unsigned DECODE_STAGING_SIZE = 0x200;
	static constexpr unsigned PCM_FRAME_COUNT = 1U << 13;

	void decoder_create();
	void decoder_destroy();
	void decoder_reset();
	void decoder_flush();
	bool decode_frame();
	void fifo_w(u8 data);

	sound_stream *m_stream;
	devcb_write_line m_drq_cb;
	plm_buffer_t *m_decode_buffer;
	plm_audio_t *m_audio_decoder;
	std::unique_ptr<float[]> m_pcm;

	u8 m_registers[0x80];
	u8 m_decode_staging[DECODE_STAGING_SIZE];
	u16 m_decode_staging_count;
	u64 m_pcm_read;
	u64 m_pcm_write;
	u8 m_control;
};

DECLARE_DEVICE_TYPE(TMS320AV110, tms320av110_device)

#endif // MAME_SOUND_TMS320AV110_H
