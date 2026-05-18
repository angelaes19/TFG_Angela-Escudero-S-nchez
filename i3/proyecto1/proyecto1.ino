#include <Arduino.h>
#include <LSM6DS3.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "cupra_logo_bitmaps.h"

// Aquí le decimos al cerebro qué hardware tenemos conectado:
// Un sensor de movimiento (IMU) y una pantalla OLED de 128x64 píxeles.
LSM6DS3 myIMU(I2C_MODE, 0x6A);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// CONFIGURACIÓN DE LA PANTALLA
const float CENTRO_X = 64.0f; // El centro horizontal de la pantalla
const float CENTRO_Y = 32.0f; // El centro vertical de la pantalla
const int   ANCHO    = 128;       // La pantalla tiene 128 píxeles de ancho
const int   ALTO     = 64;        // Y 64 píxeles de alto
const int   RADIO_PUNTO = 4;      // El tamaño del puntito que se va a mover (4 píxeles)
const int   MARGEN      = 3;      // Para que el puntito no se salga del todo por los bordes
const unsigned long SPLASH_MS = 1000; // Cuánto dura la animación del logo al encender (1 segundo)

// --- POSICIÓN Y VELOCIDAD DEL PUNTITO ---
float px = CENTRO_X; // El puntito empieza en el centro de la pantalla (Eje X)
float py = CENTRO_Y; // El puntito empieza en el centro de la pantalla (Eje Y)
float vx = 0.0f;     // Al principio el puntito está quieto, no se mueve a los lados...
float vy = 0.0f;     // ...ni tampoco se mueve hacia arriba o abajo.

// FILTRO DE TRASLACION LINEAL 
// Se usa acelerometro filtrado, no inclinacion. El giroscopio solo bloquea giros.
float prevRawH = 0.0f;
float prevRawV = 0.0f;
float hpH = 0.0f;
float hpV = 0.0f;

// --- AJUSTES DE "SENSACIÓN" (Para que se mueva bien y no a lo loco) ---
const unsigned long INTERVALO_MS = 10;    // El código revisa el sensor cada 10 milisegundos
const int   CALIB_MUESTRAS = 200;         // Al encender, toma 200 medidas para saber qué es "estar quieto"
const float HP_ALPHA       = 0.80f;       // Filtro: ayuda a olvidar los golpes viejos rápido
const float ZONA_MUERTA    = 0.04f;       // Si tiemblas un poquito, el puntito ignora ese temblor
const float GANANCIA_H     = 23500.0f;    // Fuerza del movimiento horizontal
const float GANANCIA_V     = 14000.0f;    // Fuerza del movimiento vertical (más baja porque la pantalla es más bajita)
const float FRICCION       = 0.78f;       // Como si la pantalla tuviera "goma". Frena el punto para que no patine infinitamente
const float VEL_MINIMA     = 0.10f;       // Si va super lento, lo paramos del todo
const float FRENO_CAMBIO   = 0.5f;        // Si el punto va a la izquierda y tú empujas a la derecha, frena a la mitad
const float RESPUESTA_INICIAL = 0.1f;     // Qué tan rápido reacciona al primer empujón
const float VEL_OBJ_H      = 1050.0f;     // Velocidad máxima que queremos que alcance de lado
const float VEL_OBJ_V      = 700.0f;      // Velocidad máxima que queremos que alcance hacia arriba/abajo
const float SIGNO_H        = 1.0f;        // Dirección: Ajusta si mueves a la izquierda y el punto va a donde debe
const float SIGNO_V        = 1.0f;        // Dirección: Ajusta el sentido vertical

// Si se giras la carcasa, el punto se congela.
const float ROT_UMBRAL_DPS = 35.0f; // Límite de giro: si giras más rápido que esto, el punto se bloquea
const unsigned long GIRO_COOLDOWN_MS = 90; // Cuánto tiempo (en milisegundos) se queda congelado tras girar

// RETORNO AL CENTRO 
const unsigned long TIEMPO_IDLE_MS = 120; // Si dejas de moverte 120ms, vuelve al centro
const float FUERZA_RETORNO = 16.0f; // Con qué fuerza el muelle invisible tira del punto hacia el centro
const float FRENO_RETORNO  = 0.95f; // Freno para que cuando llegue al centro no se quede rebotando eternamente

