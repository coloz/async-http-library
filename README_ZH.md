# AsyncHTTP
[English](./README.md)  
**非阻塞异步 HTTP 客户端库，适用于 Arduino UNO R4 WiFi 和 ESP32 系列。**

## 功能特性

- ✅ 支持 **GET / POST / PUT / PATCH / DELETE / HEAD** 请求
- ✅ 支持发送 **纯文本** 和 **JSON** 数据
- ✅ 基于 **回调函数** 的异步设计，永不阻塞 `loop()`
- ✅ 最多 **4 个请求并发**（可配置）
- ✅ 自动解析响应状态码、Headers、Body
- ✅ 超时控制 & 全局错误回调
- ✅ 兼容 **Arduino UNO R4 WiFi**、**ESP32**
- ✅ ESP32 支持 HTTPS（可选跳过证书验证）

## 安装

### 方式一：手动安装

将整个 `arduino-async-http` 文件夹复制到 Arduino 库目录：

- **Windows**: `文档\Arduino\libraries\AsyncHTTP\`
- **macOS**: `~/Documents/Arduino/libraries/AsyncHTTP/`
- **Linux**: `~/Arduino/libraries/AsyncHTTP/`

### 方式二：PlatformIO

在 `platformio.ini` 中添加：

```ini
lib_deps =
    https://github.com/aily-project/arduino-async-http.git
```

## 快速开始

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

  // 发送 GET 请求（非阻塞）
  http.get("http://httpbin.org/get", onResponse);
}

void loop() {
  http.update();   // ← 必须在 loop() 中调用
  // 其他非阻塞代码 ...
}
```

## API 参考

### 初始化

| 方法 | 说明 |
|------|------|
| `http.begin()` | 初始化（自动创建内部 WiFiClient） |
| `http.begin(clients[], count)` | 使用外部传入的 Client 对象 |
| `http.setTimeout(ms)` | 设置请求超时（默认 10000ms） |
| `http.setHeader(name, value)` | 添加全局默认 Header |
| `http.clearHeaders()` | 清除所有默认 Header |
| `http.onError(callback)` | 设置全局错误回调 |

### 发送请求

所有请求方法均 **立即返回**，返回值为请求 ID（≥0）或错误码（<0）。

| 方法 | 说明 |
|------|------|
| `http.get(url, callback)` | GET 请求 |
| `http.post(url, body, contentType, callback)` | POST 请求（自定义 Content-Type） |
| `http.postJson(url, jsonBody, callback)` | POST JSON (`application/json`) |
| `http.put(url, body, contentType, callback)` | PUT 请求 |
| `http.putJson(url, jsonBody, callback)` | PUT JSON |
| `http.patch(url, body, contentType, callback)` | PATCH 请求 |
| `http.patchJson(url, jsonBody, callback)` | PATCH JSON |
| `http.del(url, callback)` | DELETE 请求 |
| `http.request(method, url, body, ct, callback)` | 通用请求方法 |

### 回调签名

```cpp
// 成功回调
void onResponse(const AsyncHTTPResponse& response, void* userData);

// 错误回调
void onError(int errorCode, const String& message, void* userData);
```

### AsyncHTTPResponse 对象

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `statusCode()` | `int` | HTTP 状态码 (200, 404, …) |
| `body()` | `const String&` | 响应体 |
| `isSuccess()` | `bool` | 状态码在 200–299 范围内 |
| `header(name)` | `String` | 获取指定响应头的值 |
| `contentLength()` | `int` | Content-Length (-1 = 未知) |

### 管理

| 方法 | 说明 |
|------|------|
| `http.update()` | **必须** 在 `loop()` 中调用 |
| `http.pending()` | 返回进行中的请求数量 |
| `http.abort(id)` | 取消指定请求 |
| `http.abortAll()` | 取消所有请求 |

### 错误码

| 常量 | 值 | 说明 |
|------|----|------|
| `ASYNC_HTTP_ERR_POOL_FULL` | -1 | 请求池已满 |
| `ASYNC_HTTP_ERR_INVALID_URL` | -2 | 无效的 URL |
| `ASYNC_HTTP_ERR_CONNECT_FAIL` | -3 | 连接失败 |
| `ASYNC_HTTP_ERR_TIMEOUT` | -4 | 请求超时 |
| `ASYNC_HTTP_ERR_SEND_FAIL` | -5 | 发送失败 |
| `ASYNC_HTTP_ERR_PARSE_FAIL` | -6 | 解析失败 |

## 编译时配置

在 `#include <AsyncHTTP.h>` **之前** 定义宏即可修改默认值：

```cpp
#define ASYNC_HTTP_MAX_REQUESTS    8     // 最大并发请求数 (默认 4)
#define ASYNC_HTTP_BODY_BUF_SIZE   8192  // 响应体缓冲区大小 (默认 4096)
#define ASYNC_HTTP_DEFAULT_TIMEOUT 30000 // 默认超时 (默认 10000ms)
#define ASYNC_HTTP_MAX_HEADERS     32    // 最大存储响应头数 (默认 16)
```

## HTTPS 支持

| 平台 | HTTPS |
|------|-------|
| ESP32 | ✅ 支持 (WiFiClientSecure) |
| Arduino UNO R4 WiFi | ❌ 仅支持 HTTP |

ESP32 默认启用 `setInsecure()`（跳过证书验证）以方便开发调试。生产环境建议配置 CA 证书或指纹验证。

## 示例

- [BasicGet](examples/BasicGet/BasicGet.ino) — 基本 GET 请求
- [PostJson](examples/PostJson/PostJson.ino) — POST JSON 数据
- [PostPlainText](examples/PostPlainText/PostPlainText.ino) — POST 纯文本
- [MultipleRequests](examples/MultipleRequests/MultipleRequests.ino) — 多请求并发

## 工作原理

```
setup():
  http.begin()          → 初始化客户端池
  http.get(url, cb)     → 解析URL → 分配槽位 → 排入队列 (立即返回)

loop():
  http.update()         → 遍历所有活跃槽位:
                            STATE_CONNECTING   → 尝试TCP连接
                            STATE_SENDING      → 发送HTTP请求头+Body
                            STATE_RECEIVING_HEADERS → 逐字节读取并解析响应头
                            STATE_RECEIVING_BODY    → 读取响应体
                            STATE_COMPLETE     → 触发回调 → 释放槽位
```

## License

MIT License © 2026 Aily Project
