# Como instalar o Zephyr RTOS manualmente (Windows)

## 1. Instalando o Chocolatey

O caminho mais limpo e automatizado para gerenciar ferramentas de compilação no Windows é usando o **Chocolatey**.

1. Clique com o botão direito no menu Iniciar do Windows e selecione **Terminal (Administrador)** ou **PowerShell (Administrador)**.

2. Execute o comando abaixo para instalar o Chocolatey:

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
```

3. Feche o terminal e abra-o novamente (como Administrador) para garantir que o comando `choco` esteja ativo.

## 2. Instalando as dependências

Com o Chocolatey pronto, instale todas as ferramentas de compilação necessárias de uma só vez:

```powershell
choco install -y cmake ninja gperf python git dtc-msys2 wget 7zip
```

*Nota: O pacote `dtc-msys2` instala o Device Tree Compiler necessário para o Zephyr analisar o hardware.*

## 3. Instalar o West e Inicializar o Workspace

O `west` gerencia todo o ecossistema do Zephyr. Vamos instalá-lo e criar a pasta do projeto.

```powershell
pip3 install --user -U west
```

No Windows, o Python costuma instalar o `west` dentro do diretório de scripts do usuário (geralmente em `AppData\Roaming\Python\Python3x\Scripts`). Para garantir que o prompt reconheça o comando, precisamos adicionar esse caminho à variável de ambiente `Path` do usuário.

No PowerShell, você pode fazer isso de forma permanente rodando:

```powershell
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$westPath = "$env:USERPROFILE\AppData\Roaming\Python\Python313\Scripts" # Verifique sua versão do Python (ex: Python313)
[Environment]::SetEnvironmentVariable("Path", $userPath + ";" + $westPath, "User")
```

> ⚠️ **Atenção:** Verifique em `C:\Users\SeuUsuario\AppData\Roaming\Python\` qual é o número exato da pasta do seu Python antes de rodar o comando acima.

Feche o terminal e abra-o novamente (agora você pode usar o Terminal normal, sem ser administrador).

Navegue até a pasta onde deseja concentrar seus projetos (por exemplo, dentro do seu usuário) e inicialize o Workspace dentro da pasta **STM**:

```powershell
cd ~
mkdir ZephyrRTOS
cd ZephyrRTOS
west init STM
cd STM
```

Já que o foco é trabalhar com microcontroladores ARM da STM, habilite especificamente o grupo da ST (`hal_st`) e as dependências de arquitetura (`cmsis`).

```powershell
west config manifest.group-filter -- "+hal_st,+cmsis"
west update --narrow --fetch-opt=--depth=1
```

## 4. Instalando o Toolchain STM32

Agora, você precisa instalar o compilador que vai transformar o seu código em um binário compatível com o seu microcontrolador (no nosso caso, o ARM). Para isso, [clique aqui]([Releases · zephyrproject-rtos/sdk-ng · GitHub](https://github.com/zephyrproject-rtos/sdk-ng/releases/)) e depois **clique no título** da versão *latest*.

Na próxima página, aparece duas tabelas: **SDK Bundle** e **GNU Toolchains**. Primeiro, instale a versão **Minimal** compatível com o seu *sistema* (Linux, macOS ou Windows). Depois, instale o `tollchain` de acordo com seu *microcontrolador* e seu *sistema*. Veja em mais em detalhes em seguida.

Em vez de baixar o pacote completo, que vem com compiladores para todas as arquiteturas do planeta, nós vamos baixar o pacote **`minimal` para Windows** e adicionar **apenas** a toolchain para ARM. Faça o que segue.

Criar a pasta `toolchain` assumindo que você ainda está na pasta `STM`.

```bash
cd ../
mkdir toolchain
cd toolchain
```

Execute a sequência de comandos para baixar e extrair o SDK e a Toolchain usando o PowerShell:

```powershell
# 1. Baixe o SDK Minimal v1.0.1 para Windows (x86_64)
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/zephyr-sdk-1.0.1_windows-x86_64_minimal.7z -OutFile zephyr-sdk-minimal.7z

# 2. Extraia o SDK (usando o 7-Zip instalado via choco)
7z x zephyr-sdk-minimal.7z
cd zephyr-sdk-1.0.1

# 3. Baixe APENAS a toolchain do compilador GNU ARM
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/toolchain_gnu_windows-x86_64_arm-zephyr-eabi.7z -OutFile toolchain_arm.7z

