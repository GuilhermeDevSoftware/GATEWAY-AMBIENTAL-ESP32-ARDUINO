# Gateway Ambiental ESP32 + Arduino

<img width="1600" height="900" alt="d56c7bdc-d202-4535-9688-38fe051ee091" src="https://github.com/user-attachments/assets/9c6309a2-b1e6-4c1f-bde6-447aef6ead60" />


## Status do projeto

**Projeto concluído.**

Este repositório documenta um gateway ambiental IoT completo, desenvolvido com **Arduino Uno**, **ESP32**, **Ethernet W5500**, **Wi-Fi reserva**, **MQTT**, **Node-RED**, **SQLite**, **microSD**, **Watchdog**, **FreeRTOS**, **página web local** e **atualização OTA**.

O sistema foi projetado para simular uma solução real de monitoramento ambiental, com aquisição de dados, comunicação entre microcontroladores, envio para broker MQTT, visualização em dashboard, armazenamento local e remoto, tolerância a falhas, supervisão técnica e atualização remota de firmware.

---

## Visão geral

O projeto utiliza um **Arduino Uno** como unidade de aquisição de dados dos sensores. O Arduino realiza as leituras ambientais e envia um pacote estruturado para o **ESP32 Gateway** via **UART**.

O **ESP32** recebe, valida e processa os dados. Em seguida, publica as leituras no broker **MQTT Mosquitto**, registra histórico no **microSD**, mantém uma fila offline de pacotes pendentes, disponibiliza uma **página web local** de monitoramento, executa tarefas separadas com **FreeRTOS**, utiliza **watchdog** para aumentar a confiabilidade e permite atualização remota do firmware por **OTA**.

No computador, o **Node-RED** recebe os dados MQTT, exibe gráficos e indicadores, gera alertas, salva registros em CSV e armazena o histórico em banco **SQLite**.

---

## Objetivo do projeto

Desenvolver um gateway ambiental capaz de:

* Ler temperatura, umidade do ar, umidade do solo, luminosidade e chuva;
* Enviar dados do Arduino para o ESP32 por UART;
* Validar pacotes recebidos no ESP32;
* Conectar o ESP32 pela Ethernet usando W5500;
* Utilizar Wi-Fi como conexão reserva;
* Publicar leituras por MQTT;
* Exibir dados no MQTT Explorer e no Node-RED;
* Registrar leituras em CSV;
* Armazenar histórico em SQLite;
* Gravar histórico local no microSD;
* Salvar pacotes pendentes quando o MQTT estiver indisponível;
* Reenviar automaticamente os pacotes pendentes após reconexão;
* Disponibilizar página web local de monitoramento;
* Disponibilizar painel técnico com diagnóstico interno;
* Executar o firmware organizado em tarefas FreeRTOS;
* Monitorar o sistema com watchdog;
* Permitir atualização OTA pela página web.

---

## Arquitetura do sistema

Fluxo principal do projeto:

```text
Sensores → Arduino Uno → UART → ESP32 Gateway → Ethernet/Wi-Fi → MQTT → Node-RED → SQLite
                                      │
                                      ├── microSD
                                      │     ├── Histórico local
                                      │     └── Pacotes MQTT pendentes
                                      │
                                      ├── Página web local
                                      ├── Painel técnico
                                      ├── Watchdog
                                      ├── FreeRTOS
                                      └── OTA
```

Arquitetura lógica:

```text
Sensores ambientais
   │
   ▼
Arduino Uno
   │ UART
   ▼
ESP32 Gateway
   │
   ├── Ethernet W5500
   ├── Wi-Fi reserva
   ├── MQTT
   ├── microSD
   ├── Web Server
   ├── OTA
   ├── Watchdog
   └── FreeRTOS
          │
          ▼
Computador
   ├── Mosquitto
   ├── MQTT Explorer
   ├── Node-RED Dashboard
   ├── Arquivo CSV
   └── SQLite
```

---

## Componentes utilizados

* Arduino Uno;
* ESP32 DevKit;
* Módulo Ethernet W5500;
* Módulo leitor de cartão microSD;
* Cartão microSD;
* Sensor DHT11;
* Sensor de umidade do solo;
* LDR;
* Sensor de chuva;
* Resistores;
* Protoboard;
* Jumpers;
* Cabo Ethernet;
* Computador com Mosquitto, MQTT Explorer, Node-RED e SQLite.

