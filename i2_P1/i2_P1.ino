#include <Wire.h>
#include <U8g2lib.h>
#define MPU 0x68

int16_t AcX, AcY, AcZ;

//VELOCIDAD
float slowX = 0;
float slowZ = 0;

// Rápido
float fastX = 0;
float fastZ = 0;

// Medida del Punto
float px = 64;
float py = 32;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

unsigned long lastRead = 0;

void resetMPU() {
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  delay(100);}

void setup() {
  Serial.begin(115200);

  //while(!Serial);
  Wire.begin();
  Wire.setTimeout(100);

  u8g2.begin();
  resetMPU(); }

void loop() {

  if (millis() - lastRead >= 20) {
    lastRead = millis();

    Wire.beginTransmission(MPU);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) {
      resetMPU();
      return;
    }

    int bytes = Wire.requestFrom(MPU, 6, true);
    if (bytes != 6) {
      resetMPU();
      return;
    }

    // Acelerómetro
    AcX = Wire.read() << 8 | Wire.read();
    AcY = Wire.read() << 8 | Wire.read();
    AcZ = Wire.read() << 8 | Wire.read();

    // Normalizar
    float ax = AcX / 16384.0;
    float az = AcZ / 16384.0;

    // FILTRO LENTO (gravedad + inclinación) 
    slowX = slowX * 0.98 + ax * 0.02;
    slowZ = slowZ * 0.98 + az * 0.02;

    // ACELERACIÓN 
    float dx = ax - slowX;
    float dz = az - slowZ;

    // SUAVIZADO 
    fastX = fastX * 0.8 + dx * 0.2;
    fastZ = fastZ * 0.8 + dz * 0.2;

    // MAPEO 
    px = 64 + fastX * 150;   
    py = 32 + fastZ * 150;

    // Clamp suave
    if (px < 5) px = 5;
    if (px > 122) px = 122;
    if (py < 5) py = 5;
    if (py > 58) py = 58;  }

  // 7. DIBUJAR
  u8g2.clearBuffer();
  u8g2.drawDisc((int)px, (int)py, RADIO_PUNTO);
  u8g2.sendBuffer();   }