// CRONOMETRO INTERNO (en milisegundos)
unsigned long lastUpdate = 0;
unsigned long lastMovimiento = 0;
unsigned long lastGiro = 0;

// FUNCIÓN: Dibuja el logo de Cupra centrado en la pantalla
void dibujarLogoCupra(const uint8_t *bitmap, uint8_t ancho, uint8_t alto) {
  uint8_t x = (ANCHO - ancho) / 2;
  uint8_t y = (ALTO - alto) / 2;
  u8g2.drawXBMP(x, y, ancho, alto, bitmap);
}
// FUNCIÓN: Hace una animación donde el logo de Cupra aparece pequeño y va creciendo/desvaneciéndose
void animarLogoCupra() {
  delay(500);
  unsigned long inicio = millis();

  while (millis() - inicio < SPLASH_MS) {
    unsigned long elapsed = millis() - inicio;
    const uint8_t *bitmap = CUPRA_LOGO_FULL;
    uint8_t ancho = CUPRA_LOGO_FULL_W;
    uint8_t alto = CUPRA_LOGO_FULL_H;
    
// Dependiendo del milisegundo en el que estemos, elige una imagen u otra (efecto animación)
    if (elapsed < 250) {
      bitmap = CUPRA_LOGO_SMALL;
      ancho = CUPRA_LOGO_SMALL_W;
      alto = CUPRA_LOGO_SMALL_H;
    } else if (elapsed < 520) {
      bitmap = CUPRA_LOGO_MID;
      ancho = CUPRA_LOGO_MID_W;
      alto = CUPRA_LOGO_MID_H;
    } else if (elapsed < 780) {
      bitmap = CUPRA_LOGO_FULL;
      ancho = CUPRA_LOGO_FULL_W;
      alto = CUPRA_LOGO_FULL_H;
    } else if (elapsed < 910) {
      bitmap = CUPRA_LOGO_FADE_1;
      ancho = CUPRA_LOGO_FADE_1_W;
      alto = CUPRA_LOGO_FADE_1_H;
    } else {
      bitmap = CUPRA_LOGO_FADE_2;
      ancho = CUPRA_LOGO_FADE_2_W;
      alto = CUPRA_LOGO_FADE_2_H;
    }

    u8g2.clearBuffer();     // Borra la pantalla anterior
    dibujarLogoCupra(bitmap, ancho, alto);  // Dibuja el logo que toca
    u8g2.sendBuffer(); // Muestra el dibujo en la pantalla real
    delay(50); // Espera 50 ms
    }

  u8g2.clearBuffer(); // Al terminar la animación, limpia la pantalla
  u8g2.sendBuffer();
}

// FUNCIÓN: Si el sensor de movimiento está roto o desconectado, avisa en pantalla
void dibujarErrorIMU() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(10, 35, "IMU ERROR");
  u8g2.sendBuffer();
}

// CONFIGURACIÓN INICIAL (Se ejecuta SOLO UNA VEZ al encender) 
void setup() {
  Serial.begin(115200);
  u8g2.begin();
  animarLogoCupra();

// Intenta encender el sensor. Si falla (da un número distinto de 0), rompe el programa mostrando el error
  if (myIMU.begin() != 0) {
    dibujarErrorIMU();
    while (1);
  }

  // CALIBRACIÓN: Lee el sensor 200 veces muy rápido para saber cómo está la gravedad en a tiempo real
  float sumaH = 0.0f;
  float sumaV = 0.0f;
  for (int i = 0; i < CALIB_MUESTRAS; i++) {
    sumaH += myIMU.readFloatAccelY(); // Lee el eje Y del sensor
    sumaV += myIMU.readFloatAccelZ(); // Lee el eje Z del sensor
    delay(2);
  }

  // Saca la media. Este resultado será nuestro "punto cero" (la caja está quieta)
  prevRawH = sumaH / CALIB_MUESTRAS;
  prevRawV = sumaV / CALIB_MUESTRAS;

  unsigned long ahora = millis();
  lastUpdate = ahora;
  lastMovimiento = ahora;

  // Dibuja el puntito en el centro por primera vez
  u8g2.clearBuffer();
  u8g2.drawDisc((int)CENTRO_X, (int)CENTRO_Y, RADIO_PUNTO);
  u8g2.sendBuffer();
}

