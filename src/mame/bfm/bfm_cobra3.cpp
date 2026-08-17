// license:BSD-3-Clause
// copyright-holders:David Haywood, James Wallace, blueonesarefaster, MagikalUnicorn

/* Bellfruit SWP (Skill With Prizes) Video hardware
    aka Cobra 3

   MPEG video and audio decoding are preliminary.

   Telly Addicts user notes:
   - Blank NVRAM produces a RAM ERROR on every startup.  Setting the stored
     volume does not clear this error, but it does not prevent the game from
     running.  The volume initially starts at its minimum setting.  From
     attract mode, toggle Refill/Volume Setup (R), adjust it with Up/Down,
     press Start to exit, then toggle Refill/Volume Setup off.
   - To use the test routines, open the Back and Front Doors (T), then press
     Test (F1).  Use Up/Down to choose a test and Start to enter or leave it.
     METER TEST requires Refill/Volume Setup to be on before entering and off
     again before leaving.
   - With the doors open, pressing Test twice within one second supplies two
     demonstration credits.  This is free play rather than an automatic demo:
     the player must still answer the questions.  Counters and payouts remain
     disabled.
   - CASHFLOW TEST is a read-only audit display.  Cash and meter counters do
     not advance while the test routines are active.
   - Inserting £1 or 20p coins with Refill/Volume Setup active replenishes the
     corresponding tube and records CASH REFILL rather than CASH IN.
   - The Initial Tube Fill adjusters are sampled when the machine starts.  At
     the default 100%, the £1 tube holds 40 coins (£40) and the 20p tube holds
     150 coins (£30), allowing immediate payouts.  Restart after changing an
     initial fill setting.

   BTANB:
   - Pressing Test with the Back and Front Doors closed can skip or interrupt
     the current video and leave its overlay out of position.  This is not a
     valid operating sequence; the doors must be opened before using Test.
   - Enabling the Demo Sounds DIL does not make every attract sequence play
     sound.  It permits demo sounds, which the game uses for only a proportion
     of attract sequences.
   - RESET ERROR 1 is the game's anti-tamper response to fewer than five
     detected resets, not an emulation failure.  Close the Back and Front
     Doors and allow the alarm delay to expire to clear it.
*/



#include "emu.h"

#include "bus/nscsi/cd.h"
#include "bus/rs232/rs232.h"
#include "machine/68340.h"
#include "machine/bacta_datalogger.h"
#include "machine/meters.h"
#include "machine/ncr5380.h"
#include "machine/nscsi_bus.h"
#include "machine/nvram.h"
#include "machine/rescap.h"
#include "machine/scc66470.h"
#include "machine/watchdog.h"
#include "sound/tms320av110.h"
#include "sound/ymz280b.h"
#include "video/ramdac.h"
#include "video/sti3400.h"

#include "screen.h"
#include "speaker.h"

#include "c3_telly.lh"

namespace {

class bfm_cobra3_state : public driver_device
{
public:
	bfm_cobra3_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_cpuregion(*this, "maincpu"),
		m_nvram(*this, "nvram"),
		m_av110(*this, "av110"),
		m_ymz(*this, "ymz280b"),
		m_palette(*this, "palette"),
		m_ramdac(*this, "ramdac"),
		m_scc66470(*this, "scc66470"),
		m_sti3400(*this, "sti3400"),
		m_strobein(*this, "STROBE%u", 0),
		m_iostatus(*this, "IOSTATUS"),
		m_meters(*this, "meters"),
		m_lamps(*this, "lamp%u", 0U),
		m_scsibus(*this, "scsi"),
		m_scsic(*this, "ncr5380"),
		m_watchdog(*this, "watchdog")
	{ }

	void bfm_cobra3(machine_config &config) ATTR_COLD;
	int meter_sense_r();

protected:
	// devices
	required_device<m68340_cpu_device> m_maincpu;
	required_region_ptr<uint16_t> m_cpuregion;
	required_device<nvram_device> m_nvram;
	required_device<tms320av110_device> m_av110;
	required_device<ymz280b_device> m_ymz;
	required_device<palette_device> m_palette;
	required_device<ramdac_device> m_ramdac;
	required_device<scc66470_device> m_scc66470;
	required_device<sti3400_device> m_sti3400;
	required_ioport_array<5> m_strobein;
	required_ioport m_iostatus;
	required_device<meters_device> m_meters;
	output_finder<24> m_lamps;
	required_device<nscsi_bus_device> m_scsibus;
	required_device<ncr5380_device> m_scsic;
	required_device<watchdog_timer_device> m_watchdog;

	std::unique_ptr<uint16_t[]> m_mainram;

	uint8_t m_active_strobe;
	uint8_t m_vol_clock;
	uint8_t m_volume;
	uint16_t m_lamp_latch;
	uint8_t m_lamp_port_a;
	uint8_t m_meter_latch;

