// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn
/***************************************************************************

    ISO/IEC 11172-1 MPEG-1 systems stream demultiplexer.

    Pack and packet syntax follows ISO/IEC 11172-1:1993, section 2.4.3.
    Buffering, stream selection and presentation timing belong to the device
    using this parser.  MPEG-2 program and transport streams are not supported.

***************************************************************************/

#include "emu.h"
#include "mpeg_system.h"


mpeg_system::mpeg_system()
{
	clear();
}

void mpeg_system::clear()
{
	m_state = SCAN;
	m_start_code = 0xffffffff;
	m_stream_id = 0;
	m_remaining = 0;
	std::fill(std::begin(m_header), std::end(m_header), 0);
	m_header_size = 0;
	m_header_needed = 0;
	m_stuffing = 0;
	m_std_present = false;
	m_first = false;
	m_pts_valid = false;
	m_dts_valid = false;
	m_pts = 0;
	m_dts = 0;
	m_scr_valid = false;
	m_scr = 0;
	m_mux_rate = 0;
}

void mpeg_system::register_save_state(device_t &device, int index)
{
	device.save_item(m_state, "mpeg_system.state", index);
	device.save_item(m_start_code, "mpeg_system.start_code", index);
	device.save_item(m_stream_id, "mpeg_system.stream_id", index);
	device.save_item(m_remaining, "mpeg_system.remaining", index);
	device.save_item(m_header, "mpeg_system.header", index);
	device.save_item(m_header_size, "mpeg_system.header_size", index);
	device.save_item(m_header_needed, "mpeg_system.header_needed", index);
	device.save_item(m_stuffing, "mpeg_system.stuffing", index);
	device.save_item(m_std_present, "mpeg_system.std_present", index);
	device.save_item(m_first, "mpeg_system.first", index);
	device.save_item(m_pts_valid, "mpeg_system.pts_valid", index);
	device.save_item(m_dts_valid, "mpeg_system.dts_valid", index);
	device.save_item(m_pts, "mpeg_system.pts", index);
	device.save_item(m_dts, "mpeg_system.dts", index);
	device.save_item(m_scr_valid, "mpeg_system.scr_valid", index);
	device.save_item(m_scr, "mpeg_system.scr", index);
	device.save_item(m_mux_rate, "mpeg_system.mux_rate", index);
}

bool mpeg_system::timestamp_valid(const u8 *data, u8 prefix)
{
	return (data[0] & 0xf1) == (prefix << 4 | 1) && BIT(data[2], 0) && BIT(data[4], 0);
}

u64 mpeg_system::timestamp(const u8 *data)
{
	return (u64(data[0] & 0x0e) << 29) | (u64(data[1]) << 22) |
			(u64(data[2] & 0xfe) << 14) | (u64(data[3]) << 7) | (data[4] >> 1);
}

mpeg_system::parse_result mpeg_system::invalid_packet()
{
	// Once a packet length is known, skip its remainder rather than interpreting
	// elementary-stream start codes in damaged payload as system framing.
	m_state = m_remaining ? SKIP : SCAN;
	m_start_code = 0xffffffff;
	return parse_result::INVALID_DATA;
}

