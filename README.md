# STM32 FreeRTOS TCP Server using W5500 and MQ2 Gas Sensor

## Project Overview

This project implements a multithreaded TCP server on an STM32 Blue Pill (STM32F103C8T6) using the W5500 Ethernet module and FreeRTOS. The server uses a static IP address and demonstrates real-time operating system concepts by running three concurrent tasks:
- TCP Server Task (Highest Priority) – Handles client connections and commands
- LED Control Task (Medium Priority) – Manages LED state based on commands
- Sensor Task (Lowest Priority) – Periodically reads the MQ-2 gas sensor

This architecture allows non-blocking, simultaneous operations: you can control the LED and request sensor readings while periodic sensor data streams to the client — all without interrupting any task.

**Key Features:**
- FreeRTOS with 3 concurrent tasks (preemptive multitasking)
- TCP server with static IP (192.168.1.10) – no DHCP, direct PC connection
- Remote LED control (ON/OFF/TOGGLE/STATUS)
- MQ-2 gas sensor monitoring (voltage, PPM, gas level)
- Periodic sensor data streaming (every 2 seconds) controlled by client
- Inter-task communication using thread flags and message queues
- Real-time command response without blocking

### Memory Usage Note

FreeRTOS provides true multitasking with precise timing, but adds overhead:

| Version | Flash | RAM | Free Flash (64KB) |
|---------|-------|-----|-------------------|
| Non-RTOS | 31 KB | 3.6 KB | 33 KB |
| **FreeRTOS** | 53 KB | **11.5 KB** | **11 KB** |

**Overhead:** +22 KB Flash, +8 KB RAM. Fine for 64KB+ devices, but be mindful on tight budgets.


## Project Functionality
### Hardware Configuration

https://github.com/user-attachments/assets/2e1807cc-5cb7-4580-93d2-7d81173928da


### TCP client control

https://github.com/user-attachments/assets/97861650-09b9-4ed4-864d-9dad6cf6cea2

Left: UART debug output showing STM32 startup, sensor initialization, and FreeRTOS task activity <br>
Right: Hercules TCP client connected to the server at `192.168.1.10:5000`, sending commands and receiving responses

### Static IP Configuration in windows

<img width="610" height="931" alt="static_ip_config" src="https://github.com/user-attachments/assets/a6d57274-0687-4945-a00d-b688afe37c9c" />

## Project Schematic

<img width="1460" height="555" alt="schematic diagram" src="https://github.com/user-attachments/assets/096cd2ac-f3b1-4e56-966a-4a4b92427c16" />

## Pin Configuration
| Peripheral | Pin | Connection | Notes |
|------------|-----|------------|-------|
| **MQ-2 Gas Sensor** | PA0 | A0 | Analog Connection |
| | 5V | VCC | Power |
| | GND | GND | Common ground |
| **Wiznet W5500** | PA5 | SCK | SPI1 Clock |
| | PA6 | MISO | SPI1 Master In Slave Out |
| | PA7 | MOSI | SPI1 Master Out Slave In |
| | PA4 | CS | Chip Select |
| | PA3 | Reset | Reset Pin |
| | 3.3V | VCC | Power |
| | GND | GND | Common ground |
| **USART1** | PA9 | TX to USB-Serial RX | 115200 baud, 8-N-1 |
| | PA10 | RX to USB-Serial TX | Optional for commands |

## Available Commands
| Command | Description |
|---------|-------------|
| `ON` | Turn LED on |
| `OFF` | Turn LED off |
| `TOGGLE` | Toggle LED state |
| `STATUS` | Check LED status |
| `GAS` | Get single gas reading |
| `START` | Begin periodic sensor data (2 sec interval) |
| `STOP` | Stop periodic sensor data |
| `HELP` | Show all commands |

## FreeRTOS Task Architecture
The application is divided into three independent tasks managed by the FreeRTOS scheduler:

| Task | Priority | Stack Size | Function |
|------|----------|------------|----------|
| TcpReceiveTask | osPriorityNormal (Highest) | 2048 bytes | Listens for client commands, parses them, and sends responses |
| LedControlTask | osPriorityNormal1 (Medium) | 1024 bytes | Waits for thread flags to turn LED ON/OFF/TOGGLE |
| SensorTask | osPriorityNormal2 (Lowest) | 1024 bytes | Reads MQ-2 sensor every 2 seconds and sends data to TCP client (if enabled) |

### Inter-Task Communication
- Thread Flags: Used between `TcpReceiveTask` and `LedControlTask` for LED commands
- Message Queue: `SensorDataQueue` (5 floats) can be used to pass sensor data from `SensorTask` to `TcpReceiveTask`
- Global Variables: `periodic_send_enabled` and `led_current_state` are shared to communicate between tasks

