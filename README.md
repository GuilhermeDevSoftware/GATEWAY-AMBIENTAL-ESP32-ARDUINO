# Gateway de Monitoramento Ambiental com Arduino e ESP32

Projeto de um sistema embarcado para monitoramento ambiental utilizando um **Arduino Uno** para realizar a leitura dos sensores e um **ESP32** para funcionar como gateway de comunicação.

O Arduino coleta os dados ambientais e envia as informações para o ESP32 por comunicação UART. O ESP32 recebe e valida os dados, conecta-se à rede Ethernet por meio do módulo W5500 e publica as leituras em um broker MQTT. No computador, o Mosquitto recebe as mensagens MQTT e o Node-RED exibe os dados em dashboard, registra leituras em CSV e armazena o histórico em banco de dados SQLite.

---

## Objetivo do projeto

Desenvolver um gateway ambiental capaz de:

* Ler a temperatura do ambiente;
* Ler a umidade do ar;
* Ler a umidade do solo;
* Medir a luminosidade;
* Detectar chuva;
* Enviar os dados do Arduino para o ESP32;
* Validar os pacotes recebidos via UART;
* Conectar o ESP32 à rede Ethernet usando W5500;
* Publicar os dados utilizando MQTT;
* Visualizar os dados em tempo real no Node-RED;
* Gerar gráficos, medidores e alertas no dashboard;
* Registrar as leituras em arquivo CSV;
* Armazenar o histórico em banco SQLite;
* Futuramente armazenar dados no microSD do ESP32;
* Futuramente implementar reenvio de dados, Wi-Fi reserva, página web, watchdog, FreeRTOS e OTA.

---

## Arquitetura do sistema

```text
Sensores
   │
   ▼
Arduino Uno
   │
   │ UART
   ▼
ESP32 Gateway
   │
   ├── Ethernet W5500
   ├── MQTT
   └── Publicação dos dados
          │
          ▼
Computador
   │
   ├── Mosquitto
   ├── MQTT Explorer
   ├── Node-RED Dashboard
   ├── Arquivo CSV
   └── Banco SQLite
```

Fluxo principal do sistema:

```text
Sensores → Arduino Uno → UART → ESP32 → Ethernet W5500 → MQTT → Node-RED → SQLite
```

---

## Componentes utilizados

* 1 Arduino Uno;
* 1 ESP32;
* 1 módulo Ethernet W5500;
* 1 sensor DHT11;
* 1 sensor de umidade do solo;
* 1 LDR;
* 1 sensor de chuva;
* Resistores;
* Protoboard;
* Jumpers;
* Cabo Ethernet;
* Computador executando Mosquitto, MQTT Explorer e Node-RED.

### Componentes planejados para as próximas etapas

* Módulo leitor de cartão microSD;
* Cartão microSD;
* LED ou buzzer para indicação de falhas;
* Fonte de alimentação independente;
* Caixa para montagem do protótipo.

---

## Função do Arduino Uno

O Arduino funciona como um dispositivo remoto de aquisição de dados.

Ele é responsável por:

* Ler os sensores;
* Organizar as leituras;
* Criar um pacote de dados;
* Enviar o pacote para o ESP32 por UART.

Exemplo de pacote enviado:

```text
DADOS,22.6,48.0,1023,462,374
```

Ordem dos dados:

```text
DADOS,TEMPERATURA,UMIDADE_AR,UMIDADE_SOLO,LUMINOSIDADE,CHUVA
```

---

## Função do ESP32

O ESP32 funciona como o gateway do sistema.

Ele é responsável por:

* Receber os dados enviados pelo Arduino;
* Verificar se o pacote recebido é válido;
* Separar os valores recebidos;
* Conectar-se à rede Ethernet pelo W5500;
* Conectar-se ao broker MQTT;
* Publicar os dados dos sensores em tópicos MQTT;
* Publicar também um pacote completo em formato de objeto para o Node-RED;
* Futuramente armazenar dados no cartão microSD;
* Futuramente disponibilizar uma página web;
* Futuramente utilizar Wi-Fi como conexão reserva.

