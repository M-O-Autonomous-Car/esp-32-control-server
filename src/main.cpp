#include <main.h>

// ############################ Setup ############################
void setup()
{
  Serial.begin(115200);
  delay(3000); // Wait 3 seconds to start so that the serial monitor can be opened
  Serial.println("Starting ESP32 Web Server...");

  // Initialize the output variables as outputs
  pinMode(output48, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output48, LOW);

  // Disconnect from previously connected Wi-Fi
  WiFi.disconnect();

  // Read in HTML file
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

  // Connect to Wi-Fi
  connect_to_wifi();
}

// ############################ Loop ############################
void loop()
{
  // Listen for incoming clients
  // 1. Each HTTP request will be handled as a fresh client connect, thus why the server will keep listening for new clients
  // 2. The client connection is not persistent
  if (!listening_for_client())
  {
    server_empty_count++;
  } else {
    server_empty_count = 0;
  }

  // Restart the server and reconnect to WiFi if it has been empty for a while
  if (server_empty_count >= server_restart_limit)
  {
    Serial.println("Server has been empty for a while. Restarting server and reconnecting to WiFi...");
    server_empty_count = 0;
    connect_to_wifi();
  }
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
  WiFi.macAddress(mac);
  WiFi.mode(WIFI_STA);

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
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  // Print IP address and start web server once connected to WiFi
  Serial.println("");
  Serial.println("Connected to WiFi");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

// Listen for incoming clients
boolean listening_for_client()
{
  // Check if a client has connected
  Serial.print("Waiting for new client/request...");
  WiFiClient client = server.available();
  if (client)
  {
    Serial.println("");
    Serial.println("Client connected...");
    execute_request(client);
    return true;
  }
  else
  {
    Serial.println(".");
    delay(500);
    return false;
  }
}

// Execute the HTTP request
void execute_request(WiFiClient client)
{
  currentTime = millis();
  previousTime = currentTime;
  String currentLine = ""; // make a String to hold incoming data from the client

  while (client.connected() && currentTime - previousTime <= timeoutTime)
  { // loop while the client's connected
    currentTime = millis();
    if (client.available())
    {                         // if there's bytes to read from the client,
      char c = client.read(); // read a byte, then
      Serial.write(c);        // print it out the serial monitor
      header += c;
      if (c == '\n')
      { // if the byte is a newline character
        // if the current line is blank, you got two newline characters in a row.
        // that's the end of the client HTTP request, so send a response:
        if (currentLine.length() == 0)
        {
          // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
          // and a content-type so the client knows what's coming, then a blank line:
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();

          // Parse the HTML header data
          parse_drive_direction_request(header);

          // Display the HTML web page
          client.println(F(html_code_str.c_str()));

          // The HTTP response ends with another blank line
          client.println();
          // Break out of the while loop
          break;
        }
        else
        { // if you got a newline, then clear currentLine
          currentLine = "";
        }
      }
      else if (c != '\r')
      {                   // if you got anything else but a carriage return character,
        currentLine += c; // add it to the end of the currentLine
      }
    }
  }
  // Clear the header variable
  header = "";
  // Close the connection after each response
  client.stop();
  Serial.println("Response sent to client");
  Serial.println("");
}

// Turn on the GPIO pin
void turn_on_gpio_pin(int pin)
{
  Serial.print("GPIO ");
  Serial.print(pin);
  Serial.println(" on");
  digitalWrite(pin, HIGH);
}

void turn_off_gpio_pin(int pin)
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
  turn_on_gpio_pin(output48);
}

// Turn off the LED
void turn_off_led()
{
  output48State = "off";
  turn_off_gpio_pin(output48);
}

// Parse the header for drive direction request
// If the GET request is of the format "GET /press/{some direction}", turn on the GPIO pin and change drive status
// If the get request is of the format "GET /release/{some direction}", turn off the GPIO pin and change drive status
void parse_drive_direction_request(String header)
{
  if (header.indexOf("GET /press/left") >= 0)
  {
    left();
  }
  else if (header.indexOf("GET /press/right") >= 0)
  {
    right();
  }
  else if (header.indexOf("GET /press/forward") >= 0)
  {
    forward();
  }
  else if (header.indexOf("GET /press/backward") >= 0)
  {
    backward();
  }
  else if (header.indexOf("GET /release/left") >= 0)
  {
    Serial.println("Releasing left");
    drive_status = 0;
    turn_off_led();
  }
  else if (header.indexOf("GET /release/right") >= 0)
  {
    Serial.println("Releasing right");
    drive_status = 0;
    turn_off_led();
  }
  else if (header.indexOf("GET /release/forward") >= 0)
  {
    Serial.println("Releasing forward");
    drive_status = 0;
    turn_off_led();
  }
  else if (header.indexOf("GET /release/backward") >= 0)
  {
    Serial.println("Releasing backward");
    drive_status = 0;
    turn_off_led();
  }
  else
  {
    Serial.println("Invalid request");
  }
}

// ###################### Drive Controls Functions ######################
void forward()
{
  Serial.println("Driving forward");
  drive_status = 1;
  turn_on_led();
}

void left()
{
  Serial.println("Turning left");
  drive_status = 2;
  turn_on_led();
}

void right()
{
  Serial.println("Turning right");
  drive_status = 3;
  turn_on_led();
}

void backward()
{
  Serial.println("Driving backward");
  drive_status = 4;
  turn_on_led();
}