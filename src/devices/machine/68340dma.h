// license:BSD-3-Clause
// copyright-holders:David Haywood
#ifndef MAME_MACHINE_68340DMA_H
#define MAME_MACHINE_68340DMA_H

#pragma once

class m68340_cpu_device;


class m68340_dma
{
public:
	void reset();
	void module_reset();

	uint16_t read(offs_t offset, uint16_t mem_mask);
	void write(m68340_cpu_device &cpu, offs_t offset, uint16_t data, uint16_t mem_mask);
	void dreq_w(m68340_cpu_device &cpu, unsigned channel, int state);
	uint8_t irq_level() const;
	uint8_t arbitrate(uint8_t level) const;
	uint8_t irq_vector(uint8_t level) const;

private:
	friend class m68340_cpu_device;

	struct channel_state
	{
		uint16_t mcr;
		uint16_t intr;
		uint16_t ccr;
		uint8_t csr;
		uint8_t fcr;
		uint32_t sar;
		uint32_t dar;
		uint32_t btc;
		uint8_t dreq;
	};

	channel_state m_channel[2];

	bool irq_pending(channel_state const &channel) const;
	void run(m68340_cpu_device &cpu, unsigned channel);
	void transfer(m68340_cpu_device &cpu, unsigned channel);
	void set_status(m68340_cpu_device &cpu, channel_state &channel, uint8_t status);
};

#endif // MAME_MACHINE_68340DMA_H
