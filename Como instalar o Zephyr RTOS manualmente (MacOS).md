# Como instalar o Zephyr RTOS manualmente (MacOS)

## 1. Instalando o Homebrew

O caminho mais limpo e sem dores de cabeça no Mac é usando o **Homebrew**. Se você não tiver o Homebrew instalado, basta rodar o comando oficial deles no terminal antes de começar:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

## 2. Instalando as dependências

Abra o Terminal e instale as ferramentas de compilação e o Python atualizado.

```bash
brew install cmake ninja gperf python3 ccache dfu-util dtc wget xz coreutils
```

## 3. Instalar o West e Inicializar o Workspace

O `west` gerencia todo o ecossistema do Zephyr. Vamos instalá-lo e criar a pasta do projeto.

```bash
pip3 install --user -U west
```

O `west`normalmente é instalado no diretório `~/Library/Python/3.x/bin`. O `x` depende da versão do Python instalada no seu sistema. Verifique antes do caminho correto. 

Por exemplo, suponha que o `west`foi instalado em `/Library/Python/3.13/bin`. Neste caso, você terá que inserir esse diretório na sua variável `PATH`. Faça isso editando o arquivo de configuração `~/.zshrc` ou `~/.bashrc` (dependendo se seu shell é o ZSH ou BASH). Em qualquer um dos casos, edite o arquivo de configuração e acrescente a linha que segue no final.

```bash
export PATH="$HOME/Library/Python/3.13/bin:$PATH"
```

Feche seu terminal e o abra novamente. Isso vai garantir que a variável `PATH` foi atualizada.

Agora, inicalize o Workspace dentro da pasta **STM**.

```bash
west init STM
cd STM
```

Já que vamos trabalhar com microcontroladores ARM da STM, habilite especificamente o grupo da ST (hal_st) e dependências de arquitetura (cmsis).

```bash
west config manifest.group-filter -- "+hal_st,+cmsis"
west update --narrow --fetch-opt=--depth=1
```

## 4. Instalando o Toolchain STM32