---

## Comunicação UART

A comunicação entre o Arduino e o ESP32 é realizada utilizando UART.

### Ligações

| Arduino Uno | ESP32 | Função |
| ----------- | ----- | ------ |
| TX | RX2 | Envio dos dados |
| GND | GND | Referência elétrica comum |

A velocidade utilizada na comunicação é:

```text
9600 baud
```

> O TX do Arduino trabalha com nível lógico próximo de 5 V, enquanto o ESP32 utiliza 3,3 V. É recomendado utilizar um divisor resistivo ou conversor de nível lógico entre o TX do Arduino e o RX do ESP32.

Fluxo da comunicação:

```text
Arduino TX ─────► ESP32 RX2
Arduino GND ──── ESP32 GND
```

---

## Sensores conectados ao Arduino

| Sensor | Pino do Arduino |
| ------ | --------------- |
| DHT11 | D2 |
| Umidade do solo | A0 |
| LDR | A1 |
| Sensor de chuva | A2 |

---

## Conexão do W5500 com o ESP32

O módulo W5500 utiliza comunicação SPI.

| W5500 | ESP32 |
| ----- | ----- |
| SCK | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |
| CS | GPIO 5 |
| RST | GPIO 26 |
| GND | GND |
| VCC | Alimentação compatível com o módulo |

Após a conexão, o ESP32 solicita automaticamente um endereço IP utilizando DHCP.

Exemplo de funcionamento:

```text
Obtendo endereço IP por DHCP...
IP do ESP32: 192.168.100.103
Gateway: 192.168.100.1
Máscara: 255.255.255.0
IP do broker MQTT: 192.168.100.17
```

---

## Protocolo MQTT

O protocolo MQTT é utilizado para enviar as leituras do gateway para o computador.

O computador executa o broker **Mosquitto** e o ESP32 funciona como cliente MQTT.

### Porta utilizada

```text
1883
```

### Tópicos MQTT utilizados

```text
gateway/ambiental/temperatura
gateway/ambiental/umidade_ar
gateway/ambiental/umidade_solo
gateway/ambiental/luminosidade
gateway/ambiental/chuva
gateway/ambiental/dados
```

Exemplo de publicação individual:

```text
gateway/ambiental/temperatura 22.6
gateway/ambiental/umidade_ar 48.0
gateway/ambiental/umidade_solo 1023
gateway/ambiental/luminosidade 462
gateway/ambiental/chuva 374
```

Exemplo de pacote completo utilizado pelo Node-RED para salvar no SQLite:

```json
{
  "temperatura": 23.4,
  "umidade_ar": 46,
  "umidade_solo": 1023,
  "luminosidade": 740,
  "chuva": 1022
}
```

---

## Node-RED

O Node-RED é utilizado para receber os dados MQTT e criar a parte visual do sistema.

Funções já implementadas:

* Recebimento dos tópicos MQTT;
* Exibição dos valores em medidores;
* Exibição dos valores em gráficos;
* Exibição de textos no dashboard;
* Geração de alertas;
* Registro das leituras em arquivo CSV;
* Registro das leituras em banco de dados SQLite;
* Consulta das últimas leituras salvas no SQLite.

Fluxo geral utilizado no Node-RED:

```text
MQTT → Node-RED → Dashboard
MQTT → Node-RED → CSV
MQTT → Node-RED → SQLite
```

---

## Banco de dados SQLite

O banco de dados SQLite foi implementado no Node-RED para armazenar o histórico das leituras ambientais.

Arquivo utilizado:

```text
C:\gateway\gateway.db
```

Tabela criada:

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

Fluxo de gravação:

```text
mqtt in Dados SQLite
        ↓
Salvar no SQLite
        ↓
sqlite gravar
        ↓
Confirmar gravação
        ↓
debug gravação
```

Tópico utilizado para gravação no banco:

```text
gateway/ambiental/dados
```

Consulta utilizada para visualizar as últimas leituras:

