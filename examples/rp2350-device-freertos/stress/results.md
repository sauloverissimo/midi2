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

| Profile | Rate | Volume | Result | Throughput |
|---|---|---|---|---|
| Low load sanity | 1 k UMP/s | 25 000 | pending hardware | - |
| Realistic DAW | 10 k UMP/s | 50 000 | pending hardware | - |
| Burst | unrestricted | 5 000 | pending hardware | - |

> Results pending: run the profiles above on the physical RP2350 and paste the
> measured throughput, drop counts, and the queue/FIFO sizes that achieved the
> zero-loss ceiling.
