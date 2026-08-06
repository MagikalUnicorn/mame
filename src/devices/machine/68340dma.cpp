// license:BSD-3-Clause
// copyright-holders:David Haywood
/* 68340 DMA module */

#include "emu.h"
#include "68340.h"

#include <algorithm>


uint16_t m68340_cpu_device::m68340_internal_dma_r(offs_t offset, uint16_t mem_mask)
{
	assert(m_m68340DMA);
	return m_m68340DMA->read(offset, mem_mask);
}

void m68340_cpu_device::m68340_internal_dma_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	assert(m_m68340DMA);
	m_m68340DMA->write(*this, offset, data, mem_mask);
}


namespace {

constexpr uint8_t CSR_IRQ  = 0x80;
constexpr uint8_t CSR_DONE = 0x40;
constexpr uint8_t CSR_BES  = 0x20;
constexpr uint8_t CSR_BED  = 0x10;
constexpr uint8_t CSR_CONF = 0x08;
constexpr uint8_t CSR_BRKP = 0x04;
constexpr uint8_t CSR_CLEARABLE = CSR_DONE | CSR_BES | CSR_BED | CSR_CONF | CSR_BRKP;

constexpr uint16_t MCR_STP = 0x8000;
constexpr uint16_t MCR_SHARED = 0xe00f;

constexpr uint16_t CCR_INTB = 0x8000;
constexpr uint16_t CCR_INTN = 0x4000;
constexpr uint16_t CCR_INTE = 0x2000;
constexpr uint16_t CCR_SAPI = 0x0800;
constexpr uint16_t CCR_DAPI = 0x0400;
constexpr uint16_t CCR_REQ  = 0x0030;
constexpr uint16_t CCR_SD   = 0x0002;
constexpr uint16_t CCR_STR  = 0x0001;

unsigned transfer_size(unsigned field)
{
	switch (field & 3)
	{
	case 0: return 4;
	case 1: return 1;
	case 2: return 2;
	default: return 0;
	}
}

} // anonymous namespace


uint16_t m68340_dma::read(offs_t offset, uint16_t mem_mask)
{
	unsigned const byte_offset = offset * 2;
	channel_state const &channel = m_channel[BIT(byte_offset, 5)];

	switch (byte_offset & 0x1e)
	{
	case 0x00: return channel.mcr;
	case 0x04: return channel.intr;
	case 0x08: return channel.ccr;
	case 0x0a: return (uint16_t(channel.csr) << 8) | channel.fcr;
	case 0x0c: return channel.sar >> 16;
	case 0x0e: return channel.sar;
	case 0x10: return channel.dar >> 16;
	case 0x12: return channel.dar;
	case 0x14: return channel.btc >> 16;
	case 0x16: return channel.btc;
	default: return 0;
	}
}


void m68340_dma::write(m68340_cpu_device &cpu, offs_t offset, uint16_t data, uint16_t mem_mask)
{
	unsigned const byte_offset = offset * 2;
	unsigned const channel_number = BIT(byte_offset, 5);
	channel_state &channel = m_channel[channel_number];
	unsigned const reg = byte_offset & 0x1e;

	auto combine16 = [data, mem_mask] (uint16_t &value)
	{
		value = (value & ~mem_mask) | (data & mem_mask);
	};
	auto combine32 = [reg, data, mem_mask] (uint32_t &value)
	{
		if (BIT(reg, 1))
			value = (value & ~(uint32_t(mem_mask))) | (data & mem_mask);
		else
			value = (value & ~(uint32_t(mem_mask) << 16)) | (uint32_t(data & mem_mask) << 16);
	};

	switch (reg)
	{
	case 0x00:
		combine16(channel.mcr);
		m_channel[channel_number ^ 1].mcr = (m_channel[channel_number ^ 1].mcr & ~MCR_SHARED) | (channel.mcr & MCR_SHARED);
		cpu.update_ipl();
		break;

	case 0x04:
		combine16(channel.intr);
		cpu.update_ipl();
		break;

	case 0x08:
		combine16(channel.ccr);
		if (channel.csr & CSR_IRQ)
			channel.ccr &= ~CCR_STR;
		cpu.update_ipl();
		run(cpu, channel_number);
		break;

	case 0x0a:
		if (ACCESSING_BITS_8_15)
		{
			channel.csr &= ~uint8_t((data >> 8) & CSR_CLEARABLE);
			if (!(channel.csr & CSR_CLEARABLE))
				channel.csr &= ~CSR_IRQ;
		}
		if (ACCESSING_BITS_0_7)
			channel.fcr = data;
		cpu.update_ipl();
		break;

	case 0x0c:
	case 0x0e:
		combine32(channel.sar);
		break;

	case 0x10:
	case 0x12:
		combine32(channel.dar);
		break;

	case 0x14:
	case 0x16:
		combine32(channel.btc);
		break;
	}
}


void m68340_dma::dreq_w(m68340_cpu_device &cpu, unsigned channel_number, int state)
{
	channel_state &channel = m_channel[channel_number];
	uint8_t const old_state = channel.dreq;
	channel.dreq = bool(state);

	if (channel.dreq && !old_state)
		run(cpu, channel_number);
}