### Task Communication Diagram
```
┌─────────────────────────────────────────────────────────────────────┐
│                         FreeRTOS Scheduler                          │
│                    (Preemptive, Priority-Based)                     │
└─────────────────────────────────────────────────────────────────────┘
                                    │
        ┌───────────────────────────┼───────────────────────────┐
        │                           │                           │
        ▼                           ▼                           ▼
┌───────────────┐           ┌───────────────┐           ┌───────────────┐
│ TcpReceiveTask│           │ LedControlTask│           │  SensorTask   │
│  (Priority:0) │           │  (Priority:1) │           │  (Priority:2) │
│   Stack:2KB   │           │   Stack:1KB   │           │   Stack:1KB   │
└───────┬───────┘           └───────▲───────┘           └───────┬───────┘
        │                           │                           │
        │  osThreadFlagsSet(0x01)   │                           │
        ├──────────────────────────►│                           │
        │  (ON/OFF/TOGGLE commands) │                           │
        │                           │                           │
        │                           │                           │
        │  osMessageQueuePut()      │                           │
        ├───────────────────────────────────────────────────────┤
        │  (Sensor data)                                        │
        │                                                       │
        │                           │                           │
        │  Shared Global Variables: │                           │
        │  ◄────────────────────────┼──────────────────────────►│
        │     periodic_send_enabled, led_current_state          │
        │                                                       │
        ▼                           ▼                           ▼
┌───────────────┐           ┌───────────────┐           ┌───────────────┐
│   TCP Client  │           │     LED       │           │   MQ-2 Sensor │
│  (Network)    │           │   (GPIO)      │           │    (ADC)      │
└───────────────┘           └───────────────┘           └───────────────┘
```

## TCP Server
The TCP server runs on the STM32 and listens for incoming client connections on port 5000 with a static IP address (192.168.1.10).

### How it works (with FreeRTOS):
- Initializes the W5500 Ethernet controller via SPI communication
- Creates a TCP socket and enters listening mode
- The `TcpReceiveTask` waits for client connection (non-blocking)
- Once connected, it processes incoming commands in a loop with `osDelay(10)` to yield CPU
- All commands execute immediately without blocking other tasks
- Periodic sensor data is sent by `SensorTask` every 2 seconds when enabled

### Command processing is fully non-blocking:
- SensorTask runs independently, sending data every 2 seconds (if START received)
- LED commands are handled by LedControlTask via thread flags
- Single GAS command provides instant sensor reading without interfering with periodic sending

🔗 [View TCP Server Code](https://github.com/rubin-khadka/STM32_FreeRTOS_W5500_TCP_Server/blob/main/Core/Src/main.c)

## MQ-2 Gas Sensor Driver
The MQ-2 sensor detects combustible gases (LPG, propane, methane, smoke, hydrogen, alcohol) by measuring resistance changes in its tin dioxide (SnO₂) sensing element.

### How it works:
- Sensor is powered with 5V and outputs analog voltage (0-5V)
- A voltage divider (10kΩ + 20kΩ) scales the output to 0-3.3V for STM32 ADC
- Clean air produces ~0.2-0.3V; gas presence increases voltage up to 4-5V

### Driver features (optimized for RTOS):
- Reads raw ADC value from PA0 pin (non-blocking)
- Applies scaling factor (8x) to compensate for sensor variations
- Converts voltage to PPM using linear formula: `ppm = 50 + (voltage - 0.20) * 500`
- Classifies gas level: NORMAL (<0.5V), LOW (0.5-1.2V), MEDIUM (1.2-2.0V), HIGH (2.0-3.0V), CRITICAL (>3.0V)
- Provides alarm detection when voltage exceeds 2.5V

No calibration required – driver uses fixed formulas tuned for this specific sensor. The driver is designed to be called periodically from `SensorTask` without blocking.

🔗 [View MQ-2 Gas Sensor Driver](https://github.com/rubin-khadka/STM32_FreeRTOS_W5500_TCP_Server/blob/main/Core/Src/mq2_sensor.c)

## Related Projects 
- [STM32_W5500_TCP_Server (Non-RTOS version)](https://github.com/rubin-khadka/STM32_W5500_TCP_Server)

## Resources
- [STM32F103 Datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [STM32F103 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [WIZNET W5500 Datasheet](https://cdn.sparkfun.com/datasheets/Dev/Arduino/Shields/W5500_datasheet_v1.0.2_1.pdf)
- [MQ-2 Gas Sensor Datasheet](https://www.handsontec.com/dataspecs/MQ2-Gas%20Sensor.pdf)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/02-Kernel/07-Books-and-manual/01-RTOS_book)

## Project Status
- **Status**: Complete
- **Version**: v1.0
- **Last Updated**: April 2026

## Contact
**Rubin Khadka Chhetri**  
📧 rubinkhadka84@gmail.com <br>
🐙 GitHub: https://github.com/rubin-khadka