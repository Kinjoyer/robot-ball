#include <TB6612_ESP32.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

#define PIN_NEO_PIXEL  16 
#define NUM_PIXELS     16 

Adafruit_NeoPixel NeoPixel(NUM_PIXELS, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800);

#define AIN1 13 // ESP32 Pin D13 to TB6612FNG Pin AIN1
#define BIN1 12 // ESP32 Pin D12 to TB6612FNG Pin BIN1
#define AIN2 14 // ESP32 Pin D14 to TB6612FNG Pin AIN2
#define BIN2 27 // ESP32 Pin D27 to TB6612FNG Pin BIN2
#define PWMA 26 // ESP32 Pin D26 to TB6612FNG Pin PWMA
#define PWMB 25 // ESP32 Pin D25 to TB6612FNG Pin PWMB
#define STBY 33 // ESP32 Pin D33 to TB6612FNG Pin STBY

const char* ssid ="OnePlus Nord 5";
const char* password = "228666777";

#define SERVO_PIN 18

int direction = 0;
int slider = 255;
int led = 0;
int servoState = 0;
int currentSpeedSigned = 0;   // 0 = stop, >0 = forward, <0 = backward
int currentTurnSpeed = 0;   // 0 = нет поворота, >0 = поворот с этой скоростью
bool isTurningRight = false;

Servo myservo;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

JSONVar readings;

// these constants are used to allow you to make your motor configuration
// line up with function names like forward.  Value can be 1 or -1
const int offsetA = 1;
const int offsetB = 1;

