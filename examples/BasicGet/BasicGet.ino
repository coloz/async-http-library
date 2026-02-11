/*
 * AsyncHTTP - Basic GET Request Example
 *
 * Demonstrates a simple non-blocking HTTP GET request using the AsyncHTTP
 * library. Compatible with Arduino UNO R4 WiFi and ESP32.
 *
 * Connect to WiFi, then fire an async GET request. The result arrives
 * in the callback without blocking loop().
 */

#include <AsyncHTTP.h>

// ---- WiFi credentials ----
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

AsyncHTTP http;

// ---------------------------------------------------------------------------
// Callback – called automatically when the response arrives
// ---------------------------------------------------------------------------
void onGetResponse(const AsyncHTTPResponse& res, void* userData) {
  (void)userData;
  Serial.println("=== GET Response ===");
  Serial.print("Status: ");
  Serial.println(res.statusCode());
  Serial.print("Body:   ");
  Serial.println(res.body());
  Serial.println("====================");
}

// ---------------------------------------------------------------------------
// Error callback (optional)
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

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(" connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Initialise the async HTTP client
  http.begin();
  http.setTimeout(15000);            // 15 second timeout
  http.onError(onHttpError);         // global error handler

  // Add a default header (applied to every request)
  http.setHeader("User-Agent", "AsyncHTTP/1.0");

  // Fire an async GET request – returns immediately
  int reqId = http.get("http://httpbin.org/get", onGetResponse);
  if (reqId >= 0) {
    Serial.print("GET request queued, id = ");
    Serial.println(reqId);
  }
}

// ---------------------------------------------------------------------------
void loop() {
  // IMPORTANT: call http.update() every loop iteration to drive the
  //            state machine and fire callbacks when data arrives.
  http.update();

  // Your other non-blocking code goes here …
}