mpeg_system::parse_result mpeg_system::parse(std::span<const u8> input, std::size_t &consumed, const payload_handler &output)
{
	consumed = 0;
	while (consumed < input.size())
	{
		if (m_state == PAYLOAD)
		{
			const auto payload = input.subspan(consumed, std::min<std::size_t>(m_remaining, input.size() - consumed));
			const packet_info info{ m_stream_id, m_first, m_pts_valid, m_dts_valid, m_pts, m_dts };
			const std::size_t accepted = output(info, payload);
			assert(accepted <= payload.size());
			const std::size_t count = std::min(accepted, payload.size());
			consumed += count;
			m_remaining -= count;
			if (count)
				m_first = false;
			if (!m_remaining)
				m_state = SCAN;
			if (count != payload.size())
				return parse_result::BLOCKED;
			continue;
		}
		if (m_state == SKIP)
		{
			const std::size_t count = std::min<std::size_t>(m_remaining, input.size() - consumed);
			consumed += count;
			m_remaining -= count;
			if (!m_remaining)
				m_state = SCAN;
			continue;
		}

		const u8 data = input[consumed++];
		switch (m_state)
		{
		case SCAN:
			m_start_code = (m_start_code << 8) | data;
			if ((m_start_code & 0xffffff00) != 0x00000100 || data < 0xb9)
				break;
			m_start_code = 0xffffffff;
			m_stream_id = data;
			m_header_size = 0;
			if (data == 0xb9)
				return parse_result::SYSTEM_END;
			m_state = data == 0xba ? PACK : LENGTH;
			break;

		case PACK:
			m_header[m_header_size++] = data;
			if (m_header_size == 8)
			{
				m_state = SCAN;
				if (!timestamp_valid(m_header, 2) || !BIT(m_header[5], 7) || !BIT(m_header[7], 0))
					return parse_result::INVALID_DATA;
				m_scr = timestamp(m_header);
				m_scr_valid = true;
				m_mux_rate = (u32(m_header[5] & 0x7f) << 15) | (u32(m_header[6]) << 7) | (m_header[7] >> 1);
			}
			break;

		case LENGTH:
			m_header[m_header_size++] = data;
			if (m_header_size == 2)
			{
				m_remaining = (u16(m_header[0]) << 8) | m_header[1];
				m_header_size = 0;
				if (m_stream_id == 0xbb)
				{
					if (m_remaining < 6 || (m_remaining - 6) % 3)
						return invalid_packet();
					m_state = SYSTEM_HEADER;
				}
				else
				{
					m_first = true;
					m_pts_valid = false;
					m_dts_valid = false;
					m_pts = 0;
					m_dts = 0;
					m_stuffing = 0;
					m_std_present = false;
					if (m_stream_id == 0xbf)
						m_state = m_remaining ? PAYLOAD : SCAN;
					else if (!m_remaining)
						return invalid_packet();
					else
						m_state = PACKET_HEADER;
				}
			}
			break;

		case SYSTEM_HEADER:
			--m_remaining;
			m_header[m_header_size++] = data;
			if (m_header_size == 6)
			{
				if (!BIT(m_header[0], 7) || !BIT(m_header[2], 0) || !BIT(m_header[4], 5) || m_header[5] != 0xff)
					return invalid_packet();
				m_header_size = 0;
				m_state = m_remaining ? SYSTEM_STREAM : SCAN;
			}
			break;

		case SYSTEM_STREAM:
			--m_remaining;
			m_header[m_header_size++] = data;
			if (m_header_size == 3)
			{
				if (!BIT(m_header[0], 7) || (m_header[1] & 0xc0) != 0xc0)
					return invalid_packet();
				m_header_size = 0;
				if (!m_remaining)
					m_state = SCAN;
			}
			break;

		case PACKET_HEADER:
			--m_remaining;
			if (data == 0xff && !m_std_present)
			{
				if (++m_stuffing > 16 || !m_remaining)
					return invalid_packet();
			}
			else if ((data & 0xc0) == 0x40 && !m_std_present)
			{
				if (!m_remaining)
					return invalid_packet();
				m_std_present = true;
				m_state = STD_BUFFER;
			}
			else if ((data & 0xf0) == 0x20 || (data & 0xf0) == 0x30)
			{
				m_header[0] = data;
				m_header_size = 1;
				m_header_needed = BIT(data, 4) ? 10 : 5;
				if (m_remaining < m_header_needed - 1)
					return invalid_packet();
				m_state = TIMESTAMP;
			}
			else if (data == 0x0f)
				m_state = m_remaining ? PAYLOAD : SCAN;
			else
				return invalid_packet();
			break;

		case STD_BUFFER:
			if (!--m_remaining)
				return invalid_packet();
			m_state = PACKET_HEADER;
			break;

		case TIMESTAMP:
			--m_remaining;
			m_header[m_header_size++] = data;
			if (m_header_size == m_header_needed)
			{
				const bool has_dts = m_header_needed == 10;
				if (!timestamp_valid(m_header, has_dts ? 3 : 2) || (has_dts && !timestamp_valid(m_header + 5, 1)))
					return invalid_packet();
				m_pts_valid = true;
				m_dts_valid = has_dts;
				m_pts = timestamp(m_header);
				m_dts = has_dts ? timestamp(m_header + 5) : 0;
				m_state = m_remaining ? PAYLOAD : SCAN;
			}
			break;
		}
	}
	return parse_result::NEED_DATA;
}
