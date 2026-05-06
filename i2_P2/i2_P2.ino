#include <Arduino.h>
#include <LSM6DS3.h>
#include <U8g2lib.h>
#include <Wire.h>

LSM6DS3 myIMU(I2C_MODE, 0x6A);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- POSICIÓN Y VELOCIDAD ---
float px = 64.0, py = 32.0;
float vx = 0.0,  vy = 0.0;

// --- VECTOR GRAVEDAD ESTIMADO ---
float gvX = 0, gvY = 0, gvZ = 0;

// Filtro complementario equilibrado
const float COMP_ALPHA = 0.60; 
// Filtro paso alto más permisivo (subido de 0.70 a 0.85 para captar sutiles)
const float HP_ALPHA   = 0.85; 

// --- AJUSTES PARA MOVIMIENTOS PEQUEÑOS ---
const float ZONA_MUERTA   = 0.04;    // MUCHO MÁS SENSIBLE (antes 0.15)
const float GANANCIA_H    = 25000.0; // MÁS POTENCIA para movimientos cortos
const float GANANCIA_V    = 25000.0; 
const float FRICCION      = 0.82;    // Menos fricción para que el punto fluya mejor
const float VEL_MINIMA    = 0.1;
const int   MARGEN        = 5;
const int   RADIO_PUNTO   = 4;

// --- RETORNO AL CENTRO SUAVE ---
const unsigned long TIEMPO_IDLE_MS = 100;
const float FUERZA_RETORNO         = 12.0; 

// --- TIMING Y FILTROS ---
unsigned long lastUpdate = 0;
unsigned long lastMovimiento = 0;
const unsigned long INTERVALO_MS = 10;
float prevLinH = 0, prevLinV = 0;
float hpH = 0, hpV = 0;
int contadorReposo = 0;

void setup() {
  Serial.begin(115200);
  u8g2.begin();

  if (myIMU.begin() != 0) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(10, 30, “ERROR IMU”);
    u8g2.sendBuffer();
    while (1);
  }

  // Calibración (Esencial que la placa esté quieta aquí)
  gvX = 0; gvY = 0; gvZ = 0;
  for (int i = 0; i < 200; i++) {
    gvX += myIMU.readFloatAccelX();
    gvY += myIMU.readFloatAccelY();
    gvZ += myIMU.readFloatAccelZ();
    delay(2);
  }
  gvX /= 200.0; gvY /= 200.0; gvZ /= 200.0;

  lastUpdate = millis();
}

void loop() {
  unsigned long ahora = millis();
  if (ahora - lastUpdate < INTERVALO_MS) return;

  float dt = (ahora - lastUpdate) * 0.001f;
  lastUpdate = ahora;

  // 1. LEER SENSORES
  float ax = myIMU.readFloatAccelX();
  float ay = myIMU.readFloatAccelY();
  float az = myIMU.readFloatAccelZ();
  float wx = myIMU.readFloatGyroX() * DEG_TO_RAD;
  float wy = myIMU.readFloatGyroY() * DEG_TO_RAD;
  float wz = myIMU.readFloatGyroZ() * DEG_TO_RAD;

  // 2. ESTIMACIÓN RÁPIDA DE GRAVEDAD
  float rgX = gvX + (wy * gvZ - wz * gvY) * dt;
  float rgY = gvY + (wz * gvX - wx * gvZ) * dt;
  float rgZ = gvZ + (wx * gvY - wy * gvX) * dt;

  gvX = COMP_ALPHA * rgX + (1.0f - COMP_ALPHA) * ax;
  gvY = COMP_ALPHA * rgY + (1.0f - COMP_ALPHA) * ay;
  gvZ = COMP_ALPHA * rgZ + (1.0f - COMP_ALPHA) * az;

  float mag = sqrtf(gvX * gvX + gvY * gvY + gvZ * gvZ);
  if (mag > 0.01f) { gvX /= mag; gvY /= mag; gvZ /= mag; }

  // 3. ACELERACIÓN LINEAL
  float linY = ay - gvY; 
  float linZ = az - gvZ; 

  // 4. PASO ALTO (Filtrado sutil)
  hpH = HP_ALPHA * (hpH + linY - prevLinH);
  hpV = HP_ALPHA * (hpV + linZ - prevLinV);
  prevLinH = linY;
  prevLinV = linZ;

  // 5. MAPEO E INVERSIÓN (IZQ=IZQ, ARRIBA=ARRIBA)
  float moveH = -hpH; 
  float moveV = -hpV;

  // Zona muerta reducida para captar lo sutil
  if (fabsf(moveH) < ZONA_MUERTA) moveH = 0;
  if (fabsf(moveV) < ZONA_MUERTA) moveV = 0;

  // 6. LÓGICA DE MOVIMIENTO
  if (moveH != 0 || moveV != 0) {
    lastMovimiento = ahora;
    contadorReposo = 0;
  } else {
    contadorReposo++;
  }

  // Integrar aceleración a velocidad
  vx += moveH * GANANCIA_H * dt;
  vy += moveV * GANANCIA_V * dt;

  // Retorno al centro si no se mueve la placa
  if (ahora - lastMovimiento > TIEMPO_IDLE_MS) {
    vx += (64.0f - px) * FUERZA_RETORNO * dt;
    vy += (32.0f - py) * FUERZA_RETORNO * dt;
    
    // Frenado extra cuando está volviendo al centro
    vx *= 0.95f;
    vy *= 0.95f;
  }

  // Fricción constante
  vx *= FRICCION;
  vy *= FRICCION;

  // Aplicar posición
  px += vx * dt;
  py += vy * dt;

  // Bordes
  if (px < MARGEN) { px = MARGEN; vx = 0; }
  if (px > 125 - MARGEN) { px = 125 - MARGEN; vx = 0; }
  if (py < MARGEN) { py = MARGEN; vy = 0; }
  if (py > 61 - MARGEN) { py = 61 - MARGEN; vy = 0; }

  // 7. DIBUJAR
  u8g2.clearBuffer();
// u8g2.drawPixel(64, 32); // Centro
  u8g2.drawDisc((int)px, (int)py, RADIO_PUNTO);
  u8g2.sendBuffer();
}