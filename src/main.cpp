#include <main.h>

// ############################ Setup ############################
void setup()
{
  Serial.begin(115200);
  delay(3000); // Wait 3 seconds to start so that the serial monitor can be opened
  Serial.println("Starting ESP32 Web Server...");

  // Initialize the output variables as outputs
  drive_pins_setup();
  digitalWrite(output48, LOW);

  // Setups
  Serial.println("############## Setup start ##############");
  Wire.begin(I2C_SDA, I2C_SCL);
  WiFi.disconnect(true);
  read_html_file();
  connect_to_wifi();
  Serial.println("############## Setup complete ##############");

  // Websocket Begin
  Serial.println("############## Websocket begin ##############");
  init_websocket();
  init_web_socket_routing();
  Serial.println("############## Websocket complete ##############");
}

// ############################ Loop ############################
void loop()
{
  // Execute the WebSocket request
  ws.cleanupClients();
  // Execute Drive
  execute_drive();

}

// ###################### Functions Definition ######################
// Read the file from the SPIFFS
FileStruct read_custom_file(std::string file_path)
{
  FileStruct file_struct;
  file_struct.status = 0;
  // Check to see if SPIFFS is mounted
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return file_struct;
  }

  // Open file for reading
  File file = SPIFFS.open(file_path.c_str(), "r");
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return file_struct;
  }

  file_struct.file = file;
  file_struct.status = 1;
  
  return file_struct;
}

// Read html file
void read_html_file()
{
  FileStruct file_struct = read_custom_file("/server.html");
  if (file_struct.status == 0)
  {
    Serial.println("Failed to read HTML file from SPIFFS");
    return;
  }
  Serial.println("Reading HTML file from SPIFFS...");
  while (file_struct.file.available())
  {
    html_code_str += file_struct.file.readStringUntil('\n');
  }
  Serial.println("HTML file read from SPIFFS");
}

// Connect to Wi-Fi
void connect_to_wifi()
{
  // Get the Wi-Fi credentials from the file
  FileStruct file_struct = read_custom_file("/wifi_creds.txt");
  if (file_struct.status == 0)
  {
    Serial.println("Failed to read Wi-Fi credentials from file");
    return;
  }

  while (file_struct.file.available())
  {
    String line = file_struct.file.readStringUntil('\n');
    if (line.indexOf("ssid") >= 0)
    {
      ssid = line.substring(ssid_title_length);
    }
    else if (line.indexOf("password") >= 0)
    {
      password = line.substring(password_title_length);
    }
    else if (line.indexOf("username") >= 0)
    {
      username = line.substring(username_title_length);
    }
  }
  // Connect to Wi-Fi network with SSID and password, see if it is WPA2
  Serial.println("Connecting to WiFi with the following credentials:");
  Serial.println("SSID: " + ssid);
  Serial.println("Username: " + username);
  Serial.println("");
  Serial.println("MAC Address: ");
  Serial.println(WiFi.macAddress());
  uint8_t mac[6];

  // Wifi Configs
  WiFi.macAddress(mac);
  WiFi.mode(WIFI_STA);

  // Starting the WiFi, indicate with servo turn
  servo_turn(MIN_ANGLE);

  // Retry until wifi reconencts
  while (WiFi.status() != WL_CONNECTED) {
    int wifi_count = 0;
    if (username == "")
    {
      Serial.println("Connecting to WiFi...");
      WiFi.begin(ssid, password);
    }
    else
    {
      Serial.println("Connecting to WPA2 Enterprise WiFi...");
      esp_wifi_sta_wpa2_ent_set_identity((uint8_t*) eap_anonymous_id, strlen(eap_anonymous_id));
      esp_wifi_sta_wpa2_ent_set_username((uint8_t*) username.c_str(), strlen(username.c_str()));
      esp_wifi_sta_wpa2_ent_set_password((uint8_t*) password.c_str(), strlen(password.c_str()));
      esp_wifi_sta_wpa2_ent_enable();
      WiFi.begin(ssid);
    }

    Serial.print("Attempting to establish connection to WiFi");
    
    // Retry until wifi reconencts
    while (WiFi.status() != WL_CONNECTED && wifi_count < WIFI_RECONNECT_LIMIT) {
      delay(1000);
      Serial.print(".");
      wifi_count++;
    }
    Serial.println("");
    Serial.println("Reconnecting to WiFi...");
  }

  // Print IP address and start web server once connected to WiFi
  Serial.println("");
  Serial.println("Connected to WiFi");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());

  // Server is ready, turn servo left, right, straight
  servo_turn(DEF_ANGLE);
}