	virtual void machine_start() override ATTR_COLD;
	virtual void update_meters(uint16_t data);
	virtual void cabinet_outputs_w(uint16_t) { }

	void volume_control(uint8_t direction, uint8_t clock);
	void lamp_latch_w(uint16_t data, uint16_t mem_mask);
	void lamp_port_a_w(uint8_t data);
	void update_lamps();
	void av110_reset_strobe_w(u8 data);
	uint16_t mem_r(offs_t offset, uint16_t mem_mask = ~0);
	void mem_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	void scc66470_irq(int state);

	void bfm_cobra3_map(address_map &map) ATTR_COLD;
	void ramdac_map(address_map &map) ATTR_COLD;
	void scc66470_map(address_map &map) ATTR_COLD;
};

class c3_telly_state : public bfm_cobra3_state
{
public:
	c3_telly_state(const machine_config &mconfig, device_type type, const char *tag) :
		bfm_cobra3_state(mconfig, type, tag),
		m_initial_tube_fill(*this, "TUBE%u", 0U)
	{ }

	void c3_telly(machine_config &config) ATTR_COLD;
	int meter_sense_r();
	int pound_tube_low_r();
	int twenty_p_tube_low_r();
	DECLARE_INPUT_CHANGED_MEMBER(coin_inserted);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;
	virtual void cabinet_outputs_w(uint16_t data) override;

private:
	required_ioport_array<2> m_initial_tube_fill;
	uint8_t m_triac_latch;
	uint16_t m_pound_tube_level;
	uint16_t m_twenty_p_tube_level;
	bool m_tube_levels_initialized;
};

void bfm_cobra3_state::update_lamps()
{
	for (unsigned i = 0; i < 16; i++)
		m_lamps[i] = BIT(m_lamp_latch, i);

	for (unsigned i = 0; i < 8; i++)
		m_lamps[16 + i] = BIT(m_lamp_port_a, i);
}

void bfm_cobra3_state::lamp_latch_w(uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_lamp_latch);
	update_lamps();
}

void bfm_cobra3_state::lamp_port_a_w(uint8_t data)
{
	m_lamp_port_a = data;
	update_lamps();
}

void bfm_cobra3_state::update_meters(uint16_t data)
{
	m_meter_latch = data & 0x0f;

	for (unsigned i = 0; i < 4; i++)
		m_meters->update(i, BIT(m_meter_latch, i));
}

void c3_telly_state::cabinet_outputs_w(uint16_t data)
{
	uint8_t const triacs = (data >> 4) & 0x07;
	uint8_t const rising = triacs & ~m_triac_latch;
	m_triac_latch = triacs;

	if (!BIT(m_strobein[2]->read(), 2))
		return;

	if (BIT(rising, 0) && m_pound_tube_level)
		m_pound_tube_level--;
	if (BIT(rising, 2) && m_twenty_p_tube_level)
		m_twenty_p_tube_level--;
}

void bfm_cobra3_state::volume_control(uint8_t direction, uint8_t clock)
{
	uint8_t const clock_changed = m_vol_clock ^ clock;

	m_vol_clock = clock;
	if (clock_changed)
	{ // digital volume clock line changed
		if (!clock)
		{ // changed from high to low,
			if (!direction)
			{
				if (m_volume < 31)
					m_volume++; //0-31 expressed as 1-32
			}
			else
			{
				if (m_volume > 0)
					m_volume--;
			}

			float const fraction = (32 - m_volume) / 32.0f;

			m_ymz->set_output_gain(0, fraction);
			m_ymz->set_output_gain(1, fraction);
			m_av110->set_output_gain(0, fraction);
			m_av110->set_output_gain(1, fraction);
		}
	}
}

void bfm_cobra3_state::av110_reset_strobe_w(u8)
{
	// This decoded write pulses the AV110's active-low RESET input.
	m_av110->reset_w(0);
	m_av110->reset_w(1);
}

int bfm_cobra3_state::meter_sense_r()
{
	for (unsigned i = 0; i < 4; i++)
	{
		if (m_meters->get_activity(i))
			return 1;
	}

	return 0;
}

int c3_telly_state::meter_sense_r()
{
	for (unsigned i = 0; i < 2; i++)
	{
		if (m_meters->get_activity(i))
			return 1;
	}

	return 0;
}

int c3_telly_state::pound_tube_low_r()
{
	return m_pound_tube_level <= 13; // the level switch opens above £13
}

int c3_telly_state::twenty_p_tube_low_r()
{
	return m_twenty_p_tube_level <= 23; // the level switch opens above £4.60
}