Motor motor1 = Motor(AIN1, AIN2, PWMA, offsetA, STBY, 5000, 8, 5); // 5000 - freq; 8 - resolution; 1 - channel
Motor motor2 = Motor(BIN1, BIN2, PWMB, offsetB, STBY, 5000, 8, 6);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  body {height:100vh;overflow:hidden;margin:0;}
  .parent {display:grid;grid-template-columns:repeat(2,1fr);grid-template-rows:repeat(2,1fr);height:100%;grid-gap:0;}
  .div1 {grid-area:1/2/3/3;}
  .div2 {grid-area:1/1/2/2;padding:0.5rem;}
  .div3 {
  grid-area: 2 / 1 / 3 / 2;
  display: flex;
  justify-content: center;
  gap: 1rem;
  align-items: center;
  padding: 0.5rem;
  }
  .parent-j {display:grid;grid-template-columns:repeat(3,1fr);grid-template-rows:repeat(3,1fr);height:100%;}
  .div-u {grid-area:1/2/2/3;border:1px solid gray;padding:2rem 3rem;border-radius:100% 100% 0 0;}
  .div-l {grid-area:2/1/3/2;border:1px solid gray;padding:2.95rem 2.4rem;border-radius:100% 0 0 100%;}
  .div-d {grid-area:3/2/4/3;border:1px solid gray;padding:2rem 3rem;border-radius:0 0 100% 100%;}
  .div-r {grid-area:2/3/3/4;border:1px solid gray;padding:2.95rem 2.4rem;border-radius:0 100% 100% 0;}
  .div-s {grid-area:2/2/3/3;border:1px solid #e95050;padding:3rem 2.3rem;}
  .analog {border:1px solid #d8d3d3;display:flex;border-radius:15px;justify-content:space-between;align-items:stretch;}
  .analog-value{font-size:5rem;}
  .btn-set{width:35%;border-radius:0 10px 10px 0;border:none;background:#524FF0;color:white;font-size:1.3rem}
  .analog-data{display:flex;flex-wrap:wrap;justify-content:flex-start;padding:0.5rem;}
  .div-slider{width:100%;}
  .slider{width:90%;}
  .circ-btn{border-radius:100%;border:1px solid red;width:180px;height:180px;background:none;}
  @media (orientation:portrait) {
    .parent {grid-template-columns:1fr;grid-template-rows:repeat(3,1fr);}
    .div1 {grid-area:3/1/4/2;}
    .div2 {grid-area:2/1/3/2;}
    .div3 {grid-area:1/1/2/2;}
  }
</style>
<div class="parent">
  <div class="div1">
    <div class="parent-j">
      <button class="div-u" id="btn-up">F</button>
      <button class="div-l" id="btn-lt">L</button>
      <button class="div-d" id="btn-dn">B</button>
      <button class="div-r" id="btn-rt">R</button>
      <button class="div-s" id="btn-sp">STOP</button>
    </div>
  </div>
  <div class="div2">
    <div class="analog">
      <div class="analog-data">
        <div class="analog-value" id="slider-txt">255</div>
        <div class="div-slider"><input id="slider-val" class="slider" type="range" value="255" min="0" max="255"/></div>
      </div> 
      <button class="btn-set" id="btn-set">SET</button>
    </div>
  </div>
  <div class="div3">
    <button class="circ-btn" id="btn-fire">SERVO</button>
    <button class="circ-btn" id="btn-led">LED</button>
  </div>
</div>

<script>
let ws = new WebSocket(`ws://${location.hostname}/ws`);
const sliderTxt = document.getElementById("slider-txt");
const sliderVal = document.getElementById("slider-val");
let fireState = 0, ledState = 0;

const send = (obj) => ws.send(JSON.stringify(obj));

document.getElementById('btn-up').addEventListener('touchstart', () => send({dir:11}));
document.getElementById('btn-up').addEventListener('touchend', () => send({dir:12}));

document.getElementById('btn-dn').addEventListener('touchstart', () => send({dir:21}));
document.getElementById('btn-dn').addEventListener('touchend', () => send({dir:22}));

document.getElementById('btn-lt').addEventListener('touchstart', () => send({dir:31}));
document.getElementById('btn-lt').addEventListener('touchend', () => send({dir:32}));

document.getElementById('btn-rt').addEventListener('touchstart', () => send({dir:41}));
document.getElementById('btn-rt').addEventListener('touchend', () => send({dir:42}));

document.getElementById('btn-set').addEventListener('click', () => {
  const val = parseInt(sliderVal.value);
  sliderTxt.textContent = val;
  send({slider: val});
});

document.getElementById('btn-led').addEventListener('click', () => {
  ledState = !ledState;
  document.getElementById('btn-led').style.background = ledState ? '#ff0000' : '#ffffff';
  send({led: ledState ? 1 : 0});
});

document.getElementById('btn-fire').addEventListener('click', () => {
  fireState = !fireState;
  document.getElementById('btn-fire').style.background = fireState ? '#ff0000' : '#ffffff';
  send({fire: fireState ? 1 : 0});
});

sliderVal.addEventListener('input', () => {
  sliderTxt.textContent = sliderVal.value;
});
</script>
)rawliteral";


String getSensorReadings(){
  readings["s"] = String(slider);
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    
    JSONVar myObject = JSON.parse((const char*)data);
    if (myObject.hasOwnProperty("slider")) {
      slider = (int)myObject["slider"];
    }
    else if (myObject.hasOwnProperty("fire")) {
      servoState = (int)myObject["fire"];      
    }
    else if (myObject.hasOwnProperty("led")) {
      led = (int)myObject["led"];      
    }
    else if (myObject.hasOwnProperty("dir")) {
      direction = (int)myObject["dir"];
      move(direction, slider);      
    }

    String sensorReadings = getSensorReadings();
    notifyClients(sensorReadings);
  }
}

// ==========================
//   ПЛАВНОЕ УСКОРЕНИЕ
// ==========================

void smoothForward(int targetSpeed, int currentSpeed) {
  if (targetSpeed == currentSpeed) {
    forward(motor1, motor2, targetSpeed);
    return;
  }

  int step = (targetSpeed > currentSpeed) ? 1 : -1;

  for (int s = currentSpeed; s != targetSpeed; s += step) {
    forward(motor1, motor2, s);
    delay(3);
  }

  forward(motor1, motor2, targetSpeed);
}

void smoothBack(int targetSpeed, int currentSpeed) {
  if (targetSpeed == currentSpeed) {
    back(motor1, motor2, targetSpeed);
    return;
  }

  int step = (targetSpeed > currentSpeed) ? 1 : -1;

  for (int s = currentSpeed; s != targetSpeed; s += step) {
    back(motor1, motor2, s);
    delay(3);
  }

  back(motor1, motor2, targetSpeed);
}

void moveForwardWithRamp(int targetSpeed) {
  int startSpeed = (currentSpeedSigned > 0) ? currentSpeedSigned : 0;
  smoothForward(targetSpeed, startSpeed);
  currentSpeedSigned = targetSpeed;
}

void moveBackWithRamp(int targetSpeed) {
  int startSpeed = (currentSpeedSigned < 0) ? -currentSpeedSigned : 0;
  smoothBack(targetSpeed, startSpeed);
  currentSpeedSigned = -targetSpeed;
}

void stopWithRamp() {
  if (currentSpeedSigned > 0) {
    smoothForward(0, currentSpeedSigned);
  } else if (currentSpeedSigned < 0) {
    smoothBack(0, -currentSpeedSigned);
  }

  motor1.brake();
  motor2.brake();
  currentSpeedSigned = 0;
}

void smoothTurn(int targetSpeed, int currentSpeed, bool isRight) {
  if (targetSpeed == currentSpeed) {
    if (isRight) {
      motor1.drive(targetSpeed);
      motor2.drive(-targetSpeed);
    } else {
      motor1.drive(-targetSpeed);
      motor2.drive(targetSpeed);
    }
    return;
  }

  int step = (targetSpeed > currentSpeed) ? 1 : -1;

  for (int s = currentSpeed; s != targetSpeed; s += step) {
    if (isRight) {
      motor1.drive(s);
      motor2.drive(-s);
    } else {
      motor1.drive(-s);
      motor2.drive(s);
    }
    delay(3);
  }

  // Установить конечную скорость
  if (isRight) {
    motor1.drive(targetSpeed);
    motor2.drive(-targetSpeed);
  } else {
    motor1.drive(-targetSpeed);
    motor2.drive(targetSpeed);
  }
}


void move(int direction, int speed) {
  int targetSpeed = slider;

  if (direction == 11) { // Forward start
    moveForwardWithRamp(targetSpeed);
  }
  else if (direction == 12) { // Forward stop
    stopWithRamp();
    currentTurnSpeed = 0; // выходим из поворота
  }
  else if (direction == 21) { // Backward start
    moveBackWithRamp(targetSpeed);
  }
  else if (direction == 22) { // Backward stop
    stopWithRamp();
    currentTurnSpeed = 0;
  }
  else if (direction == 31) { // Left start — плавный поворот
    smoothTurn(targetSpeed, currentTurnSpeed, false); // false = left
    currentTurnSpeed = targetSpeed;
    isTurningRight = false;
    currentSpeedSigned = 0; // не вперёд/назад
  }
  else if (direction == 32) { // Left stop → остановка с рампой
    smoothTurn(0, currentTurnSpeed, false);
    motor1.brake();
    motor2.brake();
    currentTurnSpeed = 0;
    currentSpeedSigned = 0;
  }
  else if (direction == 41) { // Right start
    smoothTurn(targetSpeed, currentTurnSpeed, true); // true = right
    currentTurnSpeed = targetSpeed;
    isTurningRight = true;
    currentSpeedSigned = 0;
  }
  else if (direction == 42) { // Right stop
    smoothTurn(0, currentTurnSpeed, true);
    motor1.brake();
    motor2.brake();
    currentTurnSpeed = 0;
    currentSpeedSigned = 0;
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
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

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup()
{
  NeoPixel.begin(); 
  Serial.begin(115200);

  myservo.setPeriodHertz(50);
  myservo.attach(SERVO_PIN, 500, 2400);
  myservo.write(0);

  initWiFi();
  initWebSocket();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  server.begin();
}

void loop()
{
  if (led == 1) {
    for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {          
      NeoPixel.setPixelColor(pixel, NeoPixel.Color(255, 255, 255));
    }
  }
  else {
    NeoPixel.clear();
  }
  NeoPixel.show();


  myservo.write(servoState ? 180 : 0);

  delay(20);

  //Serial.println(String(direction) + " " + String(slider) + " " + String(fire) + " " + String(led));
}