```sql
SELECT *
FROM leituras
ORDER BY id DESC
LIMIT 10;
```

Exemplo de registro salvo:

```text
ID: 1752
Data/Hora: 2026-06-20 11:21:49
Temperatura: 23.4 °C
Umidade do ar: 46 %
Umidade do solo: 1023
Luminosidade: 716
Chuva: 1022
```

> O arquivo `gateway.db` é um banco gerado em tempo de execução. Para o GitHub, o recomendado é versionar o fluxo do Node-RED e documentar o caminho do banco, mas não versionar o banco com os dados coletados.

---

## Arquivo CSV

Além do SQLite, o Node-RED também registra as leituras em arquivo CSV.

Caminho utilizado:

```text
C:\gateway\dados_gateway.csv
```

Exemplo de linha gerada:

```csv
2026-06-20 10:30:00,23.4,46,1023,716,1022
```

O CSV serve como registro simples e fácil de abrir em planilhas, enquanto o SQLite permite consultas históricas mais organizadas.

---

# Etapas do projeto

## Etapa 1 — Teste individual dos sensores

Cada sensor foi testado separadamente no Arduino.

Sensores testados:

* DHT11;
* Sensor de umidade do solo;
* LDR;
* Sensor de chuva.

**Status:** concluído.

---

## Etapa 2 — Leitura conjunta dos sensores

Todos os sensores foram conectados ao Arduino e passaram a ser lidos pelo mesmo programa.

Exemplo de leitura:

```text
Temperatura: 22.2 °C
Umidade do ar: 43.0 %
Umidade do solo: 1023
Luminosidade: 570
Chuva: 6
```

**Status:** concluído.

---

## Etapa 3 — Criação do pacote de dados

As leituras foram organizadas em uma única linha para facilitar a transmissão.

Formato do pacote:

```text
DADOS,TEMPERATURA,UMIDADE_AR,UMIDADE_SOLO,LUMINOSIDADE,CHUVA
```

**Status:** concluído.

---

## Etapa 4 — Comunicação UART entre Arduino e ESP32

O Arduino envia os dados pelo pino TX e o ESP32 recebe os dados utilizando uma porta serial adicional.

**Status:** concluído.

---

## Etapa 5 — Validação dos dados

O ESP32 valida o pacote antes de utilizar as informações.

As principais verificações realizadas são:

* Verificar se o pacote começa com `DADOS`;
* Verificar a quantidade correta de valores;
* Separar os campos por vírgulas;
* Converter os campos para números;
* Descartar mensagens incompletas;
* Descartar leituras inválidas.

**Status:** concluído.

---

## Etapa 6 — Teste do módulo Ethernet W5500

O ESP32 foi conectado ao W5500 utilizando comunicação SPI.

Testes realizados:

* Inicialização do módulo W5500;
* Obtenção de endereço IP por DHCP;
* Verificação do endereço IP;
* Teste de comunicação pela rede cabeada.

**Status:** concluído.

---

## Etapa 7 — Instalação do broker Mosquitto

O Mosquitto foi instalado no computador para funcionar como broker MQTT.

**Status:** concluído.

---

## Etapa 8 — Publicação dos dados por MQTT

O ESP32 conecta-se ao broker Mosquitto e publica as leituras dos sensores.

**Status:** concluído.

---

## Etapa 9 — Visualização no MQTT Explorer

O MQTT Explorer foi utilizado para visualizar os tópicos e os valores publicados pelo ESP32.

Estrutura esperada:

```text
gateway
└── ambiental
    ├── temperatura
    ├── umidade_ar
    ├── umidade_solo
    ├── luminosidade
    ├── chuva
    └── dados
```

**Status:** concluído.

---

## Etapa 10 — Integração com Node-RED

O Node-RED foi integrado ao broker Mosquitto para receber os dados MQTT e criar uma interface visual.

Implementações realizadas:

* MQTT in para os sensores;
* Dashboard com gauges;
* Gráficos em tempo real;
* Textos de estado;
* Alertas;
* Registro em CSV;
* Registro em SQLite.

