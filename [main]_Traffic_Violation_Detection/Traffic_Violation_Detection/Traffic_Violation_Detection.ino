#include <WiFi.h>
#include <HTTPClient.h>
HTTPClient http;
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "img_converters.h"
#include <Preferences.h>
Preferences preferences;

// image transfering data structure
typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

// --------------Streaming configuration---------------
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;
// ----------------------------------------------------

// --------------GPIO PIN configuration----------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
// ----------------------------------------------------

// --------------State Definition-----------------------
byte ReceiveState=0;
byte cmdState=1;
byte strState=1;
byte questionstate=0;
byte equalstate=0;
byte semicolonstate=0;
// ----------------------------------------------------

//default Wifi credentials
String wifi_ssid ="FireShark 776t";
String wifi_password ="123456789";

// Access Point configuration
const char* apssid = "Smart_Traffic 18";
const char* appassword = "123456789";
// command parameter definition
String Feedback="";

String Command="";
String cmd="";
String P1="";
String P2="";
String P3="";
String P4="";
String P5="";
String P6="";
String P7="";
String P8="";
String P9="";

void setup() {
  //to-do : fail-safe ketika arus tidak stabil
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // --------------ESP CAM Configuration-----------------------
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // ----------------------------------------------------

  //---------------------------------------------------------------------------
  // WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
  //            Ensure ESP32 Wrover Module or other board with PSRAM is selected
  //            Partial images will be transmitted if image exceeds buffer size
  //   
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  //----------------------------------------------------------------------------

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  //Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  //--------------------Default video configuration----------------------
  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_hmirror(s, 1);

  ledcAttachPin(4, 4);  
  ledcSetup(4, 5000, 8);    
  //----------------------------------------------------------------------------

  //--------------------------------Wifi & AP INIT--------------------------------
  WiFi.mode(WIFI_AP_STA);

  Serial.println();

  //Preferences_write("wifi", "ssid", "");
  //Preferences_write("wifi", "password", "");
        
  wifi_ssid = Preferences_read("wifi", "ssid");
  wifi_password = Preferences_read("wifi", "password");

  if (wifi_ssid!="") {
    for (int i=0;i<2;i++) {
      WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());]
    
      delay(1000);
      Serial.println("");
      Serial.print("Connecting to ");
      Serial.println(wifi_ssid);
      
      long int StartTime=millis();
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          if ((StartTime+5000) < millis()) break;]
      } 
    
      if (WiFi.status() == WL_CONNECTED) {]
        WiFi.softAP((WiFi.localIP().toString()+"_"+(String)apssid).c_str(), appassword);]示客戶端IP         
        Serial.println("");
        Serial.println("STAIP address: ");
        Serial.println(WiFi.localIP());
        Serial.println("");
    
        for (int i=0;i<5;i++) {]
          ledcWrite(4,10);
          delay(200);
          ledcWrite(4,0);
          delay(200);    
        }

        // line notify API TOKEN
        if (LineToken != "") 
          LineNotify_http_get(LineToken, WiFi.localIP().toString());
        break;
      }
    } 
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.softAP((WiFi.softAPIP().toString()+"_"+(String)apssid).c_str(), appassword);         

  // flashing led 2 times
    for (int i=0;i<2;i++) {
      ledcWrite(4,10);
      delay(1000);
      ledcWrite(4,0);
      delay(1000);    
    }
  }  

  Serial.println();
  Serial.println("Access Point IP address: ");
  Serial.println(WiFi.softAPIP()); 
  Serial.println();
  startCameraServer(); 

  // turning on LED as indicator
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);      
}

void loop {
    
}

//-----------------------------------image encoding function-----------------------------------------------------------
static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}
//----------------------------------------------------------------------------------------------------------------------
// interface translation for cautions area (angle)
int transferAngle(int angle, String side) {     
  if (angle > 180)
     angle = 180;
  else if (angle < 0)
    angle = 0;
  if (side="right")
    angle = 180 - angle;     
  return angle*6300/180+1700;
}

//---------------------------Read/Write saved configuration function---------------------------------------------------
void Preferences_write(const char * name, const char* key, const char* value) {
  preferences.clear();
  preferences.begin(name, false);
  Serial.printf("Put %s = %s\n", key, value);
  preferences.putString(key, value);
  preferences.end();
}

String Preferences_read(const char * name, const char* key) {
  preferences.begin(name, false);
  String myData = preferences.getString(key, "");
  Serial.printf("Get %s = %s\n", key, myData);
  preferences.end();
  return myData;
}
//------------------------------------------------------------------------------------------------------------------

