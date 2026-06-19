# Gateway de Monitoramento Ambiental com Arduino e ESP32

Projeto de um sistema embarcado para monitoramento ambiental utilizando um **Arduino Uno** para realizar a leitura dos sensores e um **ESP32** para funcionar como gateway de comunicação.

O Arduino coleta os dados ambientais e envia as informações para o ESP32 por comunicação UART. O ESP32 recebe e valida os dados, conecta-se à rede Ethernet por meio do módulo W5500 e publica as leituras em um broker MQTT.

---

## Objetivo do projeto

Desenvolver um gateway ambiental capaz de:

* Ler a temperatura do ambiente;
* Ler a umidade do ar;
* Ler a umidade do solo;
* Medir a luminosidade;
* Detectar chuva;
* Enviar os dados do Arduino para o ESP32;
* Conectar o ESP32 à rede Ethernet;
* Publicar os dados utilizando MQTT;
* Armazenar as leituras em um cartão microSD;
* Criar uma página web para monitoramento;
* Criar gráficos e dashboards utilizando Node-RED.

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
ESP32
   │
   ├── Ethernet W5500
   ├── MQTT
   ├── microSD
   ├── Página Web
   └── Wi-Fi de reserva
          │
          ▼
Computador
   │
   ├── Mosquitto
   ├── MQTT Explorer
   ├── Node-RED
   └── Banco de dados
```

Fluxo principal do sistema:

```text
Sensores → Arduino Uno → UART → ESP32 → Ethernet W5500 → MQTT → Computador
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
* Computador executando o broker Mosquitto.

### Componentes que serão adicionados

* Módulo leitor de cartão microSD;
* Cartão microSD;
* LED ou buzzer para indicação de falhas;
* Fonte de alimentação independente.

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
* Conectar-se à rede Ethernet;
* Conectar-se ao broker MQTT;
* Publicar os dados dos sensores;
* Armazenar dados no cartão microSD futuramente;
* Disponibilizar uma página web futuramente;
* Utilizar Wi-Fi como conexão reserva futuramente.

---

## Comunicação UART

A comunicação entre o Arduino e o ESP32 é realizada utilizando UART.

### Ligações

| Arduino Uno | ESP32 | Função                    |
| ----------- | ----- | ------------------------- |
| TX          | RX2   | Envio dos dados           |
| GND         | GND   | Referência elétrica comum |

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

| Sensor          | Pino do Arduino |
| --------------- | --------------- |
| DHT11           | D2              |
| Umidade do solo | A0              |
| LDR             | A1              |
| Sensor de chuva | A2              |

---

## Conexão do W5500 com o ESP32

O módulo W5500 utiliza comunicação SPI.

| W5500 | ESP32                               |
| ----- | ----------------------------------- |
| SCK   | GPIO 18                             |
| MISO  | GPIO 19                             |
| MOSI  | GPIO 23                             |
| CS    | GPIO 5                              |
| RST   | GPIO 26                             |
| GND   | GND                                 |
| VCC   | Alimentação compatível com o módulo |

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

### Tópicos MQTT

```text
gateway/ambiental/temperatura
gateway/ambiental/umidade_ar
gateway/ambiental/umidade_solo
gateway/ambiental/luminosidade
gateway/ambiental/chuva
```

Exemplo de publicação:

```text
gateway/ambiental/temperatura 22.6
gateway/ambiental/umidade_ar 48.0
gateway/ambiental/umidade_solo 1023
gateway/ambiental/luminosidade 462
gateway/ambiental/chuva 374
```

---

# Etapas do projeto

## Etapa 1 — Teste individual dos sensores

Cada sensor foi testado separadamente no Arduino.

Sensores testados:

* DHT11;
* Sensor de umidade do solo;
* LDR;
* Sensor de chuva.

Objetivos desta etapa:

* Verificar as conexões;
* Testar a alimentação dos sensores;
* Conferir os valores analógicos;
* Identificar possíveis sensores defeituosos;
* Observar a variação dos valores.

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

Exemplo:

```text
DADOS,21.8,49.0,1023,585,500
```

**Status:** concluído.

---

## Etapa 4 — Comunicação UART entre Arduino e ESP32

O Arduino envia os dados pelo pino TX.

O ESP32 recebe os dados utilizando uma porta serial adicional.

O ESP32 verifica se a mensagem começa com:

```text
DADOS,
```

Depois da validação inicial, cada valor é separado e convertido para o tipo correto.

Exemplo de resultado no monitor serial:

```text
====================================
PACOTE AMBIENTAL VÁLIDO
====================================
Temperatura: 22.6 °C
Umidade do ar: 48.0 %
Umidade do solo: 1023
Luminosidade: 462
Chuva: 374
```

**Status:** concluído.

---

## Etapa 5 — Validação dos dados

O ESP32 valida o pacote antes de utilizar as informações.

As principais verificações realizadas são:

* Verificar se o pacote começa com `DADOS`;
* Verificar a quantidade correta de valores;
* Verificar os campos separados por vírgulas;
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
* Verificação do gateway da rede;
* Teste de comunicação pela rede cabeada.

**Status:** concluído.

---

## Etapa 7 — Instalação do broker Mosquitto

O Mosquitto foi instalado no computador para funcionar como broker MQTT.

O computador e o ESP32 devem estar conectados à mesma rede.

Exemplo de configuração:

```text
Broker MQTT: 192.168.100.17
Porta: 1883
```

**Status:** concluído.

---

## Etapa 8 — Publicação dos dados por MQTT

