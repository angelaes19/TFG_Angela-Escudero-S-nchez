#include <Wire.h>

const int MPU = 0x68; 
int16_t AcX, AcY, AcZ;

void setup() {
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B); 
  Wire.write(0);    
  Wire.endTransmission(true);
  
  Serial.begin(9600);
  Serial.println(“Leyendo X, Y, Z...”);
  Serial.println(“---------------------------”);
}

void loop() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B); 
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true); 

  // DETECCIÓN DE 3 ejes
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();

  // IMPRIMACIÓN 
  Serial.print(“X: “); Serial.print(AcX);
  Serial.print(“ | Y: “); Serial.print(AcY);
  Serial.print(“ | Z: “); Serial.println(AcZ);
  // DETECTOR DE MOVIMIENTO  

  if (abs(AcX) > 5000 || abs(AcY) > 5000 || abs(AcZ - 16384) > 5000) {
    Serial.println(“   >>> ¡MOVIMIENTO DETECTADO! <<<”);
  }