/*
 * AsyncHTTP - Multiple Concurrent Requests Example
 *
 * Demonstrates firing several requests at once. The library drives all
 * of them concurrently (up to ASYNC_HTTP_MAX_REQUESTS) and invokes
 * the corresponding callback for each when it completes.
 *
 * Compatible with Arduino UNO R4 WiFi and ESP32.
 */

#include <AsyncHTTP.h>

const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

AsyncHTTP http;
bool requestsSent = false;

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void onGet(const AsyncHTTPResponse& res, void* userData) {
  const char* label = (const char*)userData;
  Serial.print("[");
  Serial.print(label);
  Serial.print("] Status: ");
  Serial.print(res.statusCode());
  Serial.print("  Body length: ");
  Serial.println(res.body().length());
}

void onPost(const AsyncHTTPResponse& res, void* userData) {
  (void)userData;
  Serial.print("[POST] Status: ");
  Serial.println(res.statusCode());
  Serial.println(res.body().substring(0, 200)); // first 200 chars
}

void onDelete(const AsyncHTTPResponse& res, void* userData) {
  (void)userData;
  Serial.print("[DELETE] Status: ");
  Serial.println(res.statusCode());
}

void onError(int code, const String& msg, void* userData) {
  (void)userData;
  Serial.print("[ERR] ");
  Serial.print(code);
  Serial.print(" ");
  Serial.println(msg);
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.print("WiFi ...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.println(" OK");

  http.begin();
  http.setTimeout(20000);
  http.onError(onError);
}

// ---------------------------------------------------------------------------
void loop() {
  http.update();

  if (!requestsSent) {
    requestsSent = true;

    // Fire multiple requests concurrently
    http.get("http://httpbin.org/get",    onGet, (void*)"GET-1");
    http.get("http://httpbin.org/ip",     onGet, (void*)"GET-IP");

    String json = "{\"hello\":\"world\"}";
    http.postJson("http://httpbin.org/post", json, onPost);

    http.del("http://httpbin.org/delete", onDelete);

    Serial.print("Pending requests: ");
    Serial.println(http.pending());
  }

  // Demonstrate non-blocking behaviour – blink LED
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}