**Status:** concluído.

---

## Etapa 11 — Registro em arquivo CSV

O Node-RED foi configurado para transformar as leituras em linhas CSV e gravar os dados em arquivo local.

**Status:** concluído.

---

## Etapa 12 — Banco de dados SQLite

O SQLite foi implementado para armazenar o histórico das leituras.

Implementações realizadas:

* Instalação do nó SQLite no Node-RED;
* Criação do banco `gateway.db`;
* Criação da tabela `leituras`;
* Inserção automática das leituras;
* Consulta das últimas leituras;
* Formatação da consulta no debug do Node-RED.

**Status:** concluído.

---

## Etapa 13 — Armazenamento no cartão microSD do ESP32

O ESP32 deverá armazenar as leituras em um arquivo no cartão microSD.

Objetivo da etapa:

* Registrar dados localmente no próprio gateway;
* Permitir funcionamento mesmo sem conexão MQTT;
* Preparar a lógica de armazenamento offline;
* Futuramente reenviar os dados quando a conexão voltar.

Nome de arquivo planejado:

```text
/dados_ambientais.csv
```

Exemplo de conteúdo planejado:

```csv
data_hora,temperatura,umidade_ar,umidade_solo,luminosidade,chuva,status_mqtt
2026-06-20 11:30:00,23.4,46,1023,716,1022,online
```

**Status:** próximo passo.

---

## Etapa 14 — Armazenamento offline e reenvio

Quando o ESP32 perder a conexão com o broker MQTT, os dados serão armazenados no cartão microSD.

Funcionamento planejado:

1. O ESP32 detecta a falha de conexão;
2. As novas leituras são armazenadas no cartão microSD;
3. O gateway continua recebendo dados do Arduino;
4. O ESP32 tenta reconectar ao broker;
5. Depois da reconexão, os dados armazenados são reenviados;
6. Os registros enviados são removidos ou marcados como processados.

**Status:** pendente.

---

## Etapa 15 — Wi-Fi como conexão reserva

A Ethernet será a conexão principal do projeto.

Caso o cabo Ethernet seja desconectado ou a comunicação falhe, o ESP32 tentará utilizar Wi-Fi.

Ordem de prioridade:

1. Ethernet;
2. Wi-Fi;
3. Armazenamento no cartão microSD.

**Status:** pendente.

---

## Etapa 16 — Página web do gateway

O ESP32 poderá disponibilizar uma página web com informações sobre o sistema.

Informações planejadas:

* Estado da conexão Ethernet;
* Estado da conexão Wi-Fi;
* Estado da conexão MQTT;
* Últimas leituras recebidas;
* Tempo de funcionamento;
* Quantidade de pacotes recebidos;
* Quantidade de erros;
* Endereço IP do ESP32;
* Opções de configuração.

**Status:** pendente.

---

## Etapa 17 — Watchdog

O watchdog será utilizado para reiniciar o ESP32 caso alguma parte do sistema deixe de responder.

Situações que poderão ser monitoradas:

* Travamento da comunicação UART;
* Falha do módulo W5500;
* Falha da conexão MQTT;
* Travamento na gravação do cartão microSD;
* Bloqueio de alguma tarefa do sistema.

**Status:** pendente.

---

## Etapa 18 — FreeRTOS

O projeto será dividido em tarefas utilizando FreeRTOS.

Organização planejada:

```text
Tarefa 1: receber dados pela UART
Tarefa 2: validar os dados recebidos
Tarefa 3: publicar os dados no MQTT
Tarefa 4: verificar Ethernet e Wi-Fi
Tarefa 5: gravar os dados no cartão microSD
Tarefa 6: executar o servidor web
Tarefa 7: monitorar o funcionamento do sistema
```

**Status:** pendente.

---

## Etapa 19 — Atualização OTA

A atualização OTA permitirá atualizar o programa do ESP32 pela rede.

Isso evita a necessidade de conectar o cabo USB sempre que uma nova versão do programa for enviada.

**Status:** pendente.

---

# Situação atual do projeto

