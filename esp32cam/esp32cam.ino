// Freenove ESP32-S3-WROOM CAM: streams the camera over Wi-Fi and forwards
// steering commands from the Mac to the robot over serial.
//
//   video:    http://redball.local:81/stream   (MJPEG)
//   commands: UDP port 4210, e.g. "forward", "left 0.40", "stop"
//   robot:    one command per line on GPIO 14 (TX) / 21 (RX), 115200 baud
//
// Arduino IDE: board "ESP32S3 Dev Module", PSRAM "OPI PSRAM",
// Flash Size "8MB", Partition Scheme "8M with spiffs"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include "esp_camera.h"
#include "esp_http_server.h"

const char* WIFI_SSID = "Dalriada";        // -> Dalriada.local
const char* WIFI_PASS = "CsacskaMacskaMocska";
const char* HOSTNAME  = "redball";        // -> redball.local
const uint16_t CMD_PORT = 4210;
const int ROBOT_TX = 14, ROBOT_RX = 21;
const uint32_t TIMEOUT_MS = 500;          // stop the robot if the Mac goes quiet

WiFiUDP udp;
uint32_t lastCmd = 0;
bool stopped = true;

#define BOUNDARY "frame"
static const char* STREAM_TYPE = "multipart/x-mixed-replace;boundary=" BOUNDARY;
static const char* STREAM_SEP  = "\r\n--" BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

esp_err_t streamHandler(httpd_req_t* req) {
  char hdr[64];
  esp_err_t res = httpd_resp_set_type(req, STREAM_TYPE);
  while (res == ESP_OK) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return ESP_FAIL;
    size_t n = snprintf(hdr, sizeof hdr, STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, STREAM_SEP, strlen(STREAM_SEP));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, hdr, n);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
  }
  return res;
}

void startCamera() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = 11; c.pin_d1 = 9;  c.pin_d2 = 8;  c.pin_d3 = 10;
  c.pin_d4 = 12; c.pin_d5 = 18; c.pin_d6 = 17; c.pin_d7 = 16;
  c.pin_xclk = 15; c.pin_pclk = 13; c.pin_vsync = 6; c.pin_href = 7;
  c.pin_sccb_sda = 4; c.pin_sccb_scl = 5;
  c.pin_pwdn = -1; c.pin_reset = -1;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size   = FRAMESIZE_VGA;         // 640x480; QVGA is faster if Wi-Fi is weak
  c.jpeg_quality = 12;                    // 10-63, lower = better quality
  c.fb_count     = 2;
  c.fb_location  = CAMERA_FB_IN_PSRAM;
  c.grab_mode    = CAMERA_GRAB_LATEST;    // always send the newest frame
  if (esp_camera_init(&c) != ESP_OK) {
    Serial.println("camera init failed, restarting");
    delay(1000);
    ESP.restart();
  }
}

void startStream() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 81;
  httpd_handle_t server = nullptr;
  httpd_uri_t uri = { "/stream", HTTP_GET, streamHandler, nullptr };
  if (httpd_start(&server, &cfg) == ESP_OK) httpd_register_uri_handler(server, &uri);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, ROBOT_RX, ROBOT_TX);
  startCamera();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                   // lower latency
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print("."); }
  Serial.printf("\nconnected: http://%s:81/stream\n", WiFi.localIP().toString().c_str());

  MDNS.begin(HOSTNAME);
  startStream();
  udp.begin(CMD_PORT);
}

void loop() {
  if (udp.parsePacket()) {
    char buf[32];
    int len = udp.read(buf, sizeof buf - 1);
    if (len > 0) {
      buf[len] = 0;
      Serial2.println(buf);               // forward to the robot
      lastCmd = millis();
      stopped = false;
    }
  } else if (!stopped && millis() - lastCmd > TIMEOUT_MS) {
    Serial2.println("stop");
    stopped = true;
  }
}
