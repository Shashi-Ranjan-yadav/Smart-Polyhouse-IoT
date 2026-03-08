#define BLYNK_TEMPLATE_ID   "TMPL3CK2BFxa8"
#define BLYNK_TEMPLATE_NAME "smart home"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

/* ================= WIFI ================= */
char auth[] = "WuqEY0OzDo-Nx_EmzsVlk0Y87JKLT3a5";
char ssid[] = "iQOO Z9 Lite 5G";
char pass[] = "Shashi123";

/* ================= INPUT PINS ================= */
#define SOIL_PIN   35
#define GAS_PIN    34
#define FLAME_PIN  27
#define DHTPIN     26
#define LDR_PIN    32

/* ================= OUTPUT RELAYS (ACTIVE LOW) ================= */
#define R_GARDEN   33
#define R_FIRE     25
#define R_FAN      19
#define R_EXH      15
#define R_HEATER   18
#define R_LIGHT    14
#define BUZZER     23

#define DHTTYPE DHT11

/* ================= THRESHOLDS ================= */
// Soil moisture
#define SOIL_ON    3500
#define SOIL_OFF   3200

// Gas
#define GAS_ON     2100
#define GAS_OFF    1800

// Temperature
#define FAN_ON     32
#define FAN_OFF    29

#define HEATER_ON  18
#define HEATER_OFF 22

/* ================= NOTIFICATION ================= */
#define NOTIFY_INTERVAL 7000   // 7 seconds

unsigned long lastFireNotify = 0;
unsigned long lastGasNotify  = 0;

/* ================= BLYNK LIVE DATA ================= */
unsigned long lastBlynkSend = 0;
#define BLYNK_SEND_INTERVAL 2000   // 2 seconds

/* ================= OBJECTS ================= */
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,16,2);

/* ================= STATES ================= */
bool soilState=false, gasState=false, fireState=false;
bool fanState=false, heaterState=false, ldrLightState=false;

/* ================= MANUAL FLAGS ================= */
bool mGarden=false, mFan=false, mExh=false, mHeater=false, mLight=false;
bool autoGarden=true, autoFan=true, autoExh=true, autoHeater=true, autoLight=true;

/* ================= TIMERS ================= */
unsigned long lastLCD=0;

void setup() {
  Serial.begin(115200);

  pinMode(FLAME_PIN, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);

  pinMode(R_GARDEN, OUTPUT);
  pinMode(R_FIRE, OUTPUT);
  pinMode(R_FAN, OUTPUT);
  pinMode(R_EXH, OUTPUT);
  pinMode(R_HEATER, OUTPUT);
  pinMode(R_LIGHT, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // All OFF initially
  digitalWrite(R_GARDEN, HIGH);
  digitalWrite(R_FIRE, HIGH);
  digitalWrite(R_FAN, HIGH);
  digitalWrite(R_EXH, HIGH);
  digitalWrite(R_HEATER, HIGH);
  digitalWrite(R_LIGHT, HIGH);
  digitalWrite(BUZZER, LOW);

  dht.begin();
  lcd.init();
  lcd.backlight();

  Blynk.begin(auth, ssid, pass);

  lcd.setCursor(0,0);
  lcd.print("SMART HOME");
  lcd.setCursor(0,1);
  lcd.print("SYSTEM READY");
  delay(1500);
  lcd.clear();
}

void loop() {
  Blynk.run();

  /* ========== READ SENSORS ========== */
  int soil = analogRead(SOIL_PIN);
  int gas  = analogRead(GAS_PIN);
  fireState = (digitalRead(FLAME_PIN) == LOW);

  // LDR: HIGH = DARK
  bool ldrDark = (digitalRead(LDR_PIN) == HIGH);

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  if(isnan(temp)) temp=0;
  if(isnan(hum)) hum=0;

  /* ========== AUTOMATION STATES (HYSTERESIS) ========== */
  if(soil > SOIL_ON) soilState=true;
  if(soil < SOIL_OFF) soilState=false;

  if(gas > GAS_ON) gasState=true;
  if(gas < GAS_OFF) gasState=false;

  if(temp > FAN_ON) fanState=true;
  if(temp < FAN_OFF) fanState=false;

  if(temp < HEATER_ON) heaterState=true;
  if(temp > HEATER_OFF) heaterState=false;

  ldrLightState = ldrDark;

  /* ========== RELAY CONTROL (AUTO / MANUAL) ========== */
  digitalWrite(R_GARDEN, autoGarden ? (soilState?LOW:HIGH) : (mGarden?LOW:HIGH));
  digitalWrite(R_FAN,    autoFan    ? (fanState?LOW:HIGH) : (mFan?LOW:HIGH));
  digitalWrite(R_HEATER, autoHeater ? (heaterState?LOW:HIGH):(mHeater?LOW:HIGH));
  digitalWrite(R_LIGHT,  autoLight  ? (ldrLightState?LOW:HIGH):(mLight?LOW:HIGH));

  digitalWrite(R_FIRE, fireState?LOW:HIGH);
  digitalWrite(R_EXH,  gasState?LOW:(autoExh?HIGH:(mExh?LOW:HIGH)));

  /* ========== BUZZER ========== */
  digitalWrite(BUZZER, (fireState||gasState)?HIGH:LOW);

  /* ========== BLYNK NOTIFICATIONS ========== */
  if(fireState && millis()-lastFireNotify>NOTIFY_INTERVAL){
    Blynk.logEvent("fire_alert","🔥 Fire detected!");
    lastFireNotify=millis();
  }
  if(!fireState) lastFireNotify=0;

  if(gasState && millis()-lastGasNotify>NOTIFY_INTERVAL){
    Blynk.logEvent("gas_leak","⚠️ Gas leakage detected!");
    lastGasNotify=millis();
  }
  if(!gasState) lastGasNotify=0;

  /* ========== BLYNK LIVE DATA UPDATE ========== */
  if (millis() - lastBlynkSend > BLYNK_SEND_INTERVAL) {
    lastBlynkSend = millis();
    Blynk.virtualWrite(V0, temp);             // Temperature
    Blynk.virtualWrite(V1, hum);              // Humidity
    Blynk.virtualWrite(V2, soil);             // Soil moisture
    Blynk.virtualWrite(V3, gas);              // Gas level
    Blynk.virtualWrite(V4, fireState ? 1 : 0);// Fire status
  }

  /* ========== LCD DISPLAY (PRIORITY) ========== */
  if(millis()-lastLCD>1000){
    lastLCD=millis();
    lcd.setCursor(0,0);
    lcd.print("T:");lcd.print((int)temp);
    lcd.print(" H:");lcd.print((int)hum);
    lcd.print(" S:");lcd.print(soil);

    lcd.setCursor(0,1);
    lcd.print("G:");lcd.print(gas);lcd.print(" ");
    if(fireState) lcd.print("FIRE ALERT");
    else if(gasState) lcd.print("GAS ALERT ");
    else if(ldrLightState) lcd.print("NIGHT MODE");
    else lcd.print("HOME SAFE ");
  }
}

/* ========== BLYNK MANUAL SWITCHES ========== */
BLYNK_WRITE(V5){ mGarden=param.asInt(); autoGarden=!mGarden; }
BLYNK_WRITE(V6){ mFan=param.asInt();    autoFan=!mFan; }
BLYNK_WRITE(V7){ mExh=param.asInt();    autoExh=!mExh; }
BLYNK_WRITE(V9){ mHeater=param.asInt(); autoHeater=!mHeater; }
BLYNK_WRITE(V10){ mLight=param.asInt(); autoLight=!mLight; }