Agora, você precisa instalar o compilador que vai transformar o seu código em um binário compatível com o seu microcontrolador (no nosso caso, o ARM). Para isso, [clique aqui]([Releases · zephyrproject-rtos/sdk-ng · GitHub](https://github.com/zephyrproject-rtos/sdk-ng/releases/)) e depois **clique no título** da versão *latest*.

Na próxima página, aparece duas tabelas: **SDK Bundle** e **GNU Toolchains**. Primeiro, instale a versão **Minimal** compatível com o seu *sistema* (Linux, macOS ou Windows). Depois, instale o `tollchain` de acordo com seu *microcontrolador* e seu *sistema*. Veja em mais em detalhes em seguida.

Em vez de baixar o pacote completo (`_gnu.tar.xz`), que vem com compiladores para todas as arquiteturas do planeta, nós vamos baixar o pacote **`minimal` para macOS** e adicionar **apenas** a toolchain para ARM. Faça o que segue.

Criar a pasta `toolchain` assumindo que você ainda está na pasta `STM`.

```bash
cd ../
mkdir toolchain
cd toolchain
```

Se o seu Mac for **Apple Silicon (M1/M2/M3)**, faça o seguinte no terminal:

```bash
# 1. Baixe o SDK Minimal v1.0.1 para macOS (ARM64 nativo)
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/zephyr-sdk-1.0.1_macos-aarch64_minimal.tar.xz

# 2. Extraia o conteúdo (ele criará a pasta zephyr-sdk-1.0.1)
tar xvf zephyr-sdk-1.0.1_macos-aarch64_minimal.tar.xz
cd zephyr-sdk-1.0.1

# 3. Baixe APENAS a toolchain do compilador GNU ARM (usado pelos STM32)
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/toolchain_gnu_macos-aarch64_arm-zephyr-eabi.tar.xz

# 4. Extraia a toolchain diretamente para dentro do SDK
tar xvf toolchain_gnu_macos-aarch64_arm-zephyr-eabi.tar.xz
```

Agora, precisamos registrar o toolchain no sistema. Só precisa rodar o script de configuração para que o CMake do Zephyr saiba exatamente onde encontrá-los:

```bash
./setup.sh -t arm-zephyr-eabi
```

O script vai registrar o caminho no arquivo de configuração global do CMake do seu usuário (`~/.cmake/packages/Zephyr-sdk`). Pronto! Você não precisa configurar variáveis de ambiente complexas no seu `~/.zshrc` ou `~/.bashrc`.

## 5. Onde inserir seu projeto

Sua pasta `~/ZephyrRTOS` ficará organizada de forma limpa e otimizada como segue:

```
~/ZephyrRTOS/
├── .west/               <-- Seu Workspace Zep
├── STM/               <-- Seu Workspace Zephyr (Core + HAL ST + Seus Projetos)
│   └── zephyr/
│   |   └── samples/
│   └── .west/         <-- Pasta criada pelo comando west init
│   └── meu_projeto/   <-- Pasta base com seu projeto
└── toolchain/         <-- O Compilador Isolado
    └── zephyr-sdk-1.0.1/
        ├── arm-zephyr-eabi/   <-- Único compilador instalado (Cortex-M)
        └── setup.sh
```

Como indicado, ao criar um novo projeto, é importante que a pasta base deste projeto (no exemplo, ela é referenciada por **meu_projeto**) fique no mesmo nível da pasta oculta **.west**, criada pelo comando `west init`.

## 6. Build

Uma vez que sue projeto for criado e seu código fonte estiver pronto, você pode compilá-lo. Para isso, use o comando que segue.

```bash
west build -b weact_stm32g431_core/stm32g431xx
```

Cada parte dele tem uma função específica no ecossistema do Zephyr:

1. **`west`**  
   É a ferramenta de gerenciamento do Zephyr (meta-tool). Ela funciona como um "guarda-chuva" que orquestra várias ferramentas por baixo dos panos (como Git, CMake, Ninja, compiladores GCC para ARM e gravadores).

2. **`build`**  
   É o subcomando do `west` responsável por invocar o sistema de compilação (CMake + Ninja). Ele analisa o código-fonte (`src/`), as configurações (`prj.conf`), o mapeamento de hardware (`app.overlay`) e os arquivos de compilação (`CMakeLists.txt`) para gerar os binários.

3. **`-b`** (ou `--board`)  
   Sinaliza para o compilador qual é a placa/plataforma de destino.

4. **`weact_stm32g431_core/stm32g431xx`**  
   Indica o identificador da placa e a variante do microcontrolador (SoC):
   
   - `weact_stm32g431_core`: Nome do modelo da placa de desenvolvimento da WeAct.
   - `/stm32g431xx`: A variante específica do chip STM32G431 utilizada nela.

### 6.1 Como identificar sua placa

A melhor forma de descobrir o nome exato que vem depois do `-b` (board) no Zephyr é usando a própria ferramenta **West** através do terminal. Como a árvore de dispositivos (Device Tree) e os nomes das placas mudam ou ganham variações conforme o Zephyr atualiza, os comandos internos são sempre a fonte da verdade.

Aqui estão as três melhores maneiras de descobrir e listar esses nomes:

#### 6.1.1. O Método Definitivo: `west boards` (A partir do terminal)

O comando mais rápido e prático para listar todas as placas suportadas no seu workspace atual é o `west boards`.

Por exemplo, caso você esteja focado estritamente na família **STM32**, você pode filtrar a saída usando o comando padrão do terminal (`grep` no macOS/Linux).

Navegue até a pasta do seu workspace e rode:

```bash
cd ~/ZephyrRTOS/STM/zephyr
west boards | grep stm32
```

Se você quiser procurar especificamente por uma placa de um fabricante alternativo (como a **WeAct Studio**, que você mencionou), você pode filtrar por ela:

```bash
west boards | grep stm32 | grep weact
```

O terminal vai retornar exatamente a string mágica que você precisa passar para o `-b`, por exemplo: `weact_stm32g431_core` ou `weact_stm32g431_core/stm32g431xx`.

#### 6.1.2. Olhando a estrutura de pastas do Zephyr

Se você quiser entender de onde o West tira esses nomes, eles refletem exatamente a estrutura de arquivos dentro do repositório do Zephyr.

Todas as placas ficam mapeadas dentro do diretório `boards/`. Você pode listar as pastas dessa região para encontrar o que procura:

```bash
ls ~/ZephyrRTOS/STM/zephyr/boards/weact/
```

*(Nas versões mais recentes do Zephyr, as placas passaram a ser organizadas por fabricante dentro de `boards/<fabricante>/<nome_da_placa>`).*

Se você entrar na pasta da placa (ex: `boards/weact/weact_stm32g431_core/`), você verá um arquivo chamado `board.yml`. É dentro desse arquivo que o Zephyr define os alvos de build (como o `stm32g431xx`).

#### 6.1.3. Deixando o West sugerir (Forçando um erro amigável)

Se você souber parte do nome da placa mas não lembrar o resto, você pode simplesmente digitar um nome errado ou incompleto de propósito. O West interromperá o build e listará as opções mais próximas ou todas as placas disponíveis no sistema.

Por exemplo:

```bash
west build -b weact_g431_errado samples/basic/blinky
```

O Zephyr vai retornar uma mensagem de erro parecida com:

```
FATAL ERROR: board 'weact_g431_errado' not found.
Valid boards include:
  - nucleo_g431rb
  - weact_stm32g431_core
  ...
```

No seu dia a dia desenvolvendo para a linha STM32G431 ou similares da ST, deixe um terminal aberto e use sempre o `west boards | grep g431` para validar se o Zephyr mapeou sua placa exatamente com o sufixo do chip (`/stm32g431xx`) ou apenas com o nome base da placa.

## 7. Flash

Após o build, com o binário gerado, ele pode ser gravado no microcontrolador e executado. Para isso, use o comando que segue.

```bash
west flash -d build -r openocd --config ./openocd_hla.cfg
```

Em seguida, cada parte deste comando é dicutida em detalhes.

1.  **`west`**
   
   A ferramenta de gerenciamento do ecossistema Zephyr. Ela atua como uma interface unificada para diversas ações de desenvolvimento (como gerenciar repositórios, compilar e gravar).

2. **`flash`**
   
   É o subcomando do `west` encarregado de transferir (gravar) o binário compilado da memória do seu computador diretamente para a memória Flash do microcontrolador (por exemplo, o STM32G431).

3.  **`-d build`** (ou `--build-dir build`)
   
   Especifica a pasta onde estão localizados os arquivos gerados no processo de build (compilação).
   
   - Por padrão, o `west` procura uma pasta chamada `build/` no diretório atual.
   
   - Ele precisa acessar essa pasta para localizar o arquivo de metadados do runner (`runners.yaml`) e o arquivo binário compilado (`zephyr.hex` ou `zephyr.bin`).

4. **`-r openocd`** (ou `--runner openocd`)
   
   Seleciona o **runner** (o driver/ferramenta de gravação) que será usado para conversar com a placa.
   
   - O Zephyr suporta múltiplos runners (ex: `jlink`, `pyocd`, `nrfjprog`, `dfu`, etc.).
   
   - Ao declarar `-r openocd`, você está instruindo o `west` a invocar o utilitário **OpenOCD** (Open On-Chip Debugger) para gerenciar a comunicação com o programador ST-Link conectado.

5.  **`--config ./openocd_hla.cfg`** Esses são argumentos extras passados diretamente para o backend do runner selecionado (OpenOCD).
   
   - Ele diz ao OpenOCD para ler o arquivo de configuração local 
   
   - openocd_hla.cfg em vez de usar as configurações geradas automaticamente pelo Zephyr.
   
   - Isso é essencial para que o OpenOCD saiba que deve operar no modo de alto nível (`hla_swd`) e em velocidade reduzida (`500 kHz`).

#### 7.1 O arquivo `openocd_hla.cfg`

Para quem usa aquele gravadores não oficiais e baratos, que usam a versão 2 do ST Link, o arquivo `openocd_hla.cfg` é indispensável. O conteúdo desde arquivo é o que segue.

```
source [find interface/stlink.cfg]
transport select hla_swd
source [find target/stm32g4x.cfg]
adapter speed 500
```

Esse arquivo garante que o OpenOCD usará configurações mais modestas, compatível com o gravador.

## 8. Conclusão

A instalação manual do **Zephyr RTOS** no macOS oferece um ambiente de desenvolvimento extremamente limpo, robusto e otimizado para sistemas embarcados. Ao optar pelo uso do **West** combinado com a instalação cirúrgica do SDK *Minimal* e apenas o toolchain necessário para a arquitetura **ARM** (Cortex-M), evita-se o desperdício de espaço em disco e o acúmulo de dependências desnecessárias.

Além disso, a integração direta com o **CMake** dispensa a necessidade de gerenciar variáveis de ambiente complexas no sistema, enquanto o uso do **OpenOCD** com configurações personalizadas (como o `openocd_hla.cfg`) confere a flexibilidade necessária para trabalhar de forma estável com diferentes hardwares de gravação. Com o ecossistema devidamente estruturado e os comandos de *build* e *flash* compreendidos, o workspace está pronto para o desenvolvimento de aplicações robustas e de alto desempenho para a linha **STM32**.
