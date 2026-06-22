# Gateway de Monitoramento Ambiental com Arduino e ESP32

Projeto de um sistema embarcado para monitoramento ambiental utilizando um **Arduino Uno** para realizar a leitura dos sensores e um **ESP32** para funcionar como gateway de comunicação.

O Arduino coleta os dados ambientais e envia as informações para o ESP32 por comunicação UART. O ESP32 recebe e valida os dados, conecta-se à rede Ethernet por meio do módulo W5500, utiliza Wi-Fi como conexão reserva, publica as leituras em um broker MQTT e também grava as leituras localmente em um cartão microSD. Caso o broker MQTT esteja indisponível, o ESP32 salva os pacotes como pendentes no microSD e realiza o reenvio automático quando a conexão MQTT volta a funcionar. O gateway também disponibiliza uma página web local com dashboard de diagnóstico, estados das conexões, últimas leituras, uptime, contadores e rota de configuração inicial. No computador, o Mosquitto recebe as mensagens MQTT e o Node-RED exibe os dados em dashboard, registra leituras em CSV e armazena o histórico em banco de dados SQLite.

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
* Armazenar as leituras localmente no microSD do ESP32;
* Salvar pacotes pendentes no microSD quando o MQTT falhar;
* Reenviar automaticamente os pacotes pendentes quando o MQTT voltar;
* Utilizar Wi-Fi como conexão reserva;
* Disponibilizar uma página web local de monitoramento e diagnóstico;
* Futuramente implementar watchdog, FreeRTOS e OTA.

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
   ├── Wi-Fi reserva
   ├── MQTT
   ├── microSD
   │     ├── Histórico local
   │     └── Pacotes pendentes
   ├── Página web local
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
Sensores → Arduino Uno → UART → ESP32 → Ethernet W5500/Wi-Fi → MQTT → Node-RED → SQLite
                              ├── microSD → histórico local e pendentes MQTT
                              └── Página web → dashboard local do gateway
```

---

## Componentes utilizados

* 1 Arduino Uno;
* 1 ESP32;
* 1 módulo Ethernet W5500;
* 1 módulo leitor de cartão microSD;
* 1 cartão microSD;
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

* LED ou buzzer para indicação de falhas críticas;
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
* Armazenar o histórico das leituras no cartão microSD;
* Salvar pacotes pendentes no microSD quando o MQTT estiver indisponível;
* Reenviar automaticamente os pacotes pendentes quando o MQTT voltar;
* Utilizar Wi-Fi como conexão reserva quando a Ethernet não estiver disponível;
* Disponibilizar uma página web local com dashboard de monitoramento;
* Disponibilizar rota `/status` em JSON para diagnóstico e integração futura;
* Disponibilizar rota `/config` como base para configurações futuras.

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

## microSD no ESP32

O microSD foi implementado diretamente no ESP32 para permitir armazenamento local no próprio gateway.

O cartão é utilizado para duas funções principais:

* Registrar o histórico local das leituras;
* Guardar pacotes MQTT pendentes quando o broker estiver desligado ou indisponível.

Arquivos utilizados no microSD:

```text
/dados_gateway.csv
/pendentes_mqtt.txt
/pendentes_tmp.txt
```

Função de cada arquivo:

| Arquivo | Função |
| ------- | ------ |
| `/dados_gateway.csv` | Histórico local das leituras recebidas via UART |
| `/pendentes_mqtt.txt` | Pacotes JSON que não conseguiram ser publicados no MQTT |
| `/pendentes_tmp.txt` | Arquivo temporário usado durante o reenvio dos pendentes |

Formato do CSV salvo no ESP32:

```csv
timestamp_ms,temperatura,umidade_ar,umidade_solo,luminosidade,chuva
12500,21.40,41.00,1023,474,1022
```

Exemplo de pacote pendente salvo no microSD:

```json
{"temperatura":21.4,"umidade_ar":41.0,"umidade_solo":1023,"luminosidade":474,"chuva":1022}
```

Funcionamento da fila de pendentes:

```text
ESP32 tenta publicar no MQTT
        │
        ├── Publicou com sucesso
        │       └── Mantém apenas o histórico no CSV
        │
        └── Falhou
                └── Salva o JSON em /pendentes_mqtt.txt
```

Quando o MQTT volta a funcionar:

```text
ESP32 reconecta ao Mosquitto
        ↓
Abre /pendentes_mqtt.txt
        ↓
Reenvia os pacotes salvos
        ↓
Remove os pacotes reenviados
        ↓
