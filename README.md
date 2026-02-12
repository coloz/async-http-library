# AsyncHTTP
[中文](./README_ZH.md)  
**Non-blocking async HTTP client library for Arduino UNO R4 WiFi and ESP32.**

## Features

- ✅ Supports **GET / POST / PUT / PATCH / DELETE / HEAD** requests
- ✅ Supports sending **plain text** and **JSON** payloads
- ✅ **Callback-based** async design — never blocks `loop()`
- ✅ Up to **4 concurrent requests** (configurable)
- ✅ Automatic parsing of response status code, headers, and body
- ✅ Timeout control & global error callback
- ✅ Compatible with **Arduino UNO R4 WiFi** and **ESP32**
- ✅ HTTPS support on ESP32 (optional certificate verification skip)

## Installation

### Option 1: Manual Installation

Copy the entire `arduino-async-http` folder to your Arduino libraries directory:

- **Windows**: `Documents\Arduino\libraries\AsyncHTTP\`
- **macOS**: `~/Documents/Arduino/libraries/AsyncHTTP/`
- **Linux**: `~/Arduino/libraries/AsyncHTTP/`

### Option 2: PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    https://github.com/aily-project/arduino-async-http.git
```

## Quick Start

```cpp
#include <AsyncHTTP.h>

AsyncHTTP http;

void onResponse(const AsyncHTTPResponse& res, void* userData) {
  Serial.print("Status: ");
  Serial.println(res.statusCode());
  Serial.println(res.body());
}

void onError(int code, const String& msg, void* userData) {
  Serial.print("Error: ");
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin("SSID", "PASSWORD");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  http.begin();
  http.setTimeout(15000);
  http.onError(onError);

  // Send a non-blocking GET request
  http.get("http://httpbin.org/get", onResponse);
}

void loop() {
  http.update();   // ← Must be called in loop()
  // Other non-blocking code ...
}
```

## API Reference

### Initialization

| Method | Description |
|--------|-------------|
| `http.begin()` | Initialize (auto-creates internal WiFiClient) |
| `http.begin(clients[], count)` | Initialize with externally provided Client objects |
| `http.setTimeout(ms)` | Set request timeout (default 10000ms) |
| `http.setHeader(name, value)` | Add a global default header |
| `http.clearHeaders()` | Clear all default headers |
| `http.onError(callback)` | Set global error callback |

### Sending Requests

All request methods **return immediately**. The return value is a request ID (≥0) or an error code (<0).

| Method | Description |
|--------|-------------|
| `http.get(url, callback)` | GET request |
| `http.post(url, body, contentType, callback)` | POST request (custom Content-Type) |
| `http.postJson(url, jsonBody, callback)` | POST JSON (`application/json`) |
| `http.put(url, body, contentType, callback)` | PUT request |
| `http.putJson(url, jsonBody, callback)` | PUT JSON |
| `http.patch(url, body, contentType, callback)` | PATCH request |
| `http.patchJson(url, jsonBody, callback)` | PATCH JSON |
| `http.del(url, callback)` | DELETE request |
| `http.request(method, url, body, ct, callback)` | Generic request method |

### Callback Signatures

```cpp
// Success callback
void onResponse(const AsyncHTTPResponse& response, void* userData);

// Error callback
void onError(int errorCode, const String& message, void* userData);
```

### AsyncHTTPResponse Object

| Method | Return Type | Description |
|--------|-------------|-------------|
| `statusCode()` | `int` | HTTP status code (200, 404, …) |
| `body()` | `const String&` | Response body |
| `isSuccess()` | `bool` | Status code is in the 200–299 range |
| `header(name)` | `String` | Get a specific response header value |
| `contentLength()` | `int` | Content-Length (-1 = unknown) |

### Management

| Method | Description |
|--------|-------------|
| `http.update()` | **Must** be called in `loop()` |
| `http.pending()` | Returns the number of in-flight requests |
| `http.abort(id)` | Cancel a specific request |
| `http.abortAll()` | Cancel all requests |

### Error Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `ASYNC_HTTP_ERR_POOL_FULL` | -1 | Request pool is full |
| `ASYNC_HTTP_ERR_INVALID_URL` | -2 | Invalid URL |
| `ASYNC_HTTP_ERR_CONNECT_FAIL` | -3 | Connection failed |
| `ASYNC_HTTP_ERR_TIMEOUT` | -4 | Request timed out |
| `ASYNC_HTTP_ERR_SEND_FAIL` | -5 | Send failed |
| `ASYNC_HTTP_ERR_PARSE_FAIL` | -6 | Parse failed |

## Compile-Time Configuration

Define macros **before** `#include <AsyncHTTP.h>` to override defaults:

```cpp
#define ASYNC_HTTP_MAX_REQUESTS    8     // Max concurrent requests (default 4)
#define ASYNC_HTTP_BODY_BUF_SIZE   8192  // Response body buffer size (default 4096)
#define ASYNC_HTTP_DEFAULT_TIMEOUT 30000 // Default timeout (default 10000ms)
#define ASYNC_HTTP_MAX_HEADERS     32    // Max stored response headers (default 16)
```

## HTTPS Support

| Platform | HTTPS |
|----------|-------|
| ESP32 | ✅ Supported (WiFiClientSecure) |
| Arduino UNO R4 WiFi | ❌ HTTP only |

ESP32 enables `setInsecure()` by default (skips certificate verification) for development convenience. For production, configure CA certificates or fingerprint verification.

## Examples

- [BasicGet](examples/BasicGet/BasicGet.ino) — Basic GET request
- [PostJson](examples/PostJson/PostJson.ino) — POST JSON data
- [PostPlainText](examples/PostPlainText/PostPlainText.ino) — POST plain text
- [MultipleRequests](examples/MultipleRequests/MultipleRequests.ino) — Multiple concurrent requests

## How It Works

```
setup():
  http.begin()          → Initialize client pool
  http.get(url, cb)     → Parse URL → Allocate slot → Enqueue (returns immediately)

loop():
  http.update()         → Iterate over all active slots:
                            STATE_CONNECTING        → Attempt TCP connection
                            STATE_SENDING           → Send HTTP request headers + body
                            STATE_RECEIVING_HEADERS → Read and parse response headers byte by byte
                            STATE_RECEIVING_BODY    → Read response body
                            STATE_COMPLETE          → Fire callback → Release slot
```

## License

MIT License © 2026 Aily Project