---

## Função do Arduino Uno

O Arduino funciona como unidade remota de aquisição de dados.

Responsabilidades:

* Ler os sensores;
* Organizar as leituras;
* Criar o pacote ambiental;
* Enviar o pacote para o ESP32 por UART.

Formato do pacote enviado:

```text
DADOS,TEMPERATURA,UMIDADE_AR,UMIDADE_SOLO,LUMINOSIDADE,CHUVA
```

Exemplo:

```text
DADOS,22.6,48.0,1023,462,374
```

Sensores conectados ao Arduino:

| Sensor | Pino do Arduino |
| ------ | --------------- |
| DHT11 | D2 |
| Umidade do solo | A0 |
| LDR | A1 |
| Sensor de chuva | A2 |

---

## Função do ESP32 Gateway

O ESP32 é o núcleo de comunicação, armazenamento, supervisão e integração do sistema.

Responsabilidades:

* Receber dados do Arduino via UART;
* Validar o pacote recebido;
* Separar e converter os valores;
* Conectar à rede Ethernet pelo W5500;
* Usar Wi-Fi como conexão reserva;
* Publicar dados no MQTT;
* Registrar leituras no microSD;
* Armazenar pacotes pendentes quando o MQTT falhar;
* Reenviar pendentes quando o MQTT voltar;
* Executar servidor web local;
* Disponibilizar rotas de monitoramento e configuração;
* Exibir painel técnico;
* Executar tarefas FreeRTOS;
* Alimentar o watchdog;
* Permitir atualização OTA.

---

## Comunicação UART

A comunicação entre Arduino e ESP32 é feita por UART em **9600 baud**.

| Arduino Uno | ESP32 | Função |
| ----------- | ----- | ------ |
| TX | RX2 | Envio dos dados |
| GND | GND | Referência elétrica comum |

Fluxo:

```text
Arduino TX ─────► ESP32 RX2
Arduino GND ──── ESP32 GND
```

> O Arduino Uno trabalha com nível lógico de 5 V e o ESP32 trabalha com 3,3 V. É recomendado usar divisor resistivo ou conversor de nível lógico entre o TX do Arduino e o RX do ESP32.

---

## Ethernet W5500

O módulo W5500 é a interface principal de rede do gateway.

| W5500 | ESP32 |
| ----- | ----- |
| SCK | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |
| CS | GPIO 5 |
| RST | GPIO 26 |
| GND | GND |
| VCC | Alimentação compatível com o módulo |

Exemplo de operação:

```text
Obtendo endereço IP por DHCP...
IP do ESP32: 192.168.100.102
Gateway: 192.168.100.1
Broker MQTT: 192.168.100.17:1883
```

---

## Wi-Fi reserva

A Ethernet é a conexão principal. O Wi-Fi atua como conexão de reserva para manter o gateway operando caso a rede cabeada esteja indisponível.

Ordem de prioridade:

```text
1. Ethernet
2. Wi-Fi reserva
3. microSD com fila de pendentes
```

---

## MQTT

O MQTT é utilizado para enviar as leituras ambientais do ESP32 para o computador.

Broker utilizado:

```text
Mosquitto
Porta: 1883
```

Tópicos utilizados:

```text
gateway/ambiental/temperatura
gateway/ambiental/umidade_ar
gateway/ambiental/umidade_solo
gateway/ambiental/luminosidade
gateway/ambiental/chuva
gateway/ambiental/dados
```

Exemplo de pacote JSON:

```json
{
  "temperatura": 23.4,
  "umidade_ar": 46.0,
  "umidade_solo": 1023,
  "luminosidade": 740,
  "chuva": 1022
}
```

---

## Node-RED

O Node-RED é a camada de integração e visualização do projeto.

Funções implementadas:

* Recebimento dos tópicos MQTT;
* Exibição dos valores em gauges;
* Exibição dos valores em gráficos;
* Textos de status;
* Alertas ambientais;
* Registro em CSV;
* Registro em banco SQLite;
* Consulta das últimas leituras salvas.

Fluxo geral:

