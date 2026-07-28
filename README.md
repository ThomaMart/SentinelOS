<div align="center">

# 🛡️ SentinelOS

**Secure Embedded Firmware for ESP32**

Embedded Software • FreeRTOS • Secure OTA • Embedded Security

[![CI](https://github.com/ThomaMart/SentinelOS/actions/workflows/ci.yml/badge.svg)](https://github.com/ThomaMart/SentinelOS/actions)
[![Platform](https://img.shields.io/badge/Platform-ESP32-E7352C?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF%20v6-blue)](https://github.com/espressif/esp-idf)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-00979D)](https://www.freertos.org/)
[![Language](https://img.shields.io/badge/Language-C-00599C?logo=c)]()
[![Architecture](https://img.shields.io/badge/Architecture-Modular-success)]()
[![Security](https://img.shields.io/badge/Security-Secure%20Boot%20%7C%20OTA-red)]()
[![License](https://img.shields.io/badge/License-MIT-green)]()

</div>

SentinelOS is a personal embedded software engineering project designed to demonstrate modern firmware development practices for connected embedded systems.

The project focuses on designing a modular, maintainable and security-oriented firmware architecture inspired by industrial products. It combines hardware/software integration, real-time programming, secure firmware updates, system diagnostics and embedded communication protocols.

Rather than being a demonstration application, SentinelOS is developed as a production-oriented firmware platform showcasing embedded software architecture and cybersecurity concepts.

<img width="1086" height="1448" alt="sentinelOS" src="https://github.com/user-attachments/assets/50dcc798-ea23-4cd0-aa69-0f8bdd8bf5f0" />

---

# Highlights

- Embedded development in **C** using **ESP-IDF**
- **FreeRTOS** multitasking architecture
- Modular component-based firmware
- Hardware Abstraction Layer (HAL)
- Secure OTA update pipeline
- Secure Boot support
- SHA-256 firmware integrity verification
- ECDSA firmware signature verification
- UART framed communication protocol
- Wi-Fi CSI motion sensing
- Rogue Access Point detection
- I²C device scanner
- Watchdog supervision
- Crash dump diagnostics
- Runtime monitoring
- LVGL graphical interface

---

# Technologies

| Category | Technologies |
|----------|--------------|
| Language | C |
| Framework | ESP-IDF v6 |
| RTOS | FreeRTOS |
| MCU | ESP32 |
| GUI | LVGL |
| Security | Secure Boot, OTA, SHA-256, ECDSA, HMAC |
| Communication | UART, SPI, I²C, Wi-Fi |
| Storage | NVS, SPI Flash |
| Build | CMake |
| Tools | Git, Python, GitHub Actions |

---

# Hardware

- ESP32-2432S028R
- Dual-core ESP32
- ILI9341 LCD
- XPT2046 Touch Controller
- Wi-Fi 802.11 b/g/n

---

# Software Architecture

The firmware follows a modular architecture where each component exposes a minimal public API while hiding its internal implementation.

```text
firmware/
├── main/
├── components/
│   ├── bsp/
│   ├── config/
│   ├── system/
│   ├── storage/
│   ├── wifi/
│   ├── ota/
│   ├── protocol/
│   ├── display/
│   ├── radar/
│   ├── rogue_ap/
│   ├── i2c/
│   ├── diag/
│   └── tasks/
```

This architecture keeps business logic separated from hardware drivers, making the firmware easier to maintain, extend and test.

---

# Security Features

SentinelOS demonstrates several security mechanisms commonly found in embedded products.

- Secure OTA update pipeline
- Firmware integrity verification
- Firmware authenticity verification (ECDSA)
- Secure Boot support
- Automatic rollback
- HMAC-based challenge/response authentication
- Defensive UART parser
- Runtime watchdog monitoring

---

# Main Features

## Secure OTA

- HTTPS firmware download
- Manifest-based update process
- SHA-256 verification
- ECDSA signature validation
- Automatic rollback

## Communication

- Binary UART protocol
- CRC16 protection
- Packet framing
- Robust parser
- Host authentication using HMAC

## Diagnostics

- Runtime monitoring
- Heap statistics
- Uptime
- Reset reason
- Watchdog
- Core dump support

## Wireless Features

- Wi-Fi Station mode
- Wi-Fi CSI motion detection
- Rogue AP detection
- Active Wi-Fi scanning

## Hardware

- SPI peripherals
- I²C scanner
- Touchscreen interface
- LCD graphical interface

---

# Testing

The project is designed with modular components that can be individually validated.

Validation includes:

- Protocol parser tests
- CRC validation
- OTA workflow validation
- Runtime diagnostics
- Hardware integration testing

---

# Build

```bash
cd firmware
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

or

```bash
python3 tools/run.py
```

---

# Repository

```text
SentinelOS/
├── firmware/
├── tools/
├── docs/
└── README.md
```

---

# Roadmap

- [x] ESP-IDF firmware
- [x] FreeRTOS architecture
- [x] Modular components
- [x] LVGL interface
- [x] Wi-Fi support
- [x] Secure OTA
- [x] Firmware signature verification
- [x] Secure Boot
- [x] UART communication protocol
- [x] Wi-Fi CSI sensing
- [x] Rogue AP detection
- [x] I²C scanner
- [x] Watchdog
- [x] Crash diagnostics
- [ ] Flash encryption
- [ ] BLE monitoring

---

# Project Goals

SentinelOS is a long-term learning project focused on embedded software engineering, embedded cybersecurity and real-time systems.

Its purpose is to demonstrate skills relevant to professional embedded software positions, including:

- Embedded C development
- RTOS programming
- Embedded Linux ecosystem
- Hardware/Software integration
- Embedded security
- Firmware architecture
- System debugging
- Communication protocols
- Software quality and maintainability

---

# License

Personal project for learning and portfolio purposes.

---

<div align="center">

Projet personnel

</div>