INPUT_CHANGED_MEMBER(c3_telly_state::coin_inserted)
{
	if (!newval || !BIT(m_strobein[2]->read(), 2))
		return;

	if ((param == 100) && (m_pound_tube_level < 40)) // £40 capacity
		m_pound_tube_level++;
	else if ((param == 20) && (m_twenty_p_tube_level < 150)) // £30 capacity
		m_twenty_p_tube_level++;
}

uint16_t bfm_cobra3_state::mem_r(offs_t offset, uint16_t mem_mask)
{
	int cs = m_maincpu->get_cs(offset * 2);

	switch (cs)
	{
		case 1: // ROM
			return m_cpuregion[offset & 0x7ffff];
		case 2:// (NV)RAM
			return m_mainram[offset & 0x1fff];

		case 3: // I/O
			{
				offset &= 0x7ff;
				offs_t cs_addr_8_11 = (offset * 2) & 0xf00;

				switch (cs_addr_8_11)
				{
					case 0x300: //YMZ stereo sound accesses
						if (ACCESSING_BITS_0_7)
						{
							return m_ymz->read(offset & 1);
						}
						break;

					case 0x400:
						{
							return (m_strobein[m_active_strobe]->read() << 8) | m_iostatus->read();
						}
					case 0x500: //SCSI DMA
						if (ACCESSING_BITS_8_15)
						{
							return m_scsic->dma_r() << 8;
						}
						break;

					case 0x600: //Looks like the RAMDAC hookup
						if (ACCESSING_BITS_0_7)
						{
							if ((offset & 7) == 1)
							{
								return m_ramdac->pal_r();
							}
						}
						break;

					default:
						logerror("%s maincpu read access offset %08x mem_mask %08x cs %d\n", machine().describe_context(), offset*4, mem_mask, cs);
						break;
				}
			}
			break;

		case 4: // SCSI controller
			if (ACCESSING_BITS_8_15)
			{
				return m_scsic->read(offset & 0x0f) << 8;
			}
			break;

		default:
			logerror("%s maincpu read access offset %08x mem_mask %08x cs %d\n", machine().describe_context(), offset*4, mem_mask, cs);
	}

	return 0x0000;
}

void bfm_cobra3_state::mem_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	int cs = m_maincpu->get_cs(offset * 2);

	switch (cs)
	{
		case 1:// ROM, shouldn't write here?
			logerror("%sx maincpu write access(1) offset %08x data %08x mem_mask %08x cs %d\n", machine().describe_context(), offset*4, data, mem_mask, cs);
			break;

		case 2:// (NV)RAM
			COMBINE_DATA(&m_mainram[offset & 0x1fff]);
			break;

		case 3: // I/O
			{
				offset &= 0x7ff;
				offs_t cs_addr_8_11 = (offset * 2) & 0xf00;
				switch (cs_addr_8_11)
				{
					case 0x000:
						lamp_latch_w(data, mem_mask);
						break;

					case 0x100:
						if (ACCESSING_BITS_8_15)
						{
							uint8_t const enables = data >> 8;
							machine().bookkeeping().coin_lockout_w(0, !BIT(enables, 3)); // £1
							machine().bookkeeping().coin_lockout_w(1, !BIT(enables, 2)); // 50p
							machine().bookkeeping().coin_lockout_w(2, !BIT(enables, 1)); // 20p
							machine().bookkeeping().coin_lockout_w(3, !BIT(enables, 0)); // 10p
							machine().bookkeeping().coin_lockout_w(4, !BIT(enables, 4)); // fifth validator channel
						}
						break;

					case 0x200:
						if (data > 0x100)
						{
							logerror("%s maincpu write access io latch offset %08x data %08x mem_mask %08x cs %d\n", machine().describe_context(), offset*4, data, mem_mask, cs);
						}
						update_meters(data);
						cabinet_outputs_w(data);
						volume_control(BIT(data,7), BIT(data,15));
						m_watchdog->reset_line_w(BIT(data , 8));

						for (int i=10; i<15; i++)
						{
							if (BIT(data, i))
							{
								m_active_strobe = i - 10;
							}
						}
						break;

					case 0x300:
						if (ACCESSING_BITS_0_7)
						{
							m_ymz->write(offset & 1, data);
						}
						break;

					case 0x500: // SCSI DMA
						if (ACCESSING_BITS_8_15)
						{
							m_scsic->dma_w(data >> 8);
						}
						break;

					case 0x700: // RAMDAC for palettes
						if (ACCESSING_BITS_0_7)
						{
							offset &= 7;
							switch (offset)
							{
							case 0:
								m_ramdac->index_w(data);
								break;
							case 1:
								m_ramdac->pal_w(data);
								break;
							case 2:
								m_ramdac->mask_w(data);
								break;
							case 3:
								m_ramdac->index_r_w(data);
								break;
							}
						}
						break;

					default:
						// coin divert, hoppers, note validator must be somewhere
						logerror("%s maincpu write access(3) offset %08x data %08x mem_mask %08x cs %d\n", machine().describe_context(), offset*4, data, mem_mask, cs);
						break;
				}
			}
			break;

		case 4: // SCSI controller
			offset &= 0x0f;
			if (ACCESSING_BITS_8_15)
			{
				m_scsic->write(offset, data >> 8);
			}
			break;

		default:
			logerror("%s maincpu write access(0) offset %08x data %08x mem_mask %08x cs %d\n", machine().describe_context(), offset*4, data, mem_mask, cs);
			break;
	}
}


