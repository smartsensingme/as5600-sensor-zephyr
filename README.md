# AS5600 Magnetic Encoder & Kalman Filter - Zephyr RTOS

*Read in other languages: [Português](README.pt-br.md)*

This repository contains a demonstration of the integration of the **AS5600** magnetic encoder and the **BTS7960 (IBT-2) H-Bridge DC Motor Driver** with **Zephyr RTOS**, utilizing a native *out-of-tree* sensor driver, a high-performance Dual-PWM (slow decay) motor controller, and an advanced **3D Kalman Filter** to estimate position, speed (RPM), and acceleration (RPM/s) of a rotating shaft in real time.

The project is configured to run on the **WeAct STM32G431 Core Board** development board and consumes its external dependencies through Git submodules.

---

## 🛠️ Driver Architecture (`custom_as5600`)

The AS5600 driver is hosted in the [zephyr_custom_drivers](https://github.com/smartsensingme/zephyr_custom_drivers.git) repository and is imported into this project as a local submodule in the `custom_drivers` directory. It exposes encoder readings through the standard Zephyr RTOS sensor API (`<zephyr/drivers/sensor.h>`).

### Supported Sensor Channels
The driver provides three channels for sampling:
*   **`SENSOR_CHAN_ROTATION`** (Processed Angle): Returns the smoothed angle from the **`ANGLE`** hardware register (with hysteresis and digital filtering configured directly on the chip's register).
*   **`SENSOR_CHAN_RAW_ROTATION`** (Raw Angle - Custom): Private channel (`SENSOR_CHAN_PRIV_START + 0`) that reads the value from the **`RAW ANGLE`** register (direct reading from the sensor's CORDIC processor, without internal filtering or hardware delay).
*   **`SENSOR_CHAN_STATUS`** (Magnet Status - Custom): Private channel (`SENSOR_CHAN_PRIV_START + 1`) that returns the sensor diagnostics byte, allowing verification of the presence and strength of the magnetic field:
    *   `AS5600_STATUS_MD` (Magnet Detected): Magnet detected.
    *   `AS5600_STATUS_ML` (Magnet Too Weak): Magnetic field too weak.
    *   `AS5600_STATUS_MH` (Magnet Too Strong): Magnetic field too strong.

### Features & Optimizations
1.  **High-Speed Burst Read:** When fetching the rotation channels (`SENSOR_CHAN_ROTATION` or `SENSOR_CHAN_RAW_ROTATION`), the driver performs a single I2C transaction of 4 continuous bytes (starting at `RAW_ANGLE_H`), optimizing I2C bus usage and allowing a maximum theoretical sampling frequency of **6.06 kHz**.
2.  **Configurable Thread Safety:** The driver supports safe execution in multithreaded systems. By enabling the `CONFIG_AS5600_THREAD_SAFE=y` option, an internal mutex synchronizes access to the I2C bus and the driver's internal variables. For high-frequency, single-threaded scenarios where mutex overhead is undesirable, the option can be disabled (`CONFIG_AS5600_THREAD_SAFE=n`).

---

## 📈 Kalman Filter (2D and 3D)

The project consumes the pure C Kalman Filter library from [kalman-filter-c](https://github.com/smartsensingme/kalman-filter-c.git) (mapped under the directory [src/kalman-filter-c](src/kalman-filter-c)), which implements the classical equations for 2D and 3D state-space models:
*   **2D Kalman:** Estimates angular position ($\theta$) and angular velocity ($\omega$).
*   **3D Kalman:** Estimates angular position ($\theta$), angular velocity ($\omega$), and angular acceleration ($\alpha$).

### Angular Transition Correction (Wrap-around)
Due to the circular behavior of the encoder ($0^\circ \to 360^\circ$), the main loop ([main.c](src/main.c)) implements the specialized function `engine_angle_kalman_3d_update` (and the alternative `engine_angle_kalman_2d_update`) to normalize the measurement error (innovation) to the range of $[-180^\circ, 180^\circ]$.

This prevents false spikes and instability in the velocity and acceleration calculations when the sensor transitions through the physical boundary between $360^\circ$ and $0^\circ$. The final angle estimate is always kept strictly in the range $[0^\circ, 360^\circ)$.

---

## 🏎️ BTS7960 (IBT-2) H-Bridge Engine Driver

The project includes a custom modular H-Bridge DC motor driver located in the [src/engine-driver](src/engine-driver) directory, which is imported as a Git submodule from the [zephyr-stmg431rb-engine-driver](https://github.com/smartsensingme/zephyr-stmg431rb-engine-driver.git) repository.

### Key Features
* **Slow-Decay Drive Mode:** Drives the motor using two complementary hardware PWM channels (`TIM2` CH1 on `PA0` for forward and `TIM2` CH2 on `PA1` for reverse), resulting in active low-side freewheeling/braking. This provides excellent speed-torque linearity.
* **Direct Hardware Resolution Mapping:** Speed commands from `-100.0f` to `100.0f` are mapped directly to the actual hardware timer's clock cycle steps (e.g. 7200 steps at 20 kHz), maximizing precision without arbitrary intermediate bit limits.
* **Boot Diagnostics:** During initialization, the driver prints the configured PWM frequency and actual hardware resolution steps, issuing warnings if the steps drop below 1024 to ensure optimal loop performance.
* **Software Dead-Time & Safety:** Automatically disables both PWM gates and blocks for 50 microseconds during direction changes to protect against shoot-through current.

---

## ⚙️ Project Configurations

### Devicetree (`app.overlay`)
The sensor is defined on the board's I2C2 bus (pins `PA9` for SCL and `PA8` for SDA) operating in *Fast Mode* (400 kHz). The sensor's hardware filter properties can be tuned via dts properties:

```dts
/ {
	engine: engine {
		compatible = "generic-engine";
		pwms = <&pwm2 1 50000 PWM_POLARITY_NORMAL>, /* TIM2 CH1 on PA0 (50us = 20kHz) */
		       <&pwm2 2 50000 PWM_POLARITY_NORMAL>; /* TIM2 CH2 on PA1 (50us = 20kHz) */
		enable-gpios = <&gpioa 4 GPIO_ACTIVE_HIGH>;  /* Enable (R_EN/L_EN) on PA4 */
		status = "okay";
	};
};

&i2c2 {
	status = "okay";
	pinctrl-0 = <&i2c2_scl_pa9 &i2c2_sda_pa8>;
	pinctrl-names = "default";
	clock-frequency = <400000>;

	as5600: as5600@36 {
		compatible = "custom,as5600";
		reg = <0x36>;                  /* Default I2C address */
		hysteresis = <0>;              /* Hysteresis in LSB (0, 1, 2 or 3) */
		slow-filter = <2>;             /* Hardware slow filter (16x, 8x, 4x, 2x) */
		fast-filter-threshold = <2>;   /* Fast filter threshold in LSB (0 to 7) */
	};	
};

&timers2 {
	status = "okay";
	pwm2: pwm {
		status = "okay";
		pinctrl-0 = <&tim2_ch1_pa0 &tim2_ch2_pa1>; /* PA0 and PA1 configured as TIM2 CH1/CH2 */
		pinctrl-names = "default";
	};
};
```

### Kconfig (`prj.conf`)
To activate the custom driver and define thread safety:
```kconfig
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_AS5600=y
CONFIG_AS5600_THREAD_SAFE=n

# PWM and Engine Configuration
CONFIG_PWM=y
CONFIG_ENGINE_THREAD_SAFE=y
```

---

## 🚀 How to Compile and Run

1.  **Clone the project and its dependencies:**
    Since this repository uses Git submodules to manage its external dependencies, clone the project using the `--recursive` flag:
    ```bash
    git clone --recursive git@github-ssme:smartsensingme/as5600-sensor-zephyr.git
    ```
    If you have already cloned the project without submodules, fetch the dependencies by running:
    ```bash
    git submodule update --init --recursive
    ```

2.  **Compile the Project:**
    Use the `west` utility specifying the target board:
    ```bash
    west build -b weact_stm32g431_core/stm32g431xx --pristine
    ```

3.  **Flash the Firmware:**
    ```bash
    west flash
    ```

4.  **View Serial Output:**
    The application runs the reading and Kalman Filter loop at **1 kHz** (`dt = 0.001s`). Logs are displayed at a controlled rate of 5 Hz (every 200 ms) to avoid saturating the serial port, and the magnet diagnostics are updated every 2 seconds:
    
    ```text
    AS5600 Magnetic Encoder Demonstration - High Speed Raw Sampling & 3D Kalman Filter
    AS5600 device custom_as5600@36 is ready
    Initializing Engine Driver...
    [Engine Driver] Dual PWM initialized at 20000 Hz.
    [Engine Driver] Hardware Resolution: 7200 steps.
    Engine Driver initialized successfully!
    Setting engine speed to 16.0%
    [AS5600 Status] Byte: 0x20 | Magnet: DETECTED | Strength: OK
    Raw Angle: 12.350 deg | Kalman Angle: 12.352 deg | Speed: 42.100 RPM | Accel: 1.250 RPM/s
    Raw Angle: 12.980 deg | Kalman Angle: 12.972 deg | Speed: 43.500 RPM | Accel: 0.820 RPM/s
    ```
    
    > **Note on Float Formatting:** The project uses a helper function (`printf_f`) to print fractional values (with 3 decimal places) because standard `printf` floating-point formatting is disabled by default in minimal Zephyr projects to save Flash memory.

---

## 📦 How to Reuse this Driver in Another Project

Since the driver was developed according to Zephyr's *out-of-tree* development guidelines and is hosted in the [zephyr_custom_drivers](https://github.com/smartsensingme/zephyr_custom_drivers.git) repository, you can easily reuse it in any other Zephyr project in two ways:

### Option A: Direct Copy to the Project
1.  Copy the `drivers/` and `dts/` folders from the `custom_drivers/` directory to the root of your new project.
2.  In your main `CMakeLists.txt`, add:
    ```cmake
    add_subdirectory(drivers)
    ```
3.  Create or modify the `Kconfig` file in the root of your project to include the driver path:
    ```kconfig
    source "Kconfig.zephyr"
    osource "drivers/Kconfig"
    ```

### Option B: Addition as an External Module / Git Submodule (Recommended)
This option keeps the driver centralized as a clean external dependency.
1.  In your new project, add the driver repository as a submodule:
    ```bash
    git submodule add https://github.com/smartsensingme/zephyr_custom_drivers.git custom_drivers
    ```
2.  In your main `CMakeLists.txt`, append the submodule location to the `ZEPHYR_EXTRA_MODULES` variable **before** including the main Zephyr package:
    ```cmake
    cmake_minimum_required(VERSION 3.20.0)
    
    list(APPEND ZEPHYR_EXTRA_MODULES "${CMAKE_CURRENT_LIST_DIR}/custom_drivers")
    
    find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
    project(my_new_project)
    ```
3.  Declare the sensor in your `app.overlay` and enable it with `CONFIG_AS5600=y` in `prj.conf`.

---
![SmartSensing.me Logo](https://smartsensing.me/ssme-logo.png)

## 📝 Description

This project is part of the **SmartSensing.me** ecosystem and goes beyond basic examples found online. Here, we apply real fundamentals of instrumentation engineering and high-performance embedded systems.

Unlike superficial, clickbait content, this repository delivers:
- **Originality:** Unique implementations based on nearly 30 years of academic experience.
- **Technical Depth:** Professional usage of the ESP-IDF framework and FreeRTOS.
- **Pedagogy:** Documented and structured code for those seeking genuine technical growth.

> "We transform signals from the physical world into digital intelligence, with no shortcuts."

---

## 🛠️ Technologies and Compatibility
- **Language:** Pure C (C99 or higher) and C++
- **Target Hardware:** Any microcontroller architecture (ESP32, STM32, ARM Cortex, RISC-V, AVR, etc.) or desktop architecture
- **Environments/RTOS:** ESP-IDF (as a native Component), Zephyr RTOS, FreeRTOS, Bare-metal, Desktop (Windows, Linux, macOS)
- **Build System:** Native CMake
- **Simulation:** LTSpice (Sensor modeling and validation)

---

## 👤 About the Author

**José Alexandre de França** *Associate Professor at the Department of Electrical Engineering of UEL*

Electrical Engineer with nearly three decades of experience in undergraduate and postgraduate teaching. PhD in Electrical Engineering, researcher in electronic instrumentation, and embedded systems developer. SmartSensing.me is my commitment to raising the bar of technology education in Brazil.

- 🌐 **Website:** [smartsensing.me](https://smartsensing.me)
- 📧 **E-mail:** [info@smartsensing.me](mailto:info@smartsensing.me)
- 📺 **YouTube:** [@smartsensingme](https://youtube.com/@smartsensingme)
- 📸 **Instagram:** [@smartsensing.me](https://instagram.com/smartsensing.me)

---

## 📄 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
