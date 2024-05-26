#include <Arduino.h>
#include <WiFi.h>
#include <string>
#include "esp_wpa2.h"
#include "esp_wifi.h"
#include "SPIFFS.h"
#include <Wire.h>
#include <Servo.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

// # TODO:
// 1. Webserverl ibrary from arduino
// 2. Webserver.h
// 3. Elegant OTA does not work on Eduroam
// 4. TAKSs ON ESP32 (won't interfere)

// ###################### Structs ######################
typedef struct
{
    File file;
    int status;
} FileStruct;

typedef struct {
    float new_angle;
    float time_delta;
} Numbers;

// ###################### Functions Declaration ######################
void turn_high_gpio_pin(int pin);
void turn_low_gpio_pin(int pin);

void init_websocket();
void init_web_socket_routing();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);

FileStruct read_custom_file(std::string file_path);
void read_html_file();
void connect_to_wifi();
void drive_pins_setup();
void servo_turn(int angle);
void execute_steer();
void execute_drive();

int limit_angle(int angle);
Numbers listen_to_serial();
Numbers parse_serial_input(String input);
int execute_command();

bool WireWriteDataArray(uint8_t reg, int8_t *val, unsigned int len);
void turn_on_led();
void turn_off_led();

void forward();
void left();
void right();
void backward();
void stop_drive();
void stop_steer();

// ############################ WiFi Setup ############################
String ssid, password, username;
const char *eap_anonymous_id = "anonymous@northwestern.edu";
const int password_title_length = 9;
const int ssid_title_length = 5;
const int username_title_length = 9;
#define WIFI_RECONNECT_LIMIT 3

// Set web server port number to 80
// WiFiServer server(80);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Variable to store the HTTP request
String header;

// ############################ Variables ############################
unsigned long currentTime = millis();
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 20000;

// Server status related
int server_restart_limit = 50;
int server_empty_count = 0;

// Auxiliar variables to store the current output state
String output48State = "off";
const int output48 = 48;

// Car Variables
#define MOTOR_FIXED_SPEED_ADDR    51 //
#define MOTOR_I2C_ADDR 0x34
#define I2C_SCL 47 // orange wire
#define I2C_SDA 45 // yellow wire
#define SERVO_PIN 15
int pulsewidth;     //占空比

int8_t car_forward[4]={16,0,-16,0};   // forward
int8_t car_back[4]={-16,0,16,0};  // backward
int8_t car_stop[4]={0,0,0,0};

// Drive variables
#define ONE_DELAY 10 // ms
#define TURN_DELAY 100
#define DELAY_DELAY 100
#define NO_SERIAL_INPUT 0
int drive_status = 0; // 0: Not in drive, 1: Forward, 2: Backward
int steer_status = 0; // 0: Not steering, 1: Left, 2: Right
int drive_speed = 25;

int seq_num = 0;

// Servo variables
#define MAX_ANGLE 135
#define MIN_ANGLE 45
#define DEF_ANGLE 90
const int turn_speed = 2; // Minimum is 1
int curr_angle = 90;
int goal_angle = 90;

// HTML web page variable
String html_code_str;
