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
#define DISTANCIA_MINIMA 35    // cm - Distancia de seguridad (aumentada)
#define DISTANCIA_CRITICA 20   // cm - Detención inmediata (aumentada)
#define VELOCIDAD_MINIMA 80    // Velocidad inicial (reducida)
#define VELOCIDAD_MAXIMA 140   // Velocidad máxima (reducida)
#define VELOCIDAD_GIRO 100     // Velocidad de giro (reducida)

// Factores de corrección (ajustar si un motor es más rápido)
#define FACTOR_MOTOR_IZQ 1.0   // Ajustar entre 0.8 - 1.2
#define FACTOR_MOTOR_DER 1.0   // Ajustar entre 0.8 - 1.2

// Variables de control
int velocidadActual = VELOCIDAD_MINIMA;
int contadorAtasco = 0;  // Contador para detectar atasco
long distanciaAnterior = 400;

Servo servoSensor;
long distancia = 0;

// Declaración de funciones
long medirDistancia();
void avanzar();
void avanzarConVelocidad(int velocidad);
void retroceder();
void girarDerecha();
void girarIzquierda();
void detener();
int buscarMejorDireccion();
void acelerarProgresivo();

void setup() {
  Serial.begin(9600);
  
  // Configurar pines del sensor
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Configurar pines de motores
  pinMode(MOTOR_IZQ_IN1, OUTPUT);
  pinMode(MOTOR_IZQ_IN2, OUTPUT);
  pinMode(MOTOR_IZQ_ENA, OUTPUT);
  pinMode(MOTOR_DER_IN3, OUTPUT);
  pinMode(MOTOR_DER_IN4, OUTPUT);
  pinMode(MOTOR_DER_ENB, OUTPUT);
  
  // Inicializar servo
  servoSensor.attach(SERVO_PIN);
  servoSensor.write(90);  // Posición central
  
  delay(1000);
  Serial.println("Robot iniciado");
}

void loop() {
  // Medir distancia sin detener el movimiento
  static unsigned long ultimaMedicion = 0;
  unsigned long tiempoActual = millis();
  
  // Medir cada 150ms en lugar de cada ciclo
  if (tiempoActual - ultimaMedicion >= 150) {
    ultimaMedicion = tiempoActual;
    
    // Medir rápidamente sin detener
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);
    
    long duracion = pulseIn(ECHO_PIN, HIGH, 25000);
    long distanciaReal = (duracion * 0.034) / 2;
    
    // Detectar atasco INMEDIATO: sensor da 0 o muy cerca
    if (distanciaReal == 0 || distanciaReal < 3) {
      Serial.println("🚨 SENSOR BLOQUEADO/PEGADO - Escape inmediato!");
      detener();
      delay(100);
      retroceder();
      delay(1200);  // Retroceder mucho
      detener();
      delay(400);
      
      // Girar 180 grados
      Serial.println("🔄 Girando 180°");
      girarDerecha();
      delay(1300);
      detener();
      delay(500);
      
      contadorAtasco = 0;
      velocidadActual = VELOCIDAD_MINIMA;
      distancia = 400;  // Resetear distancia
    } else {
      // Actualizar distancia solo si la lectura es válida
      if (distanciaReal > 400) distanciaReal = 400;
      distancia = distanciaReal;
      
      // Detectar atasco: si la distancia es muy corta
      if (distancia < 8) {
        contadorAtasco++;
        
        // Si está MUY pegado (< 5cm), escape inmediato
        if (distancia < 5) {
          Serial.println("🚨 PEGADO A LA PARED - Escape inmediato!");
          detener();
          delay(100);
          retroceder();
          delay(1000);
          detener();
          delay(300);
          
          // Girar 180 grados
          Serial.println("🔄 Girando 180°");
          girarDerecha();
          delay(1200);
          detener();
          delay(500);
          
          contadorAtasco = 0;
          velocidadActual = VELOCIDAD_MINIMA;
        }
        // Si está cerca, contador más agresivo (solo 2 lecturas)
        else if (contadorAtasco > 2) {
          Serial.println("⚠️ ROBOT ATASCADO - Rutina de escape");
          detener();
          delay(200);
          retroceder();
          delay(900);
          detener();
          delay(300);
          
          // Girar 180 grados
          Serial.println("🔄 Girando 180°");
          girarDerecha();
          delay(1200);
          detener();
          delay(500);
          
          contadorAtasco = 0;
          velocidadActual = VELOCIDAD_MINIMA;
        }
      } else if (distancia > 25) {
        // Si hay espacio, resetear contador
        contadorAtasco = 0;
      } else if (distancia > 15 && contadorAtasco > 0) {
        // Reducir contador gradualmente
        contadorAtasco--;
      }
    }
    
    Serial.print("Dist: ");
    Serial.print(distancia);
    Serial.print(" cm | Vel: ");
    Serial.print(velocidadActual);
    Serial.print(" | Atasco: ");
    Serial.println(contadorAtasco);
    
    distanciaAnterior = distancia;
  }
  
  // Decidir acción según distancia (sin pausas)
  if (distancia > DISTANCIA_MINIMA) {
    // Camino libre - acelerar progresivamente mientras avanza
    acelerarProgresivo();
    avanzarConVelocidad(velocidadActual);
    
  } else if (distancia > DISTANCIA_CRITICA) {
    // Obstáculo cerca - reducir velocidad rápidamente
    if (velocidadActual > VELOCIDAD_MINIMA) {
      velocidadActual -= 15; // Desacelerar más rápido
      if (velocidadActual < VELOCIDAD_MINIMA) {
        velocidadActual = VELOCIDAD_MINIMA;
      }
    }
    avanzarConVelocidad(velocidadActual);
    
  } else if (distancia >= 8) {
    // Obstáculo cerca - realizar maniobra de evasión normal
    Serial.println("🛑 Obstáculo!");
    detener();
    velocidadActual = VELOCIDAD_MINIMA;
    delay(200);
    
    // Retroceder
    Serial.println("↩️ Retrocediendo");
    retroceder();
    delay(600);  // Retroceder más
    detener();
    delay(300);
    
    // Buscar mejor dirección
    int direccion = buscarMejorDireccion();
    
    if (direccion == 1) {
      Serial.println("↪️ Girando DERECHA");
      girarDerecha();
      delay(700);  // Giro más amplio
    } else {
      Serial.println("↩️ Girando IZQUIERDA");
      girarIzquierda();
      delay(700);  // Giro más amplio
    }
    
    detener();
    delay(400);
    contadorAtasco = 0;  // Resetear al hacer maniobra exitosa
  } else {
    // Si distancia < 8 cm, detener completamente y esperar rutina de atasco
    detener();
  }
  
  // Pequeña pausa para no saturar el procesador
  delay(10);
}

