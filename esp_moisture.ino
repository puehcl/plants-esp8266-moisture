#include <functional>

#include <ESP8266WiFi.h>
#include <DHT.h>


namespace {

  DHT dht{D5, DHT22};

  constexpr int MOISTURE_SENSORS = 5;
  constexpr int SENSOR_NR_BITS = 4;
  
  std::array<int, MOISTURE_SENSORS> moistures = {0};
  double humidity = 0;
  double temperature = 0;

  const std::array<int, SENSOR_NR_BITS> analog_switch_pins{D0, D1, D2, D3};
  //const std::array<int, SENSOR_NR_BITS> pwr_switch_pins{D4, D5, D6, D7};

  void setSensorPin(const int sensor_nr, const int value) {
    digitalWrite(analog_switch_pins[sensor_nr], value);
    //digitalWrite(pwr_switch_pins[sensor_nr], value);
  }

  void turnOffSensors() {
    for(int i = 0; i < SENSOR_NR_BITS; ++i) // mux auf ausgang 15 stellen
      setSensorPin(i, 1); 
  }

  void turnOnSensor(int sensor_nr) {
    if(sensor_nr >= MOISTURE_SENSORS)
      return;

    for(int i = 0; i < SENSOR_NR_BITS; ++i) {
      setSensorPin(i, sensor_nr & 1);
      sensor_nr >>= 1;
    }
  }

  void switchToSensor(const int sensor_nr) {
    turnOffSensors();
    turnOnSensor(sensor_nr);
  }

  void switchToSensorAndWait(const int sensor_nr) {
    switchToSensor(sensor_nr);
    turnOnSensor(sensor_nr);
  }
  
  int readSensor(const int sensor_nr) {
    switchToSensor(sensor_nr);
    delay(500);
    return analogRead(A0);
  }

  void readAllSensors() {
    for(int i = 0; i < MOISTURE_SENSORS; ++i) {
      const int result = readSensor(i);
      moistures[i] = result;

      Serial.print("Sensor ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(result);
    }
    turnOffSensors();

    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    Serial.print("hum:");
    Serial.println(humidity);
    Serial.print("temp:");
    Serial.println(temperature);
  }


  WiFiServer server{80};
  
  void connectToWiFi() {
    WiFi.begin("Suche...", "agonzalongs");
  
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
  }

  void setupHttpServer() {
    server.begin();
  }

  String doubleToJsonField(const String& name, double number) {
    const String number_string{number};
    return "\"" + name + "\":" + number_string;
  }

  void handleHttpClient(WiFiClient& client) {

    const auto to_json_field = [](int nr, int value) {
      const String nr_str{nr};
      const String value_str{value};
      return "\"moisture" + nr_str + "\":" + value_str;
    };

    String result{'{'};
    for(int i = 0; i < MOISTURE_SENSORS; ++i) {
      result += to_json_field(i, moistures[i]);
      result += ',';
    }

    result += doubleToJsonField("temperature", temperature);
    result += ',';
    result += doubleToJsonField("humidity", humidity);
    result += '}';
    
//    const auto last = MOISTURE_SENSORS - 1;
//    result += to_json_field(last, moistures[last]);
//    result += '}';
    
    const char* cstr = result.c_str();
    const String cstr_length{strlen(cstr)};
    const String cstr_length_header{"Content-Length: " + cstr_length + "\r\n"};

    client.write("HTTP/1.1 200 OK\r\n");
    client.write(cstr_length_header.c_str());
    client.write("Content-Type: application/json\r\n\r\n");
    client.write(cstr);
    
    client.stop();
    Serial.println("Client disconnected");
  }

  unsigned long last_time;

}

void setup() {

  Serial.begin(115200);

  dht.begin();
  
  pinMode(A0, INPUT);
  for(const auto pin : analog_switch_pins) {
    pinMode(pin, OUTPUT);
  }
//  for(const auto pin : pwr_switch_pins) {
//    pinMode(pin, OUTPUT);
//  }
  readAllSensors();
  last_time = millis();
  
  connectToWiFi();
  setupHttpServer();
}

void loop() {

  WiFiClient client = server.available();
  if(client)
    handleHttpClient(client);

  delay(10);

  const auto current_time = millis();
  if(current_time < last_time) //overflow
    last_time = current_time;

  if(current_time - last_time >= 10000) {
    readAllSensors();
    last_time = current_time;
  }
  
}