Mantém apenas os que ainda falharem
```

Teste realizado:

```text
MQTT desconectado. Pacote nao publicado.
Pacote salvo como pendente no microSD.
```

Depois que o Mosquitto voltou:

```text
Pendente reenviado:
{"temperatura":21.4,"umidade_ar":41.0,"umidade_solo":1023,"luminosidade":474,"chuva":1022}

Reenvio finalizado. Reenviados: 47 | Ainda pendentes: 0
```

Resultado da etapa:

```text
Backup offline funcionando
Fila de pendentes funcionando
Reenvio automático funcionando
Nenhum pacote pendente restante após a reconexão
```

---

## Página web local do gateway

O ESP32 disponibiliza uma página web local para monitoramento e diagnóstico do gateway.

A página pode ser acessada pelo navegador utilizando o endereço IP do ESP32 na rede.

Exemplo:

```text
http://192.168.100.103
```

Rotas implementadas:

```text
/
/config
/status
```

Função de cada rota:

| Rota | Função |
| ---- | ------ |
| `/` | Dashboard principal do gateway |
| `/config` | Página base para configurações futuras |
| `/status` | Retorno em JSON com informações do sistema |

Informações exibidas no dashboard:

* Estado da conexão Ethernet;
* Estado do Wi-Fi reserva;
* Estado da conexão MQTT;
* Endereço IP da Ethernet;
* Endereço IP do Wi-Fi;
* Broker MQTT configurado;
* Últimas leituras ambientais;
* Temperatura;
* Umidade do ar;
* Umidade do solo;
* Luminosidade;
* Chuva;
* Tempo de funcionamento do ESP32;
* Total de pacotes válidos recebidos;
* Total de erros detectados;
* Falhas MQTT consecutivas;
* Estado do microSD;
* Conexão em uso.

O visual foi organizado em formato de dashboard industrial, com tema escuro, cards de status, cards de leitura e área de diagnóstico técnico.

Objetivo da página web:

```text
Permitir que o gateway seja monitorado diretamente pelo navegador, sem depender somente do Node-RED ou do MQTT Explorer.
```

Essa etapa aproxima o projeto de um produto real, pois o equipamento passa a ter uma interface própria de supervisão e diagnóstico.

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

O ESP32 armazena as leituras recebidas via UART em um arquivo CSV no cartão microSD.

Objetivo da etapa:

* Registrar dados localmente no próprio gateway;
* Manter um histórico independente do computador;
* Permitir análise posterior das leituras;
* Preparar a lógica de armazenamento offline.

Arquivo utilizado:

```text
/dados_gateway.csv
```

Exemplo de conteúdo:

```csv
timestamp_ms,temperatura,umidade_ar,umidade_solo,luminosidade,chuva
12500,21.40,41.00,1023,474,1022
```

Resultado obtido no monitor serial:

```text
microSD inicializado com sucesso.
Leitura salva no microSD.
```

**Status:** concluído.

---

## Etapa 14 — Armazenamento offline e reenvio

Quando o ESP32 perde a conexão com o broker MQTT, os pacotes são armazenados no cartão microSD como pendentes.

Funcionamento implementado:

1. O ESP32 recebe os dados do Arduino por UART;
2. O pacote é validado;
3. O ESP32 tenta publicar no MQTT;
4. Se a publicação falhar, o JSON é salvo em `/pendentes_mqtt.txt`;
5. O gateway continua funcionando e recebendo novas leituras;
6. O ESP32 tenta reconectar ao broker;
7. Depois da reconexão, os pacotes pendentes são reenviados;
8. Os pacotes reenviados são removidos da fila de pendentes.

Arquivos utilizados:

```text
/pendentes_mqtt.txt
/pendentes_tmp.txt
```

Resultado obtido durante o teste com o Mosquitto desligado:

```text
MQTT desconectado. Pacote nao publicado.
Pacote salvo como pendente no microSD.
```

Resultado obtido após ligar o Mosquitto novamente:

```text
Reenvio finalizado. Reenviados: 47 | Ainda pendentes: 0
```

**Status:** concluído.

---

## Etapa 15 — Wi-Fi como conexão reserva

A Ethernet é a conexão principal do projeto.

O Wi-Fi foi implementado como conexão reserva para manter o gateway funcionando quando a Ethernet não estiver disponível.

Ordem de prioridade:

1. Ethernet;
2. Wi-Fi reserva;
3. Armazenamento no cartão microSD.

Funcionamento esperado:

* Utilizar Ethernet como caminho principal;
* Utilizar Wi-Fi quando necessário;
* Manter o MQTT ativo usando a conexão disponível;
* Continuar salvando pacotes no microSD caso nenhuma conexão consiga publicar no broker.

**Status:** concluído.

---

## Etapa 16 — Página web do gateway

O ESP32 disponibiliza uma página web local com informações sobre o sistema.

A primeira versão foi implementada de forma funcional e, em seguida, evoluída para um dashboard com aparência profissional.

Informações exibidas:

* Estado da conexão Ethernet;
* Estado da conexão Wi-Fi reserva;
* Estado da conexão MQTT;
* Últimas leituras recebidas;
* Tempo de funcionamento;
* Quantidade de pacotes válidos recebidos;
* Quantidade de erros;
* Endereço IP da Ethernet;
* Endereço IP do Wi-Fi;
* Estado do microSD;
* Falhas MQTT consecutivas;
* Conexão atualmente em uso;
* Opções de configuração futuras.

Rotas implementadas:

```text
/
/config
/status
```

Características da interface:

* Tema escuro industrial;
* Cards de status das conexões;
* Cards das leituras ambientais;
* Área de diagnóstico técnico;
* Atualização automática da página;
* Página responsiva para computador e celular;
* Rota JSON para integração futura.

**Status:** concluído.

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
* Registrar dados em arquivo CSV pelo Node-RED;
* Registrar dados em banco SQLite;
* Consultar as últimas leituras salvas no SQLite;
* Inicializar o microSD no ESP32;
* Gravar histórico local em `/dados_gateway.csv`;
* Detectar falha de publicação MQTT;
* Salvar pacotes MQTT pendentes no microSD;
* Reenviar automaticamente os pacotes pendentes após reconexão;
* Limpar a fila de pendentes após o reenvio com sucesso;
* Utilizar Wi-Fi como conexão reserva;
* Disponibilizar página web local do gateway;
* Exibir dashboard web com tema profissional;
* Exibir estados de Ethernet, Wi-Fi, MQTT e microSD;
* Exibir uptime, pacotes, erros e falhas MQTT;
* Disponibilizar rota `/config` como base para configurações futuras;
* Disponibilizar rota `/status` em JSON.

---

## Próximo passo

O próximo passo recomendado é implementar o **watchdog**.

O watchdog será usado para aumentar a confiabilidade do gateway. Ele deverá reiniciar o ESP32 automaticamente caso o sistema trave ou alguma parte crítica deixe de responder.

Objetivo do próximo passo:

```text
Tornar o gateway mais robusto, capaz de se recuperar sozinho de travamentos ou falhas críticas.
```

Situações que poderão ser monitoradas:

* Travamento do loop principal;
* Falha prolongada na comunicação UART;
* Falha prolongada do MQTT;
* Falha na comunicação com o W5500;
* Bloqueio durante acesso ao microSD;
* Tempo excessivo sem receber pacotes válidos;
* Erros consecutivos acima de um limite definido.

Fluxo desejado da próxima etapa:

```text
ESP32 funcionando normalmente
        ↓
