#include <Arduino.h>
#include <Servo.h>

// Pines del sensor ultrasonico HC-SR04 (Shield V5)
#define TRIGGER_PIN 12
#define ECHO_PIN 13
#define SERVO_PIN 11

// Pines L298N conectados al Shield V5
// Motor Izquierdo
#define MOTOR_IZQ_IN1 8   // Dirección
#define MOTOR_IZQ_IN2 9   // Dirección
#define MOTOR_IZQ_ENA 5   // PWM velocidad (Shield pin 5)

// Motor Derecho
#define MOTOR_DER_IN3 10  // Dirección
#define MOTOR_DER_IN4 7   // Dirección
#define MOTOR_DER_ENB 6   // PWM velocidad (Shield pin 6)

// Constantes
#define DISTANCIA_MINIMA 25    // cm - Distancia de seguridad
#define VELOCIDAD_MINIMA 100   // Velocidad inicial
#define VELOCIDAD_MAXIMA 160   // Velocidad máxima
#define VELOCIDAD_GIRO 120     // Velocidad de giro

// Factores de corrección (motor izquierdo más rápido)
#define FACTOR_MOTOR_IZQ 0.90  // Reducido porque es más rápido
#define FACTOR_MOTOR_DER 1.0   // Motor derecho normal

// Variables de control
int velocidadActual = VELOCIDAD_MINIMA;
Servo servoSensor;
long distancia = 0;

// Declaración de funciones
long medirDistancia();
void avanzarConVelocidad(int velocidad);
void retroceder();
void girarDerecha();
void girarIzquierda();
void detener();
void acelerarProgresivo();

void setup() {
  Serial.begin(9600);
  
  // Configurar pines
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  pinMode(MOTOR_IZQ_IN1, OUTPUT);
  pinMode(MOTOR_IZQ_IN2, OUTPUT);
  pinMode(MOTOR_IZQ_ENA, OUTPUT);
  
  pinMode(MOTOR_DER_IN3, OUTPUT);
  pinMode(MOTOR_DER_IN4, OUTPUT);
  pinMode(MOTOR_DER_ENB, OUTPUT);
  
  // Inicializar servo
  servoSensor.attach(SERVO_PIN);
  servoSensor.write(90);  // Centro
  delay(500);
  
  Serial.println("🤖 Robot 2 Ruedas - Iniciando...");
  delay(1000);
  
  // ESCANEO INICIAL: Buscar mejor dirección al arrancar
  Serial.println("🔍 Escaneo inicial 360°");
  
  servoSensor.write(90);  // Frente
  delay(500);
  int distFrente = medirDistancia();
  Serial.print("  Frente: ");
  Serial.print(distFrente);
  Serial.println(" cm");
  
  servoSensor.write(10);  // Derecha
  delay(500);
  int distDerecha = medirDistancia();
  Serial.print("  Derecha: ");
  Serial.print(distDerecha);
  Serial.println(" cm");
  
  servoSensor.write(170);  // Izquierda
  delay(500);
  int distIzquierda = medirDistancia();
  Serial.print("  Izquierda: ");
  Serial.print(distIzquierda);
  Serial.println(" cm");
  
  servoSensor.write(90);  // Volver al centro
  delay(500);
  
  // Girar hacia donde hay más espacio
  if (distDerecha > distFrente && distDerecha > distIzquierda) {
    Serial.println("↪️ Orientándose a la DERECHA");
    girarDerecha();
    delay(400);
    detener();
  } else if (distIzquierda > distFrente && distIzquierda > distDerecha) {
    Serial.println("↩️ Orientándose a la IZQUIERDA");
    girarIzquierda();
    delay(400);
    detener();
  } else {
    Serial.println("⬆️ Frente tiene más espacio");
  }
  
  delay(1000);
  Serial.println("✅ ¡Iniciando navegación!\n");
}