// Initialize the WebSocket
void init_websocket() {
  server.begin(); // begin web server
  AsyncElegantOTA.begin(&server); // begin OTA server
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Turn the servo
void servo_turn(int angle) {
  myservo.write(SERVO_PIN, angle);
}

// Write data array to the I2C
bool WireWriteDataArray(uint8_t reg, int8_t *val, unsigned int len)
{
    unsigned int i;

    Wire.beginTransmission(MOTOR_I2C_ADDR);
    Wire.write(reg);    
    for(i = 0; i < len; i++) {
        Wire.write(val[i]);
    }
    if(Wire.endTransmission() != 0) {
        return false;
    }

    return true;
}

// Execute the drive
void execute_drive()
{
  // Drive the car based on the drive status
  if (drive_status == 1)
  {
    Serial.println("Driving forward");
    WireWriteDataArray(MOTOR_FIXED_SPEED_ADDR, car_forward, 4);
    delay(10);
  }
  else if (drive_status == 2)
  {
    Serial.println("Driving backward");
    WireWriteDataArray(MOTOR_FIXED_SPEED_ADDR, car_back, 4);
    delay(10);
  }
  else {
    // Stop moving
    WireWriteDataArray(MOTOR_FIXED_SPEED_ADDR, car_stop, 4);
    delay(10);
  }

  // Steer the car based on the steer status
  if (steer_status == 1)
  {
    Serial.print("Turning left angle: ");
    Serial.println(curr_angle);
    if (curr_angle > MIN_ANGLE) {
      curr_angle -= turn_speed;
      servo_turn(curr_angle);
      delay(10);
    }
  }
  else if (steer_status == 2)
  {
    Serial.print("Turning right angle:");
    Serial.println(curr_angle);
    if (curr_angle < MAX_ANGLE) {
      curr_angle += turn_speed;
      servo_turn(curr_angle);
      delay(10);
    }
  }
  else {
    // Serial.println("No Steer input");
  }
}

// Setup the drive pins
void drive_pins_setup()
{
  // Set the GPIO pins as outputs
  pinMode(I2C_SCL, OUTPUT);
  pinMode(I2C_SDA, OUTPUT);
  pinMode(output48, OUTPUT); // LED
}

// Notify the clients
void notifyClients() {
  Serial.println("Drive Status: ");
  Serial.println(drive_status);
  Serial.println("Steer Status: ");
  Serial.println(steer_status);

  ws.textAll(String(drive_status) + String(steer_status));
}

// Handle the WebSocket message
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    Serial.println("Received WS message: ");
    Serial.println((char*)data);
    Serial.println("");
    if (strcmp((char*)data, "left") == 0) {
      left();
    } else if (strcmp((char*)data, "right") == 0) {
      right();
    } else if (strcmp((char*)data, "forward") == 0) {
      forward();
    } else if (strcmp((char*)data, "backward") == 0) {
      backward();
    } else if (strcmp((char*)data, "not left") == 0 || strcmp((char*)data, "not right") == 0){
      steer_status = 0;
    } else if (strcmp((char*)data, "not forward") == 0 || strcmp((char*)data, "not backward") == 0){
      drive_status = 0;
    }
    notifyClients(); // Notify the clients
  }
}

// Handle the WebSocket event
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Websocket routing handler
void init_web_socket_routing() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", html_code_str.c_str());
  });
}

// ###################### Drive Controls Functions ######################
void forward()
{
  Serial.println("Driving forward");
  drive_status = 1;
  turn_on_led();
}

void backward()
{
  Serial.println("Driving backward");
  drive_status = 2;
  turn_on_led();
}

void left()
{
  Serial.println("Turning left");
  steer_status = 1;
  turn_on_led();
}

void right()
{
  Serial.println("Turning right");
  steer_status = 2;
  turn_on_led();
}

void stop_drive()
{
  Serial.println("Stopping drive");
  drive_status = 0;
  turn_off_led();
}

void stop_steer()
{
  Serial.println("Stopping steer");
  steer_status = 0;
  turn_off_led();
}

// ###################### GPIO Functions ######################
// Turn on the GPIO pin
void turn_high_gpio_pin(int pin)
{
  Serial.print("GPIO ");
  Serial.print(pin);
  Serial.println(" on");
  digitalWrite(pin, HIGH);
}

void turn_low_gpio_pin(int pin)
{
  Serial.print("GPIO ");
  Serial.print(pin);
  Serial.println(" off");
  digitalWrite(pin, LOW);
}

// Turn on the LED
void turn_on_led()
{
  output48State = "on";
  turn_high_gpio_pin(output48);
}

// Turn off the LED
void turn_off_led()
{
  output48State = "off";
  turn_low_gpio_pin(output48);
}