```text
MQTT → Node-RED → Dashboard
MQTT → Node-RED → CSV
MQTT → Node-RED → SQLite
```

---

## Banco de dados SQLite

O SQLite armazena o histórico das leituras ambientais recebidas pelo Node-RED.

Arquivo utilizado:

```text
C:\gateway\gateway.db
```

Tabela:

```sql
CREATE TABLE IF NOT EXISTS leituras (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    data_hora TEXT DEFAULT (datetime('now','localtime')),
    temperatura REAL,
    umidade_ar REAL,
    umidade_solo INTEGER,
    luminosidade INTEGER,
    chuva INTEGER
);
```

Consulta das últimas leituras:

```sql
SELECT *
FROM leituras
ORDER BY id DESC
LIMIT 10;
```

---

## Registro em CSV

Além do SQLite, o Node-RED salva as leituras em arquivo CSV.

Caminho utilizado:

```text
C:\gateway\dados_gateway.csv
```

Exemplo de linha:

```csv
2026-06-20 10:30:00,23.4,46,1023,716,1022
```

---

## microSD no ESP32

O cartão microSD permite armazenamento local no gateway.

Arquivos utilizados:

```text
/dados_gateway.csv
/pendentes_mqtt.txt
/pendentes_tmp.txt
```

Função de cada arquivo:

| Arquivo | Função |
| ------- | ------ |
| `/dados_gateway.csv` | Histórico local das leituras recebidas via UART |
| `/pendentes_mqtt.txt` | Pacotes JSON não publicados no MQTT |
| `/pendentes_tmp.txt` | Arquivo temporário usado durante o reenvio |

Formato do CSV no ESP32:

```csv
timestamp_ms,temperatura,umidade_ar,umidade_solo,luminosidade,chuva
12500,21.40,41.00,1023,474,1022
```

---

## Armazenamento offline e reenvio MQTT

Quando o broker MQTT está indisponível, o ESP32 não perde a leitura. O pacote JSON é salvo no microSD como pendente.

Fluxo:

```text
ESP32 recebe pacote válido
        ↓
Tenta publicar no MQTT
        ↓
Publicou?
   ├── Sim → mantém histórico local
   └── Não → salva em /pendentes_mqtt.txt
```

Quando o MQTT volta:

```text
ESP32 reconecta ao broker
        ↓
Lê /pendentes_mqtt.txt
        ↓
Reenvia pacotes pendentes
        ↓
Remove os pacotes reenviados
        ↓
Mantém apenas os que ainda falharem
```

Resultado de teste:

```text
Reenvio finalizado. Reenviados: 47 | Ainda pendentes: 0
```

---

## Página web local do gateway

<img width="1270" height="792" alt="b863781e-f661-400e-9f91-5b91b267a667" src="https://github.com/user-attachments/assets/09445553-fc29-4292-95ae-993df09121fc" />


O ESP32 disponibiliza uma interface web local para monitoramento do sistema diretamente pelo navegador.

Exemplo de acesso:

```text
http://192.168.100.102
```

Rotas implementadas:

| Rota | Função |
| ---- | ------ |
| `/` | Dashboard principal do gateway |
| `/config` | Página de configurações locais |
| `/status` | Retorno JSON com informações do sistema |
| `/tecnico` | Painel técnico do gateway |
| `/ota` | Página de atualização OTA |

Informações exibidas no painel principal:

* Estado da Ethernet;
* Estado do Wi-Fi reserva;
* Estado do MQTT;
* IP do ESP32;
* Broker MQTT configurado;
* Últimas leituras ambientais;
* Temperatura;
* Umidade do ar;
* Umidade do solo;
* Luminosidade;
* Chuva;
* Uptime;
* Pacotes válidos;
* Erros detectados;
* Estado do microSD;
* Falhas MQTT;
* Conexão em uso.

Características visuais:

* Tema escuro;
* Aparência de dashboard industrial;
* Cards de status;
* Cards de leitura;
* Área de diagnóstico;
* Botões para atualização, configurações, JSON, painel técnico e OTA;
* Interface embarcada no próprio ESP32.

---

## Watchdog

<img width="1271" height="845" alt="image" src="https://github.com/user-attachments/assets/f306a74f-3a36-42f0-99c6-7e2c52aa387a" />


