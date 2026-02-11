/*
 * AsyncHTTP - Non-blocking async HTTP client for Arduino
 * Compatible with Arduino UNO R4 WiFi and ESP32 series
 *
 * Copyright (c) 2026 Aily Project
 * Licensed under the MIT License
 */

#include "AsyncHTTP.h"

// ===========================================================================
// AsyncHTTPResponse helpers
// ===========================================================================

String AsyncHTTPResponse::header(const String& name) const {
  for (uint8_t i = 0; i < _headerCount; i++) {
    if (_headers[i].name.equalsIgnoreCase(name)) {
      return _headers[i].value;
    }
  }
  return String();
}

void AsyncHTTPResponse::_addHeader(const String& name, const String& value) {
  if (_headerCount < ASYNC_HTTP_MAX_HEADERS) {
    _headers[_headerCount].name  = name;
    _headers[_headerCount].value = value;
    _headerCount++;
  }
}

// ===========================================================================
// AsyncHTTPRequest::reset
// ===========================================================================

void AsyncHTTPRequest::reset() {
  active          = false;
  state           = STATE_IDLE;
  method          = HTTP_GET;
  host            = "";
  port            = 80;
  path            = "";
  tls             = false;
  requestHeaders  = "";
  requestBody     = "";
  timeoutMs       = ASYNC_HTTP_DEFAULT_TIMEOUT;
  startTime       = 0;
  headersDone     = false;
  chunked         = false;
  remainingBytes  = -1;
  _headerLineBuf  = "";

  // Reset response
  response._statusCode    = 0;
  response._body          = "";
  response._contentLength = -1;
  response._headerCount   = 0;

  onResponseCb   = nullptr;
  onResponseData  = nullptr;
  onErrorCb       = nullptr;
  onErrorData     = nullptr;
  // NOTE: client pointer is NOT reset; it is managed by the pool
}

// ===========================================================================
// AsyncHTTP implementation
// ===========================================================================

AsyncHTTP::AsyncHTTP() {
  memset(_ownedClients, 0, sizeof(_ownedClients));
}