Atualmente, o sistema consegue:

* Ler todos os sensores no Arduino;
* Criar o pacote ambiental;
* Enviar os dados por UART;
* Receber os dados no ESP32;
* Validar o pacote recebido;
* Separar as leituras;
* Conectar o ESP32 pela Ethernet usando W5500;
* Obter endereço IP por DHCP;
* Conectar-se ao Mosquitto;
* Publicar as leituras utilizando MQTT;
* Exibir os dados no MQTT Explorer;
* Exibir os dados no Node-RED Dashboard;
* Exibir gauges, gráficos e textos no dashboard;
* Gerar alertas no Node-RED;
* Registrar dados em arquivo CSV;
* Registrar dados em banco SQLite;
* Consultar as últimas leituras salvas no SQLite.

---

## Próximo passo

O próximo passo recomendado é iniciar o armazenamento local no **cartão microSD conectado ao ESP32**.

Objetivo do próximo passo:

```text
Salvar as leituras no próprio gateway para que o sistema continue registrando dados mesmo se o MQTT, o computador ou o Node-RED ficarem indisponíveis.
```

Fluxo desejado da próxima etapa:

```text
Arduino → UART → ESP32 → validação → MQTT
                              └── microSD
```

Primeira implementação do próximo passo:

1. Conectar o módulo microSD ao ESP32;
2. Inicializar o cartão microSD no código;
3. Criar ou abrir um arquivo CSV;
4. Gravar as leituras recebidas via UART;
5. Confirmar a gravação pelo monitor serial;
6. Manter a publicação MQTT funcionando junto com a gravação local.

---

## Melhorias futuras

* Adicionar data e hora utilizando NTP ou módulo RTC;
* Converter as leituras dos sensores analógicos para porcentagem;
* Calibrar o sensor de umidade do solo;
* Calibrar o sensor de chuva;
* Configurar usuário e senha no MQTT;
* Adicionar criptografia TLS ao MQTT;
* Publicar os dados no formato JSON;
* Adicionar uma identificação exclusiva para o gateway;
* Criar níveis de alerta;
* Implementar confirmação de entrega;
* Adicionar CRC ao pacote UART;
* Implementar buffer offline no microSD;
* Reenviar dados armazenados quando a conexão voltar;
* Criar uma página web de configuração;
* Implementar watchdog;
* Separar o código em tarefas FreeRTOS;
* Criar uma placa de circuito impresso;
* Substituir os jumpers por conectores;
* Adicionar uma fonte de alimentação protegida;
* Montar o projeto em uma caixa.

---

## Tecnologias utilizadas

* Arduino;
* ESP32;
* C e C++;
* UART;
* SPI;
* Ethernet;
* W5500;
* MQTT;
* Mosquitto;
* MQTT Explorer;
* Node-RED;
* Node-RED Dashboard;
* SQLite;
* CSV;
* Cartão microSD;
* FreeRTOS;
* HTML;
* CSS;
* JavaScript.

---

## Aprendizados do projeto

Este projeto permite estudar conceitos importantes de sistemas embarcados e redes, como:

* Leitura de sensores digitais e analógicos;
* Comunicação entre microcontroladores;
* Protocolo UART;
* Protocolo SPI;
* Comunicação Ethernet;
* Arquitetura cliente-servidor;
* Protocolo MQTT;
* Desenvolvimento de gateways IoT;
* Visualização de dados em dashboard;
* Registro de dados em CSV;
* Utilização de bancos de dados SQLite;
* Armazenamento local;
* Sistemas tolerantes a falhas;
* Execução de tarefas concorrentes com FreeRTOS;
* Utilização de watchdog;
* Atualização de firmware por OTA;
* Desenvolvimento de um sistema embarcado completo.

---

## Autor

Desenvolvido por **Guilherme Costa** como projeto de estudo nas áreas de:

* Engenharia da Computação;
* Sistemas embarcados;
* Internet das Coisas;
* Redes de computadores;
* Eletrônica;
* Comunicação entre dispositivos.