void bfm_cobra3_state::bfm_cobra3_map(address_map &map)
{
	map(0x00000000, 0xffffffff).rw(FUNC(bfm_cobra3_state::mem_r), FUNC(bfm_cobra3_state::mem_w));
	map(0x00800000, 0x009fffff).m(m_scc66470, FUNC(scc66470_device::map)).cswidth(16);
	map(0x00a40000, 0x00a4007f).rw(m_sti3400, FUNC(sti3400_device::read), FUNC(sti3400_device::write));
	map(0x00a80000, 0x00a80001).w(FUNC(bfm_cobra3_state::av110_reset_strobe_w)).umask16(0x00ff);
	map(0x00a81000, 0x00a810ff).rw(m_av110, FUNC(tms320av110_device::read), FUNC(tms320av110_device::write)).umask16(0x00ff);
}

void bfm_cobra3_state::ramdac_map(address_map &map)
{
	map(0x000, 0x3ff).rw(m_ramdac, FUNC(ramdac_device::ramdac_pal_r), FUNC(ramdac_device::ramdac_rgb666_w));
}

void bfm_cobra3_state::scc66470_map(address_map &map)
{
	map(0x00000, 0x7ffff).ram();
}

static INPUT_PORTS_START( bfm_cobra3 )
	PORT_START("IOSTATUS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN4 ) PORT_IMPULSE(3)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN3 ) PORT_IMPULSE(3)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 ) PORT_IMPULSE(3)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 ) PORT_IMPULSE(3)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN5 ) PORT_IMPULSE(3)
	// Current through any active meter is returned on a shared sensing line.
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_CUSTOM ) PORT_READ_LINE_MEMBER(FUNC(bfm_cobra3_state::meter_sense_r))
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_SERVICE ) PORT_NAME("Test") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START("STROBE0")
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START("STROBE1")
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START("STROBE2")
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START("STROBE3")
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START("STROBE4")
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

static INPUT_PORTS_START( c3_telly )
	PORT_INCLUDE(bfm_cobra3)

	PORT_MODIFY("IOSTATUS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN4 ) PORT_NAME("10p") PORT_IMPULSE(3)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN3 ) PORT_NAME("20p") PORT_IMPULSE(3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(c3_telly_state::coin_inserted), 20)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 ) PORT_NAME("50p") PORT_IMPULSE(3)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 ) PORT_NAME(u8"£1") PORT_IMPULSE(3) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(c3_telly_state::coin_inserted), 100)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_CUSTOM ) PORT_READ_LINE_MEMBER(FUNC(c3_telly_state::meter_sense_r))

	PORT_MODIFY("STROBE0")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("A (Left)") PORT_CODE(KEYCODE_A)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("B (Left)") PORT_CODE(KEYCODE_B)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("C (Left)") PORT_CODE(KEYCODE_C)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_GAMBLE_TAKE ) PORT_NAME("Collect")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON6 ) PORT_NAME("C (Right)")
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_NAME("B (Right)")
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_NAME("A (Right)")

	PORT_MODIFY("STROBE1")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Select") PORT_CODE(KEYCODE_S)
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_MODIFY("STROBE2")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_CUSTOM ) PORT_READ_LINE_MEMBER(FUNC(c3_telly_state::pound_tube_low_r))
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_CUSTOM ) PORT_READ_LINE_MEMBER(FUNC(c3_telly_state::twenty_p_tube_low_r))
	PORT_CONFNAME( 0x04, 0x04, "Payout Unit" )
	PORT_CONFSETTING(    0x00, "Not Fitted" )
	PORT_CONFSETTING(    0x04, "Fitted" )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_DOOR ) PORT_NAME("Cash Door") PORT_CODE(KEYCODE_Y) PORT_TOGGLE
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_DOOR ) PORT_NAME("Back and Front Doors") PORT_CODE(KEYCODE_T) PORT_TOGGLE
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_SERVICE ) PORT_NAME("Refill/Volume Setup") PORT_CODE(KEYCODE_R) PORT_TOGGLE
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_MODIFY("STROBE4")
	PORT_DIPNAME( 0x01, 0x00, "Credit on Reset" ) PORT_DIPLOCATION("DIL:!01")
	PORT_DIPSETTING(    0x00, "Retained" )
	PORT_DIPSETTING(    0x01, "Lost" )
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("DIL:!05")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0xc0, 0x00, "Target Percentage" ) PORT_DIPLOCATION("DIL:!07,!08")
	PORT_DIPSETTING(    0x00, "30%" )
	PORT_DIPSETTING(    0x40, "35%" )
	PORT_DIPSETTING(    0x80, "40%" )
	PORT_DIPSETTING(    0xc0, "50%" )

	PORT_START("TUBE0")
	PORT_ADJUSTER(100, u8"Initial £1 Tube Fill")

	PORT_START("TUBE1")
	PORT_ADJUSTER(100, "Initial 20p Tube Fill")