AsyncHTTP::~AsyncHTTP() {
  abortAll();
  if (_ownsClients) {
    for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
      if (_ownedClients[i]) {
        _destroyClient(_ownedClients[i], _requests[i].tls);
        _ownedClients[i] = nullptr;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// begin – initialise with internally-created clients
// ---------------------------------------------------------------------------
void AsyncHTTP::begin() {
  _ownsClients = true;
  for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
    _requests[i].reset();
    _requests[i].client = nullptr; // lazily created on demand
  }
}

// ---------------------------------------------------------------------------
// begin – initialise with user-supplied clients
// ---------------------------------------------------------------------------
void AsyncHTTP::begin(Client* clients[], uint8_t count) {
  _ownsClients = false;
  uint8_t n = min((uint8_t)ASYNC_HTTP_MAX_REQUESTS, count);
  for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
    _requests[i].reset();
    _requests[i].client = (i < n) ? clients[i] : nullptr;
  }
}

// ===========================================================================
// Public convenience methods
// ===========================================================================

int AsyncHTTP::get(const String& url,
                   AsyncHTTPRequest::ResponseCallback onResponse,
                   void* userData) {
  return request(HTTP_GET, url, "", "", onResponse, userData);
}

int AsyncHTTP::post(const String& url, const String& body,
                    const String& contentType,
                    AsyncHTTPRequest::ResponseCallback onResponse,
                    void* userData) {
  return request(HTTP_POST, url, body, contentType, onResponse, userData);
}

int AsyncHTTP::postJson(const String& url, const String& jsonBody,
                        AsyncHTTPRequest::ResponseCallback onResponse,
                        void* userData) {
  return request(HTTP_POST, url, jsonBody, "application/json", onResponse, userData);
}

int AsyncHTTP::put(const String& url, const String& body,
                   const String& contentType,
                   AsyncHTTPRequest::ResponseCallback onResponse,
                   void* userData) {
  return request(HTTP_PUT, url, body, contentType, onResponse, userData);
}

int AsyncHTTP::putJson(const String& url, const String& jsonBody,
                       AsyncHTTPRequest::ResponseCallback onResponse,
                       void* userData) {
  return request(HTTP_PUT, url, jsonBody, "application/json", onResponse, userData);
}

int AsyncHTTP::patch(const String& url, const String& body,
                     const String& contentType,
                     AsyncHTTPRequest::ResponseCallback onResponse,
                     void* userData) {
  return request(HTTP_PATCH, url, body, contentType, onResponse, userData);
}

int AsyncHTTP::patchJson(const String& url, const String& jsonBody,
                         AsyncHTTPRequest::ResponseCallback onResponse,
                         void* userData) {
  return request(HTTP_PATCH, url, jsonBody, "application/json", onResponse, userData);
}

int AsyncHTTP::del(const String& url,
                   AsyncHTTPRequest::ResponseCallback onResponse,
                   void* userData) {
  return request(HTTP_DELETE, url, "", "", onResponse, userData);
}

// ===========================================================================
// Generic request entry-point
// ===========================================================================

int AsyncHTTP::request(AsyncHTTPMethod method,
                       const String& url,
                       const String& body,
                       const String& contentType,
                       AsyncHTTPRequest::ResponseCallback onResponse,
                       void* userData) {
  int slot = _allocSlot();
  if (slot < 0) {
    // Fire global error callback
    if (_globalErrorCb) {
      _globalErrorCb(ASYNC_HTTP_ERR_POOL_FULL,
                     F("Request pool full"), _globalErrorData);
    }
    return ASYNC_HTTP_ERR_POOL_FULL;
  }

  AsyncHTTPRequest& req = _requests[slot];
  req.method = method;

  // ---- Parse URL ----
  if (!_parseUrl(url, req.host, req.port, req.path, req.tls)) {
    req.reset();
    if (_globalErrorCb) {
      _globalErrorCb(ASYNC_HTTP_ERR_INVALID_URL,
                     F("Invalid URL"), _globalErrorData);
    }
    return ASYNC_HTTP_ERR_INVALID_URL;
  }

  req.requestBody     = body;
  req.timeoutMs       = _defaultTimeout;
  req.onResponseCb    = onResponse;
  req.onResponseData  = userData;
  req.onErrorCb       = _globalErrorCb;
  req.onErrorData     = _globalErrorData;

  // Build HTTP header block
  _buildRequestHeader(req, body, contentType);

  // ---- Create / reuse client ----
  if (!req.client) {
    req.client = _createClient(req.tls);
    if (_ownsClients) {
      _ownedClients[slot] = req.client;
    }
  }

  // ---- Start async connect ----
  req.state     = STATE_CONNECTING;
  req.startTime = millis();
  req.active    = true;

  return slot;  // return request ID
}

// ===========================================================================
// Settings
// ===========================================================================

void AsyncHTTP::setHeader(const String& name, const String& value) {
  _defaultHeaders += name + ": " + value + "\r\n";
}

void AsyncHTTP::clearHeaders() {
  _defaultHeaders = "";
}

void AsyncHTTP::setTimeout(unsigned long ms) {
  _defaultTimeout = ms;
}

void AsyncHTTP::onError(AsyncHTTPRequest::ErrorCallback cb, void* userData) {
  _globalErrorCb   = cb;
  _globalErrorData  = userData;
}

// ===========================================================================
// update()  – MUST be called in loop()
// ===========================================================================

void AsyncHTTP::update() {
  for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
    if (_requests[i].active) {
      _processSlot(_requests[i]);
    }
  }
}

uint8_t AsyncHTTP::pending() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
    if (_requests[i].active) n++;
  }
  return n;
}