# 4. Extraia a toolchain diretamente para dentro do SDK
tar xvf toolchain_gnu_macos-aarch64_arm-zephyr-eabi.tar.xz
```

Agora, precisamos registrar o toolchain no sistema. No Windows, em vez de um script `.sh`, executamos o script em lote para registrar o caminho no CMake global do usuário (`%USERPROFILE%\.cmake\packages\Zephyr-sdk`):

```powershell
setup.cmd /t arm-zephyr-eabi
```

Pronto! O CMake do Zephyr agora sabe exatamente onde encontrar o compilador, sem a necessidade de criar variáveis de ambiente globais complexas e poluídas.

## 5. Onde inserir seu projeto

Sua pasta `~/ZephyrRTOS` ficará organizada de forma limpa e otimizada como segue:

```
C:\Users\SeuUsuario\ZephyrRTOS\
├── .west/               <-- Seu Workspace Zep
├── STM/               <-- Seu Workspace Zephyr (Core + HAL ST + Seus Projetos)
│   └── zephyr/
│   |   └── samples/
│   └── .west/         <-- Pasta criada pelo comando west init
│   └── meu_projeto/   <-- Pasta base com seu projeto
└── toolchain/         <-- O Compilador Isolado
    └── zephyr-sdk-1.0.1/
        ├── arm-zephyr-eabi/   <-- Único compilador instalado (Cortex-M)
        └── setup.cmd
```

Como indicado, ao criar um novo projeto, é importante que a pasta base deste projeto (no exemplo, ela é referenciada por **meu_projeto**) fique no mesmo nível da pasta oculta **.west**, criada pelo comando `west init`.

## 6. Build

Uma vez que sue projeto for criado e seu código fonte estiver pronto, você pode compilá-lo. Para isso, use o comando que segue.

```powershell
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

Por exemplo, caso você esteja focado estritamente na família **STM32**, você pode filtrar a saída usando o comando padrão do terminal (`Select-String` no Windows).

Navegue até a pasta do seu workspace e rode:

```powershell
cd ~\ZephyrRTOS\STM\zephyr
west boards | Select-String "stm32"
```

Se você quiser procurar especificamente por uma placa de um fabricante alternativo (como a **WeAct Studio**, que você mencionou), você pode filtrar por ela:

```powershell
west boards | Select-String "stm32" | Select-String "weact"
```

O terminal vai retornar exatamente a string mágica que você precisa passar para o `-b`, por exemplo: `weact_stm32g431_core` ou `weact_stm32g431_core/stm32g431xx`.

#### 6.1.2. Olhando a estrutura de pastas do Zephyr

Se você quiser entender de onde o West tira esses nomes, eles refletem exatamente a estrutura de arquivos dentro do repositório do Zephyr.

Todas as placas ficam mapeadas dentro do diretório `boards/`. Você pode listar as pastas dessa região para encontrar o que procura:

```powershell
Get-ChildItem ~\ZephyrRTOS\STM\zephyr\boards\weact\
```

*(Nas versões mais recentes do Zephyr, as placas passaram a ser organizadas por fabricante dentro de `boards\<fabricante>\<nome_da_placa>`).*

Se você entrar na pasta da placa (ex: `boards\weact\weact_stm32g431_core\`), você verá um arquivo chamado `board.yml`. É dentro desse arquivo que o Zephyr define os alvos de build (como o `stm32g431xx`).

#### 6.1.3. Deixando o West sugerir (Forçando um erro amigável)

Se você souber parte do nome da placa mas não lembrar o resto, você pode simplesmente digitar um nome errado ou incompleto de propósito. O West interromperá o build e listará as opções mais próximas ou todas as placas disponíveis no sistema.

Por exemplo:

```bash
west build -b weact_g431_errado samples\basic\blinky
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

A instalação manual do **Zephyr RTOS** no Windows oferece um ambiente de desenvolvimento extremamente limpo, robusto e otimizado para sistemas embarcados. Ao optar pelo uso do **West** combinado com a instalação cirúrgica do SDK *Minimal* e apenas o toolchain necessário para a arquitetura **ARM** (Cortex-M), evita-se o desperdício de espaço em disco e o acúmulo de dependências desnecessárias.

Além disso, a integração direta com o **CMake** dispensa a necessidade de gerenciar variáveis de ambiente complexas no sistema, enquanto o uso do **OpenOCD** com configurações personalizadas (como o `openocd_hla.cfg`) confere a flexibilidade necessária para trabalhar de forma estável com diferentes hardwares de gravação. Com o ecossistema devidamente estruturado e os comandos de *build* e *flash* compreendidos, o workspace está pronto para o desenvolvimento de aplicações robustas e de alto desempenho para a linha **STM32**.
