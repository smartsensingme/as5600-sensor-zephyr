# AS5600 Magnetic Encoder & Kalman Filter - Zephyr RTOS

*Read in other languages: [Português](README.pt-br.md)*

This repository contains a demonstration of the integration of the **AS5600** magnetic encoder with **Zephyr RTOS**, utilizing a native *out-of-tree* sensor driver and an advanced **3D Kalman Filter** to estimate the position, speed (RPM), and acceleration (RPM/s) of a rotating shaft in real time.

The project is configured to run on the **WeAct STM32G431 Core Board** development board.

---

## 🛠️ Driver Architecture (`custom_as5600`)

The AS5600 driver is implemented as an external module and exposes encoder readings through the standard Zephyr RTOS sensor API (`<zephyr/drivers/sensor.h>`).

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

The project includes a linear Kalman Filter library in [kalman.h](file:///Volumes/data2005/jalexdefranca/Coding/src/ZephyrRTOS/STM/AS5600/src/kalman.h) and [kalman.c](file:///Volumes/data2005/jalexdefranca/Coding/src/ZephyrRTOS/STM/AS5600/src/kalman.c) that implements the classical equations for 2D and 3D state-space models:
*   **2D Kalman:** Estimates angular position ($\theta$) and angular velocity ($\omega$).
*   **3D Kalman:** Estimates angular position ($\theta$), angular velocity ($\omega$), and angular acceleration ($\alpha$).

### Angular Transition Correction (Wrap-around)
Due to the circular behavior of the encoder ($0^\circ \to 360^\circ$), the main loop ([main.c](file:///Volumes/data2005/jalexdefranca/Coding/src/ZephyrRTOS/STM/AS5600/src/main.c)) implements the specialized function `engine_angle_kalman_3d_update` (and the alternative `engine_angle_kalman_2d_update`) to normalize the measurement error (innovation) to the range of $[-180^\circ, 180^\circ]$.

This prevents false spikes and instability in the velocity and acceleration calculations when the sensor transitions through the physical boundary between $360^\circ$ and $0^\circ$. The final angle estimate is always kept strictly in the range $[0^\circ, 360^\circ)$.

---

## ⚙️ Project Configurations

### Devicetree (`app.overlay`)
The sensor is defined on the board's I2C2 bus (pins `PA9` for SCL and `PA8` for SDA) operating in *Fast Mode* (400 kHz). The sensor's hardware filter properties can be tuned via dts properties:

```dts
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
```

### Kconfig (`prj.conf`)
To activate the custom driver and define thread safety:
```kconfig
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_AS5600=y

# Set to 'y' if you need to access the driver from multiple Zephyr threads
CONFIG_AS5600_THREAD_SAFE=n
```

---

## 🚀 How to Compile and Run

1.  **Compile the Project:**
    Use the `west` utility specifying the target board:
    ```bash
    west build -b weact_stm32g431_core/stm32g431xx --pristine
    ```

2.  **Flash the Firmware:**
    ```bash
    west flash
    ```

3.  **View Serial Output:**
    The application runs the reading and Kalman Filter loop at **1 kHz** (`dt = 0.001s`). Logs are displayed at a controlled rate of 5 Hz (every 200 ms) to avoid saturating the serial port, and the magnet diagnostics are updated every 2 seconds:
    
    ```text
    AS5600 Magnetic Encoder Demonstration - High Speed Raw Sampling & 3D Kalman Filter
    AS5600 device custom_as5600@36 is ready
    [AS5600 Status] Byte: 0x20 | Magnet: DETECTED | Strength: OK
    Raw Angle: 12.350 deg | Kalman Angle: 12.352 deg | Speed: 0.000 RPM | Accel: 0.000 RPM/s
    Raw Angle: 12.980 deg | Kalman Angle: 12.972 deg | Speed: 10.500 RPM | Accel: 0.820 RPM/s
    ```
    
    > **Note on Float Formatting:** The project uses a helper function (`printf_f`) to print fractional values (with 3 decimal places) because standard `printf` floating-point formatting is disabled by default in minimal Zephyr projects to save Flash memory.

---

## 📦 How to Reuse this Driver in Another Project

Since the driver was developed according to Zephyr's *out-of-tree* development guidelines, you can easily reuse it in two ways:

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

### Option B: Addition as an External Module (Recommended)
This option keeps the driver centralized in a single folder and avoids unnecessary file copies.
1.  In your new project's `CMakeLists.txt`, append the absolute or relative location of the `custom_drivers` repository to the `ZEPHYR_EXTRA_MODULES` variable **before** including the main Zephyr package:
    ```cmake
    cmake_minimum_required(VERSION 3.20.0)
    
    list(APPEND ZEPHYR_EXTRA_MODULES "/path/to/custom_drivers")
    
    find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
    project(my_new_project)
    ```
2.  Now you can add the declarations in your `app.overlay` and enable it with `CONFIG_AS5600=y` in `prj.conf`.

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

## 🛠️ Technologies
- **Target Hardware:** ESP32 / ESP32-S3
- **Framework:** ESP-IDF v5.x / v6.x
- **Language:** C / C++
- **Simulation:** LTSpice (Sensor Modeling)

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
