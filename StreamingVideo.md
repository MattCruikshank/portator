# Streaming Video in Portator

## Context

Portator needs to stream framebuffer output from guest apps to a browser for graphical/interactive applications. The guest app writes pixels via `PORTATOR_SYS_PRESENT` (syscall 0x7000), and the host must deliver those frames to a connected browser with low latency.

CivetWeb is already compiled with WebSocket support (`-DUSE_WEBSOCKET`), and the syscall interface for `PRESENT` and `POLL` is already defined in `portator.h`.

## Alternatives Evaluated

### 1. JPEG-over-WebSocket

Guest calls `PRESENT` with a framebuffer pointer and dimensions. The host encodes the framebuffer to JPEG using [stb_image_write.h](https://github.com/nothings/stb/blob/master/stb_image_write.h) (single public-domain C header), then sends the binary JPEG over a WebSocket connection via `mg_websocket_write()`. The browser decodes the JPEG and draws it to a `<canvas>`. Input events flow back over the same WebSocket.

```
Guest App                  Portator Host              Browser
   |                            |                        |
   |--PRESENT(buf,w,h)-------->|                        |
   |                            |--JPEG encode (stb)--->|
   |                            |--WS binary frame----->|
   |                            |                        |--canvas draw
   |                            |<--WS input event------|
   |<--POLL(event)-------------|                        |
```

| Metric | Value |
|---|---|
| Complexity | Low |
| New dependencies | `stb_image_write.h` (1 file, public domain, pure C) |
| Latency | 30-60ms on LAN |
| Bandwidth | ~1 MB/s at 640x480 quality 70 @ 30fps |
| Audio | Not included (can be added separately) |

**Pros:**
- Zero new library dependencies beyond a single header
- Builds on CivetWeb WebSocket support already compiled into Portator
- Every frame is independently decodable (no inter-frame state)
- Works behind firewalls, proxies, and NATs (standard HTTP upgrade)
- Browser JPEG decoding is highly optimized and sub-millisecond

**Cons:**
- Higher latency than UDP-based approaches (TCP head-of-line blocking)
- JPEG is intraframe only, so higher bandwidth than inter-frame codecs (H.264/VP8)
- Not suitable for high-resolution WAN streaming without significant bandwidth

### 2. MJPEG-over-HTTP (Multipart)

Classic approach used by webcams. The server sends an endless HTTP response with `Content-Type: multipart/x-mixed-replace`, each part being a JPEG frame. Can be displayed in a raw `<img>` tag with no JavaScript.

| Metric | Value |
|---|---|
| Complexity | Low |
| Latency | 30-60ms |
| Bandwidth | ~1 MB/s at 640x480 @ 30fps |

**Pros:** Simplest possible implementation; works in an `<img>` tag.

**Cons:** Unidirectional (server-to-browser only) -- still need a separate channel for input events. Some browsers handle multipart streams poorly. No backpressure or quality adaptation.

**Verdict:** Inferior to WebSocket for interactive applications due to the unidirectional limitation.

### 3. MPEG1-over-WebSocket (JSMpeg)

[JSMpeg](https://github.com/phoboslab/jsmpeg) is a JavaScript MPEG-TS demuxer + MPEG1 video + MP2 audio decoder. The server encodes MPEG1 transport stream chunks and sends them over WebSocket. Inter-frame compression gives much better bandwidth than JPEG.

| Metric | Value |
|---|---|
| Complexity | Medium |
| Latency | ~50ms |
| Bandwidth | ~300 KB/s at 640x480 @ 30fps |

**Pros:** Inter-frame compression; includes audio via MP2; mature JS decoder.

**Cons:** Requires an MPEG1 encoder on the server side -- no single-header C library exists for this. MPEG1 is an ancient codec with poor quality-per-bit. The JS decoder uses more CPU than native browser JPEG decoding.

**Verdict:** Interesting for audio+video but the server-side encoding complexity is a barrier.

### 4. Raw Pixel Streaming

Send uncompressed RGBA pixels over WebSocket, render directly to canvas via `ImageData`.

| Metric | Value |
|---|---|
| Complexity | Trivial |
| Latency | ~5ms (no encode time) |
| Bandwidth | ~35 MB/s at 640x480 @ 30fps |

**Verdict:** Only viable for very small framebuffers (e.g., 160x120 retro-style apps) or very low framerates.

### 5. libdatachannel (WebRTC)

[libdatachannel](https://github.com/paullouisageneau/libdatachannel) is a C++17 library with C bindings implementing WebRTC data channels, optional media transport (SRTP), WebSockets, and ICE/STUN/TURN.

| Metric | Value |
|---|---|
| Complexity | Very high |
| Dependencies | libjuice (C), usrsctp (C), OpenSSL/mbedTLS (C), plog (C++) |
| Latency | ~20ms (UDP transport) |
| Bandwidth | Adaptive (congestion control) |

**Why it doesn't work for Portator:**

1. **C++17 core.** The library itself is C++17 despite exposing a C API. Cosmopolitan's C++ support (`cosmo++`) is still maturing -- its experimental CTL (Cosmopolitan Template Library) exists because standard STL support is incomplete. C++17 features like `std::variant`, `std::optional`, and structured bindings may not work reliably.

2. **No codecs included.** libdatachannel handles WebRTC *transport* only. You still need a video encoder (x264, libvpx) and audio encoder (Opus) -- each a massive porting effort to cosmocc.

3. **Heavy dependency chain.** Even the C dependencies (usrsctp, libjuice) use platform-specific socket and threading APIs that may conflict with Cosmopolitan's abstractions.

4. **Unnecessary for the use case.** WebRTC solves peer-to-peer connectivity and NAT traversal. Portator's browser connects directly to the host -- there is no NAT to traverse.

**Verdict:** Weeks-to-months of porting effort for minimal benefit over WebSocket in this architecture.

### 6. Custom WebRTC Stack in Pure C

Assemble a minimal WebRTC data channel from individual C libraries:

| Layer | Library | cosmocc feasibility |
|---|---|---|
| ICE/STUN/TURN | libjuice (pure C, no deps) | High |
| DTLS | mbedTLS (already in cosmo) | High |
| SCTP over DTLS | usrsctp (C) | Medium |
| SDP negotiation | Custom | Medium |

This is essentially reimplementing libdatachannel's core in C. SCTP-over-DTLS requires careful state machine management and is non-trivial.

**Verdict:** Only pursue if WebRTC becomes a hard requirement (e.g., WAN streaming with adaptive bitrate).

## Recommendation

### Phase 1: JPEG-over-WebSocket

Implement framebuffer streaming using `stb_image_write.h` for JPEG encoding and CivetWeb's existing WebSocket support for delivery. This requires:

1. Vendor `stb_image_write.h` into `include/`
2. Implement `PORTATOR_SYS_PRESENT` handler -- read framebuffer from guest memory, encode to JPEG
3. Add WebSocket handler in `web_server.c` via `mg_set_websocket_handler()`
4. Push JPEG frames via `mg_websocket_write()` with `MG_WEBSOCKET_OPCODE_BINARY`
5. Parse incoming WebSocket messages for input events, queue for guest `POLL` calls
6. Browser-side JavaScript: WebSocket client, `ArrayBuffer` to `Image` to `<canvas>`, keyboard/mouse capture

### Phase 2: Optimizations (if needed)

- Replace stb JPEG encoder with libjpeg-turbo (2-6x faster with SIMD)
- Delta encoding: only send changed rectangular regions
- Quality adaptation based on WebSocket backpressure or frame timing feedback
- Audio via Web Audio API + PCM samples over a second WebSocket message type

### Phase 3: WebRTC (only if truly needed)

WebRTC becomes necessary only if:
- Peer-to-peer connectivity is required (browser-to-browser, not browser-to-host)
- UDP transport is needed for lower WAN latency
- Adaptive bitrate / congestion control is needed for unreliable networks

If that time comes, the most pragmatic path is: libjuice (pure C) + cosmo's mbedTLS + usrsctp, giving data channels only.