void loop() {
  static unsigned long ultimaMedicion = 0;
  static unsigned long tiempoAvanzando = 0;
  static unsigned long tiempoSinCambios = 0;
  static bool estabaAvanzando = false;
  static int contadorAtasco = 0;
  static long distanciaAnterior = 400;
  static int cambiosDistancia = 0;
  static int ciclosSinCambio = 0;
  unsigned long tiempoActual = millis();
  
  // 1. MEDIR cada 150ms mientras el robot se mueve
  if (tiempoActual - ultimaMedicion >= 150) {
    ultimaMedicion = tiempoActual;
    distancia = medirDistancia();
    
    // Detectar si la distancia cambia (señal de que está avanzando realmente)
    if (abs(distancia - distanciaAnterior) > 3) {
      cambiosDistancia++;
      ciclosSinCambio = 0;
      tiempoSinCambios = tiempoActual;
    } else {
      // No hubo cambio significativo
      ciclosSinCambio++;
    }
    
    // BLOQUEO FÍSICO: Si avanza pero distancia NO cambia por 2 segundos
    if (estabaAvanzando && ciclosSinCambio > 13 && (tiempoActual - tiempoSinCambios > 2000)) {
      Serial.println("🚫 BLOQUEO FÍSICO detectado (obstáculo no visible)!");
      detener();
      delay(200);
      
      // Retroceder para liberar
      Serial.println("↩️ Retrocediendo de bloqueo físico...");
      retroceder();
      delay(400);
      detener();
      delay(300);
      
      // Scan de emergencia
      Serial.println("🔍 Scan después de bloqueo...");
      servoSensor.write(10);
      delay(400);
      int distDer = medirDistancia();
      
      servoSensor.write(170);
      delay(400);
      int distIzq = medirDistancia();
      
      servoSensor.write(90);
      delay(300);
      
      // Girar 120° hacia mejor lado
      if (distDer > distIzq) {
        Serial.println("↪️ Girando DERECHA para evitar bloqueo");
        girarDerecha();
        delay(600);
      } else {
        Serial.println("↩️ Girando IZQUIERDA para evitar bloqueo");
        girarIzquierda();
        delay(600);
      }
      detener();
      delay(300);
      
      velocidadActual = VELOCIDAD_MINIMA;
      ciclosSinCambio = 0;
      cambiosDistancia = 0;
      estabaAvanzando = false;
    }
    
    Serial.print("📏 Dist: ");
    Serial.print(distancia);
    Serial.print(" cm | Vel: ");
    Serial.print(velocidadActual);
    Serial.print(" | Atasco: ");
    Serial.println(contadorAtasco);
    
    // DETECTAR ATASCO: si está muy pegado o sensor bloqueado
    if (distancia < 5 || distancia == 0) {
      contadorAtasco++;
      if (contadorAtasco > 3) {
        // Rutina de escape por atasco
        Serial.println("🚨 ATASCADO! Rutina de escape");
        detener();
        delay(200);
        
        // Retroceder más tiempo
        retroceder();
        delay(400);
        detener();
        delay(300);
        
        // Escanear
        Serial.println("🔍 Escaneando después de atasco...");
        servoSensor.write(10);
        delay(400);
        int distDer = medirDistancia();
        
        servoSensor.write(170);
        delay(400);
        int distIzq = medirDistancia();
        
        servoSensor.write(90);
        delay(300);
        
        // Girar 180° hacia el lado con más espacio
        if (distDer > distIzq) {
          Serial.println("↪️ Girando 180° a la DERECHA");
          girarDerecha();
          delay(800);
        } else {
          Serial.println("↩️ Girando 180° a la IZQUIERDA");
          girarIzquierda();
          delay(800);
        }
        detener();
        delay(300);
        
        contadorAtasco = 0;
        velocidadActual = VELOCIDAD_MINIMA;
        distancia = 400;
      }
    } else if (distancia > 15) {
      // Si hay espacio, resetear contador
      contadorAtasco = 0;
    }
    
    distanciaAnterior = distancia;
  }
  
  // DETECTAR ATASCO POR TIEMPO: Si avanza más de 10 segundos
  if (tiempoActual - tiempoAvanzando > 10000 && estabaAvanzando) {
    // Verificar si hubo cambios en la distancia
    if (cambiosDistancia < 5) {
      // Casi no hubo cambios = ATASCADO sin avanzar
      Serial.println("⏱️ ATASCO DETECTADO por tiempo (10s sin avanzar)");
      detener();
      delay(200);
      
      // Retroceder
      Serial.println("↩️ Retrocediendo por atasco...");
      retroceder();
      delay(500);
      detener();
      delay(300);
      
      // Escanear para salir
      Serial.println("🔍 Scan de emergencia...");
      servoSensor.write(10);
      delay(400);
      int distDer = medirDistancia();
      Serial.print("  Derecha: ");
      Serial.println(distDer);
      
      servoSensor.write(170);
      delay(400);
      int distIzq = medirDistancia();
      Serial.print("  Izquierda: ");
      Serial.println(distIzq);
      
      servoSensor.write(90);
      delay(300);
      
      // Girar 180° hacia mejor lado
      if (distDer > distIzq) {
        Serial.println("↪️ Girando 180° DERECHA");
        girarDerecha();
        delay(800);
      } else {
        Serial.println("↩️ Girando 180° IZQUIERDA");
        girarIzquierda();
        delay(800);
      }
      detener();
      delay(300);
      
      velocidadActual = VELOCIDAD_MINIMA;
      contadorAtasco = 0;
    }
    
    // Resetear temporizador
    tiempoAvanzando = tiempoActual;
    cambiosDistancia = 0;
  }
  
  // 2. DECIDIR acción según distancia (sin detener las mediciones)
  if (distancia > DISTANCIA_MINIMA) {
    // ✅ Camino libre - AVANZAR con aceleración progresiva
    acelerarProgresivo();
    avanzarConVelocidad(velocidadActual);
    
    // Iniciar temporizador si acaba de empezar a avanzar
    if (!estabaAvanzando) {
      tiempoAvanzando = tiempoActual;
      cambiosDistancia = 0;
      estabaAvanzando = true;
    }
    
  } else if (distancia > 15) {
    // ⚠️ Obstáculo cerca - reducir velocidad
    velocidadActual = VELOCIDAD_MINIMA;
    avanzarConVelocidad(velocidadActual);
    
    // Continuar avanzando lento
    if (!estabaAvanzando) {
      tiempoAvanzando = tiempoActual;
      cambiosDistancia = 0;
      estabaAvanzando = true;
    }
    
  } else {
    // ❌ OBSTÁCULO detectado
    Serial.println("🛑 Obstáculo detectado!");
    detener();
    velocidadActual = VELOCIDAD_MINIMA;
    estabaAvanzando = false;  // Resetear temporizador
    delay(200);
    
    // 3. RETROCEDER un poco
    Serial.println("↩️ Retrocediendo...");
    retroceder();
    delay(200);
    detener();
    delay(300);
    
    // 4. ESCANEAR: medir frente, derecha e izquierda
    Serial.println("🔍 Escaneando 360°...");
    
    servoSensor.write(90);  // Frente
    delay(400);
    int distFrente = medirDistancia();
    Serial.print("  Frente: ");
    Serial.print(distFrente);
    Serial.println(" cm");
    
    servoSensor.write(10);  // Derecha
    delay(400);
    int distDerecha = medirDistancia();
    Serial.print("  Derecha: ");
    Serial.print(distDerecha);
    Serial.println(" cm");
    
    servoSensor.write(170);  // Izquierda
    delay(400);
    int distIzquierda = medirDistancia();
    Serial.print("  Izquierda: ");
    Serial.print(distIzquierda);
    Serial.println(" cm");
    
    servoSensor.write(90);  // Volver al centro
    delay(300);
    
    // 5. GIRAR hacia donde hay MAYOR distancia
    if (distFrente >= distDerecha && distFrente >= distIzquierda) {
      Serial.println("⬆️ Mejor dirección: FRENTE");
      // No girar, ya está bien orientado
    } else if (distDerecha > distIzquierda) {
      Serial.println("↪️ Girando a la DERECHA");
      girarDerecha();
      delay(400);  // ~90 grados
      detener();
      delay(200);
    } else {
      Serial.println("↩️ Girando a la IZQUIERDA");
      girarIzquierda();
      delay(400);  // ~90 grados
      detener();
      delay(200);
    }
    
    Serial.println("✅ Listo para continuar\n");
    contadorAtasco = 0;  // Resetear al evitar obstáculo exitosamente
    estabaAvanzando = false;  // Resetear temporizador
    cambiosDistancia = 0;
  }
  
  delay(10);  // Pausa mínima para no saturar
}

