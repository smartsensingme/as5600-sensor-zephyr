# AS5600 Magnetic Encoder & Kalman Filter - Zephyr RTOS

*Leia em outros idiomas: [English](README.md)*

Este repositório contém a demonstração de integração do codificador magnético **AS5600** com o **Zephyr RTOS**, utilizando um driver nativo do tipo *out-of-tree* e um **Filtro de Kalman 3D** avançado para estimar a posição, a velocidade (RPM) e a aceleração (RPM/s) de um eixo rotativo em tempo real.

O projeto está configurado para rodar na placa de desenvolvimento **WeAct STM32G431 Core Board**.

---

## 🛠️ Arquitetura do Driver (`custom_as5600`)

O driver do AS5600 está implementado como um módulo externo e expõe as leituras do codificador através da API de sensores padrão do Zephyr RTOS (`<zephyr/drivers/sensor.h>`).

### Canais de Sensor Suportados
O driver disponibiliza três canais para amostragem:
*   **`SENSOR_CHAN_ROTATION`** (Ângulo Processado): Retorna o ângulo suavizado do registrador de hardware **`ANGLE`** (com processamento de histerese e filtros digitais configurados diretamente no registrador do chip).
*   **`SENSOR_CHAN_RAW_ROTATION`** (Ângulo Bruto - Customizado): Canal privado (`SENSOR_CHAN_PRIV_START + 0`) que lê o valor do registrador **`RAW ANGLE`** (leitura direta do processador CORDIC do sensor, sem filtragem interna ou atraso de hardware).
*   **`SENSOR_CHAN_STATUS`** (Status do Ímã - Customizado): Canal privado (`SENSOR_CHAN_PRIV_START + 1`) que retorna o byte de diagnóstico do sensor, permitindo verificar a presença e intensidade do campo magnético:
    *   `AS5600_STATUS_MD` (Magnet Detected): Ímã detectado.
    *   `AS5600_STATUS_ML` (Magnet Too Weak): Campo magnético muito fraco.
    *   `AS5600_STATUS_MH` (Magnet Too Strong): Campo magnético muito forte.

### Recursos & Otimizações
1.  **Leitura em Bloco (*Burst Read*) de Alta Velocidade:** Ao realizar o *fetch* dos canais de rotação (`SENSOR_CHAN_ROTATION` ou `SENSOR_CHAN_RAW_ROTATION`), o driver executa uma única transação I2C de 4 bytes contínuos (iniciando em `RAW_ANGLE_H`), otimizando o uso do barramento I2C e permitindo uma frequência teórica máxima de leitura de **6.06 kHz**.
2.  **Thread Safety Configurável:** O driver suporta execução segura em sistemas multithread. Ao habilitar a opção `CONFIG_AS5600_THREAD_SAFE=y`, um mutex interno sincroniza os acessos ao I2C e às variáveis internas do driver. Para cenários de alta frequência e thread única onde o overhead de mutexes é indesejado, a opção pode ser desativada (`CONFIG_AS5600_THREAD_SAFE=n`).

---

## 📈 Filtro de Kalman (2D e 3D)

