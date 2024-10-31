
// important settings:
// - USB CDC on Boot: Enabled (to see the Serial.print)
// - PSRAM: QSPI PSRAM (to have enough memory for the framebuffer)

#include "Camera.h"

#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 21
#define SIOC_GPIO_NUM 14
#define Y9_GPIO_NUM 11
#define Y8_GPIO_NUM 9
#define Y7_GPIO_NUM 8
#define Y6_GPIO_NUM 6
#define Y5_GPIO_NUM 4
#define Y4_GPIO_NUM 2
#define Y3_GPIO_NUM 3
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 13
#define HREF_GPIO_NUM 12
#define PCLK_GPIO_NUM 7
#define LED_GPIO_NUM 34

Camera camera;
camera_config_t config;

void setup() {
  Serial.begin(115200);
  while(!Serial);

  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // YUV422,GRAYSCALE,RGB565,JPEG
  config.frame_size = FRAMESIZE_HD; //
  config.jpeg_quality = 16;
  config.fb_count = 2;                     // 8
  config.fb_location = CAMERA_FB_IN_PSRAM; /*!< The location where the frame
                                              buffer will be allocated */
  config.grab_mode = CAMERA_GRAB_LATEST;   /*!< When buffers should be filled */


  if (!camera.begin(config)){
    Serial.println("Camera failed");
    while(true);
  }
  Serial.print("Recording...");
}

void loop() { 
    
    auto fb = camera.frameBuffer(); 
    Serial.println(fb->len);

}