void AsyncHTTP::abort(int requestId) {
  if (requestId >= 0 && requestId < ASYNC_HTTP_MAX_REQUESTS) {
    AsyncHTTPRequest& req = _requests[requestId];
    if (req.active && req.client) {
      req.client->stop();
    }
    if (_ownsClients && _ownedClients[requestId]) {
      _destroyClient(_ownedClients[requestId], req.tls);
      _ownedClients[requestId] = nullptr;
    }
    req.reset();
    req.client = nullptr;
  }
}

void AsyncHTTP::abortAll() {
  for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
    abort(i);
  }
}

// ===========================================================================
// Internal: allocate a slot
// ===========================================================================

int AsyncHTTP::_allocSlot() {
  for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
    if (!_requests[i].active) {
      _requests[i].reset();
      return i;
    }
  }
  return -1;
}

// ===========================================================================
// Internal: parse URL   http(s)://host(:port)/path
// ===========================================================================

bool AsyncHTTP::_parseUrl(const String& url, String& host, uint16_t& port,
                          String& path, bool& tls) {
  String u = url;
  tls = false;

  if (u.startsWith("https://")) {
    tls = true;
    u = u.substring(8);
  } else if (u.startsWith("http://")) {
    u = u.substring(7);
  } else {
    return false;  // unsupported scheme
  }

  // Separate host(+port) from path
  int slashIdx = u.indexOf('/');
  String hostPort;
  if (slashIdx < 0) {
    hostPort = u;
    path = "/";
  } else {
    hostPort = u.substring(0, slashIdx);
    path = u.substring(slashIdx);
  }

  // Port
  int colonIdx = hostPort.indexOf(':');
  if (colonIdx > 0) {
    host = hostPort.substring(0, colonIdx);
    port = (uint16_t)hostPort.substring(colonIdx + 1).toInt();
  } else {
    host = hostPort;
    port = tls ? 443 : 80;
  }

  return host.length() > 0;
}

// ===========================================================================
// Internal: build the HTTP request header string
// ===========================================================================

void AsyncHTTP::_buildRequestHeader(AsyncHTTPRequest& req,
                                     const String& body,
                                     const String& contentType) {
  static const char* methodNames[] = {
    "GET", "POST", "PUT", "PATCH", "DELETE", "HEAD"
  };

  String h;
  h.reserve(256);

  // Request line
  h += methodNames[(int)req.method];
  h += ' ';
  h += req.path;
  h += F(" HTTP/1.1\r\n");

  // Host header
  h += F("Host: ");
  h += req.host;
  if ((req.tls && req.port != 443) || (!req.tls && req.port != 80)) {
    h += ':';
    h += String(req.port);
  }
  h += F("\r\n");

  // Default headers
  if (_defaultHeaders.length() > 0) {
    h += _defaultHeaders;
  }

  // Content-Type
  if (contentType.length() > 0) {
    h += F("Content-Type: ");
    h += contentType;
    h += F("\r\n");
  }

  // Content-Length
  if (body.length() > 0) {
    h += F("Content-Length: ");
    h += String(body.length());
    h += F("\r\n");
  }

  // Connection: close (simpler to handle)
  h += F("Connection: close\r\n");
  h += F("\r\n");

  req.requestHeaders = h;
}

// ===========================================================================
// Internal: chunked encoding helper (forward declaration)
// ===========================================================================

// A simple helper that strips chunked transfer-encoding framing from the body.
// Works for typical small-to-medium responses.
static void _stripChunkedEncoding(String& body) {
  String result;
  result.reserve(body.length());
  int pos = 0;
  int len = body.length();

  while (pos < len) {
    // Find end of chunk size line
    int lineEnd = body.indexOf("\r\n", pos);
    if (lineEnd < 0) break;

    // Parse chunk size (hex)
    String sizeStr = body.substring(pos, lineEnd);
    sizeStr.trim();
    long chunkSize = strtol(sizeStr.c_str(), nullptr, 16);
    if (chunkSize <= 0) break;  // terminal chunk or error

    int dataStart = lineEnd + 2;
    int dataEnd   = dataStart + (int)chunkSize;
    if (dataEnd > len) dataEnd = len;

    result += body.substring(dataStart, dataEnd);
    pos = dataEnd + 2; // skip trailing \r\n after chunk data
  }

  body = result;
}