O watchdog foi implementado para aumentar a robustez do gateway. Ele monitora a execução do sistema e reinicia o ESP32 caso o firmware deixe de responder corretamente.

Funções implementadas:

* Configuração do watchdog do ESP32;
* Alimentação periódica por tarefa supervisora;
* Monitoramento do funcionamento geral;
* Exibição do status no painel técnico;
* Contagem de resets por watchdog;
* Indicação do último motivo de reset;
* Proteção durante processos críticos, incluindo OTA.

Informações exibidas no painel técnico:

* Status do watchdog;
* Timeout configurado;
* Tarefa alimentadora;
* Último reset;
* Quantidade de resets por WDT;
* Teste serial habilitado;
* Estado das filas FreeRTOS;
* Estado de comunicação;
* Memória livre;
* Heap mínimo;
* Uptime;
* Número de tarefas ativas.

---

## FreeRTOS

<img width="1208" height="857" alt="image" src="https://github.com/user-attachments/assets/e814a21f-69e4-4bee-9dbf-e0c941d3ca33" />


O projeto foi organizado em tarefas FreeRTOS para separar responsabilidades e aproximar o firmware de uma arquitetura profissional de sistemas embarcados.

Tarefas implementadas:

| Tarefa | Responsabilidade |
| ------ | ---------------- |
| UART | Receber dados do Arduino |
| Validação | Validar e converter pacotes recebidos |
| MQTT | Publicar dados e reenviar pendentes |
| Rede | Verificar Ethernet e Wi-Fi |
| microSD | Gravar histórico e pendentes |
| Web Server | Manter a página web local |
| Monitor | Supervisionar sistema e alimentar watchdog |

Recursos utilizados:

* Filas FreeRTOS para comunicação entre tarefas;
* Mutex para proteger recursos compartilhados;
* Separação entre recepção, validação, publicação e armazenamento;
* Painel técnico para visualização das filas e estados internos.

---

## Atualização OTA

<img width="1237" height="600" alt="image" src="https://github.com/user-attachments/assets/bd6a07d0-0ede-4506-9425-009e275b5196" />


A atualização OTA permite atualizar o firmware do ESP32 pela rede, sem usar cabo USB após a primeira gravação.

Funcionalidades implementadas:

* Página `/ota`;
* Upload de arquivo `.bin`;
* Autenticação básica;
* Exibição da versão atual do firmware;
* Exibição do chip;
* Exibição do espaço livre para OTA;
* Mensagens de sucesso ou erro;
* Reinício automático após atualização bem-sucedida;
* Integração com o painel técnico;
* Cuidados com watchdog durante o processo.

Fluxo de uso:

```text
Arduino IDE
   ↓
Sketch → Export Compiled Binary
   ↓
Arquivo .bin gerado
   ↓
Acesso à página /ota do ESP32
   ↓
Upload do firmware
   ↓
ESP32 grava a nova versão
   ↓
ESP32 reinicia automaticamente
```

> Para utilizar OTA, é necessário selecionar um esquema de partição com suporte a OTA na Arduino IDE.

---

# Etapas do projeto

| Etapa | Descrição | Status |
| ----- | --------- | ------ |
| 1 | Teste individual dos sensores | Concluída |
| 2 | Leitura conjunta dos sensores | Concluída |
| 3 | Criação do pacote de dados | Concluída |
| 4 | Comunicação UART entre Arduino e ESP32 | Concluída |
| 5 | Validação dos dados | Concluída |
| 6 | Teste do módulo Ethernet W5500 | Concluída |
| 7 | Instalação do broker Mosquitto | Concluída |
| 8 | Publicação dos dados por MQTT | Concluída |
| 9 | Visualização no MQTT Explorer | Concluída |
| 10 | Integração com Node-RED | Concluída |
| 11 | Registro em arquivo CSV | Concluída |
| 12 | Banco de dados SQLite | Concluída |
| 13 | Armazenamento no cartão microSD do ESP32 | Concluída |
| 14 | Armazenamento offline e reenvio MQTT | Concluída |
| 15 | Wi-Fi como conexão reserva | Concluída |
| 16 | Página web do gateway | Concluída |
| 17 | Watchdog | Concluída |
| 18 | FreeRTOS | Concluída |
| 19 | Atualização OTA | Concluída |