//---------------------Custom Parameter------------------------------------------------------
// references : https://github.com/fustyles/Arduino/blob/master/ESP32-CAM_Tensorflow.js/ESP32-CAM_coco-ssd_CautionArea/ESP32-CAM_coco-ssd_CautionArea.ino
void getCommand(char c)
{
  if (c=='?') ReceiveState=1;
  if ((c==' ')||(c=='\r')||(c=='\n')) ReceiveState=0;
  
  if (ReceiveState==1)
  {
    Command=Command+String(c);
    
    if (c=='=') cmdState=0;
    if (c==';') strState++;
  
    if ((cmdState==1)&&((c!='?')||(questionstate==1))) cmd=cmd+String(c);
    if ((cmdState==0)&&(strState==1)&&((c!='=')||(equalstate==1))) P1=P1+String(c);
    if ((cmdState==0)&&(strState==2)&&(c!=';')) P2=P2+String(c);
    if ((cmdState==0)&&(strState==3)&&(c!=';')) P3=P3+String(c);
    if ((cmdState==0)&&(strState==4)&&(c!=';')) P4=P4+String(c);
    if ((cmdState==0)&&(strState==5)&&(c!=';')) P5=P5+String(c);
    if ((cmdState==0)&&(strState==6)&&(c!=';')) P6=P6+String(c);
    if ((cmdState==0)&&(strState==7)&&(c!=';')) P7=P7+String(c);
    if ((cmdState==0)&&(strState==8)&&(c!=';')) P8=P8+String(c);
    if ((cmdState==0)&&(strState>=9)&&((c!=';')||(semicolonstate==1))) P9=P9+String(c);
    
    if (c=='?') questionstate=1;
    if (c=='=') equalstate=1;
    if ((strState>=9)&&(c==';')) semicolonstate=1;
  }
}
// --------------------------------------------------------------------------

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  //http://192.168.xxx.xxx/
  httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = index_handler,
      .user_ctx  = NULL
  };

  //http://192.168.xxx.xxx/status
  httpd_uri_t status_uri = {
      .uri       = "/status",
      .method    = HTTP_GET,
      .handler   = status_handler,
      .user_ctx  = NULL
  };

  //http://192.168.xxx.xxx/control
  httpd_uri_t cmd_uri = {
      .uri       = "/control",
      .method    = HTTP_GET,
      .handler   = cmd_handler,
      .user_ctx  = NULL
  }; 

  //http://192.168.xxx.xxx/capture
  httpd_uri_t capture_uri = {
      .uri       = "/capture",
      .method    = HTTP_GET,
      .handler   = capture_handler,
      .user_ctx  = NULL
  };

  //http://192.168.xxx.xxx:81/stream
  httpd_uri_t stream_uri = {
      .uri       = "/stream",
      .method    = HTTP_GET,
      .handler   = stream_handler,
      .user_ctx  = NULL
  };
  
  httpd_uri_t wifi_uri = {
    .uri       = "/wifi",
    .method    = HTTP_GET,
    .handler   = index_wifi_handler,
    .user_ctx  = NULL
  };   
  
  httpd_uri_t Horizontal_uri = {
    .uri       = "/Horizontal",
    .method    = HTTP_GET,
    .handler   = index_Horizontal_handler,
    .user_ctx  = NULL
  };
      
  httpd_uri_t Vertical_uri = {
    .uri       = "/Vertical",
    .method    = HTTP_GET,
    .handler   = index_Vertical_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t Rect_uri = {
    .uri       = "/Rectangular",
    .method    = HTTP_GET,
    .handler   = index_Rect_handler,
    .user_ctx  = NULL
  };
    
  Serial.printf("Starting web server on port: '%d'\n", config.server_port);  //Server Port
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
      //http request handler
      httpd_register_uri_handler(camera_httpd, &index_uri);
      httpd_register_uri_handler(camera_httpd, &cmd_uri);
      httpd_register_uri_handler(camera_httpd, &status_uri);
      httpd_register_uri_handler(camera_httpd, &capture_uri);
      httpd_register_uri_handler(camera_httpd, &wifi_uri); 
      httpd_register_uri_handler(camera_httpd, &Horizontal_uri);
      httpd_register_uri_handler(camera_httpd, &Vertical_uri);
      httpd_register_uri_handler(camera_httpd, &Rect_uri);
  }
  
  config.server_port += 1;  //Stream Port
  config.ctrl_port += 1;    //UDP Port
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}