// ===========================================================================
// Internal: per-slot state machine   (called from update())
// ===========================================================================

void AsyncHTTP::_processSlot(AsyncHTTPRequest& req) {
  if (!req.client) return;

  // ---- Timeout check ----
  if (req.state != STATE_COMPLETE && req.state != STATE_ERROR &&
      req.state != STATE_IDLE) {
    if (millis() - req.startTime > req.timeoutMs) {
      _finishWithError(req, ASYNC_HTTP_ERR_TIMEOUT, F("Request timed out"));
      return;
    }
  }

  switch (req.state) {

    // ---------------------------------------------------------------
    case STATE_CONNECTING: {
      // Try non-blocking connect
      if (req.client->connected()) {
        req.state = STATE_SENDING;
        break;
      }

      // Attempt connect (on most Arduino cores this is blocking the first
      // time, but returns quickly on subsequent calls if already connected)
      int rc;
#if ASYNC_HTTP_SSL_SUPPORT
      if (req.tls) {
  #if defined(ESP32)
        WiFiClientSecure* sc = static_cast<WiFiClientSecure*>(req.client);
        if (_insecure) sc->setInsecure();
  #endif
      }
#endif
      rc = req.client->connect(req.host.c_str(), req.port);
      if (rc) {
        req.state = STATE_SENDING;
      } else {
        // Connection failed immediately
        _finishWithError(req, ASYNC_HTTP_ERR_CONNECT_FAIL,
                         F("Connection failed"));
      }
      break;
    }

    // ---------------------------------------------------------------
    case STATE_SENDING: {
      // Send header + body in one go
      size_t written = req.client->print(req.requestHeaders);
      if (req.requestBody.length() > 0) {
        written += req.client->print(req.requestBody);
      }
      if (written == 0) {
        _finishWithError(req, ASYNC_HTTP_ERR_SEND_FAIL, F("Send failed"));
        return;
      }
      req.requestHeaders = ""; // free memory
      req.requestBody    = "";
      req.state = STATE_RECEIVING_HEADERS;
      break;
    }

    // ---------------------------------------------------------------
    case STATE_RECEIVING_HEADERS: {
      while (req.client->available()) {
        char c = (char)req.client->read();

        if (c == '\n') {
          // Remove trailing \r
          if (req._headerLineBuf.endsWith("\r")) {
            req._headerLineBuf.remove(req._headerLineBuf.length() - 1);
          }

          if (req._headerLineBuf.length() == 0) {
            // Empty line → headers done
            req.headersDone = true;
            req.state = STATE_RECEIVING_BODY;
            return;  // will continue reading body on next update()
          }

          // Parse status line
          if (req.response._statusCode == 0 &&
              req._headerLineBuf.startsWith("HTTP/")) {
            int spaceIdx = req._headerLineBuf.indexOf(' ');
            if (spaceIdx > 0) {
              req.response._statusCode =
                req._headerLineBuf.substring(spaceIdx + 1).toInt();
            }
          } else {
            // Header line:  "Name: Value"
            int colonIdx = req._headerLineBuf.indexOf(':');
            if (colonIdx > 0) {
              String name  = req._headerLineBuf.substring(0, colonIdx);
              String value = req._headerLineBuf.substring(colonIdx + 1);
              name.trim();
              value.trim();

              req.response._addHeader(name, value);

              // Track Content-Length
              if (name.equalsIgnoreCase("Content-Length")) {
                req.response._contentLength = value.toInt();
                req.remainingBytes = value.toInt();
              }
              // Track Transfer-Encoding: chunked
              if (name.equalsIgnoreCase("Transfer-Encoding") &&
                  value.equalsIgnoreCase("chunked")) {
                req.chunked = true;
              }
            }
          }
          req._headerLineBuf = "";
        } else {
          req._headerLineBuf += c;
        }
      }

      // If connection closed before headers finished
      if (!req.client->connected() && !req.client->available()) {
        if (req.response._statusCode > 0) {
          // We got at least a status code – treat as done
          req.state = STATE_COMPLETE;
          _finishWithResponse(req);
        } else {
          _finishWithError(req, ASYNC_HTTP_ERR_PARSE_FAIL,
                           F("Connection closed during headers"));
        }
      }
      break;
    }

    // ---------------------------------------------------------------
    case STATE_RECEIVING_BODY: {
      // Read available bytes
      while (req.client->available()) {
        char c = (char)req.client->read();

        if (req.chunked) {
          // Simple chunked parser: read chunk size line, then data
          // This is a lightweight implementation; for very large
          // chunked responses consider streaming
          req.response._body += c;
        } else {
          req.response._body += c;
          if (req.remainingBytes > 0) {
            req.remainingBytes--;
            if (req.remainingBytes == 0) {
              req.state = STATE_COMPLETE;
              _finishWithResponse(req);
              return;
            }
          }
        }

        // Safety: limit body size
        if ((int)req.response._body.length() >= ASYNC_HTTP_BODY_BUF_SIZE) {
          req.state = STATE_COMPLETE;
          _finishWithResponse(req);
          return;
        }
      }

      // Connection closed → done
      if (!req.client->connected() && !req.client->available()) {
        // For chunked encoding, strip chunk metadata
        if (req.chunked) {
          _stripChunkedEncoding(req.response._body);
        }
        req.state = STATE_COMPLETE;
        _finishWithResponse(req);
      }
      break;
    }

    // ---------------------------------------------------------------
    case STATE_COMPLETE:
    case STATE_ERROR:
    case STATE_TIMEOUT:
    case STATE_IDLE:
    default:
      break;
  }
}

