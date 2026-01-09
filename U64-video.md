# Ultimate 64 / C64 Ultimate Video and Audio Streaming Protocol

This document specifies the network protocol used by the C64 Ultimate and Ultimate 64 devices to stream video and audio over Ethernet. This specification is derived from analysis of the c64stream OBS plugin implementation and the official 1541U documentation.

## Table of Contents

1. [Overview](#overview)
2. [Network Architecture](#network-architecture)
3. [Control Protocol (TCP)](#control-protocol-tcp)
4. [Video Stream Protocol (UDP)](#video-stream-protocol-udp)
5. [Audio Stream Protocol (UDP)](#audio-stream-protocol-udp)
6. [Implementation Guide](#implementation-guide)
7. [Reference Data](#reference-data)

---

## Overview

The C64 Ultimate family of devices (1541 Ultimate II/II+/III and Ultimate 64) can stream VIC-II video output and SID audio output over Ethernet in real-time. The streaming uses:

- **TCP port 64** for control commands (starting/stopping streams)
- **UDP** for data streams (video on port 21000, audio on port 21001 by default)

### Key Characteristics

| Property | Value |
|----------|-------|
| Video Resolution | 384 × 272 (PAL) or 384 × 240 (NTSC) |
| Video Color Depth | 4-bit per pixel (16 VIC-II colors) |
| Video Frame Rate | 50.125 Hz (PAL) or 59.826 Hz (NTSC) |
| Audio Sample Rate | 47,982.887 Hz (PAL) or 47,940.341 Hz (NTSC) |
| Audio Format | 16-bit signed stereo, little-endian |
| Transport | UDP (unicast or multicast) |

---

## Network Architecture

```
┌─────────────────────┐                    ┌─────────────────────┐
│  C64 Ultimate /     │                    │    Receiving        │
│  Ultimate 64        │                    │    Application      │
│                     │                    │                     │
│  ┌───────────────┐  │   TCP port 64      │  ┌───────────────┐  │
│  │ Control Port  │◄─┼────────────────────┼──┤ TCP Client    │  │
│  └───────────────┘  │   (commands)       │  └───────────────┘  │
│                     │                    │                     │
│  ┌───────────────┐  │   UDP port 21000   │  ┌───────────────┐  │
│  │ Video Stream  │──┼────────────────────┼─►│ UDP Receiver  │  │
│  └───────────────┘  │   (video packets)  │  │ (Video)       │  │
│                     │                    │  └───────────────┘  │
│  ┌───────────────┐  │   UDP port 21001   │  ┌───────────────┐  │
│  │ Audio Stream  │──┼────────────────────┼─►│ UDP Receiver  │  │
│  └───────────────┘  │   (audio packets)  │  │ (Audio)       │  │
│                     │                    │  └───────────────┘  │
└─────────────────────┘                    └─────────────────────┘
```

### Default Ports

| Port | Protocol | Purpose |
|------|----------|---------|
| 64 | TCP | Control commands |
| 21000 | UDP | Video stream (configurable) |
| 21001 | UDP | Audio stream (configurable) |

### Multicast Support

The streams can also be sent to multicast addresses in the range `224.0.0.0` to `239.255.255.255`.

---

## Control Protocol (TCP)

Control commands are sent via TCP to port 64 on the C64 Ultimate device. Commands use a binary format with little-endian byte ordering.

### Connection Setup

1. Open a TCP connection to the C64 Ultimate's IP address on port 64
2. Send the appropriate command bytes
3. Close the connection (no response is expected)

### Command Format

Commands have the following structure:

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 byte | Command code (see below) |
| 1 | 1 byte | Always `0xFF` |
| 2 | 2 bytes | Parameter length (little-endian) |
| 4 | varies | Parameters (if any) |

### Stream IDs

| ID | Stream Type |
|----|-------------|
| 0 | VIC Video |
| 1 | Audio (SID) |

### Start Stream Command

To start a stream, send command code `0x20 + stream_id`:

| Offset | Size | Value/Description |
|--------|------|-------------------|
| 0 | 1 byte | `0x20` (video) or `0x21` (audio) |
| 1 | 1 byte | `0xFF` |
| 2 | 1 byte | Parameter length: `2 + len(destination)` |
| 3 | 1 byte | `0x00` |
| 4 | 2 bytes | Duration: `0x0000` = infinite |
| 6 | varies | Destination as ASCII string: `IP:PORT` |

**Example: Start video stream to 192.168.1.100:21000 (infinite duration)**

```
Command bytes (hex):
20 FF 15 00 00 00 31 39 32 2E 31 36 38 2E 31 2E 31 30 30 3A 32 31 30 30 30

Breakdown:
20          - Start video stream (0x20 + stream_id 0)
FF          - Command marker
15 00       - Parameter length: 21 bytes (2 + 19 chars for "192.168.1.100:21000")
00 00       - Duration: 0 = infinite
31 39 32 ... - "192.168.1.100:21000" in ASCII
```

### Stop Stream Command

To stop a stream, send command code `0x30 + stream_id`:

| Offset | Size | Value/Description |
|--------|------|-------------------|
| 0 | 1 byte | `0x30` (video) or `0x31` (audio) |
| 1 | 1 byte | `0xFF` |
| 2 | 2 bytes | `0x00 0x00` (no parameters) |

**Example: Stop video stream**

```
Command bytes (hex):
30 FF 00 00

Breakdown:
30          - Stop video stream (0x30 + stream_id 0)
FF          - Command marker
00 00       - No parameters
```

### Duration Parameter

If you want a timed stream instead of infinite:

| Value | Meaning |
|-------|---------|
| 0 | Infinite (stream until stopped) |
| N | Stream for N × 5ms ticks |

Example: Duration = 200 (0x00C8) = 200 × 5ms = 1 second

---

## Video Stream Protocol (UDP)

### Packet Structure

Each video packet is exactly **780 bytes** (12-byte header + 768-byte payload).

#### Header Format (12 bytes)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0 | 2 bytes | uint16_t LE | Sequence number |
| 2 | 2 bytes | uint16_t LE | Frame number |
| 4 | 2 bytes | uint16_t LE | Line number (bit 15 = last packet flag) |
| 6 | 2 bytes | uint16_t LE | Pixels per line (always 384) |
| 8 | 1 byte | uint8_t | Lines per packet (always 4) |
| 9 | 1 byte | uint8_t | Bits per pixel (always 4) |
| 10 | 2 bytes | uint16_t LE | Encoding type (0 = uncompressed) |

**Line Number Field Details:**
- Bits 0-14: Actual line number (0-based)
- Bit 15: Last packet flag (1 = this is the last packet of the frame)

#### Payload Format (768 bytes)

The payload contains 4 lines of video data, with each line being 192 bytes (384 pixels at 4 bits per pixel).

```
Payload layout:
┌────────────────────────────────────────────────────────────────┐
│ Line 0: 192 bytes (384 pixels × 4 bits ÷ 8)                   │
├────────────────────────────────────────────────────────────────┤
│ Line 1: 192 bytes                                             │
├────────────────────────────────────────────────────────────────┤
│ Line 2: 192 bytes                                             │
├────────────────────────────────────────────────────────────────┤
│ Line 3: 192 bytes                                             │
└────────────────────────────────────────────────────────────────┘
Total: 768 bytes
```

#### Pixel Format

Each byte contains two 4-bit VIC-II color values:

```
Byte layout:
┌───────┬───────┐
│ Bit 0-3 │ Bit 4-7 │
│ Pixel 1 │ Pixel 2 │
│ (low)   │ (high)  │
└───────┴───────┘
```

To decode:
```c
uint8_t byte = payload[offset];
uint8_t pixel1_color = byte & 0x0F;        // Lower 4 bits
uint8_t pixel2_color = (byte >> 4) & 0x0F; // Upper 4 bits
```

### Frame Assembly

Frames are sent as multiple packets that must be reassembled:

| Format | Resolution | Packets per Frame | Total Lines |
|--------|------------|-------------------|-------------|
| PAL | 384 × 272 | 68 packets | 272 lines |
| NTSC | 384 × 240 | 60 packets | 240 lines |

**Frame Assembly Algorithm:**

1. Create a frame buffer sized for the maximum resolution (384 × 272 pixels)
2. For each received packet:
   a. Parse the header to get frame_num, line_num, and last_packet flag
   b. If frame_num differs from current frame, complete previous frame and start new one
   c. Extract line_num from header (mask off bit 15)
   d. Copy payload lines to frame buffer at correct position
   e. Track which packets have been received (for handling packet loss)
3. When last_packet flag is set, mark expected_packets count
4. Frame is complete when received_packets == expected_packets

**Detecting PAL vs NTSC:**

The video format can be detected from the last packet of a frame:
- If final line_num + lines_per_packet = 272 → PAL
- If final line_num + lines_per_packet = 240 → NTSC

### Frame Timing

| Format | Frame Rate | Frame Interval |
|--------|------------|----------------|
| PAL | 50.125 Hz | 19.950125 ms (19,950,125 ns) |
| NTSC | 59.826 Hz | 16.715141 ms (16,715,141 ns) |

### Data Rate Calculations

**PAL:**
- Packets/frame: 68
- Frame rate: 50.125 Hz
- Packet rate: 68 × 50.125 = 3,408.5 packets/sec
- Data rate: 3,408.5 × 780 bytes = 2.66 MB/sec = **21.3 Mbps**

**NTSC:**
- Packets/frame: 60
- Frame rate: 59.826 Hz
- Packet rate: 60 × 59.826 = 3,589.6 packets/sec
- Data rate: 3,589.6 × 780 bytes = 2.80 MB/sec = **22.4 Mbps**

---

## Audio Stream Protocol (UDP)

### Packet Structure

Each audio packet is exactly **770 bytes** (2-byte header + 768-byte payload).

#### Header Format (2 bytes)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0 | 2 bytes | uint16_t LE | Sequence number |

#### Payload Format (768 bytes)

The payload contains 192 stereo samples in interleaved format:

```
Sample layout:
┌─────────────────┬─────────────────┬─────────────────┬─────────────────┬─────┐
│ Sample 0        │ Sample 1        │ Sample 2        │ Sample 3        │ ... │
│ L(16) | R(16)   │ L(16) | R(16)   │ L(16) | R(16)   │ L(16) | R(16)   │     │
│ 4 bytes         │ 4 bytes         │ 4 bytes         │ 4 bytes         │     │
└─────────────────┴─────────────────┴─────────────────┴─────────────────┴─────┘
                                                                    192 samples
                                                                    768 bytes total
```

Each sample is:
- 16-bit signed integer, little-endian
- Stereo: Left channel first, then Right channel
- 4 bytes per stereo sample pair

### Sample Rate

The audio sample rates are derived from the hardware clock and are NOT exactly 48 kHz:

| Format | Exact Sample Rate | Deviation from 48 kHz |
|--------|------------------|----------------------|
| PAL | 47,982.8869047619 Hz | -356.52 ppm |
| NTSC | 47,940.3408482143 Hz | -1,242.9 ppm |

**Clock Derivation:**
- PAL: `(4433618.75 Hz × 16/9 × 15/77/32) = 47982.8869047619 Hz`
- NTSC: `(3579545.45 Hz × 16/7 × 15/80/32) = 47940.3408482143 Hz`

### Packet Timing

| Format | Packet Interval |
|--------|-----------------|
| PAL | 4.001417 ms (192 samples ÷ 47,982.887 Hz) |
| NTSC | 4.005006 ms (192 samples ÷ 47,940.341 Hz) |

Packet rate: approximately 250 packets/second

### Data Rate

- Packet size: 770 bytes
- Packet rate: ~250/sec
- Data rate: 770 × 250 = 192.5 KB/sec = **1.54 Mbps**

---

## Implementation Guide

### Recommended Implementation Steps

1. **Network Setup**
   - Create UDP sockets for video (port 21000) and audio (port 21001)
   - Set socket receive buffer large (1-2 MB) to handle packet bursts
   - Set sockets to non-blocking mode

2. **Start Streaming**
   - Open TCP connection to C64 Ultimate port 64
   - Send start command for video stream (to your IP:21000)
   - Send start command for audio stream (to your IP:21001)
   - Close TCP connection

3. **Receive and Process Video**
   - Receive UDP packets on video port
   - Parse 12-byte header
   - Reassemble frames from packets
   - Convert 4-bit VIC colors to your display format
   - Display/encode frames

4. **Receive and Process Audio**
   - Receive UDP packets on audio port
   - Skip 2-byte sequence number header
   - Extract 192 stereo samples (768 bytes)
   - Resample from ~48 kHz if needed for your audio system
   - Play/encode audio

5. **Stopping**
   - Open TCP connection to C64 Ultimate port 64
   - Send stop commands for both streams
   - Close sockets

### Handling Packet Loss

UDP does not guarantee delivery. Recommended strategies:

1. **For Video:**
   - Track sequence numbers to detect gaps
   - If a packet is missing, duplicate the previous line(s) as interpolation
   - Set reasonable frame timeout (100ms) to not wait forever for lost packets
   - Move on to next frame if current frame is incomplete after timeout

2. **For Audio:**
   - Track sequence numbers
   - If packets are missing, insert silence or repeat previous samples
   - Use a jitter buffer (10-50ms) to smooth out network timing variations

### Performance Considerations

1. **Buffer Sizes:**
   - UDP receive buffer: 1-2 MB minimum
   - Frame buffer: 384 × 272 × 4 bytes = ~418 KB (for RGBA output)
   - Audio buffer: Depends on latency requirements

2. **Threading:**
   - Separate threads for video and audio reception recommended
   - Processing can be done in receiver threads or separate processor threads

3. **Timing:**
   - Video packets arrive at ~290-340 µs intervals
   - Audio packets arrive at ~4 ms intervals
   - Use non-blocking I/O to avoid missing packets

---

## Reference Data

### VIC-II Color Palette

The standard VIC-II palette (default values, can be customized):

| Index | Color Name | RGB Hex | RGB Values |
|-------|-----------|---------|------------|
| 0 | Black | #000000 | (0, 0, 0) |
| 1 | White | #FFFFFF | (255, 255, 255) |
| 2 | Red | #9F4E44 | (159, 78, 68) |
| 3 | Cyan | #6ABFC6 | (106, 191, 198) |
| 4 | Purple | #A057A3 | (160, 87, 163) |
| 5 | Green | #5CAB5E | (92, 171, 94) |
| 6 | Blue | #50459B | (80, 69, 155) |
| 7 | Yellow | #C9D487 | (201, 212, 135) |
| 8 | Orange | #A1683C | (161, 104, 60) |
| 9 | Brown | #6D5412 | (109, 84, 18) |
| 10 | Light Red | #CB7E75 | (203, 126, 117) |
| 11 | Dark Grey | #626262 | (98, 98, 98) |
| 12 | Medium Grey | #898989 | (137, 137, 137) |
| 13 | Light Green | #9AE29B | (154, 226, 155) |
| 14 | Light Blue | #887ECB | (136, 126, 203) |
| 15 | Light Grey | #ADADAD | (173, 173, 173) |

### Screen Border Dimensions

The full streamed resolution includes borders around the 320×200 active screen area:

| Format | Full Resolution | Left Border | Right Border | Top Border | Bottom Border | Screen |
|--------|-----------------|-------------|--------------|------------|---------------|--------|
| PAL | 384 × 272 | 32 px | 32 px | 35 px | 37 px | 320 × 200 |
| NTSC | 384 × 240 | 32 px | 32 px | 20 px | 20 px | 320 × 200 |

### Timing Summary

| Parameter | PAL | NTSC |
|-----------|-----|------|
| Frame Rate | 50.125 Hz | 59.826 Hz |
| Frame Interval | 19.950125 ms | 16.715141 ms |
| Video Packets/Frame | 68 | 60 |
| Video Packet Interval | ~293 µs | ~279 µs |
| Video Data Rate | 21.3 Mbps | 22.4 Mbps |
| Audio Sample Rate | 47,982.887 Hz | 47,940.341 Hz |
| Audio Packet Interval | 4.001 ms | 4.005 ms |
| Audio Data Rate | 1.54 Mbps | 1.54 Mbps |
| **Total Data Rate** | **22.8 Mbps** | **23.9 Mbps** |

### Protocol Constants Summary

```
Control Port:           64 (TCP)
Default Video Port:     21000 (UDP)
Default Audio Port:     21001 (UDP)

Video Packet Size:      780 bytes
Video Header Size:      12 bytes
Video Payload Size:     768 bytes
Pixels Per Line:        384
Lines Per Packet:       4
Bits Per Pixel:         4

Audio Packet Size:      770 bytes
Audio Header Size:      2 bytes
Audio Payload Size:     768 bytes
Samples Per Packet:     192 (stereo)
Bytes Per Sample:       4 (16-bit L + 16-bit R)
```

---

## References

- [1541U Documentation - Data Streams](https://1541u-documentation.readthedocs.io/en/latest/data_streams.html)
- [c64stream OBS Plugin](https://github.com/chrisgleissner/c64stream) - Reference implementation
- VIC-II chip documentation for color palette specifications