bool m68340_dma::irq_pending(channel_state const &channel) const
{
	return ((channel.csr & CSR_DONE) && (channel.ccr & CCR_INTN)) ||
		((channel.csr & (CSR_BES | CSR_BED | CSR_CONF)) && (channel.ccr & CCR_INTE)) ||
		((channel.csr & CSR_BRKP) && (channel.ccr & CCR_INTB));
}


uint8_t m68340_dma::irq_level() const
{
	uint8_t level = 0;
	for (channel_state const &channel : m_channel)
	{
		if (irq_pending(channel))
			level = std::max<uint8_t>(level, (channel.intr >> 8) & 7);
	}
	return level;
}


uint8_t m68340_dma::arbitrate(uint8_t level) const
{
	for (channel_state const &channel : m_channel)
	{
		if (irq_pending(channel) && (((channel.intr >> 8) & 7) == level))
			return channel.mcr & 0x0f;
	}
	return 0;
}


uint8_t m68340_dma::irq_vector(uint8_t level) const
{
	// Channel 1 has priority when both channels use the same interrupt level.
	for (channel_state const &channel : m_channel)
	{
		if (irq_pending(channel) && (((channel.intr >> 8) & 7) == level))
			return channel.intr;
	}
	return 0x0f;
}


void m68340_dma::run(m68340_cpu_device &cpu, unsigned channel_number)
{
	channel_state &channel = m_channel[channel_number];
	if (!(channel.ccr & CCR_STR) || (channel.mcr & MCR_STP))
		return;

	unsigned const request_mode = channel.ccr & CCR_REQ;
	if (!request_mode)
	{
		while (channel.ccr & CCR_STR)
			transfer(cpu, channel_number);
	}
	else if (channel.dreq)
	{
		// Burst mode continues while DREQ remains asserted; cycle-steal mode
		// performs one operand transfer for each assertion.
		do
		{
			transfer(cpu, channel_number);
		}
		while ((request_mode == 0x0020) && channel.dreq && (channel.ccr & CCR_STR));
	}
}


void m68340_dma::transfer(m68340_cpu_device &cpu, unsigned channel_number)
{
	channel_state &channel = m_channel[channel_number];
	unsigned const source_size = transfer_size(channel.ccr >> 8);
	unsigned const destination_size = transfer_size(channel.ccr >> 6);
	unsigned const transfer_bytes = std::max(source_size, destination_size);

	// Single-address transfers require modelling the external DACK/DONE bus
	// handshake.  Dual-address transfers include the DHR packing modes.
	if ((channel.ccr & CCR_SD) || !source_size || !destination_size || !channel.btc ||
		(channel.btc % transfer_bytes) || (channel.sar & (source_size - 1)) ||
		(channel.dar & (destination_size - 1)))
	{
		set_status(cpu, channel, CSR_CONF);
		return;
	}

	uint8_t const source_fc = (channel.fcr >> 4) & 7;
	uint8_t const destination_fc = channel.fcr & 7;
	uint32_t data = 0;

	for (unsigned byte = 0; byte < transfer_bytes; byte += source_size)
	{
		uint32_t source_data;
		switch (source_size)
		{
		case 1: source_data = cpu.m68ki_read_8_fc(channel.sar, source_fc); break;
		case 2: source_data = cpu.m68ki_read_16_fc(channel.sar, source_fc); break;
		default: source_data = cpu.m68ki_read_32_fc(channel.sar, source_fc); break;
		}
		data |= source_data << ((transfer_bytes - source_size - byte) * 8);
		if (channel.ccr & CCR_SAPI)
			channel.sar += source_size;
	}

	for (unsigned byte = 0; byte < transfer_bytes; byte += destination_size)
	{
		unsigned const shift = (transfer_bytes - destination_size - byte) * 8;
		switch (destination_size)
		{
		case 1: cpu.m68ki_write_8_fc(channel.dar, destination_fc, data >> shift); break;
		case 2: cpu.m68ki_write_16_fc(channel.dar, destination_fc, data >> shift); break;
		default: cpu.m68ki_write_32_fc(channel.dar, destination_fc, data); break;
		}
		if (channel.ccr & CCR_DAPI)
			channel.dar += destination_size;
	}

	channel.btc -= transfer_bytes;
	if (!channel.btc)
	{
		set_status(cpu, channel, CSR_DONE);
	}
}


void m68340_dma::set_status(m68340_cpu_device &cpu, channel_state &channel, uint8_t status)
{
	channel.csr |= CSR_IRQ | status;
	channel.ccr &= ~CCR_STR;
	cpu.update_ipl();
}

void m68340_dma::reset()
{
	for (channel_state &channel : m_channel)
	{
		channel = {};
		channel.mcr = 0x0080;
		channel.intr = 0x000f;
	}
}

void m68340_dma::module_reset()
{
	for (channel_state &channel : m_channel)
	{
		channel.ccr &= ~CCR_STR;
		channel.csr = 0;
		channel.intr = 0x000f;
	}
}