INPUT_PORTS_END


void bfm_cobra3_state::machine_start()
{
	m_active_strobe = 0;
	m_vol_clock = 0;
	m_volume = 0;
	m_lamp_latch = 0;
	m_lamp_port_a = 0;
	m_meter_latch = 0;
	m_mainram = make_unique_clear<uint16_t[]>((1024 * 16) / 2);
	m_nvram->set_base(m_mainram.get(), 1024 * 16);
	save_pointer(NAME(m_mainram), (1024 * 16) / 2);

	save_item(NAME(m_active_strobe));
	save_item(NAME(m_vol_clock));
	save_item(NAME(m_volume));
	save_item(NAME(m_lamp_latch));
	save_item(NAME(m_lamp_port_a));
	save_item(NAME(m_meter_latch));
	machine().save().register_postload(save_prepost_delegate(FUNC(bfm_cobra3_state::update_lamps), this));
}

void c3_telly_state::machine_start()
{
	bfm_cobra3_state::machine_start();

	m_triac_latch = 0;
	m_pound_tube_level = 0;
	m_twenty_p_tube_level = 0;
	m_tube_levels_initialized = false;

	save_item(NAME(m_triac_latch));
	save_item(NAME(m_pound_tube_level));
	save_item(NAME(m_twenty_p_tube_level));
	save_item(NAME(m_tube_levels_initialized));
}

void c3_telly_state::machine_reset()
{
	if (!m_tube_levels_initialized)
	{
		m_pound_tube_level = (m_initial_tube_fill[0]->read() * 40 + 50) / 100; // £40 capacity in £1 coins
		m_twenty_p_tube_level = (m_initial_tube_fill[1]->read() * 150 + 50) / 100; // £30 capacity in 20p coins
		m_tube_levels_initialized = true;
	}
}


void bfm_cobra3_state::scc66470_irq(int state)
{
	m_maincpu->set_input_line(5, state);
}

uint32_t bfm_cobra3_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	if (cliprect.min_y == cliprect.max_y)
	{
		uint32_t *const line = &bitmap.pix(cliprect.min_y);
		std::fill_n(line, 768, 0);

		if (m_sti3400->video_valid())
		{
			const unsigned source_y = std::min<unsigned>(
				cliprect.min_y - 32 + 4,
				m_sti3400->video_height() - 1);

			for (unsigned x = 0; x < 352; x++)
			{
				const unsigned source_x = (x * m_sti3400->video_width()) / 352;
				const u32 pixel = m_sti3400->video_pixel(source_x, source_y);
				line[32 + (x * 2) + 0] = pixel;
				line[32 + (x * 2) + 1] = pixel;
			}
		}

		if (m_scc66470->display_enabled())
		{
			uint32_t *dest = line;
			uint8_t buffer[768];
			uint8_t *src = buffer;
			m_scc66470->line(cliprect.min_y, buffer, sizeof(buffer));

			if (*src != 254)
				dest = std::fill_n(dest, 32, m_palette->pen(*src));
			else
				dest += 32;
			src += 32;

			/* mpeg video has significant overscan, 4 lines either side.

			Just crop it out to fit, presume the chip does this IRL */

			for (int x = 0 ; x < 352 ; x++)
			{
				if (*src != 254)
					*dest++ = m_palette->pen(*src++);
				else
				{
					dest++;
					src++;
				}

				if (*src != 254)
					*dest++ = m_palette->pen(*src++);
				else
				{
					dest++;
					src++;
				}
			}

			if (*src != 254)
				std::fill_n(dest, 32, m_palette->pen(*src));
		}
	}
	return 0;
}


