#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int MPU = 0x68;

// VARIABLES DE POSICIÓN
float xPos = 64.0;
float yPos = 32.0;

// CALIBRAR (EL “PUNTO CERO”)
int16_t offsetX = 64;
int16_t offsetY = 32;

// CONFIGURACIÓN
const float elasticidad = 0.20;  
const float multiplicador = 0.04; // Un poco más alto para que sea exagerado
const int radioCirculo = 4;

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B); 
  Wire.write(0);    
  Wire.endTransmission(true);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(20, 25);
  display.print(“CALIBRANDO...”);
  display.display();

  // LEER EL SENSOR 10 VECES PARA SACAR EL PROMEDIO DEL ERROR INICIAL
  long sumX = 0;
  long sumY = 0;
  for(int i = 0; i < 10; i++) {
    Wire.beginTransmission(MPU);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU, 4, true);
    sumX += (Wire.read() << 8 | Wire.read());
    sumY += (Wire.read() << 8 | Wire.read());
    delay(50);  }
  offsetX = sumX / 10;
  offsetY = sumY / 10;

  display.clearDisplay(); }

void loop() {
  // 1. LEER ACELERACIÓN
  Wire.beginTransmission(MPU);
  Wire.write(0x3B); 
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 4, true); 

  int16_t rawX = Wire.read() << 8 | Wire.read();
  int16_t rawY = Wire.read() << 8 | Wire.read();

  // 2. CALIBRACIÓN
  float limpioX = rawX - offsetX;
  float limpioY = rawY - offsetY;
  float objetivoX = 64.0 + (limpioY * multiplicador);
  float objetivoY = 32.0 + (limpioX * multiplicador);

  // 4. SUAVIZADO
  xPos = xPos + (objetivoX - xPos) * elasticidad;
  yPos = yPos + (objetivoY - yPos) * elasticidad;

  // 5. NUNCA SALE DE LA PANTALLA
  xPos = constrain(xPos, radioCirculo, SCREEN_WIDTH - radioCirculo);
  yPos = constrain(yPos, radioCirculo, SCREEN_HEIGHT - radioCirculo);

  // 6. DIBUJAR
  display.clearDisplay();
  display.fillCircle((int)xPos, (int)yPos, radioCirculo, WHITE);
  display.display();  
  delay(10); 
}
