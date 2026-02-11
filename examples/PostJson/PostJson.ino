/*
 * AsyncHTTP - POST JSON Example
 *
 * Demonstrates sending a JSON payload via HTTP POST and receiving
 * the response asynchronously. Works on Arduino UNO R4 WiFi and ESP32.
 */

#include <AsyncHTTP.h>

// ---- WiFi credentials ----
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

AsyncHTTP http;

// ---------------------------------------------------------------------------
// Callback – POST response
// ---------------------------------------------------------------------------
void onPostResponse(const AsyncHTTPResponse& res, void* userData) {
  (void)userData;
  Serial.println("=== POST JSON Response ===");
  Serial.print("Status: ");
  Serial.println(res.statusCode());

  if (res.isSuccess()) {
    Serial.println("Body:");
    Serial.println(res.body());
  } else {
    Serial.println("Request failed!");
  }
  Serial.println("==========================");
}

// ---------------------------------------------------------------------------
void onHttpError(int code, const String& message, void* userData) {
  (void)userData;
  Serial.print("[HTTP Error] ");
  Serial.print(code);
  Serial.print(" - ");
  Serial.println(message);
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(" connected!");

  http.begin();
  http.setTimeout(15000);
  http.onError(onHttpError);
  http.setHeader("User-Agent", "AsyncHTTP/1.0");

  // ---- POST JSON ----
  String json = "{\"sensor\":\"temperature\",\"value\":23.5}";
  int reqId = http.postJson("http://httpbin.org/post", json, onPostResponse);

  if (reqId >= 0) {
    Serial.print("POST JSON request queued, id = ");
    Serial.println(reqId);
  }
}

// ---------------------------------------------------------------------------
void loop() {
  http.update();

  // Your non-blocking application logic …
}