// ===========================================================================
// Internal: finish helpers
// ===========================================================================

void AsyncHTTP::_finishWithError(AsyncHTTPRequest& req, int code,
                                  const String& msg) {
  req.state = STATE_ERROR;
  if (req.client) req.client->stop();

  // Fire per-request or global error callback
  if (req.onErrorCb) {
    req.onErrorCb(code, msg, req.onErrorData);
  }

  // Cleanup
  if (_ownsClients) {
    // Find the slot index for this request
    for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
      if (&_requests[i] == &req && _ownedClients[i]) {
        _destroyClient(_ownedClients[i], req.tls);
        _ownedClients[i] = nullptr;
        break;
      }
    }
  }
  req.client = nullptr;
  req.active = false;
}

void AsyncHTTP::_finishWithResponse(AsyncHTTPRequest& req) {
  req.state = STATE_COMPLETE;
  if (req.client) req.client->stop();

  // Fire callback
  if (req.onResponseCb) {
    req.onResponseCb(req.response, req.onResponseData);
  }

  // Cleanup
  if (_ownsClients) {
    for (uint8_t i = 0; i < ASYNC_HTTP_MAX_REQUESTS; i++) {
      if (&_requests[i] == &req && _ownedClients[i]) {
        _destroyClient(_ownedClients[i], req.tls);
        _ownedClients[i] = nullptr;
        break;
      }
    }
  }
  req.client = nullptr;
  req.active = false;
}

// ===========================================================================
// Internal: client factory
// ===========================================================================

Client* AsyncHTTP::_createClient(bool tls) {
#if ASYNC_HTTP_SSL_SUPPORT
  if (tls) {
  #if defined(ESP32)
    WiFiClientSecure* sc = new WiFiClientSecure();
    if (_insecure) sc->setInsecure();
    return sc;
  #endif
  }
#else
  (void)tls;
#endif

#ifdef ASYNC_HTTP_USE_WIFI_CLIENT
  return new WiFiClient();
#else
  return nullptr;
#endif
}

void AsyncHTTP::_destroyClient(Client* c, bool tls) {
  (void)tls;
  if (c) {
    c->stop();
#ifdef ASYNC_HTTP_USE_WIFI_CLIENT
    delete static_cast<WiFiClient*>(c);
#else
    delete c;
#endif
  }
}
