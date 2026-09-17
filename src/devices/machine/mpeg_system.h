// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn
/***************************************************************************

    ISO/IEC 11172-1 MPEG-1 systems stream demultiplexer.

***************************************************************************/

#ifndef MAME_MACHINE_MPEG_SYSTEM_H
#define MAME_MACHINE_MPEG_SYSTEM_H

#pragma once

#include <functional>
#include <span>

class device_t;

class mpeg_system
{
public:
	enum class parse_result
	{
		NEED_DATA,
		BLOCKED,
		SYSTEM_END,
		INVALID_DATA
	};

	struct packet_info
	{
		u8 stream_id;
		bool first;
		bool pts_valid;
		bool dts_valid;
		u64 pts;
		u64 dts;
	};

	using payload_handler = std::function<std::size_t (const packet_info &, std::span<const u8>)>;

	mpeg_system() ATTR_COLD;

	// Parse headers and deliver elementary-stream payload without buffering it.
	// The callback receives bytes borrowed from input and returns how many it
	// accepted.  Returning less than offered produces BLOCKED; unaccepted bytes
	// remain with the caller.  Skipped streams can accept the entire span.
	// consumed includes accepted payload and any preceding framing bytes.
	// NEED_DATA accepts all input.  Timestamp metadata remains valid throughout
	// a packet, and first remains true until a nonempty payload is accepted.
	// Timestamps identify the first access unit commencing within the packet.
	// PTS, DTS and SCR use the MPEG 90 kHz clock and wrap at 33 bits.
	parse_result parse(std::span<const u8> input, std::size_t &consumed, const payload_handler &output);

	bool has_clock_reference() const { return m_scr_valid; }
	u64 system_clock_reference() const { return m_scr; }
	u32 mux_rate() const { return m_mux_rate; } // Units of 50 bytes/second.

	void clear() ATTR_COLD;
	void register_save_state(device_t &device, int index = 0) ATTR_COLD;

private:
	enum : u8
	{
		SCAN,
		PACK,
		LENGTH,
		SYSTEM_HEADER,
		SYSTEM_STREAM,
		PACKET_HEADER,
		STD_BUFFER,
		TIMESTAMP,
		PAYLOAD,
		SKIP
	};

	static bool timestamp_valid(const u8 *data, u8 prefix);
	static u64 timestamp(const u8 *data);
	parse_result invalid_packet();

	u8 m_state;
	u32 m_start_code;
	u8 m_stream_id;
	u16 m_remaining;
	u8 m_header[10];
	u8 m_header_size;
	u8 m_header_needed;
	u8 m_stuffing;
	bool m_std_present;
	bool m_first;
	bool m_pts_valid;
	bool m_dts_valid;
	u64 m_pts;
	u64 m_dts;
	bool m_scr_valid;
	u64 m_scr;
	u32 m_mux_rate;
};

#endif // MAME_MACHINE_MPEG_SYSTEM_H
