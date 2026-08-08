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

1. SSL is allocated (36 KB contiguous block carved from heap).
2. While SSL is active, `deserializeJson()` allocates JsonDocument blocks in the
   remaining free space — *adjacent to* the SSL region.
3. `s_http.end()` + `s_client.stop()` frees the SSL 36 KB block.
4. **But `JsonDocument` retains its pool blocks** between calls. Those blocks now
   sit in the middle of what was the SSL region.
5. The 36 KB that was freed is now split into two smaller fragments — neither
   large enough for the next SSL allocation.
6. Next `start_ssl_client()` call fails with `-32512`.

The `largest` free block metric confirms this:
```
heap after stop: free=95024  largest=38900   <- stable, doc.clear() called
heap after stop: free=84508  largest=32756   <- doc NOT cleared, SSL can't fit
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

**Rule: free the `JsonDocument` and the SSL session atomically.**

Call `doc.clear()` immediately after `s_client.stop()` — before returning from
the fetch function. Both release together, giving the heap allocator the best
chance to coalesce them into one large free block.

```cpp
s_http.end();
s_client.stop();
doc.clear();  // <- critical: release alongside SSL so heap can coalesce
return true;
```

If the `JsonDocument` is a `static` local variable, it's accessible throughout
the function scope — call `clear()` at any point after the parsing loop exits.

**Also apply these to prevent the NoMemory secondary symptom:**

1. **Use `static char` buffer instead of `String` for intermediate JSON.**
   `String` causes many small heap allocs/frees during streaming parse.
   ```cpp
   static char jsonStr[2048];  // BSS -- never heap-allocated
   // was: String jsonStr; jsonStr.reserve(1024);
   ```

2. **Make the parse-loop `JsonDocument` static** so it has a stable identity.
   Even though ArduinoJson v7 frees on `clear()`, having it static ensures its
   pointer is consistent and helps avoid allocation-order surprises:
   ```cpp
   static JsonDocument doc;
   // was: JsonDocument doc;  <- re-declared as local each call
   ```

3. **Do NOT use HTTP keep-alive (`setReuse(true)`) with streaming partial reads.**
   If you stop reading before the end of the response body, the next request
   over the same socket reads stale bytes from the previous response, causing
   permanent desync. Always use `setReuse(false)` + `s_client.stop()` when
   doing mid-stream JSON parsing.

## Anti-Patterns That Cause This

| Anti-pattern | Problem |
|---|---|
| `stream.find()` / `stream.setTimeout(N)` | `Stream::timedRead()` busy-loops without yielding; starves IDLE0 and triggers task WDT on large responses. Use a manual search with `safeStreamRead()` (which calls `delay(1)` while waiting) |
| `setReuse(true)` + partial stream read | Desync: leftover bytes read as next response headers |
| Local `JsonDocument doc` in fetch loop | Heap churn — alloc/free on every call causes fragmentation |
| `String jsonStr` for streaming parse | Many tiny allocs accelerate fragmentation |
| `s_client.stop()` without `doc.clear()` | Doc blocks fragment freed SSL region before next cycle |
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