// Medir distancia con sensor HC-SR04
long medirDistancia() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  
  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);  // Timeout 30ms
  long dist = duracion * 0.034 / 2;
  
  if (dist == 0 || dist > 400) {
    dist = 400;  // Fuera de rango
  }
  
  return dist;
}

// Buscar la mejor dirección (derecha o izquierda)
int buscarMejorDireccion() {
  // Mirar a la derecha
  servoSensor.write(10);
  delay(500);
  long distDerecha = medirDistancia();
  Serial.print("Distancia derecha: ");
  Serial.println(distDerecha);
  
  // Mirar a la izquierda
  servoSensor.write(170);
  delay(500);
  long distIzquierda = medirDistancia();
  Serial.print("Distancia izquierda: ");
  Serial.println(distIzquierda);
  
  // Volver al centro
  servoSensor.write(90);
  delay(300);
  
  // Retornar mejor dirección: 1 = derecha, -1 = izquierda
  return (distDerecha > distIzquierda) ? 1 : -1;
}

// Acelerar progresivamente
void acelerarProgresivo() {
  if (velocidadActual < VELOCIDAD_MAXIMA) {
    velocidadActual += 3; // Incremento más gradual para menos inercia
    if (velocidadActual > VELOCIDAD_MAXIMA) {
      velocidadActual = VELOCIDAD_MAXIMA;
    }
  }
}

// Funciones de control de motores
void avanzar() {
  avanzarConVelocidad(VELOCIDAD_MAXIMA);
}

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
  
  // Retroceder
  digitalWrite(MOTOR_IZQ_IN1, LOW);
  digitalWrite(MOTOR_IZQ_IN2, HIGH);
  analogWrite(MOTOR_IZQ_ENA, velIzq);
  
  digitalWrite(MOTOR_DER_IN3, LOW);
  digitalWrite(MOTOR_DER_IN4, HIGH);
  analogWrite(MOTOR_DER_ENB, velDer);
  
  Serial.println("Retroceder");
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
  
  Serial.println("Girar DERECHA");
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
  
  Serial.println("Girar IZQUIERDA");
}

void detener() {
  digitalWrite(MOTOR_IZQ_IN1, LOW);
  digitalWrite(MOTOR_IZQ_IN2, LOW);
  analogWrite(MOTOR_IZQ_ENA, 0);
  
  digitalWrite(MOTOR_DER_IN3, LOW);
  digitalWrite(MOTOR_DER_IN4, LOW);
  analogWrite(MOTOR_DER_ENB, 0);
  
  Serial.println("Detener");
}