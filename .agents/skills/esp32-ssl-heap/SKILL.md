---
name: esp32-ssl-heap
description: >
  Diagnosing and fixing ESP32 SSL memory allocation failures (-32512) caused by
  heap fragmentation from HTTPS + ArduinoJson usage. Trigger this skill when you
  see `SSL - Memory allocation failed`, `NoMemory`, or HTTPS fetches that work
  for a few cycles then permanently fail.
---

# ESP32 SSL Heap Fragmentation — Diagnosis & Fix

## Symptom Pattern

```
[E][ssl_client.cpp] _handle_error(): [start_ssl_client():264]: (-32512) SSL - Memory allocation failed
[E][WiFiClientSecure.cpp] connect(): start_ssl_client: -32512
adsb: plane JSON parse error: NoMemory
```

- First 2–4 HTTPS fetch cycles succeed normally.
- Then SSL connection permanently fails with `-32512`.
- `NoMemory` from ArduinoJson often appears in the same session.
- Rebooting recovers, but the crash recurs after the same number of cycles.

## Root Cause

The ESP32 has ~320 KB internal RAM. At runtime, after WiFi connects, usable heap
is roughly **150–200 KB** once the WiFi stack (~100 KB) is accounted for.

**Three large heap regions compete for space:**

| Owner | Size | Lifetime |
|-------|------|----------|
| mbedTLS SSL session (per request) | ~36 KB | `start_ssl_client()` → `stop_ssl_socket()` |
| ArduinoJson `JsonDocument` pool | 2–10 KB | `deserializeJson()` → `doc.clear()` |
| WiFi/lwIP stack | ~100 KB | permanent |

**The fragmentation cascade:**

1. The first `s_client.connect()` initializes MbedTLS.
2. MbedTLS lazy-allocates a **permanent ~7KB global hardware crypto context** (never freed).
3. This 7KB is carved out of the largest contiguous block (typically SRAM 1).
4. For example, a 38.9KB block is permanently reduced to ~31.7KB.
5. While SSL is active, it also allocates ~36KB of dynamic buffers (16KB RX, 4KB TX, overhead).
6. After the connection closes, the 36KB is freed, but the 7KB remains.
7. Since the ESP32's SRAM 2 region provides a hard-capped 32.7KB contiguous block, the `largest` block indicator gets permanently pinned to ~32.7KB.
8. On the **second** cycle, `start_ssl_client()` attempts to allocate 36KB again.
9. Because `36KB > 32.7KB`, the allocation fails instantly with `-32512`.

The `largest` free block metric confirms this:
```
heap[0] before first connect: free=97068  largest=38900
heap[1] after first connect:  free=89904  largest=32756  <- 7KB permanently gone!
heap[2] before second connect: free=89904 largest=32756  <- cannot fit 36KB
```

## Diagnosis

Add these two `Serial.printf` calls temporarily:

```cpp
// Before s_http.begin():
Serial.printf("heap: free=%u largest=%u\n",
    ESP.getFreeHeap(),
    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

// After s_client.stop():
Serial.printf("heap after stop: free=%u largest=%u\n",
    ESP.getFreeHeap(),
    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
```

**What to look for:**
- `largest` before SSL should be >= ~36,000 to connect successfully.
- If `largest` after stop is *smaller* than before the previous cycle's connect,
  something is leaking into the freed SSL region.
- A shrinking `largest` across cycles confirms fragmentation accumulation.

## Fix

**Rule: You must free up >43KB of contiguous heap globally so the remainder after the 7KB MbedTLS penalty is still >36KB.**

Because ESP32 Arduino Core v2.0 uses a precompiled MbedTLS library, you **cannot** dynamically reduce `WiFiClientSecure` buffer sizes (calling `setBufferSizes` will fail to compile). You must increase the available heap.

**1. Reclaim FreeRTOS Task Stacks**
If your fetch loop runs in a background task (via `xTaskCreatePinnedToCore`), check the stack size. Many examples use `16384` (16KB). By moving large local buffers (like `char jsonStr[2048]`) to `static` memory, you can safely halve the stack size to `8192` (8KB). This permanently returns 8KB of contiguous memory to the heap, which is often exactly what MbedTLS needs to survive cycle 2.

**2. Ditch `HTTPClient` for Raw `WiFiClientSecure`**
The built-in `HTTPClient` is a massive source of fragmentation because it heavily relies on `String` allocations for headers (`User-Agent`, `Connection`, etc.). These strings allocate sequentially on the heap, shattering the contiguous free space right before MbedTLS initializes. 
Instead, use a raw `WiFiClientSecure` and `printf` the HTTP/1.0 headers yourself:
```cpp
WiFiClientSecure s_client;
s_client.setInsecure(); // Or use a CA bundle
if (s_client.connect(host, 443)) {
  s_client.printf("GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
}
```

**3. Move Large Buffers to `static` (.bss)**
To prevent the NoMemory secondary symptom during JSON parsing:
- **Use a `static char` buffer instead of `String` for intermediate JSON.**
  `String` causes many small heap allocs/frees during streaming parse.
  ```cpp
  static char jsonStr[2048];  // BSS -- never heap-allocated
  // was: String jsonStr; jsonStr.reserve(1024);
  ```
- **Make the parse-loop `JsonDocument` static** so it has a stable identity.
  ```cpp
  static JsonDocument doc;
  ```

## Anti-Patterns That Cause This

| Anti-pattern | Problem |
|---|---|
| Using `HTTPClient` | Allocates multiple `String` headers right before MbedTLS initializes, shattering the heap. Use raw `WiFiClientSecure` with `printf` instead. |
| Oversized Task Stacks | `xTaskCreate` allocates stack from the heap. If a background task uses `16KB`, that's 16KB permanently gone. Use `8KB` or less by moving buffers to `static`. |
| `stream.find()` / `stream.setTimeout(N)` | `Stream::timedRead()` busy-loops without yielding; starves IDLE0 and triggers task WDT on large responses. Use a manual search with `safeStreamRead()` (which calls `delay(1)` while waiting) |
| `setReuse(true)` + partial stream read | Desync: leftover bytes read as next response headers |
| `static JsonDocument doc` in fetch func | Heap fragmentation — allocates memory permanently *after* the SSL buffer, leaving a permanent hole when SSL is freed. Use a local `JsonDocument doc;` declared *outside* the `while` loop so it destructs at the end of the function (perfect LIFO). |
| `String jsonStr` for streaming parse | Many tiny allocs accelerate fragmentation |
| Blocking drain loop (`while(stream.read() >= 0)`) | `safeStreamRead` has 5s timeout -- stalls every fetch cycle |

## Memory Budget Reference (ESP32 CYD, Arduino framework)

```
Total internal RAM:    327,680 bytes
Static (BSS+data):   ~102,000 bytes  (31%)
Dynamic heap:        ~225,000 bytes
WiFi stack:          ~100,000 bytes
Usable heap at run:  ~125,000 bytes
SSL I/O buffers:      ~20,000 bytes  (16KB in + 4KB out, mbedTLS default)
SSL handshake peak:   ~36,000 bytes  (additional temporary during setup)
Minimum largest_free needed before SSL: ~36,000 bytes
```

## Files Changed in This Fix (cyd-flight-radar)

- `src/services/adsb_client.cpp`:
  - `doc.clear()` after `s_client.stop()` in `fetchUpdate()`
  - `static JsonDocument doc` and `static char jsonStr[2048]`
  - `setReuse(false)` + `s_client.stop()` after every `s_http.end()`
  - Removed blocking drain loop; removed keep-alive
