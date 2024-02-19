#include <Arduino.h>
#include <WiFi.h>
#include <string>
#include "esp_wpa2.h"
#include "esp_wifi.h"
#include "SPIFFS.h"

// ###################### Structs ######################
typedef struct {
    File file;
    int status;
} FileStruct;

// ###################### Functions Declaration ######################
FileStruct read_custom_file(std::string file_path);
void connect_to_wifi();
boolean listening_for_client();
void execute_request(WiFiClient client);
void turn_on_led();
void turn_off_led();
void parse_drive_direction_request(String header);

void forward();
void left();
void right();
void backward();

// ############################ WiFi Setup ############################
String ssid, password, username;

const int password_title_length = 9;
const int ssid_title_length = 5;
const int wpa2_username_title_length = 11;

// set web server port number to 80
WiFiServer server(80);

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

// Assign output variables to GPIO pins
const int output48 = 48;

// Drive variables
int drive_status = 0; // 0: Not in drive, 1: Forward, 2: Left, 3: Right, 4: Backward
int drive_speed = 25;
float turn_speed = 0.5; // will be replaced by the angle from the servo

// HTML web page variable
String html_code_str;
const char* html_code = 
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"    <meta charset=\"UTF-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"    <title>RC Car Controller</title>\n"
"    <style>\n"
"        body, html {\n"
"            margin: 0;\n"
"            padding: 0;\n"
"            width: 100%;\n"
"            height: 100%;\n"
"            display: flex;\n"
"            justify-content: center;\n"
"            align-items: center;\n"
"        }\n"
"        .container {\n"
"            display: flex;\n"
"            justify-content: center;\n"
"            align-items: center;\n"
"            flex-direction: column;\n"
"            height: auto;\n"
"        }\n"
"        .controller {\n"
"            display: flex;\n"
"            flex-direction: column;\n"
"            align-items: center;\n"
"        }\n"
"        .row {\n"
"            display: flex;\n"
"            justify-content: center;\n"
"            margin: 15px;\n"
"        }\n"
"        .button {\n"
"            width: 100px; /* Increased size */\n"
"            height: 100px; /* Increased size */\n"
"            margin: 10px;\n"
"            font-size: 48px; /* Adjusted for larger button size */\n"
"            cursor: pointer;\n"
"            background-color: #007bff; /* Blue background */\n"
"            color: #FFFFFF; /* White text */\n"
"            border: none;\n"
"            border-radius: 50%; /* Rounded corners */\n"
"            box-shadow: 0 4px 8px rgba(0,0,0,0.2); /* Subtle shadow */\n"
"            transition: background-color 0.3s, transform 0.3s ease, box-shadow 0.3s ease; /* Smooth transition for press effect and color change */\n"
"            -webkit-tap-highlight-color: transparent; /* Remove highlight on tap */\n"
"        }\n"
"        .button:active {\n"
"            background-color: #0056b3; /* Darker blue when pressed */\n"
"            transform: scale(0.9); /* Press effect */\n"
"            box-shadow: 0 2px 4px rgba(0,0,0,0.3); /* Deeper shadow when pressed */\n"
"        }\n"
"        #left.button {\n"
"            margin-right: 30px; /* Increase space between left and right buttons */\n"
"        }\n"
"        #right.button {\n"
"            margin-left: 30px; /* Increase space between left and right buttons */\n"
"        }\n"
"        .status_bar {\n"
"            color: #003ba1; /* White text */\n"
"            background-color: #d1e7ff; /* Blue background */\n"
"            padding: 20px; /* Increased padding */\n"
"            margin-bottom: 30px; /* Adjusted margin */\n"
"            width: 100%; /* Increased width */\n"
"            text-align: center;\n"
"            border-radius: 5px;\n"
"            font-size: 24px; /* Increased font size */\n"
"            box-shadow: 0 2px 4px rgba(0,0,0,0.2); /* Slight shadow for depth */\n"
"        }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"<div class=\"container\">\n"
"    <div class=\"controller\">\n"
"        <div id=\"drive_status\" class=\"status_bar\">Not in Drive</div>\n"
"        <div class=\"row\">\n"
"            <button id=\"forward\" class=\"button\">↑</button>\n"
"        </div>\n"
"        <div class=\"row\">\n"
"            <button id=\"left\" class=\"button\">←</button>\n"
"            <button id=\"right\" class=\"button\">→</button>\n"
"        </div>\n"
"        <div class=\"row\">\n"
"            <button id=\"backward\" class=\"button\">↓</button>\n"
"        </div>\n"
"    </div>\n"
"</div>\n"
"<script>\n"
"    function handlePress(buttonId) {\n"
"        fetch(`/press/${buttonId}`)\n"
"        .then(response => {\n"
"            if(response.ok) {\n"
"               if (buttonId === 'forward') {\n"
"                   document.getElementById('drive_status').innerHTML = 'Driving Forward';\n"
"               } else if (buttonId === 'left') {\n"
"                   document.getElementById('drive_status').innerHTML = 'Turning Left';\n"
"               } else if (buttonId === 'right') {\n"
"                   document.getElementById('drive_status').innerHTML = 'Turning Right';\n"
"               } else if (buttonId === 'backward') {\n"
"                    document.getElementById('drive_status').innerHTML = 'Driving Backward';\n"
"               }"
"                return response.json();\n"
"            }\n"
"            throw new Error('Request failed');\n"
"        })\n"
"        .then(data => console.log(`${buttonId} Pressed:`, data))\n"
"        .catch((error) => console.error('Error:', error));\n"
"    }\n"
"\n"
"    function handleRelease(buttonId) {\n"
"        fetch(`/release/${buttonId}`)\n"
"        .then(response => {\n"
"            if(response.ok) {\n"
"                document.getElementById('drive_status').innerHTML = `Not In Drive`;\n"
"                return response.json();\n"
"            }\n"
"            throw new Error('Request failed');\n"
"        })\n"
"        .then(data => console.log(`${buttonId} Released:`, data))\n"
"        .catch((error) => console.error('Error:', error));\n"
"    }\n"
"\n"
"    const buttons = ['forward', 'left', 'right', 'backward'];\n"
"    buttons.forEach(buttonId => {\n"
"        const button = document.getElementById(buttonId);\n"
"        button.addEventListener('mousedown', () => handlePress(buttonId));\n"
"        button.addEventListener('mouseup', () => handleRelease(buttonId));\n"
"    });\n"
"</script>\n"
"</body>\n"
"</html>";