void bfm_cobra3_state::bfm_cobra3(machine_config &config)
{
	M68340(config, m_maincpu, 16000000);
	m_maincpu->set_addrmap(AS_PROGRAM, &bfm_cobra3_state::bfm_cobra3_map);
	m_maincpu->pa_out_callback().set(FUNC(bfm_cobra3_state::lamp_port_a_w));
	mc68340_serial_module_device &serial(*m_maincpu->subdevice<mc68340_serial_module_device>("serial"));
	serial.a_tx_cb().set("rs232_port1", FUNC(rs232_port_device::write_txd));
	serial.b_tx_cb().set("bacta", FUNC(bacta_datalogger_device::write_txd));

	rs232_port_device &rs232_port1(RS232_PORT(config, "rs232_port1", default_rs232_devices, nullptr));
	rs232_port1.rxd_handler().set(serial, FUNC(mc68340_serial_module_device::rx_a_w));
	rs232_port1.cts_handler().set(serial, FUNC(mc68340_serial_module_device::ip0_w));

	bacta_datalogger_device &bacta(BACTA_DATALOGGER(config, "bacta"));
	bacta.rxd_handler().set(serial, FUNC(mc68340_serial_module_device::rx_b_w));

	NVRAM(config, "nvram", nvram_device::DEFAULT_ALL_0);

	screen_device &screen(SCREEN(config, "screen"));
	screen.set_raw(15000000, 960, 0, 768, 312, 32, 312);
	screen.set_video_attributes(VIDEO_UPDATE_SCANLINE);
	screen.set_screen_update(FUNC(bfm_cobra3_state::screen_update));
	screen.screen_vblank().set(m_sti3400, FUNC(sti3400_device::vblank_w));

	PALETTE(config, m_palette).set_entries(256);

	RAMDAC(config, m_ramdac, m_palette); // MUSIC Semiconductor TR9C1710 RAMDAC
	m_ramdac->set_addrmap(0, &bfm_cobra3_state::ramdac_map);
	m_ramdac->set_split_read(1);

	SPEAKER(config, "lspeaker").front_left();
	SPEAKER(config, "rspeaker").front_right();

	YMZ280B(config, m_ymz, 16.9344_MHz_XTAL);
	m_ymz->add_route(0, "lspeaker", 1.0);
	m_ymz->add_route(1, "rspeaker", 1.0);

	TMS320AV110(config, m_av110, 24_MHz_XTAL);
	// Cobra enables decoder modes that require the optional external DRAM.
	m_av110->set_external_dram(true);
	// AV110 REQ and MC68340 DREQ2 are both active low.
	m_av110->req().set(m_maincpu, FUNC(m68340_cpu_device::dma_dreq2_w));
	m_av110->add_route(0, "lspeaker", 1.0);
	m_av110->add_route(1, "rspeaker", 1.0);

	SCC66470(config,m_scc66470,30000000);
	m_scc66470->set_addrmap(0, &bfm_cobra3_state::scc66470_map);
	m_scc66470->set_screen("screen");
	m_scc66470->irq().set(FUNC(bfm_cobra3_state::scc66470_irq));

	STI3400(config, m_sti3400, 0); // decoder clock and external-memory cycle timing are not modelled
	m_sti3400->set_dram_size(1024 * 1024); // Cobra's buffer pointers cover a 1 MiB address space
	m_sti3400->irq().set_inputline(m_maincpu, 6);

	auto &scsi(NSCSI_BUS(config, m_scsibus));
	auto &cdrom(NSCSI_CDROM(config, "cdrom"));
	scsi.set_external_device(2, cdrom);

	NCR5380(config, m_scsic);
	scsi.set_external_device(6, m_scsic);
	m_scsic->drq_handler().set(m_maincpu, FUNC(m68340_cpu_device::dma_dreq1_w)).invert();

	// Provisional values match the watchdog model used by earlier Bellfruit drivers.
	// TODO: Confirm the R/C values against Cobra 3 hardware.
	WATCHDOG_TIMER(config, m_watchdog).set_time(PERIOD_OF_555_MONOSTABLE(RES_K(120), CAP_N(100)));
	METERS(config, m_meters).set_number(4);
}

void c3_telly_state::c3_telly(machine_config &config)
{
	bfm_cobra3(config);

	m_meters->set_number(2);
}