O ESP32 conecta-se ao broker Mosquitto e publica as leituras dos sensores.

Exemplo de funcionamento:

```text
Conectando ao Mosquitto em 192.168.100.17:1883
MQTT conectado!
Dados publicados com sucesso.
```

O log do Mosquitto também mostra as mensagens recebidas do ESP32:

```text
Received PUBLISH from ESP32-Gateway
```

**Status:** concluído.

---

## Etapa 9 — Visualização no MQTT Explorer

O MQTT Explorer pode ser utilizado para visualizar os tópicos e os valores publicados pelo ESP32.

Estrutura esperada:

```text
gateway
└── ambiental
    ├── temperatura
    ├── umidade_ar
    ├── umidade_solo
    ├── luminosidade
    └── chuva
```

**Status:** próxima etapa.

---

## Etapa 10 — Integração com Node-RED

O Node-RED será utilizado para receber os dados MQTT e criar uma interface visual.

Funções planejadas:

* Conectar o Node-RED ao broker Mosquitto;
* Assinar os tópicos MQTT dos sensores;
* Mostrar os valores em tempo real;
* Criar medidores;
* Criar gráficos;
* Gerar alertas;
* Registrar o histórico das leituras.

Fluxo planejado:

```text
MQTT
 │
 ▼
Node-RED
 │
 ├── Medidores
 ├── Gráficos
 ├── Alertas
 └── Banco de dados
```

**Status:** Iniciando banco de dados.

---

## Etapa 11 — Armazenamento no cartão microSD

O ESP32 deverá armazenar as leituras em um arquivo no cartão microSD.

Nome de arquivo planejado:

```text
dados_ambientais.csv
```

Exemplo de conteúdo:

```csv
tempo,temperatura,umidade_ar,umidade_solo,luminosidade,chuva
10,22.6,48.0,1023,462,374
20,22.7,47.8,1018,470,365
```

O cartão microSD também será utilizado quando o broker MQTT estiver indisponível.

**Status:** pendente.

---

## Etapa 12 — Armazenamento offline e reenvio

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

## Etapa 13 — Wi-Fi como conexão reserva

A Ethernet será a conexão principal do projeto.

Caso o cabo Ethernet seja desconectado ou a comunicação falhe, o ESP32 tentará utilizar Wi-Fi.

Ordem de prioridade:

1. Ethernet;
2. Wi-Fi;
3. Armazenamento no cartão microSD.

**Status:** pendente.

---

## Etapa 14 — Página web do gateway

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

## Etapa 15 — Watchdog

O watchdog será utilizado para reiniciar o ESP32 caso alguma parte do sistema deixe de responder.

Situações que poderão ser monitoradas:

* Travamento da comunicação UART;
* Falha do módulo W5500;
* Falha da conexão MQTT;
* Travamento na gravação do cartão microSD;
* Bloqueio de alguma tarefa do sistema.

**Status:** pendente.

---

## Etapa 16 — FreeRTOS

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

Filas poderão ser utilizadas para transferir dados entre as tarefas com segurança.

**Status:** pendente.

---

## Etapa 17 — Atualização OTA

A atualização OTA permitirá atualizar o programa do ESP32 pela rede.

Isso evita a necessidade de conectar o cabo USB sempre que uma nova versão do programa for enviada.

Essa função será útil quando o gateway estiver instalado em um local de difícil acesso.

**Status:** pendente.

---

## Etapa 18 — Banco de dados

Os dados recebidos pelo Node-RED poderão ser armazenados em um banco de dados.

Possibilidades:

* SQLite;
* MySQL;
* PostgreSQL;
* InfluxDB.

Os dados armazenados poderão ser utilizados para:

* Consultar o histórico;
* Comparar períodos;
* Calcular médias;
* Detectar eventos de chuva;
* Verificar mudanças de temperatura;
* Analisar a umidade do solo;
* Criar gráficos de longo período.

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
* Conectar o ESP32 pela Ethernet;
* Obter um endereço IP por DHCP;
* Conectar-se ao Mosquitto;
* Publicar as leituras utilizando MQTT.

---

## Próximo passo

O próximo passo recomendado é:

```text
Instalar e configurar o Node-RED para criar o primeiro dashboard MQTT.
```

---

## Melhorias futuras

* Adicionar data e hora utilizando NTP ou um módulo RTC;
* Converter as leituras dos sensores analógicos para porcentagens;
* Calibrar o sensor de umidade do solo;
* Calibrar o sensor de chuva;
* Configurar usuário e senha no MQTT;
* Adicionar criptografia TLS ao MQTT;
* Publicar os dados no formato JSON;
* Adicionar uma identificação exclusiva para o gateway;
* Criar níveis de alerta;
* Implementar confirmação de entrega;
* Adicionar CRC ao pacote UART;
* Criar uma placa de circuito impresso;
* Substituir os jumpers por conectores;
* Adicionar uma fonte de alimentação protegida;
* Montar o projeto em uma caixa.

---

## Exemplo futuro de mensagem JSON

```json
{
  "dispositivo": "gateway-ambiental-01",
  "temperatura": 22.6,
  "umidade_ar": 48.0,
  "umidade_solo": 1023,
  "luminosidade": 462,
  "chuva": 374,
  "conexao": "ethernet"
}
```

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
* Armazenamento local;
* Sistemas tolerantes a falhas;
* Execução de tarefas concorrentes com FreeRTOS;
* Utilização de watchdog;
* Atualização de firmware por OTA;
* Criação de dashboards;
* Utilização de bancos de dados;
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
