#include <Arduino.h>
#include <Servo.h>

// Pines del sensor ultrasonico HC-SR04 (Shield V5)
#define TRIGGER_PIN 12
#define ECHO_PIN 13
#define SERVO_PIN 11

// Pines L298N conectados al Shield V5
// Motor Izquierdo
#define MOTOR_IZQ_IN1 8   // Dirección
#define MOTOR_IZQ_IN2 2   // Dirección (movido a pin 2)
#define MOTOR_IZQ_ENA 5   // PWM velocidad (Shield pin 5)

// Motor Derecho
#define MOTOR_DER_IN3 4   // Dirección (movido a pin 4)
#define MOTOR_DER_IN4 7   // Dirección
#define MOTOR_DER_ENB 6   // PWM velocidad (Shield pin 6)

// Constantes
#define DISTANCIA_MINIMA 25    // cm - Distancia de seguridad
#define VELOCIDAD_MINIMA 100   // Velocidad inicial
#define VELOCIDAD_MAXIMA 160   // Velocidad máxima
#define VELOCIDAD_GIRO 120     // Velocidad de giro

// Tiempos de giro (en milisegundos)
#define TIEMPO_GIRO_90  450    // ~90-100 grados
#define TIEMPO_GIRO_60  250    // ~55-60 grados

// Factores de corrección (motor izquierdo más rápido)
#define FACTOR_MOTOR_IZQ 0.90  // Reducido porque es más rápido
#define FACTOR_MOTOR_DER 1.0   // Motor derecho normal

// Variables de control
int velocidadActual = VELOCIDAD_MINIMA;
Servo servoSensor;
long distancia = 0;