ROM_START( c3_rtime )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "95400009.bin", 0x00001, 0x080000, CRC(a5e0a5ca) SHA1(e7063ddfb436152f15267fde2aa7695c8a262191) )
	ROM_LOAD16_BYTE( "95400010.bin", 0x00000, 0x080000, CRC(03fd5f72) SHA1(379cfc4ef5087f24989bc1f2246b6056e33fd472) )

	ROM_REGION( 0x100000, "altrevs", 0 )
	ROM_LOAD16_BYTE( "95400063.lhs", 0x00001, 0x080000, CRC(eecb5f3b) SHA1(a1c6ad61a65c5361c38aaae2a064983a978c45ea) )
	ROM_LOAD16_BYTE( "95400064.rhs", 0x00000, 0x080000, CRC(251689f5) SHA1(4589a409c6b0f2869f99a08df8d76223e54d5b3c) )
	ROM_LOAD16_BYTE( "95401063.lhs", 0x00001, 0x080000, CRC(ea98c159) SHA1(6f665d80b71af57b31194fdc981707822e62053e) )
	ROM_LOAD16_BYTE( "95401064.rhs", 0x00000, 0x080000, CRC(bc125897) SHA1(a83fdb54349d3ea5d183754bf4b9fee1f0b73be3) )
	ROM_LOAD16_BYTE( "radtimes.lhs", 0x00001, 0x080000, CRC(c6574297) SHA1(bd9744c4b08f9ae35fe1523ebcd68c52a36a32e0) )
	ROM_LOAD16_BYTE( "radtimes.rhs", 0x00000, 0x080000, CRC(ed2c24f0) SHA1(5f06b2de7e2b2dccee7763ea0938849d67256ff2) )
	ROM_LOAD16_BYTE( "rt017.lhs", 0x00001, 0x080000, CRC(d2272c39) SHA1(f583fe39c153dca2e86e875ca39056a8756e0d2c) )
	ROM_LOAD16_BYTE( "rt018.rhs", 0x00000, 0x080000, CRC(52999d03) SHA1(21d1e9034a26f6f73109e9e83272dcff104993e5) )
	ROM_LOAD16_BYTE( "rtimesp1", 0x00001, 0x080000, CRC(f856d377) SHA1(a9fac7e2188bbd087f70c1c00cbf790bc52d573b) )
	ROM_LOAD16_BYTE( "rtimesp2", 0x00000, 0x080000, CRC(130d0864) SHA1(034d6c4fdec3acd4329d16315aeac43b1f1a5e91) )

	ROM_REGION( 0x1000000, "ymz280b", 0 )
	ROM_LOAD( "95004056.bin", 0x000000, 0x080000, CRC(24e8f9fb) SHA1(0d484a8f368b0f2140f148a1dc84db85a100af38) )
	ROM_LOAD( "95004057.bin", 0x080000, 0x080000, CRC(f73c92d6) SHA1(08c7db2baccb703f99efb81f618719a7789ca564) )

	DISK_REGION("cdrom")
	DISK_IMAGE_READONLY( "95100302", 0, SHA1(20accfe236a0c85108cd2a205399ed8959f1a638) )
ROM_END

ROM_START( c3_telly )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "95400021.p1",  0x00001, 0x080000, CRC(5c969746) SHA1(7458c613d7a3e7cf6a21e55f74dcdc052404f29c) )
	ROM_LOAD16_BYTE( "95400022.p2",  0x00000, 0x080000, CRC(fa1fdb7b) SHA1(eff87c197a62dba49d95810e8669026db2edb187) )

	ROM_REGION( 0x100000, "altrevs", 0 )
	ROM_LOAD16_BYTE( "95401021.p1",  0x00001, 0x080000, CRC(24a334d3) SHA1(672f16cbd2ddf627213de71024b6fbaa28f526a5) )
	ROM_LOAD16_BYTE( "95401022.p2",  0x00000, 0x080000, CRC(90af3767) SHA1(e529ad7eef5e6d2a6951d46e77aaad2087890445) )
	ROM_LOAD16_BYTE( "tadd13lh",     0x00001, 0x080000, CRC(2d6ed08c) SHA1(efa39b9ff5605c2e29971fb5e874c9a0c178b1f0) )
	ROM_LOAD16_BYTE( "tadd14rh",     0x00000, 0x080000, CRC(26dd6ed6) SHA1(553f29017494b6f7ecc98940d527f498316ea55e) )
	ROM_LOAD16_BYTE( "telad.tl",     0x00001, 0x080000, CRC(e6906027) SHA1(20ca64417ea3795dc26adfea717cb3d724019c34) )
	ROM_LOAD16_BYTE( "telad.tr",     0x00000, 0x080000, CRC(38dbee05) SHA1(ee33cdaa7f817beb49a3cff49a5493a50d8d4504) )
	ROM_LOAD16_BYTE( "tasndl",       0x00001, 0x080000, CRC(3f0b9d2b) SHA1(6db3451c26a3e673204c316403e0bb7127191a1f) )
	ROM_LOAD16_BYTE( "tasndr",       0x00000, 0x080000, CRC(2dd9ebcf) SHA1(4d118d37e18266f82fb2acb37f5fd106e0f25a1f) )

	ROM_REGION( 0x1000000, "ymz280b", ROMREGION_ERASE00 )
	ROM_LOAD( "telsndl", 0x0000, 0x080000, CRC(74996fbd) SHA1(90e46130dccf47be1fcfaf549e548cdd4883e59d) )

	DISK_REGION("cdrom")
	DISK_IMAGE_READONLY( "95100300", 0, SHA1(98905cbff24c576c58210d1d003f710fa7064762) )
ROM_END


