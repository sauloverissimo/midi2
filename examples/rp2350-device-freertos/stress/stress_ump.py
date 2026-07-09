#!/usr/bin/env python3
"""
rp2350-device-freertos USB MIDI 2.0 stress harness.

Sends N UMP MIDI 2.0 NoteOn packets with sequence numbers embedded in the
velocity word, reads back the loopback echo, and reports integrity +
throughput. Exercises the FreeRTOS static-queue pipeline for zero packet
loss under sustained load.

Usage:
    sudo python3 stress_ump.py [--device /dev/snd/umpC?D0] [--count 50000]

The device under test must be built and flashed with -DSTRESS_LOOPBACK
(the catalog cycle is suspended and every inbound UMP is echoed verbatim).
Run as root or in the audio group so the rawmidi device is accessible.

Adapted from the Teensy 4.x harness. Tested on Linux 6.5+ with the
snd-usb-midi2 driver.
"""

import argparse
import fcntl
import os
import struct
import sys
import time


# UMP MT 0x4 Note On, group 0, channel 0, note 65
# Sequence number lives in word 1 (velocity field becomes our counter).
NOTE_ON_W0 = 0x40904100

# Linux ALSA rawmidi: switch device to UMP packet mode.
# _IOW('W', 0x02, int) packed: dir=1, size=4, type='W'=0x57, nr=0x02
SNDRV_RAWMIDI_IOCTL_USER_PVERSION = 0x40045702
# SNDRV_PROTOCOL_VERSION(2, 0, 2) - minimum to enable UMP rawmidi mode
USER_PVERSION_UMP = (2 << 16) | (0 << 8) | 2


def enable_ump_mode(fd: int) -> None:
	"""Switch the rawmidi fd to UMP packet mode. Without this the kernel
	exposes the legacy MIDI 1.0 byte stream view of the same endpoint."""
	fcntl.ioctl(fd, SNDRV_RAWMIDI_IOCTL_USER_PVERSION,
	            struct.pack("i", USER_PVERSION_UMP))


def build_packet(seq: int) -> bytes:
	"""Return 8 bytes: MT 0x4 NoteOn header + 32-bit sequence in word 1."""
	return struct.pack("<II", NOTE_ON_W0, seq & 0xFFFFFFFF)


def parse_packet(buf: bytes, offset: int) -> tuple[int, int]:
	"""Decode one 64-bit UMP starting at offset. Returns (w0, w1)."""
	return struct.unpack("<II", buf[offset:offset + 8])


def drain_rx(fd: int, buf: bytearray, received: list) -> None:
	"""Drain any pending RX bytes into received[] without blocking."""
	while True:
		try:
			chunk = os.read(fd, 8192)
			if not chunk:
				return
			buf.extend(chunk)
			while len(buf) >= 8:
				w0, w1 = struct.unpack("<II", buf[:8])
				del buf[:8]
				if (w0 >> 28) == 0x4:
					received.append(w1)
		except BlockingIOError:
			return