---

## Situação final do projeto

O sistema final consegue:

* Ler todos os sensores no Arduino;
* Enviar pacote ambiental por UART;
* Receber e validar dados no ESP32;
* Conectar pela Ethernet usando W5500;
* Usar Wi-Fi como reserva;
* Publicar dados no MQTT;
* Exibir dados no MQTT Explorer;
* Integrar com Node-RED;
* Exibir gauges, gráficos e textos no dashboard;
* Gerar alertas;
* Registrar leituras em CSV;
* Registrar histórico em SQLite;
* Gravar histórico local no microSD;
* Detectar falhas de publicação MQTT;
* Salvar pacotes pendentes no microSD;
* Reenviar pendentes após reconexão;
* Exibir painel web local;
* Exibir painel técnico do gateway;
* Monitorar heap, filas, tarefas, comunicação e watchdog;
* Executar tarefas com FreeRTOS;
* Utilizar watchdog para recuperação de falhas;
* Atualizar firmware via OTA;
* Reiniciar automaticamente após atualização bem-sucedida.

---

## Tecnologias utilizadas

* Arduino;
* ESP32;
* C/C++;
* UART;
* SPI;
* Ethernet;
* W5500;
* Wi-Fi;
* MQTT;
* Mosquitto;
* MQTT Explorer;
* Node-RED;
* Node-RED Dashboard;
* SQLite;
* CSV;
* microSD;
* JSON;
* HTML;
* CSS;
* JavaScript;
* FreeRTOS;
* Watchdog;
* OTA.

---

## Aprendizados do projeto

Este projeto permitiu estudar e aplicar conceitos importantes de sistemas embarcados, IoT e redes:

* Leitura de sensores digitais e analógicos;
* Comunicação entre microcontroladores;
* Protocolo UART;
* Protocolo SPI;
* Rede cabeada com Ethernet;
* Wi-Fi como fallback;
* Protocolo MQTT;
* Arquitetura de gateway IoT;
* Node-RED para automação e visualização;
* Armazenamento em CSV;
* Banco de dados SQLite;
* Armazenamento local em microSD;
* Fila offline e reenvio automático;
* Desenvolvimento de página web embarcada;
* Diagnóstico técnico de firmware;
* Organização com FreeRTOS;
* Monitoramento com watchdog;
* Atualização remota de firmware via OTA;
* Estruturação de um projeto embarcado com aparência de produto real.

---

## Possíveis melhorias futuras

Mesmo com o projeto concluído, algumas melhorias podem ser adicionadas em versões futuras:

* Adicionar data e hora por NTP ou RTC;
* Converter sensores analógicos para porcentagem calibrada;
* Calibrar o sensor de umidade do solo;
* Calibrar o sensor de chuva;
* Adicionar usuário e senha no MQTT;
* Adicionar TLS ao MQTT;
* Adicionar CRC ao pacote UART;
* Criar identificação única por gateway;
* Adicionar limite de tamanho para arquivos de pendentes;
* Criar placa de circuito impresso;
* Substituir jumpers por conectores;
* Montar o protótipo em caixa;
* Adicionar fonte de alimentação dedicada e protegida;
* Evoluir a comunicação para RS-485/Modbus em uma versão industrial.

---

## Conclusão

O **Gateway Ambiental ESP32 + Arduino** foi concluído como um sistema IoT funcional, com coleta de dados, comunicação entre microcontroladores, publicação MQTT, armazenamento local e remoto, dashboard, banco de dados, página web embarcada, tolerância a falhas, execução com FreeRTOS, watchdog e atualização OTA.

O resultado final representa um projeto completo de sistemas embarcados e IoT, com foco em confiabilidade, documentação, integração de hardware e software, comunicação em rede e aproximação com uma arquitetura usada em aplicações reais.

---

## Autor

Desenvolvido por **Guilherme Costa** como projeto de estudo nas áreas de:

* Engenharia da Computação;
* Sistemas Embarcados;
* Internet das Coisas;
* Redes de Computadores;
* Eletrônica;
* Comunicação entre dispositivos.
