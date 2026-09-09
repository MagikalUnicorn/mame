// license:BSD-3-Clause
// copyright-holders:David Haywood
#ifndef MAME_MACHINE_68340DMA_H
#define MAME_MACHINE_68340DMA_H

#pragma once

class m68340_cpu_device;


class mc68340_dma_module_device : public device_t
{
public:
	mc68340_dma_module_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0) ATTR_COLD;

	// External handshake callbacks use physical pin levels; DREQ, DACK and DONE are active low.
	template <unsigned Channel> auto dack_out_callback() { static_assert(Channel < 2); return m_dack_out_cb[Channel].bind(); }
	template <unsigned Channel> auto done_out_callback() { static_assert(Channel < 2); return m_done_out_cb[Channel].bind(); }
	template <unsigned Channel> void dreq_w(int state) { static_assert(Channel < 2); dreq_w(Channel, state); }
	template <unsigned Channel> void done_w(int state) { static_assert(Channel < 2); done_w(Channel, state); }

	uint16_t read(offs_t offset, uint16_t mem_mask = ~0);
	void write(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint8_t irq_level() const;
	uint8_t arbitrate(uint8_t level) const;
	uint8_t irq_vector(uint8_t level) const;
	void module_reset();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	friend class m68340_cpu_device;

	static constexpr u8 CSR_IRQ  = 0x80;
	static constexpr u8 CSR_DONE = 0x40;
	static constexpr u8 CSR_BES  = 0x20;
	static constexpr u8 CSR_BED  = 0x10;
	static constexpr u8 CSR_CONF = 0x08;
	static constexpr u8 CSR_BRKP = 0x04;
	static constexpr u8 CSR_CLEARABLE = CSR_DONE | CSR_BES | CSR_BED | CSR_CONF | CSR_BRKP;

	static constexpr u16 MCR_STP = 0x8000;
	static constexpr u16 MCR_SHARED = 0xe00f;

	static constexpr u16 CCR_INTB = 0x8000;
	static constexpr u16 CCR_INTN = 0x4000;
	static constexpr u16 CCR_INTE = 0x2000;
	static constexpr u16 CCR_ECO  = 0x1000;
	static constexpr u16 CCR_SAPI = 0x0800;
	static constexpr u16 CCR_DAPI = 0x0400;
	static constexpr u16 CCR_REQ  = 0x0030;
	static constexpr u16 CCR_SD   = 0x0002;
	static constexpr u16 CCR_STR  = 0x0001;

	static unsigned transfer_size(unsigned field);

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
		uint8_t done_in;
		uint8_t dack;
		uint8_t done_out;
	};

	channel_state m_channel[2]{};
	m68340_cpu_device *m_cpu;
	devcb_write_line::array<2> m_dack_out_cb;
	devcb_write_line::array<2> m_done_out_cb;

	static bool irq_pending(channel_state const &channel);
	void dreq_w(unsigned channel, int state);
	void done_w(unsigned channel, int state);
	void run(unsigned channel);
	void transfer(unsigned channel);
	void set_status(channel_state &channel, uint8_t status);
	void set_dack(unsigned channel, int state);
	void set_done_output(unsigned channel, int state);
	void restore_outputs();
};

DECLARE_DEVICE_TYPE(MC68340_DMA_MODULE, mc68340_dma_module_device)

#endif // MAME_MACHINE_68340DMA_H