ROM_START( c3_tellyns )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "95400023.lhs", 0x00001, 0x080000, CRC(b79279b8) SHA1(010edf0c299b0b01ab43f52dce540ff0847fb4c5) )
	ROM_LOAD16_BYTE( "95400024.rhs", 0x00000, 0x080000, CRC(835d25fd) SHA1(6d780332f6016d6e1404922e0ac439a499211be3) )

	ROM_REGION( 0x100000, "altrevs", 0 )
	ROM_LOAD16_BYTE( "95401023.lhs", 0x00001, 0x080000, CRC(85b95b56) SHA1(106e617fc92f95a6b3769db1fd4e5ab47c752c08) )
	ROM_LOAD16_BYTE( "95401024.rhs", 0x00000, 0x080000, CRC(835d25fd) SHA1(6d780332f6016d6e1404922e0ac439a499211be3) )

	ROM_REGION( 0x1000000, "ymz280b", ROMREGION_ERASE00 )
	ROM_LOAD( "telsndl", 0x0000, 0x080000, CRC(74996fbd) SHA1(90e46130dccf47be1fcfaf549e548cdd4883e59d) )

	DISK_REGION("cdrom")
	DISK_IMAGE_READONLY( "95100301", 0, SHA1(dbce040a6fb7916a240d24e2207cf6e1b3f572e7) )
ROM_END

ROM_START( c3_totp )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "95400101.lo", 0x00001, 0x080000, CRC(c95164c7) SHA1(7b2fada6a3208666219a53cba08f7acad015763d) )
	ROM_LOAD16_BYTE( "95400102.hi", 0x00000, 0x080000, CRC(5ebba159) SHA1(34bcf48140261cd87d81a32581e965d722f42f71) )

	ROM_REGION( 0x100000, "altrevs", 0 )
	ROM_LOAD16_BYTE( "95401101.lo", 0x00001, 0x080000, CRC(97d2d90a) SHA1(d4a2afd3cc551986e76f107beb66e8c660a6ee1d) )
	ROM_LOAD16_BYTE( "95401102.hi", 0x00000, 0x080000, CRC(3599427f) SHA1(16d915553b2b490a047888c64ebcf952714b3168) )

	ROM_REGION( 0x1000000, "ymz280b", ROMREGION_ERASE00 )
	ROM_LOAD( "totpsnd.lhs", 0x000000, 0x080000, CRC(56a73136) SHA1(10656ede18de9432a8a728cc59d000b5b1bf0150) )
	ROM_LOAD( "totpsnd.rhs", 0x080000, 0x080000, CRC(28d156ab) SHA1(ebf5c4e008015b9b56b3aa5228c05b8e298daa80) )

	DISK_REGION("cdrom")
	DISK_IMAGE_READONLY( "95100307", 0, SHA1(27ad1565f9a153fe71b72d9c597a6e3c3f13ded0) )
ROM_END

ROM_START( c3_ppays )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "95400687.hi", 0x00000, 0x080000, CRC(56080e1c) SHA1(49391059b5a758690d4972abad04d7e7aef23423) )
	ROM_LOAD16_BYTE( "95400687.lo", 0x00001, 0x080000, CRC(8b2c9c3d) SHA1(921c900447870f6ae51a4f3baeb60ce94e732291) )

	ROM_REGION( 0x1000000, "ymz280b", ROMREGION_ERASE00 )
	ROM_LOAD( "phrasesn.l", 0x0000, 0x080000, CRC(a436ccf8) SHA1(18c39aa2e68c32242e0de1347b25d4af44b84548) )

	DISK_REGION("cdrom")
	DISK_IMAGE_READONLY( "95100315", 0, SHA1(fc76d3ab5ff38c2dc4f06399f5399a1ae3c136e9) )
ROM_END

} // anonymous namespace

GAMEL( 1995, c3_telly,  0, c3_telly, c3_telly, c3_telly_state, empty_init, ROT0, "BFM", "Telly Addicts (Bellfruit) (Cobra 3)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_NOT_WORKING, layout_c3_telly )
GAMEL( 1995, c3_tellyns,0, c3_telly, c3_telly, c3_telly_state, empty_init, ROT0, "BFM", "Telly Addicts (New Series) (Bellfruit) (Cobra 3)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_NOT_WORKING, layout_c3_telly )
GAME( 1996, c3_rtime,  0, bfm_cobra3, bfm_cobra3, bfm_cobra3_state, empty_init, ROT0, "BFM", "Radio Times (Bellfruit) (Cobra 3)", MACHINE_IMPERFECT_GRAPHICS| MACHINE_NOT_WORKING )
GAME( 1997, c3_totp,   0, bfm_cobra3, bfm_cobra3, bfm_cobra3_state, empty_init, ROT0, "BFM", "Top of the Pops (Bellfruit) (Cobra 3?)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_NOT_WORKING )
GAME( 1998, c3_ppays,  0, bfm_cobra3, bfm_cobra3, bfm_cobra3_state, empty_init, ROT0, "BFM", "The Phrase That Pays (Bellfruit) (Cobra 3?)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_NOT_WORKING )