O projeto inclui uma biblioteca de Filtro de Kalman linear em [kalman.h](file:///Volumes/data2005/jalexdefranca/Coding/src/ZephyrRTOS/STM/AS5600/src/kalman.h) e [kalman.c](file:///Volumes/data2005/jalexdefranca/Coding/src/ZephyrRTOS/STM/AS5600/src/kalman.c) que implementa as equações clássicas para modelos de estados em 2D e 3D:
*   **Kalman 2D:** Estima a posição angular ($\theta$) e a velocidade angular ($\omega$).
*   **Kalman 3D:** Estima a posição angular ($\theta$), velocidade angular ($\omega$) e a aceleração angular ($\alpha$).

### Correção de Transição Angular (*Wrap-around*)
Devido ao comportamento circular do codificador ($0^\circ \to 360^\circ$), o loop principal ([main.c](file:///Volumes/data2005/jalexdefranca/Coding/src/ZephyrRTOS/STM/AS5600/src/main.c)) implementa a função especializada `engine_angle_kalman_3d_update` (e a alternativa `engine_angle_kalman_2d_update`) para normalizar o erro de medição (inovação) na faixa de $[-180^\circ, 180^\circ]$. 

Isso impede picos falsos e instabilidades no cálculo da velocidade e aceleração quando o sensor passa pela transição física de $360^\circ$ para $0^\circ$. A estimativa final de ângulo é sempre mantida estritamente no intervalo $[0^\circ, 360^\circ)$.

---

## ⚙️ Configurações do Projeto

### Devicetree (`app.overlay`)
O sensor é definido no barramento I2C2 da placa (pinos `PA9` para SCL e `PA8` para SDA) operando em *Fast Mode* (400 kHz). As propriedades do filtro de hardware do sensor podem ser sintonizadas via propriedades dts:

```dts
&i2c2 {
	status = "okay";
	pinctrl-0 = <&i2c2_scl_pa9 &i2c2_sda_pa8>;
	pinctrl-names = "default";
	clock-frequency = <400000>;

	as5600: as5600@36 {
		compatible = "custom,as5600";
		reg = <0x36>;                  /* Endereço I2C padrão */
		hysteresis = <0>;              /* Histerese em LSB (0, 1, 2 ou 3) */
		slow-filter = <2>;             /* Filtro lento de hardware (16x, 8x, 4x, 2x) */
		fast-filter-threshold = <2>;   /* Limiar do filtro rápido em LSB (0 a 7) */
	};	
};
```

### Kconfig (`prj.conf`)
Para ativar o driver personalizado e definir a segurança de threads:
```kconfig
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_AS5600=y

# Defina como 'y' se precisar acessar o driver de múltiplas threads do Zephyr
CONFIG_AS5600_THREAD_SAFE=n
```

---

## 🚀 Como Compilar e Executar

1.  **Compilar o Projeto:**
    Utilize o utilitário `west` especificando a placa de desenvolvimento:
    ```bash
    west build -b weact_stm32g431_core/stm32g431xx --pristine
    ```

2.  **Gravar o Firmware:**
    ```bash
    west flash
    ```

3.  **Visualizar Saída Serial:**
    A aplicação executa o loop de leitura e processamento do Filtro de Kalman a **1 kHz** (`dt = 0.001s`). Os logs são exibidos a uma taxa controlada de 5 Hz (a cada 200 ms) para evitar sobrecarregar a porta serial, e o diagnóstico magnético é atualizado a cada 2 segundos:
    
    ```text
    AS5600 Magnetic Encoder Demonstration - High Speed Raw Sampling & 3D Kalman Filter
    AS5600 device custom_as5600@36 is ready
    [AS5600 Status] Byte: 0x20 | Magnet: DETECTED | Strength: OK
    Raw Angle: 12.350 deg | Kalman Angle: 12.352 deg | Speed: 0.000 RPM | Accel: 0.000 RPM/s
    Raw Angle: 12.980 deg | Kalman Angle: 12.972 deg | Speed: 10.500 RPM | Accel: 0.820 RPM/s
    ```
    
    > **Nota sobre Formatação de Float:** O projeto utiliza uma função auxiliar (`printf_f`) para imprimir valores fracionários (com 3 casas decimais), pois a formatação de ponto flutuante no `printf` padrão do Zephyr é desativada por padrão em plataformas embarcadas para economizar memória Flash.

---

## 📦 Como Reutilizar este Driver em outro Projeto

Como o driver foi desenvolvido segundo as diretrizes de desenvolvimento fora da árvore do Zephyr (*out-of-tree*), você pode reutilizá-lo facilmente de duas maneiras:

### Opção A: Cópia Direta para o Projeto
1.  Copie a pasta `drivers/` e `dts/` da pasta `custom_drivers/` para a raiz do seu novo projeto.
2.  No seu `CMakeLists.txt` principal, adicione:
    ```cmake
    add_subdirectory(drivers)
    ```
3.  Crie ou modifique o arquivo `Kconfig` na raiz do seu projeto para incluir o caminho do driver:
    ```kconfig
    source "Kconfig.zephyr"
    osource "drivers/Kconfig"
    ```

### Opção B: Adição como Módulo Externo (Recomendado)
Esta opção mantém o driver centralizado em uma pasta e evita cópias desnecessárias de arquivos.
1.  No arquivo `CMakeLists.txt` do seu novo projeto, aponte a localização absoluta ou relativa do repositório `custom_drivers` na variável `ZEPHYR_EXTRA_MODULES` **antes** de incluir o pacote principal do Zephyr:
    ```cmake
    cmake_minimum_required(VERSION 3.20.0)
    
    list(APPEND ZEPHYR_EXTRA_MODULES "/caminho/para/custom_drivers")
    
    find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
    project(meu_novo_projeto)
    ```
2.  Agora você já pode adicionar as definições no seu `app.overlay` e ativar com `CONFIG_AS5600=y` no `prj.conf`.

---
![SmartSensing.me Logo](https://smartsensing.me/ssme-logo.png)

## 📝 Descrição

Este projeto faz parte do ecossistema **SmartSensing.me** e vai além dos exemplos básicos encontrados na internet. Aqui, aplicamos os fundamentos reais da engenharia de instrumentação e sistemas embarcados de alta performance.

Diferente de conteúdos superficiais voltados apenas para cliques, este repositório entrega:
- **Ineditismo:** Implementações originais baseadas em quase 30 anos de experiência acadêmica.
- **Densidade Técnica:** Uso profissional do framework ESP-IDF e FreeRTOS.
- **Didática:** Código documentado e estruturado para quem busca evolução técnica real.

> "Transformamos sinais do mundo físico em inteligência digital, sem atalhos."

---

## 🛠️ Tecnologias
- **Hardware Target:** ESP32 / ESP32-S3
- **Framework:** ESP-IDF v5.x / v6.x
- **Linguagem:** C / C++
- **Simulação:** LTSpice (Modelagem de Sensores)

---

## 👤 Sobre o Autor

**José Alexandre de França** *Professor Adjunto no Departamento de Engenharia Elétrica da UEL*

Engenheiro Eletricista com quase três décadas de experiência no ensino de graduação e pós-graduação. Doutor em Engenharia Elétrica, pesquisador em instrumentação eletrônica e desenvolvedor de sistemas embarcados. O SmartSensing.me é o meu compromisso de elevar o nível da educação tecnológica no Brasil.

- 🌐 **Website:** [smartsensing.me](https://smartsensing.me)
- 📧 **E-mail:** [info@smartsensing.me](mailto:info@smartsensing.me)
- 📺 **YouTube:** [@smartsensingme](https://youtube.com/@smartsensingme)
- 📸 **Instagram:** [@smartsensing.me](https://instagram.com/smartsensing.me)

---

## 📄 Licença

Este projeto está sob a licença MIT. Veja o arquivo [LICENSE](LICENSE) para detalhes.