// BUCLE PRINCIPAL
void loop() {
  unsigned long ahora = millis();
  if (ahora - lastUpdate < INTERVALO_MS) return;

  float dt = (ahora - lastUpdate) * 0.001f;
  lastUpdate = ahora;

  // 1. LEER ACELERACION LINEAL EN LOS EJES USADOS POR LA PANTALLA
  float rawH = myIMU.readFloatAccelY();
  float rawV = myIMU.readFloatAccelZ();

  // 2. PASO ALTO: elimina gravedad, inclinacion mantenida y deriva lenta.
  hpH = HP_ALPHA * (hpH + rawH - prevRawH);
  hpV = HP_ALPHA * (hpV + rawV - prevRawV);
  prevRawH = rawH;
  prevRawV = rawV;

  // 3. DETECTAR ROTACION: se usa solo para bloquear, no para mover el punto.
  float gyroX = myIMU.readFloatGyroX();
  float gyroY = myIMU.readFloatGyroY();
  float gyroZ = myIMU.readFloatGyroZ();
  float rotDps = sqrtf(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ); // Calcula la velocidad total de rotación en cualquier dirección
  float factorTraslacion = 1.0f;

  if (rotDps > ROT_UMBRAL_DPS) {
    lastGiro = ahora;
  }

  if (ahora - lastGiro < GIRO_COOLDOWN_MS) {
    // Limpia el resto del paso alto para que un giro brusco no deje cola.
    factorTraslacion = 0.0f;
    hpH *= 0.45f;
    hpV *= 0.45f;
  }

  // 4. MAPEO INVERSO: ajustado para la placa montada por debajo de la PCB.
  float moveH = SIGNO_H * hpH * factorTraslacion;
  float moveV = SIGNO_V * hpV * factorTraslacion;

  if (fabsf(moveH) < ZONA_MUERTA) moveH = 0.0f;
  if (fabsf(moveV) < ZONA_MUERTA) moveV = 0.0f;

  bool hayTraslacion = (moveH != 0.0f || moveV != 0.0f);
  if (hayTraslacion) {
    lastMovimiento = ahora;
  }

  // 5. FISICA DEL PUNTO
  if ((moveH > 0.0f && vx < 0.0f) || (moveH < 0.0f && vx > 0.0f)) {
    vx *= FRENO_CAMBIO;
  }
  if ((moveV > 0.0f && vy < 0.0f) || (moveV < 0.0f && vy > 0.0f)) {
    vy *= FRENO_CAMBIO;
  }

  if (moveH != 0.0f) {
    vx += (moveH * VEL_OBJ_H - vx) * RESPUESTA_INICIAL;
  }
  if (moveV != 0.0f) {
    vy += (moveV * VEL_OBJ_V - vy) * RESPUESTA_INICIAL;
  }

  vx += moveH * GANANCIA_H * dt;
  vy += moveV * GANANCIA_V * dt;

  if (!hayTraslacion && (ahora - lastMovimiento > TIEMPO_IDLE_MS)) {
    // Calcula la distancia al centro y añade una fuerza que tira del punto hacia allí
    vx += (CENTRO_X - px) * FUERZA_RETORNO * dt;
    vy += (CENTRO_Y - py) * FUERZA_RETORNO * dt;
    vx *= FRENO_RETORNO; // F freno especial para que no actúe como el retorno loco
    vy *= FRENO_RETORNO;
  }

  vx *= FRICCION;
  vy *= FRICCION;

  if (fabsf(vx) < VEL_MINIMA) vx = 0.0f;
  if (fabsf(vy) < VEL_MINIMA) vy = 0.0f;

  // ACTUALIZA LA POSICIÓN: Suma la velocidad calculada a la posición del píxel
  px += vx * dt;
  py += vy * dt;

  // 6. LIMITES DE PANTALLA
  if (px < MARGEN)          { px = MARGEN;          vx = 0.0f; }
  if (px > ANCHO - MARGEN)  { px = ANCHO - MARGEN;  vx = 0.0f; }
  if (py < MARGEN)          { py = MARGEN;          vy = 0.0f; }
  if (py > ALTO - MARGEN)   { py = ALTO - MARGEN;   vy = 0.0f; }

// 7. DIBUJAR EL RESULTADO
  u8g2.clearBuffer();                         // Borra el dibujo del fotograma anterior
  u8g2.drawDisc((int)px, (int)py, RADIO_PUNTO); // Dibuja el círculo en su nueva posición (px, py)
  u8g2.sendBuffer();                          // Envía el dibujo a los ojos del usuario
}
