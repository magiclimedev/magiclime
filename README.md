# MagicLime: An Open Platform for High-Performance, Secure IoT Sensors

**MagicLime** is an open-source platform designed to create secure, affordable, and high-performance IoT sensor networks. Ideal for hobbyists, developers, and industry professionals, MagicLime emphasizes ease of use, reliable connectivity, and long-term sensor endurance. MagicLime’s sensors communicate over sub-1GHz ISM (Industrial, Scientific, and Medical) bands, facilitating low-power and encrypted communication for secure and resilient IoT networks. The platform’s USB dongle base station is highly versatile, compatible across Windows, Mac, and Linux environments, and thoroughly tested on the Raspberry Pi.

## Key Features

### 1. **Open and Accessible Design**
   - **Inexpensive Components:** Built using Atmel’s 8-bit AVR RISC-based Atmega328, MagicLime sensors are affordable and readily programmable through the Arduino IDE.
   - **Compatibility:** The USB dongle-based base station is platform-agnostic, enabling use on any common desktop or embedded system, with a focus on simple setup and deployment.
   - **Power Efficiency:** Sensors are battery-powered, utilizing standard AAA batteries with a projected lifespan of up to 2 years, ensuring low maintenance and high availability.

### 2. **Secure Wireless Communication**
   - MagicLime employs sub-1GHz ISM band frequencies, chosen for their secure, low-interference properties and suitability for long-range, low-power IoT applications.
   - **Encryption** is applied at the packet level for every communication, securing data between the sensors and base station.

### 3. **Flexible Connectivity Options**
   - Using the robust [RadioHead Packet Radio library](https://www.airspayce.com/mikem/arduino/RadioHead/), MagicLime establishes a reliable star network topology in Version 0. In future releases, mesh network support will be introduced, extending sensor range and enhancing connectivity resilience.
   - Supported communication protocols allow data to be accessed and integrated through multiple interfaces:
     - **Console**
     - **MQTT**
     - **REST API**
     - **HTTP (Human Readable)**
     - **Cloud Integration Plugins** (such as ThingSpeak and other cloud services)

### 4. **Diverse Sensor Modules**
   - MagicLime Version 0 currently includes the following sensor modules, each designed to meet various IoT use cases:
     - **Beacon**
     - **Push Button**
     - **Motion (PIR)**
     - **Temperature and Humidity**
     - **Knock and Vibration**
     - **Tilt**
     - **Light**
     - **Sound**
     - **Moisture**

### 5. **Expandable Architecture**
   - Firmware on both sensors and base station is developed in Arduino C, with gateway code on the host side (e.g., Raspberry Pi) written in Node.js, allowing users to customize and expand functionality as desired.

---

MagicLime aims to bring accessible, reliable, and secure IoT sensor networks to developers worldwide. Future development plans include implementing mesh networking and enhancing integration with various cloud-based IoT platforms, ensuring MagicLime remains versatile and scalable for both experimental and production environments.

