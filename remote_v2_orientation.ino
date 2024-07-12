#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_BNO08x.h>

Adafruit_BNO08x bno08x(-1);
sh2_SensorValue_t sensorValue;

const int button_1_pin = 3;
const int button_2_pin = 9;

const int DATAGRAM_REPEATS = 10;

int old_button_1_state = 0;
int old_button_2_state = 0;
int button_1_2_hold_counter = 0;


IPAddress server(192,168,1,52);
WiFiUDP conn;
uint8_t id;
int16_t old_w = 0;
int16_t old_x = 0;
int16_t old_y = 0;
int16_t old_z = 0;
uint8_t action_flag = 0;
int action_flag_repeats = 0;

void setup(void)
{
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
  Wire.setPins(1, 10);
  Serial.begin(115200);
  while(!bno08x.begin_I2C()) {
    delay(10);
  }
  Serial.println("BNO08x Found!");
  bno08x.enableReport(SH2_ARVR_STABILIZED_RV, 2500);
  Serial.print("Connecting");
  WiFi.begin("aleph", "shibboleth");
  Serial.print("Connected to wifi");
  conn.begin(5004);
  Serial.print("UDP client created");
  pinMode(button_1_pin, INPUT_PULLUP);
  pinMode(button_2_pin, INPUT_PULLUP);
  Serial.print("Setup completed");
}

void loop(void)
{
  if (bno08x.wasReset()) {
    Serial.print("sensor was reset ");
    bno08x.enableReport(SH2_ARVR_STABILIZED_RV, 2500);
  }
  
  if (!bno08x.getSensorEvent(&sensorValue)) {
    return;
  }

  int button_1_state = digitalRead(button_1_pin);
  int button_2_state = digitalRead(button_2_pin);
  Serial.print(button_1_state);
  Serial.print("  ");
  Serial.println(button_2_state);
  // Actions:
  // 1: Button 1 clicked down (i.e. hold and release is disregarded)
  // 2: Button 2 clicked down
  // 4: press and hold buttons 1 and 2 for a bit (triggers calibration)
  // these set the action flag
  // once the action flag is set, it can't have any other value for some time
  // this is because we can't guarantee any particular UDP datagram gets received
  // so we send the same action flag in a series of datagrams in the hope that it gets picked up
  // action flag unset -- we're free to try to assign to it
  if (action_flag == 0) {
    // first we just check if any button is being pressed; i.e. the current state is HIGH and the old state is LOW
    if (button_1_state != old_button_1_state && button_1_state == LOW) {
      action_flag = 1;
    }
    if (button_2_state != old_button_2_state && button_2_state == LOW) {
      action_flag = 2;
    }
    // if in fact button 1 and 3 are being held then we override the action flag
    if (button_1_state == old_button_1_state && button_2_state == old_button_2_state && button_1_state == LOW && button_2_state == LOW) {
      button_1_2_hold_counter++;
    }
    if (button_1_2_hold_counter > 500) {
      action_flag = 4; // put this back to 4 later
      button_1_2_hold_counter = 0;
    }
  }

  id = WiFi.localIP()[3];
  int16_t w = static_cast<int16_t>(sensorValue.un.arvrStabilizedRV.real * (1 << 14));
  int16_t x = static_cast<int16_t>(sensorValue.un.arvrStabilizedRV.i * (1 << 14));
  int16_t y = static_cast<int16_t>(sensorValue.un.arvrStabilizedRV.j * (1 << 14));
  int16_t z = static_cast<int16_t>(sensorValue.un.arvrStabilizedRV.k * (1 << 14));
  if(old_w != w || old_x != x || old_y != y || old_z != z) {
    conn.beginPacket(server, 5005);
    conn.write(reinterpret_cast<uint8_t*>(&id), sizeof(id));
    auto time = millis();
    conn.write(reinterpret_cast<const uint8_t*>(&time), sizeof(time));
    conn.write(reinterpret_cast<uint8_t*>(&w), sizeof(w));
    conn.write(reinterpret_cast<uint8_t*>(&x), sizeof(x));
    conn.write(reinterpret_cast<uint8_t*>(&y), sizeof(y));
    conn.write(reinterpret_cast<uint8_t*>(&z), sizeof(z));
    conn.write(reinterpret_cast<uint8_t*>(&action_flag), sizeof(action_flag));
    conn.endPacket();
/**
    Serial.print(" Rotation Vector - r: ");
    Serial.print(static_cast<int16_t>(sensorValue.un.arvrStabilizedRV.real * (1 << 14)));
    Serial.print(" i: ");
    Serial.print(static_cast<int16_t>(sensorValue.un.arvrStabilizedRV.i * (1 << 14)));
    Serial.print(" j: ");
    Serial.print(static_cast<int16_t>(sensorValue.un.arvrStabilizedRV.j * (1 << 14)));
    Serial.print(" k: ");
    Serial.println(static_cast<int16_t>(sensorValue.un.arvrStabilizedRV.k * (1 << 14)));
    **/
    if (action_flag != 0 && action_flag_repeats > DATAGRAM_REPEATS) {
      action_flag = 0;
      action_flag_repeats = 0;
    } else if (action_flag != 0) {
      action_flag_repeats++;
    }
    old_w = w;
    old_x = x;
    old_y = y;
    old_z = z;
  }

  old_button_1_state = button_1_state;
  old_button_2_state = button_2_state;
}
