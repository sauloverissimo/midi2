# Stress test results, rp2350-device-freertos USB MIDI 2.0 loopback

Validation of the `STRESS_LOOPBACK` build on the Raspberry Pi Pico 2 (RP2350):
every inbound UMP is echoed verbatim through the FreeRTOS static-queue pipeline
(usb_task owns TinyUSB, midi_task echoes from `rx_queue` to `tx_queue`). The
harness `stress_ump.py` stamps a sequence number in word 1, reads the echo back,
and checks for gaps, reordering, and throughput.

## Build and run

```bash
cmake -B build-stress -DPICO_BOARD=pico2 -DSTRESS_LOOPBACK=ON \
      -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
cmake --build build-stress -j
# flash build-stress/rp2350-device-freertos.uf2 in BOOTSEL, then:
sudo python3 stress/stress_ump.py --device /dev/snd/umpC?D0 --count 25000
```

## Burst budget

Zero-loss holds for a sustained rate at or below USB TX throughput, with the
pipeline absorbing bursts up to the queue depth. The two knobs that set the
ceiling:

- `PIPE_QUEUE_DEPTH` = 64 messages (src/pipeline.c), for `rx_queue` and `tx_queue`
- `CFG_TUD_MIDI2_RX_BUFSIZE` / `CFG_TUD_MIDI2_TX_BUFSIZE` = 256 bytes (src/tusb_config.h)

Above the sustained ceiling the counted-drop policy engages (`pipeline_rx_drops()`
/ `pipeline_tx_drops()`), it does not crash or silently lose.

## Results

Measured on a Raspberry Pi Pico 2 (RP2350), Linux 6.x with the `snd-usb-midi2`
driver, UMP rawmidi `/dev/snd/umpC3D0`, queue depth 64 + endpoint FIFO 256 B.

| Profile | Rate | Volume | Result | Throughput |
|---|---|---|---|---|
| Low load sanity | 1 k UMP/s | 5 x 5 000 = 25 000 | **5/5 PASS**, zero loss | 999 UMP/s |
| Sustained moderate | 2 k UMP/s | 3 x 10 000 = 30 000 | 2/3 PASS (1 run: 2 miss, 2 reorder) | 1 995 UMP/s |
| Realistic DAW | 10 k UMP/s | 10 x 5 000 = 50 000 | 7/10 PASS (drops of 1..5 on 3 runs) | ~9 800 UMP/s on passing runs |
| Burst unrestricted | no cap | 3 x 5 000 | 0/3 PASS (256..600 counted drops) | flood exceeds the burst budget |

## Reading the results

- **Zero-loss ceiling is ~1 k UMP/s sustained** with the current sizing. This is
  the granularity of usb_task's service loop: it yields `vTaskDelay(1)` (~1 ms)
  to avoid starving midi_task on the single core (see the firmware's usb_task
  comment and the design note), so it services USB about once per millisecond.
  At 1 UMP/ms this is a clean 1:1 and loss is zero.
- Above the ceiling, sends outpace the 1 ms service cycle, the queues and FIFO
  fill, and the **counted-drop** policy engages (`pipeline_rx_drops()` /
  `pipeline_tx_drops()`). No crash, no silent corruption: 10 k/s loses a handful
  per run, an unrestricted flood loses more. Ordering is preserved except under
  overflow (the 2 k profile showed 2 reordered packets in one run).
- To raise the ceiling, increase `PIPE_QUEUE_DEPTH` and the
  `CFG_TUD_MIDI2_*_BUFSIZE` FIFO sizes together, and/or shorten the service
  cadence. For real musical traffic (well under 1 k UMP/s) the default sizing is
  comfortably lossless.
