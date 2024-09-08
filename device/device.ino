#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "HX711.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include "config.h" // Include the config file

// Weight and debounce variables //////// 
int counter = 0;                    /////
float lastWeight = 0.0;             /////
float totalWeight = 0.0;            /////
bool isCashierMode = false;         /////
float stableWeight = 0.0;           /////
unsigned long weightStableTime = 0; /////
bool weightStable = false;          /////
/////////////////////////////////////////

// Create display and scale objects  ////////////////////////////////////////
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); ///
HX711 scale;                                                              ///
WiFiClient wifiClient;                                                    ///
/////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////                                   ////////////////////////////////////////////////////////
////////////////////////////////                SETUP              ////////////////////////////////////////////////////////
////////////////////////////////                                   ////////////////////////////////////////////////////////
void setup() { ////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  //// Initialize display //////////////////////////////
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  ///
    Serial.println(F("SSD1306 allocation failed"));  ///
    for (;;);                                        ///
  }                                                  ///
  //////////////////////////////////////////////////////

  //// Initialize scale ////////////////////////////////
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);  ///
  scale.set_scale(scale_calibrate);                  ///
  scale.tare();                                      ///
  //////////////////////////////////////////////////////

  // Initialize Wi-Fi  ///////////////////////////
  WiFi.begin(ssid, password);                /////
  while (WiFi.status() != WL_CONNECTED) {    /////
    displayCenteredText("WiFi...");          /////
    delay(1000);                             /////
    Serial.println("Connecting to WiFi..."); /////
  }                                          /////
  Serial.println("Connected to WiFi");       /////
  Serial.print("Device IP Address: ");       /////
  Serial.println(WiFi.localIP());            /////
  ////////////////////////////////////////////////

  displayCenteredText("Ready!");
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////                                   ////////////////////////////////////////////////////////
////////////////////////////////                LOOP               ////////////////////////////////////////////////////////
////////////////////////////////                                   ////////////////////////////////////////////////////////
void loop() { /////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// Ceck push button switch mode
  bool currentSwitchState = digitalRead(BUTTON_PIN) == LOW;
  if (currentSwitchState != isCashierMode) {
    delay(50);
    currentSwitchState = digitalRead(BUTTON_PIN) == LOW;
    isCashierMode = currentSwitchState;
    
    if (isCashierMode) {
      Serial.println("Switched to Cashier Mode");
      digitalWrite(LED_PIN, HIGH);
      DisplayModeCashier(counter);
    } else {
      Serial.println("Switched to Counting Mode");
      digitalWrite(LED_PIN, LOW);
      DisplayModeItemCounter();
    }
  }

  /// Weight Item Counter Logic
  if (!isCashierMode) {
    float currentWeight = scale.get_units(2); // Reduced number of readings for faster response

    if (!weightStable) {
      if (abs(currentWeight - stableWeight) <= WEIGHT_TOLERANCE) {
        if (millis() - weightStableTime >= STABILIZATION_TIME) {
          weightStable = true;
          
          float weightDifference = currentWeight - lastWeight;
          if (abs(weightDifference) >= weightThreshold) {
            if (weightDifference > 0) {
              counter++;
              totalWeight += weightDifference;
            } else if (counter > 0) { // Only decrement if counter is positive
              counter--;
              totalWeight -= abs(weightDifference);
            }
            lastWeight = currentWeight;

            if (totalWeight < NEAR_ZERO_THRESHOLD || counter <= 0) {
              totalWeight = 0.0;
              counter = 0;
            }

            DisplayModeItemCounter();
          }
        }
      } else {
        stableWeight = currentWeight;
        weightStableTime = millis();
      }
    } else {
      if (abs(currentWeight - stableWeight) > WEIGHT_TOLERANCE) {
        weightStable = false;
        stableWeight = currentWeight;
        weightStableTime = millis();
      }
    }
  }
  delay(50);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////                                   ////////////////////////////////////////////////////////
////////////////////////////////              FUNCTIONS            ////////////////////////////////////////////////////////
////////////////////////////////                                   ////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function to display the item counter mode on the OLED display
void DisplayModeItemCounter() {
  clearDisplayBlack();

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor((SCREEN_WIDTH - getTextWidth("Items", 2)) / 2, 0);
  display.println("Items");
  
  display.setTextSize(3);
  display.setCursor((SCREEN_WIDTH - getTextWidth(String(counter).c_str(), 3)) / 2, 20);
  display.print(counter);

  display.setTextSize(1);
  display.setCursor((SCREEN_WIDTH - getTextWidth(String(totalWeight).c_str(), 1)) / 2, 50);
  display.print(totalWeight);
  display.print(" g");

  display.display();
}

// Function to handle the display in Cashier mode and send a POST request
void DisplayModeCashier(int count) {
  displayCenteredText("Finding");
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    StaticJsonDocument<200> jsonDoc;

    // jsonDoc["id"] = 1;
    // jsonDoc["items"] = count;
    jsonDoc["ItemCount"] = count;

    String requestBody;
    serializeJson(jsonDoc, requestBody);

    http.begin(wifiClient, serverName);
    http.addHeader("Content-Type", "application/json");

    Serial.print("Sending POST request: ");
    Serial.println(requestBody);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);

      StaticJsonDocument<200> responseDoc;
      deserializeJson(responseDoc, response);
      // int no = responseDoc[0]["no"];
      int no = responseDoc[0]["message"];

      if(no == 0){
        displayCenteredText("Add Item");
      } else {
        clearDisplayBlack();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor((SCREEN_WIDTH - getTextWidth("Cashier", 2)) / 2, 10);
        display.println("Cashier");

        display.setTextSize(3);
        char noString[10];
        sprintf(noString, "No: %d", no);
        display.setCursor((SCREEN_WIDTH - getTextWidth(noString, 3)) / 2, 40);
        display.print("No: ");
        display.print(no);

        display.display();
      }

    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);

      clearDisplayBlack();
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.setCursor((SCREEN_WIDTH - getTextWidth("System", 2)) / 2, 10);
      display.println("System");
      display.setTextSize(3);
      display.setCursor((SCREEN_WIDTH - getTextWidth("Error !", 2)) / 2, 40);
      display.println("Error");
      display.display();
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

// Function to clear the display and fill it with black color
void clearDisplayBlack() {
  display.clearDisplay();
  display.fillScreen(BLACK);
  display.display();
}

// Function to get the width of the text in pixels
int getTextWidth(const char* text, int textSize) {
  return strlen(text) * 6 * textSize;
}

// Function to print text on OLED screen, maximized and centered
void displayCenteredText(const char* text) {
  clearDisplayBlack();
  // Start with the maximum font size
  int textSize = 1;
  int textWidth = getTextWidth(text, textSize);
  // Find the maximum font size that fits the screen width
  while (textWidth <= SCREEN_WIDTH && textSize <= 10) {
    textSize++;
    textWidth = getTextWidth(text, textSize);
  }
  // Step back to the last fitting size
  textSize--;
  textWidth = getTextWidth(text, textSize);
  // Calculate the x and y position to center the text
  int x = (SCREEN_WIDTH - textWidth) / 2;
  int y = (SCREEN_HEIGHT - (8 * textSize)) / 2; // Each line is 8 pixels high in default font
  // Set text size and print text
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(text);
  display.display();
}

void restartESP() {
  // Reset the NodeMCU after 3 seconds
  delay(3000);
  Serial.println("Restarting NodeMCU...");
  ESP.restart();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////