/*
 * AsyncHTTP - Non-blocking async HTTP client for Arduino
 * Compatible with Arduino UNO R4 WiFi and ESP32 series
 *
 * Copyright (c) 2026 Aily Project
 * Licensed under the MIT License
 */

#ifndef ASYNC_HTTP_H
#define ASYNC_HTTP_H

#include <Arduino.h>
#include <Client.h>

// ---------------------------------------------------------------------------
// Platform-specific WiFi / SSL headers
// ---------------------------------------------------------------------------
#if defined(ARDUINO_UNOWIFIR4)
  #include <WiFiS3.h>
  #define ASYNC_HTTP_USE_WIFI_CLIENT
  #define ASYNC_HTTP_SSL_SUPPORT 0  // UNO R4 WiFi doesn't have robust SSL

#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #define ASYNC_HTTP_USE_WIFI_CLIENT
  #define ASYNC_HTTP_SSL_SUPPORT 1

#else
  // Fallback – expect the user to provide a Client
  #define ASYNC_HTTP_USE_WIFI_CLIENT
  #define ASYNC_HTTP_SSL_SUPPORT 0
#endif

// ---------------------------------------------------------------------------
// Configuration defaults
// ---------------------------------------------------------------------------
#ifndef ASYNC_HTTP_MAX_REQUESTS
  #define ASYNC_HTTP_MAX_REQUESTS    4        // max concurrent requests
#endif

#ifndef ASYNC_HTTP_HEADER_BUF_SIZE
  #define ASYNC_HTTP_HEADER_BUF_SIZE 512      // header line buffer
#endif

#ifndef ASYNC_HTTP_BODY_BUF_SIZE
  #define ASYNC_HTTP_BODY_BUF_SIZE   4096     // response body buffer
#endif

#ifndef ASYNC_HTTP_DEFAULT_TIMEOUT
  #define ASYNC_HTTP_DEFAULT_TIMEOUT 10000    // 10 s
#endif

#ifndef ASYNC_HTTP_MAX_HEADERS
  #define ASYNC_HTTP_MAX_HEADERS     16       // max stored response headers
#endif

// ---------------------------------------------------------------------------
// HTTP Method enum
// ---------------------------------------------------------------------------
enum AsyncHTTPMethod {
  HTTP_GET = 0,
  HTTP_POST,
  HTTP_PUT,
  HTTP_PATCH,
  HTTP_DELETE,
  HTTP_HEAD
};

// ---------------------------------------------------------------------------
// Request state machine
// ---------------------------------------------------------------------------
enum AsyncHTTPState {
  STATE_IDLE = 0,
  STATE_CONNECTING,
  STATE_SENDING,
  STATE_RECEIVING_HEADERS,
  STATE_RECEIVING_BODY,
  STATE_COMPLETE,
  STATE_ERROR,
  STATE_TIMEOUT
};

// ---------------------------------------------------------------------------
// Forward declaration
// ---------------------------------------------------------------------------
class AsyncHTTP;

// ---------------------------------------------------------------------------
// AsyncHTTPResponse  – result container passed to the user callback
// ---------------------------------------------------------------------------
class AsyncHTTPResponse {
public:
  int           statusCode()  const { return _statusCode; }
  const String& body()        const { return _body; }
  bool          isSuccess()   const { return _statusCode >= 200 && _statusCode < 300; }

  /// Retrieve a response header value by name (case-insensitive)
  String header(const String& name) const;

  /// Content-Length as reported by the server (-1 if unknown)
  int    contentLength()       const { return _contentLength; }

private:
  friend class AsyncHTTP;
  friend struct AsyncHTTPRequest;
  int     _statusCode     = 0;
  String  _body           = "";
  int     _contentLength  = -1;

  struct Header { String name; String value; };
  Header  _headers[ASYNC_HTTP_MAX_HEADERS];
  uint8_t _headerCount = 0;

  void _addHeader(const String& name, const String& value);
};

// ---------------------------------------------------------------------------
// AsyncHTTPRequest – internal bookkeeping for one in-flight request
// ---------------------------------------------------------------------------
struct AsyncHTTPRequest {
  bool            active          = false;
  AsyncHTTPState  state           = STATE_IDLE;

  // Request data
  AsyncHTTPMethod method          = HTTP_GET;
  String          host;
  uint16_t        port            = 80;
  String          path;
  bool            tls             = false;
  String          requestHeaders;       // pre-built header lines
  String          requestBody;

  // Timeout
  unsigned long   timeoutMs       = ASYNC_HTTP_DEFAULT_TIMEOUT;
  unsigned long   startTime       = 0;

  // Response parsing
  AsyncHTTPResponse response;
  bool            headersDone     = false;
  bool            chunked         = false;
  int             remainingBytes  = -1;   // for Content-Length
  String          _headerLineBuf;

  // Callbacks
  typedef void (*ResponseCallback)(const AsyncHTTPResponse& response, void* userData);
  typedef void (*ErrorCallback)(int errorCode, const String& message, void* userData);

  ResponseCallback onResponseCb   = nullptr;
  void*            onResponseData  = nullptr;
  ErrorCallback    onErrorCb       = nullptr;
  void*            onErrorData     = nullptr;