// Declaración de funciones
long medirDistancia();
long medirDistanciaPrecisa();
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
  
  // ESCANEO INICIAL: Buscar mejor dirección al arrancar (5 posiciones)
  Serial.println("🔍 Escaneo inicial 360° (5 posiciones)");
  
  servoSensor.write(0);  // Extremo derecha
  delay(600);
  int dist0 = medirDistanciaPrecisa();
  Serial.print("  0° (der): ");
  Serial.print(dist0);
  Serial.println(" cm");
  
  servoSensor.write(45);  // Diagonal derecha
  delay(600);
  int dist45 = medirDistanciaPrecisa();
  Serial.print("  45°: ");
  Serial.print(dist45);
  Serial.println(" cm");
  
  servoSensor.write(90);  // Frente
  delay(600);
  int dist90 = medirDistanciaPrecisa();
  Serial.print("  90° (frente): ");
  Serial.print(dist90);
  Serial.println(" cm");
  
  servoSensor.write(135);  // Diagonal izquierda
  delay(600);
  int dist135 = medirDistanciaPrecisa();
  Serial.print("  135°: ");
  Serial.print(dist135);
  Serial.println(" cm");
  
  servoSensor.write(180);  // Extremo izquierda
  delay(600);
  int dist180 = medirDistanciaPrecisa();
  Serial.print("  180° (izq): ");
  Serial.print(dist180);
  Serial.println(" cm");
  
  servoSensor.write(90);  // Volver al centro
  delay(600);
  
  // Encontrar mejor dirección
  int maxDist = dist0;
  int mejorAngulo = 0;
  if (dist45 > maxDist) { maxDist = dist45; mejorAngulo = 45; }
  if (dist90 > maxDist) { maxDist = dist90; mejorAngulo = 90; }
  if (dist135 > maxDist) { maxDist = dist135; mejorAngulo = 135; }
  if (dist180 > maxDist) { maxDist = dist180; mejorAngulo = 180; }
  
  Serial.print("✅ Mejor dirección inicial: ");
  Serial.print(mejorAngulo);
  Serial.print("° con ");
  Serial.print(maxDist);
  Serial.println(" cm");
  
  // Girar según mejor ángulo detectado
  if (mejorAngulo == 0) {
    Serial.println("↪️ Orientándose 90° DERECHA");
    girarDerecha();
    delay(TIEMPO_GIRO_90);
    detener();
  } else if (mejorAngulo == 45) {
    Serial.println("↪️ Orientándose 60° DERECHA");
    girarDerecha();
    delay(TIEMPO_GIRO_60);
    detener();
  } else if (mejorAngulo == 90) {
    Serial.println("⬆️ Frente tiene más espacio");
  } else if (mejorAngulo == 135) {
    Serial.println("↩️ Orientándose 60° IZQUIERDA");
    girarIzquierda();
    delay(TIEMPO_GIRO_60);
    detener();
  } else if (mejorAngulo == 180) {
    Serial.println("↩️ Orientándose 90° IZQUIERDA");
    girarIzquierda();
    delay(TIEMPO_GIRO_90);
    detener();
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
      
      // Scan de emergencia (5 posiciones)
      Serial.println("🔍 Scan completo después de bloqueo...");
      
      servoSensor.write(0);
      delay(600);
      int dist0 = medirDistanciaPrecisa();
      Serial.print("  0°: "); Serial.print(dist0); Serial.println(" cm");
      
      servoSensor.write(45);
      delay(600);
      int dist45 = medirDistanciaPrecisa();
      Serial.print("  45°: "); Serial.print(dist45); Serial.println(" cm");
      
      servoSensor.write(90);
      delay(600);
      int dist90 = medirDistanciaPrecisa();
      Serial.print("  90°: "); Serial.print(dist90); Serial.println(" cm");
      
      servoSensor.write(135);
      delay(600);
      int dist135 = medirDistanciaPrecisa();
      Serial.print("  135°: "); Serial.print(dist135); Serial.println(" cm");
      
      servoSensor.write(180);
      delay(600);
      int dist180 = medirDistanciaPrecisa();
      Serial.print("  180°: "); Serial.print(dist180); Serial.println(" cm");
      
      servoSensor.write(90);
      delay(500);
      
      // Encontrar mejor dirección
      int maxDist = dist0;
      int mejorAngulo = 0;
      if (dist45 > maxDist) { maxDist = dist45; mejorAngulo = 45; }
      if (dist90 > maxDist) { maxDist = dist90; mejorAngulo = 90; }
      if (dist135 > maxDist) { maxDist = dist135; mejorAngulo = 135; }
      if (dist180 > maxDist) { maxDist = dist180; mejorAngulo = 180; }
      
      Serial.print("✅ Mejor: "); Serial.print(mejorAngulo); Serial.println("°");
      
      // Girar según mejor ángulo
      if (mejorAngulo == 0) {
        girarDerecha(); delay(TIEMPO_GIRO_90);
      } else if (mejorAngulo == 45) {
        girarDerecha(); delay(TIEMPO_GIRO_60);
      } else if (mejorAngulo == 135) {
        girarIzquierda(); delay(TIEMPO_GIRO_60);
      } else if (mejorAngulo == 180) {
        girarIzquierda(); delay(TIEMPO_GIRO_90);
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
        
        // Escanear (5 posiciones)
        Serial.println("🔍 Scan completo después de atasco...");
        
        servoSensor.write(0);
        delay(600);
        int dist0 = medirDistanciaPrecisa();
        Serial.print("  0°: "); Serial.print(dist0); Serial.println(" cm");
        
        servoSensor.write(45);
        delay(600);
        int dist45 = medirDistanciaPrecisa();
        Serial.print("  45°: "); Serial.print(dist45); Serial.println(" cm");
        
        servoSensor.write(90);
        delay(600);
        int dist90 = medirDistanciaPrecisa();
        Serial.print("  90°: "); Serial.print(dist90); Serial.println(" cm");
        
        servoSensor.write(135);
        delay(600);
        int dist135 = medirDistanciaPrecisa();
        Serial.print("  135°: "); Serial.print(dist135); Serial.println(" cm");
        
        servoSensor.write(180);
        delay(600);
        int dist180 = medirDistanciaPrecisa();
        Serial.print("  180°: "); Serial.print(dist180); Serial.println(" cm");
        
        servoSensor.write(90);
        delay(500);
        
        // Encontrar mejor dirección
        int maxDist = dist0;
        int mejorAngulo = 0;
        if (dist45 > maxDist) { maxDist = dist45; mejorAngulo = 45; }
        if (dist90 > maxDist) { maxDist = dist90; mejorAngulo = 90; }
        if (dist135 > maxDist) { maxDist = dist135; mejorAngulo = 135; }
        if (dist180 > maxDist) { maxDist = dist180; mejorAngulo = 180; }
        
        Serial.print("✅ Mejor: "); Serial.print(mejorAngulo); Serial.println("°");
        
        // Girar según mejor ángulo
        if (mejorAngulo == 0) {
          girarDerecha(); delay(TIEMPO_GIRO_90);
        } else if (mejorAngulo == 45) {
          girarDerecha(); delay(TIEMPO_GIRO_60);
        } else if (mejorAngulo == 135) {
          girarIzquierda(); delay(TIEMPO_GIRO_60);
        } else if (mejorAngulo == 180) {
          girarIzquierda(); delay(TIEMPO_GIRO_90);
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
      
      // Escanear para salir (5 posiciones)
      Serial.println("🔍 Scan completo de emergencia...");
      
      servoSensor.write(0);
      delay(600);
      int dist0 = medirDistanciaPrecisa();
      Serial.print("  0°: "); Serial.print(dist0); Serial.println(" cm");
      
      servoSensor.write(45);
      delay(600);
      int dist45 = medirDistanciaPrecisa();
      Serial.print("  45°: "); Serial.print(dist45); Serial.println(" cm");
      
      servoSensor.write(90);
      delay(600);
      int dist90 = medirDistanciaPrecisa();
      Serial.print("  90°: "); Serial.print(dist90); Serial.println(" cm");
      
      servoSensor.write(135);
      delay(600);
      int dist135 = medirDistanciaPrecisa();
      Serial.print("  135°: "); Serial.print(dist135); Serial.println(" cm");
      
      servoSensor.write(180);
      delay(600);
      int dist180 = medirDistanciaPrecisa();
      Serial.print("  180°: "); Serial.print(dist180); Serial.println(" cm");
      
      servoSensor.write(90);
      delay(500);
      
      // Encontrar mejor dirección
      int maxDist = dist0;
      int mejorAngulo = 0;
      if (dist45 > maxDist) { maxDist = dist45; mejorAngulo = 45; }
      if (dist90 > maxDist) { maxDist = dist90; mejorAngulo = 90; }
      if (dist135 > maxDist) { maxDist = dist135; mejorAngulo = 135; }
      if (dist180 > maxDist) { maxDist = dist180; mejorAngulo = 180; }
      
      Serial.print("✅ Mejor: "); Serial.print(mejorAngulo); Serial.println("°");
      
      // Girar según mejor ángulo
      if (mejorAngulo == 0) {
        girarDerecha(); delay(TIEMPO_GIRO_90);
      } else if (mejorAngulo == 45) {
        girarDerecha(); delay(TIEMPO_GIRO_60);
      } else if (mejorAngulo == 135) {
        girarIzquierda(); delay(TIEMPO_GIRO_60);
      } else if (mejorAngulo == 180) {
        girarIzquierda(); delay(TIEMPO_GIRO_90);
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
    
    // 4. ESCANEAR: medir 5 posiciones para mayor precisión
    Serial.println("🔍 Escaneando 360° (5 posiciones)...");
    
    servoSensor.write(0);  // Extremo derecha
    delay(600);
    int dist0 = medirDistanciaPrecisa();
    Serial.print("  0° (der): ");
    Serial.print(dist0);
    Serial.println(" cm");
    
    servoSensor.write(45);  // Derecha diagonal
    delay(600);
    int dist45 = medirDistanciaPrecisa();
    Serial.print("  45°: ");
    Serial.print(dist45);
    Serial.println(" cm");
    
    servoSensor.write(90);  // Frente
    delay(600);
    int dist90 = medirDistanciaPrecisa();
    Serial.print("  90° (frente): ");
    Serial.print(dist90);
    Serial.println(" cm");
    
    servoSensor.write(135);  // Izquierda diagonal
    delay(600);
    int dist135 = medirDistanciaPrecisa();
    Serial.print("  135°: ");
    Serial.print(dist135);
    Serial.println(" cm");
    
    servoSensor.write(180);  // Extremo izquierda
    delay(600);
    int dist180 = medirDistanciaPrecisa();
    Serial.print("  180° (izq): ");
    Serial.print(dist180);
    Serial.println(" cm");
    
    servoSensor.write(90);  // Volver al centro
    delay(500);
    
    // 5. ENCONTRAR la dirección con MAYOR distancia
    int maxDist = dist0;
    int mejorAngulo = 0;
    
    if (dist45 > maxDist) { maxDist = dist45; mejorAngulo = 45; }
    if (dist90 > maxDist) { maxDist = dist90; mejorAngulo = 90; }
    if (dist135 > maxDist) { maxDist = dist135; mejorAngulo = 135; }
    if (dist180 > maxDist) { maxDist = dist180; mejorAngulo = 180; }
    
    Serial.print("✅ Mejor dirección: ");
    Serial.print(mejorAngulo);
    Serial.print("° con ");
    Serial.print(maxDist);
    Serial.println(" cm");
    
    // Girar según el ángulo óptimo detectado
    if (mejorAngulo == 90) {
      Serial.println("⬆️ Frente está despejado");
      // No girar
    } else if (mejorAngulo == 0) {
      // Girar 90° a la derecha
      Serial.println("↪️ Girando 90° DERECHA");
      girarDerecha();
      delay(TIEMPO_GIRO_90);
      detener();
      delay(200);
    } else if (mejorAngulo == 45) {
      // Girar 60° a la derecha
      Serial.println("↪️ Girando 60° DERECHA");
      girarDerecha();
      delay(TIEMPO_GIRO_60);
      detener();
      delay(200);
    } else if (mejorAngulo == 135) {
      // Girar 60° a la izquierda
      Serial.println("↩️ Girando 60° IZQUIERDA");
      girarIzquierda();
      delay(TIEMPO_GIRO_60);
      detener();
      delay(200);
    } else if (mejorAngulo == 180) {
      // Girar 90° a la izquierda
      Serial.println("↩️ Girando 90° IZQUIERDA");
      girarIzquierda();
      delay(TIEMPO_GIRO_90);
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

// Medir distancia con PROMEDIO de 3 lecturas para mayor precisión
long medirDistanciaPrecisa() {
  long suma = 0;
  int lecturas = 3;
  
  for (int i = 0; i < lecturas; i++) {
    suma += medirDistancia();
    delay(50);  // Pausa entre lecturas
  }
  
  return suma / lecturas;
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