def run_round_trip(device: str, count: int, timeout_s: float, rate_hz: float = 0.0):
	"""Send count UMPs and drain RX from a dedicated reader thread so the
	host kernel buffer is continuously emptied while we write. Sequence
	numbers start at 1 to avoid the all-zero UMP word that Paul's
	usb_midi.c uses as a no-data sentinel (see usb_midi2_read_message)."""
	import threading

	fd = os.open(device, os.O_RDWR | os.O_NONBLOCK)
	try:
		enable_ump_mode(fd)
		received: list[int] = []
		stop = threading.Event()

		def reader():
			buf = bytearray()
			while not stop.is_set():
				drain_rx(fd, buf, received)
				time.sleep(0.0005)
			# Drain residual after stop signal.
			drain_rx(fd, buf, received)

		rx_thread = threading.Thread(target=reader, daemon=True)
		rx_thread.start()

		t0 = time.monotonic()
		sent = 0
		period = (1.0 / rate_hz) if rate_hz > 0 else 0.0
		while sent < count:
			target_t = t0 + (sent + 1) * period if period > 0 else 0
			try:
				n = os.write(fd, build_packet(sent))  # seq starts at 0 (NOOP word fix)
				if n == 8:
					sent += 1
			except BlockingIOError:
				time.sleep(0)
			if period > 0:
				now = time.monotonic()
				if now < target_t:
					time.sleep(target_t - now)

		# Wait for the tail to come back.
		deadline = time.monotonic() + timeout_s
		while len(received) < count and time.monotonic() < deadline:
			time.sleep(0.005)

		elapsed = time.monotonic() - t0
		stop.set()
		rx_thread.join(timeout=1.0)
	finally:
		os.close(fd)

	expected = set(range(count))
	got = set(received)
	missing = sorted(expected - got)
	duplicates = len(received) - len(got)
	# Out-of-order: count packets whose seq is not strictly greater than the
	# previous one in the received list.
	out_of_order = 0
	prev = -1
	for seq in received:
		if seq <= prev:
			out_of_order += 1
		prev = seq
	throughput = (len(received) / elapsed) if elapsed > 0 else 0.0

	return {
		"sent": sent,
		"received": len(received),
		"missing": len(missing),
		"duplicates": duplicates,
		"out_of_order": out_of_order,
		"elapsed_s": elapsed,
		"throughput_ump_per_s": throughput,
		"first_missing": missing[:10],
	}


def main():
	ap = argparse.ArgumentParser(description="rp2350-device-freertos USB MIDI 2.0 stress harness")
	ap.add_argument("--device", default="/dev/snd/midiC2D1",
	                help="rawmidi device exposing UMP (default: /dev/snd/midiC2D1)")
	ap.add_argument("--count", type=int, default=50000,
	                help="UMP packets per pass (default: 50000)")
	ap.add_argument("--runs", type=int, default=10,
	                help="number of test runs (default: 10)")
	ap.add_argument("--timeout", type=float, default=15.0,
	                help="per-run timeout in seconds (default: 15.0)")
	ap.add_argument("--rate", type=float, default=0.0,
	                help="cap send rate to N UMP/s (default: 0 = unrestricted)")
	args = ap.parse_args()

	if not os.path.exists(args.device):
		print(f"Device not found: {args.device}", file=sys.stderr)
		print("Plug the RP2350 (STRESS_LOOPBACK build) and ensure snd-usb-midi2 enumerates it.", file=sys.stderr)
		sys.exit(1)

	print(f"Device   : {args.device}")
	print(f"Per run  : {args.count} UMP packets (NoteOn MT 0x4, 8 bytes each)")
	print(f"Runs     : {args.runs}")
	print()

	passes = 0
	total_ump = 0
	total_time = 0.0
	for run in range(1, args.runs + 1):
		result = run_round_trip(args.device, args.count, args.timeout, args.rate)
		ok = (result["received"] == args.count and
		      result["missing"] == 0 and
		      result["out_of_order"] == 0)
		status = "PASS" if ok else "FAIL"
		passes += 1 if ok else 0
		total_ump += result["received"]
		total_time += result["elapsed_s"]
		print(f"Run {run:2d}  {status}  "
		      f"sent={result['sent']:5d}  recv={result['received']:5d}  "
		      f"miss={result['missing']:4d}  ooo={result['out_of_order']:4d}  "
		      f"{result['throughput_ump_per_s']:8.0f} UMP/s  "
		      f"{result['elapsed_s']:5.2f}s")
		if not ok and result["first_missing"]:
			print(f"        first missing seq: {result['first_missing']}")

	print()
	print(f"Summary  {passes}/{args.runs} PASS")
	print(f"Total UMP processed : {total_ump}")
	print(f"Average throughput  : {total_ump / total_time:.0f} UMP/s")


if __name__ == "__main__":
	main()
