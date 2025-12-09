# Robot 2W Arduino UNO

Robot autónomo con 2 ruedas motrices y 1 rueda pivote, basado en Arduino UNO.

## Hardware

- **Microcontrolador**: Arduino UNO
- **Shield**: Shield V5 (expansion board)
- **Driver de motores**: L298N
- **Motores**: 2x motores DC
- **Sensor de distancia**: HC-SR04 (ultrasónico)
- **Servomotor**: Para mover el sensor HC-SR04
- **Alimentación**: 6 pilas AA (9V) recomendado

## Conexiones

### L298N → Shield V5
- **ENA** → Pin 5 PWM (velocidad motor izquierdo)
- **ENB** → Pin 6 PWM (velocidad motor derecho)
- **IN1** → Pin 8 (dirección motor izquierdo)
- **IN2** → Pin 2 (dirección motor izquierdo)
- **IN3** → Pin 4 (dirección motor derecho)
- **IN4** → Pin 7 (dirección motor derecho)

**Pines PWM libres**: 3, 9, 10 (disponibles para expansiones)

### Sensores → Shield V5
- **Servomotor** → Pin 11
- **HC-SR04 Trigger** → Pin 12
- **HC-SR04 Echo** → Pin 13

### Alimentación
- **Batería** → 12V del L298N
- **5V del L298N** → 5V del Arduino (opcional, también puede usar USB)
- **GND común** entre L298N y Arduino

## Características

✅ **Navegación autónoma** con evasión inteligente de obstáculos
✅ **Escaneo 5 posiciones** (0°, 45°, 90°, 135°, 180°) para decisiones precisas
✅ **Medición precisa** con promedio de 3 lecturas por ángulo
✅ **Aceleración progresiva** (100-160 PWM)
✅ **3 tipos de detección de atasco**:
   - Sensor bloqueado (distancia 0 o <3cm)
   - Bloqueo físico (2 seg sin cambio de distancia)
   - Atasco por tiempo (10 seg avanzando sin progreso)
✅ **Giros proporcionales** según ángulo detectado (60° o 90°)
✅ **Movimiento continuo** con medición cada 150ms sin pausas
✅ **Corrección de motores** ajustable por software

## Configuración

Ajusta estos parámetros en `main.cpp` según tu robot:

```cpp
// Distancias
#define DISTANCIA_MINIMA 25    // cm - Distancia de seguridad

// Velocidades (PWM 0-255)
#define VELOCIDAD_MINIMA 100   // PWM inicial
#define VELOCIDAD_MAXIMA 160   // PWM máxima
#define VELOCIDAD_GIRO 120     // PWM para giros

// Tiempos de giro (milisegundos)
#define TIEMPO_GIRO_90  450    // ~90-100 grados
#define TIEMPO_GIRO_60  250    // ~55-60 grados

// Corrección de motores (ajustar entre 0.8 - 1.2)
#define FACTOR_MOTOR_IZQ 0.90  // Motor izquierdo (más rápido)
#define FACTOR_MOTOR_DER 1.0   // Motor derecho
```

## Compilar y Subir

### Con PlatformIO (VS Code)
1. Abrir proyecto en VS Code con PlatformIO
2. Conectar Arduino por USB
3. Presionar botón **Upload** (→) o `Ctrl+Alt+U`

### Monitor Serial
- Presionar ícono 🔌 en barra inferior o `Ctrl+Alt+S`
- Baud rate: 9600

## Funcionamiento

### Al Iniciar
1. Hace **escaneo inicial de 5 posiciones** (0° a 180°)
2. Se orienta automáticamente hacia donde hay más espacio
3. Inicia navegación autónoma

### Durante la Navegación
1. **Mide distancia cada 150ms** mientras avanza (sin pausas)
2. **Acelera progresivamente** hasta velocidad máxima (160 PWM)
3. **Detecta obstáculo a 25cm** → se detiene
4. **Escanea 5 posiciones** (0°, 45°, 90°, 135°, 180°) con medición precisa
5. **Gira hacia el ángulo óptimo** detectado (60° o 90° según necesidad)
6. Continúa avanzando

### Sistema Anti-Atasco (3 niveles)

#### 1. Sensor Bloqueado
- Detecta: distancia 0 o <3cm repetidamente
- Acción: Retrocede, escanea, gira hacia espacio libre

#### 2. Bloqueo Físico
- Detecta: Motores encendidos pero sin cambio de distancia por 2 segundos
- Utilidad: Detecta objetos delgados que el sensor no ve (patas de sillas)
- Acción: Retrocede, escanea 5 posiciones, gira hacia mejor dirección

#### 3. Atasco por Tiempo
- Detecta: Avanzando más de 10 segundos sin cambios significativos
- Acción: Retrocede, escanea completo, reorienta hacia espacio libre

## Licencia

MIT

## Autor

Proyecto Robot Autónomo 2024
