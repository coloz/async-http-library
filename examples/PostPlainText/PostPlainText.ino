/*
 * AsyncHTTP - Plain Text POST Example
 *
 * Sends a plain-text body via POST and reads response headers.
 * Compatible with Arduino UNO R4 WiFi and ESP32.
 */

#include <AsyncHTTP.h>

const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

AsyncHTTP http;

// ---------------------------------------------------------------------------
void onResponse(const AsyncHTTPResponse& res, void* userData) {
  (void)userData;
  Serial.println("=== Plain Text POST Response ===");
  Serial.print("Status: ");
  Serial.println(res.statusCode());
  Serial.print("Content-Type: ");
  Serial.println(res.header("Content-Type"));
  Serial.print("Content-Length: ");
  Serial.println(res.contentLength());
  Serial.println("Body:");
  Serial.println(res.body());
  Serial.println("================================");
}

void onError(int code, const String& msg, void* userData) {
  (void)userData;
  Serial.print("[Error] ");
  Serial.print(code);
  Serial.print(" ");
  Serial.println(msg);
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.print("WiFi ");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.println(" OK");

  http.begin();
  http.onError(onError);

  // POST plain text
  String textBody = "Hello from Arduino!";
  http.post("http://httpbin.org/post",
            textBody,
            "text/plain",
            onResponse);
}

// ---------------------------------------------------------------------------
void loop() {
  http.update();
}