  // TCP client – managed by the pool
  Client*         client          = nullptr;

  void reset();
};

// ---------------------------------------------------------------------------
// AsyncHTTP  – main API
// ---------------------------------------------------------------------------
class AsyncHTTP {
public:
  AsyncHTTP();
  ~AsyncHTTP();

  /// Call once in setup() – optionally pass an external Client*
  /// (otherwise the library creates WiFiClient objects automatically)
  void begin();
  void begin(Client* clients[], uint8_t count);

  // -----------------------------------------------------------------------
  // Convenience request methods – plain text body
  // -----------------------------------------------------------------------
  /// HTTP GET
  int get(const String& url,
          AsyncHTTPRequest::ResponseCallback onResponse,
          void* userData = nullptr);

  /// HTTP POST (text or empty body)
  int post(const String& url,
           const String& body,
           const String& contentType,
           AsyncHTTPRequest::ResponseCallback onResponse,
           void* userData = nullptr);

  /// HTTP POST JSON shorthand
  int postJson(const String& url,
               const String& jsonBody,
               AsyncHTTPRequest::ResponseCallback onResponse,
               void* userData = nullptr);

  /// HTTP PUT
  int put(const String& url,
          const String& body,
          const String& contentType,
          AsyncHTTPRequest::ResponseCallback onResponse,
          void* userData = nullptr);

  /// HTTP PUT JSON shorthand
  int putJson(const String& url,
              const String& jsonBody,
              AsyncHTTPRequest::ResponseCallback onResponse,
              void* userData = nullptr);

  /// HTTP PATCH
  int patch(const String& url,
            const String& body,
            const String& contentType,
            AsyncHTTPRequest::ResponseCallback onResponse,
            void* userData = nullptr);

  /// HTTP PATCH JSON shorthand
  int patchJson(const String& url,
                const String& jsonBody,
                AsyncHTTPRequest::ResponseCallback onResponse,
                void* userData = nullptr);

  /// HTTP DELETE
  int del(const String& url,
          AsyncHTTPRequest::ResponseCallback onResponse,
          void* userData = nullptr);

  // -----------------------------------------------------------------------
  // Generic request
  // -----------------------------------------------------------------------
  int request(AsyncHTTPMethod method,
              const String& url,
              const String& body,
              const String& contentType,
              AsyncHTTPRequest::ResponseCallback onResponse,
              void* userData = nullptr);

  // -----------------------------------------------------------------------
  // Per-request / global settings
  // -----------------------------------------------------------------------
  /// Set a default header applied to every request
  void setHeader(const String& name, const String& value);

  /// Clear all default headers
  void clearHeaders();

  /// Set the default timeout in milliseconds (applies to new requests)
  void setTimeout(unsigned long ms);

  /// Set an error callback that applies to ALL requests
  void onError(AsyncHTTPRequest::ErrorCallback cb, void* userData = nullptr);

  // -----------------------------------------------------------------------
  // Housekeeping – MUST be called in loop()
  // -----------------------------------------------------------------------
  void update();

  /// Number of in-flight requests
  uint8_t pending() const;

  /// Cancel one request by its id (returned from get/post/…)
  void abort(int requestId);

  /// Cancel all pending requests
  void abortAll();

#if ASYNC_HTTP_SSL_SUPPORT
  /// For ESP32 – trust all certificates (insecure, but convenient)
  void setInsecure(bool insecure) { _insecure = insecure; }
#endif

private:
  // Pool of request slots
  AsyncHTTPRequest _requests[ASYNC_HTTP_MAX_REQUESTS];

  // Internally-owned clients
  Client*  _ownedClients[ASYNC_HTTP_MAX_REQUESTS];
  bool     _ownsClients = false;

  // Default headers
  String   _defaultHeaders;

  // Default timeout
  unsigned long _defaultTimeout = ASYNC_HTTP_DEFAULT_TIMEOUT;

  // Global error callback
  AsyncHTTPRequest::ErrorCallback _globalErrorCb   = nullptr;
  void*                           _globalErrorData  = nullptr;

#if ASYNC_HTTP_SSL_SUPPORT
  bool _insecure = true; // default: allow insecure for ease of use
#endif

  // Internals
  int      _allocSlot();
  bool     _parseUrl(const String& url, String& host, uint16_t& port,
                     String& path, bool& tls);
  void     _buildRequestHeader(AsyncHTTPRequest& req, const String& body,
                                const String& contentType);
  void     _processSlot(AsyncHTTPRequest& req);
  void     _finishWithError(AsyncHTTPRequest& req, int code, const String& msg);
  void     _finishWithResponse(AsyncHTTPRequest& req);

  Client*  _createClient(bool tls);
  void     _destroyClient(Client* c, bool tls);
};

// Error codes reported via the error callback
#define ASYNC_HTTP_ERR_POOL_FULL      -1
#define ASYNC_HTTP_ERR_INVALID_URL    -2
#define ASYNC_HTTP_ERR_CONNECT_FAIL   -3
#define ASYNC_HTTP_ERR_TIMEOUT        -4
#define ASYNC_HTTP_ERR_SEND_FAIL      -5
#define ASYNC_HTTP_ERR_PARSE_FAIL     -6

#endif // ASYNC_HTTP_H
