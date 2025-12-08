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
- **IN2** → Pin 9 (dirección motor izquierdo)
- **IN3** → Pin 10 (dirección motor derecho)
- **IN4** → Pin 7 (dirección motor derecho)

### Sensores → Shield V5
- **Servomotor** → Pin 11
- **HC-SR04 Trigger** → Pin 12
- **HC-SR04 Echo** → Pin 13

### Alimentación
- **Batería** → 12V del L298N
- **5V del L298N** → 5V del Arduino (opcional, también puede usar USB)
- **GND común** entre L298N y Arduino

## Características

✅ Navegación autónoma con evasión de obstáculos
✅ Aceleración progresiva (80-140 PWM)
✅ Detección inteligente de atascos
✅ Rutina de escape cuando queda pegado a paredes
✅ Escaneo con servomotor para encontrar mejor dirección
✅ Movimiento fluido sin pausas

## Configuración

Ajusta estos parámetros en `main.cpp` según tu robot:

```cpp
#define DISTANCIA_MINIMA 35    // cm - Distancia de seguridad
#define DISTANCIA_CRITICA 20   // cm - Detención inmediata
#define VELOCIDAD_MINIMA 80    // PWM inicial
#define VELOCIDAD_MAXIMA 140   // PWM máxima
#define VELOCIDAD_GIRO 100     // PWM para giros

#define FACTOR_MOTOR_IZQ 1.0   // Ajustar si un motor es más rápido
#define FACTOR_MOTOR_DER 1.0   // Ajustar entre 0.8 - 1.2
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

El robot:
1. Avanza mientras no detecta obstáculos
2. Acelera progresivamente hasta velocidad máxima
3. Reduce velocidad cuando detecta obstáculo a 20-35 cm
4. Se detiene y maniobra cuando detecta obstáculo < 20 cm
5. Si queda atascado (sensor bloqueado o < 5 cm), ejecuta escape de emergencia
6. Gira 180° y continúa explorando

## Sistema Anti-Atasco

- Detecta sensor bloqueado (lectura 0 o < 3 cm)
- Escape inmediato si está pegado < 5 cm
- Contador de atasco si permanece < 8 cm por 300ms
- Retrocede 1.2 segundos y gira 180° para escapar

## Licencia

MIT

## Autor

Proyecto Robot Autónomo 2024