Alimenta o watchdog periodicamente
        ↓
Se o código travar ou parar de alimentar o watchdog
        ↓
ESP32 reinicia automaticamente
        ↓
Gateway volta a operar
```

Primeira implementação do próximo passo:

1. Incluir a biblioteca de watchdog do ESP32;
2. Configurar um tempo limite seguro;
3. Registrar o loop principal no watchdog;
4. Alimentar o watchdog somente quando o sistema estiver rodando corretamente;
5. Testar o reinício automático com uma simulação de travamento;
6. Exibir informações de watchdog no monitor serial;
7. Futuramente exibir o estado do watchdog na página web.

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
* Melhorar o buffer offline no microSD com limite de tamanho;
* Adicionar contagem de pacotes pendentes;
* Adicionar identificação única para cada leitura;
* Reenviar dados armazenados quando a conexão voltar;
* Tornar a página `/config` funcional;
* Implementar watchdog;
* Separar o código em tarefas FreeRTOS;
* Criar uma placa de circuito impresso;
* Substituir os jumpers por conectores;
* Adicionar uma fonte de alimentação protegida;
* Montar o projeto em uma caixa.

---

## Tecnologias utilizadas e planejadas

* Arduino;
* ESP32;
* C e C++;
* UART;
* SPI;
* Ethernet;
* Wi-Fi;
* W5500;
* MQTT;
* Mosquitto;
* MQTT Explorer;
* Node-RED;
* Node-RED Dashboard;
* SQLite;
* CSV;
* Cartão microSD;
* Página web local;
* JSON;
* Watchdog;
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
