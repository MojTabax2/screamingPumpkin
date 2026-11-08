#include <DFRobotDFPlayerMini.h>
#include <SoftwareSerial.h>

#define FSR A3
#define LED 6
SoftwareSerial mySerial(8, 9);
DFRobotDFPlayerMini myDFPlayer;
bool isPressed;
bool wasPressed;

void setup() {
  // put your setup code here, to run once:
  isPressed = false;
  wasPressed = false;
  pinMode (A3, INPUT);
  pinMode (LED, OUTPUT);
  digitalWrite(LED, LOW);
  Serial.begin (9600);
  mySerial.begin(9600);

  if (!myDFPlayer.begin(mySerial, /*isACK = */false, /*doReset = */false)) {  //Use serial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true){
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }

  myDFPlayer.volume(25);  //Set volume value. From 0 to 30
}

void loop() {
  // put your main code here, to run repeatedly:
  if (analogRead(A3)>500){
    Serial.println("Detected!");
    isPressed = true;
    if (wasPressed == false){
      digitalWrite(LED, HIGH);
      myDFPlayer.play(1);
    }
  } else {
    digitalWrite(LED, LOW);
    isPressed = false;
    myDFPlayer.stop();
  }
  wasPressed = isPressed;
}
