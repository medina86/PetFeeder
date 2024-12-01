
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <time.h>
#include <Servo.h>

#define WIFI_SSID "96e18a"
#define WIFI_PASSWORD "280882755"

#define API_KEY "AIzaSyDjiaaIxCXxoq7xeZGmiINZ1Jl3bp3WbaI"
#define DATABASE_URL "https://petfeeder-a3ad9-default-rtdb.europe-west1.firebasedatabase.app"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

Servo myservo;

const int trigPin = D6;
const int echoPin = D7;

int trajanjeOtvorenja = 3; // Vrijeme otvaranja u sekundama
unsigned long lockDuration = 4 * 3600000; // Vrijeme zaključavanja (4 sata u milisekundama)
unsigned long lastButtonOpenTime = 0; // Vrijeme posljednjeg otvaranja putem dugmeta
unsigned long lastSensorOpenTime = 0; // Vrijeme posljednjeg otvaranja putem senzora

bool timerActivated = false; // Da li je tajmer aktiviran

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  configTime(3600, 0, "pool.ntp.org", "time.nist.gov");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  myservo.attach(D4);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase authenticated successfully!");
  } else {
    Serial.printf("Firebase authentication failed: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Inicijalizuj ključ u bazi ako ne postoji
  if (!Firebase.RTDB.getInt(&fbdo, "/hranilica/timerTime")) {
    Firebase.RTDB.setInt(&fbdo, "/hranilica/timerTime", 0);
  }
}

void logOpening(const String& method) {
  if (method == "sensor") {
    time_t now = time(nullptr);
    char timeString[25];
    strftime(timeString, sizeof(timeString), "%d.%m.%Y %H:%M:%S", localtime(&now));

    String logMessage = "Hranilica se otvarala u " + String(timeString);

    if (Firebase.RTDB.setString(&fbdo, "/hranilica/lastOpenMessage", logMessage)) {
      Serial.println("Poruka zapisana u Firebase: " + logMessage);
    } else {
      Serial.println("Greška pri zapisivanju poruke: " + fbdo.errorReason());
    }
  }
}

long measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

void openFeeder(const String& method) {
  Serial.println("Otvaranje hranilice...");
  myservo.write(60);
  delay(1000);
  myservo.write(90);
  delay(trajanjeOtvorenja * 1000);
  myservo.write(120);
  delay(1000);
  myservo.write(90);
  Serial.println("Hranilica zatvorena.");

  logOpening(method);
}

void loop() {
  unsigned long currentTime = millis();

  long distance = measureDistance();
  Serial.print("Udaljenost: ");
  Serial.println(distance);

  if (distance < 15) {
    if ((currentTime - lastSensorOpenTime >= lockDuration) &&
        (currentTime - lastButtonOpenTime >= lockDuration)) {
      openFeeder("sensor");
      lastSensorOpenTime = currentTime;
    } else {
      Serial.println("Hranilica je zaključana za senzor.");
    }
  }

  if (Firebase.RTDB.getString(&fbdo, "/hranilica/komanda")) {
    String komanda = fbdo.stringData();
    if (komanda == "open") {
      openFeeder("button");
      lastButtonOpenTime = millis();

      Firebase.RTDB.setString(&fbdo, "/hranilica/komanda", "idle");
    }
  }


if (Firebase.RTDB.getInt(&fbdo, "/hranilica/timer")) {
    int timerTimestamp = fbdo.intData();
    time_t now = time(nullptr);

    // Proveri da li je tajmer validan i nije resetovan
    if (timerTimestamp > 0 && now >= timerTimestamp) {
        openFeeder("timer");

        // Resetuj tajmer nakon prvog otvaranja
        if (!Firebase.RTDB.setInt(&fbdo, "/hranilica/timer", 0)) {
            Serial.println("Greška pri resetovanju tajmera: " + fbdo.errorReason());
        } else {
            Serial.println("Tajmer resetovan nakon otvaranja.");
        }
    }
}


  delay(1000);
}