// Medir distancia con sensor HC-SR04
long medirDistancia() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  
  long duracion = pulseIn(ECHO_PIN, HIGH, 25000);
  long dist = (duracion * 0.034) / 2;
  
  if (dist == 0 || dist > 400) dist = 400;  // Límite máximo
  
  return dist;
}

// Acelerar progresivamente
void acelerarProgresivo() {
  if (velocidadActual < VELOCIDAD_MAXIMA) {
    velocidadActual += 5;
    if (velocidadActual > VELOCIDAD_MAXIMA) {
      velocidadActual = VELOCIDAD_MAXIMA;
    }
  }
}

// Funciones de control de motores
void avanzarConVelocidad(int velocidad) {
  int velIzq = velocidad * FACTOR_MOTOR_IZQ;
  int velDer = velocidad * FACTOR_MOTOR_DER;
  
  digitalWrite(MOTOR_IZQ_IN1, HIGH);
  digitalWrite(MOTOR_IZQ_IN2, LOW);
  analogWrite(MOTOR_IZQ_ENA, velIzq);
  
  digitalWrite(MOTOR_DER_IN3, HIGH);
  digitalWrite(MOTOR_DER_IN4, LOW);
  analogWrite(MOTOR_DER_ENB, velDer);
}

void retroceder() {
  int velIzq = VELOCIDAD_MAXIMA * FACTOR_MOTOR_IZQ;
  int velDer = VELOCIDAD_MAXIMA * FACTOR_MOTOR_DER;
  
  digitalWrite(MOTOR_IZQ_IN1, LOW);
  digitalWrite(MOTOR_IZQ_IN2, HIGH);
  analogWrite(MOTOR_IZQ_ENA, velIzq);
  
  digitalWrite(MOTOR_DER_IN3, LOW);
  digitalWrite(MOTOR_DER_IN4, HIGH);
  analogWrite(MOTOR_DER_ENB, velDer);
}

void girarDerecha() {
  int velGiro = VELOCIDAD_GIRO;
  
  // Motor izq avanza, motor der retrocede
  digitalWrite(MOTOR_IZQ_IN1, HIGH);
  digitalWrite(MOTOR_IZQ_IN2, LOW);
  analogWrite(MOTOR_IZQ_ENA, velGiro);
  
  digitalWrite(MOTOR_DER_IN3, LOW);
  digitalWrite(MOTOR_DER_IN4, HIGH);
  analogWrite(MOTOR_DER_ENB, velGiro);
}

void girarIzquierda() {
  int velGiro = VELOCIDAD_GIRO;
  
  // Motor izq retrocede, motor der avanza
  digitalWrite(MOTOR_IZQ_IN1, LOW);
  digitalWrite(MOTOR_IZQ_IN2, HIGH);
  analogWrite(MOTOR_IZQ_ENA, velGiro);
  
  digitalWrite(MOTOR_DER_IN3, HIGH);
  digitalWrite(MOTOR_DER_IN4, LOW);
  analogWrite(MOTOR_DER_ENB, velGiro);
}

void detener() {
  digitalWrite(MOTOR_IZQ_IN1, LOW);
  digitalWrite(MOTOR_IZQ_IN2, LOW);
  analogWrite(MOTOR_IZQ_ENA, 0);
  
  digitalWrite(MOTOR_DER_IN3, LOW);
  digitalWrite(MOTOR_DER_IN4, LOW);
  analogWrite(MOTOR_DER_ENB, 0);
}
