#include <SPI.h>
#include <Ethernet.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SD.h>

// ==================================================
// CONFIGURACAO DOS PINOS
// ==================================================

// W5500
#define PINO_W5500_CS   5
#define PINO_W5500_RST  26

// microSD
#define PINO_SD_CS      4

// SPI compartilhado entre W5500 e microSD
#define PINO_SPI_SCK    18
#define PINO_SPI_MISO   19
#define PINO_SPI_MOSI   23

// UART ESP32 <-> Arduino
#define PINO_UART_RX    16
#define PINO_UART_TX    17

// ==================================================
// CONFIGURACAO DO microSD
// ==================================================

bool microSdOk = false;

const char* ARQUIVO_DADOS      = "/dados_gateway.csv";
const char* ARQUIVO_PENDENTES  = "/pendentes_mqtt.txt";
const char* ARQUIVO_TEMP       = "/pendentes_tmp.txt";

// ==================================================
// CONFIGURACAO DA ETHERNET
// ==================================================

byte mac[] = {
  0x02, 0x12, 0x34, 0x56, 0x78, 0x90
};

bool ethernetComIP = false;

// ==================================================
// CONFIGURACAO DO WI-FI RESERVA
// ==================================================

// IMPORTANTE:
// O Wi-Fi precisa estar na mesma rede do computador onde roda o Mosquitto,
// ou precisa conseguir acessar o IP do broker MQTT configurado abaixo.

const char* ssidWiFi  = "coloca o nome do wifi";
const char* senhaWiFi = "coloca sua senha";

// ==================================================
// CONFIGURACAO DO MQTT
// ==================================================

IPAddress ipBroker(192, 168, 100, 17);
const uint16_t portaMQTT = 1883;

const char* topicoDados        = "gateway/ambiental/dados";
const char* topicoTemperatura  = "gateway/ambiental/temperatura";
const char* topicoUmidadeAr    = "gateway/ambiental/umidade_ar";
const char* topicoUmidadeSolo  = "gateway/ambiental/umidade_solo";
const char* topicoLuminosidade = "gateway/ambiental/luminosidade";
const char* topicoChuva        = "gateway/ambiental/chuva";
const char* topicoStatus       = "gateway/ambiental/status";

EthernetClient clienteEthernet;
WiFiClient clienteWiFi;
PubSubClient clienteMQTT(clienteEthernet);

// ==================================================
// PRIORIDADE DE CONEXAO
// ==================================================

enum TipoConexao {
  CONEXAO_ETHERNET,
  CONEXAO_WIFI,
  CONEXAO_OFFLINE
};

TipoConexao conexaoAtual = CONEXAO_OFFLINE;

uint8_t falhasMQTTConsecutivas = 0;
const uint8_t limiteFalhasMQTTEthernet = 3;

bool wifiReservaPorFalhaEthernet = false;
unsigned long inicioFallbackWiFi = 0;
const unsigned long tempoRetornoParaEthernet = 30000;

// ==================================================
// CONFIGURACAO DA UART
// ==================================================

HardwareSerial SerialSensores(2);

char linhaUART[100];
uint8_t indiceLinha = 0;

// ==================================================
// CONTROLES DE TEMPO
// ==================================================

unsigned long ultimaTentativaMQTT = 0;
const unsigned long intervaloReconexaoMQTT = 5000;

unsigned long ultimoReenvioPendentes = 0;
const unsigned long intervaloReenvioPendentes = 10000;

unsigned long ultimoCheckConexao = 0;
const unsigned long intervaloCheckConexao = 5000;

unsigned long ultimaTentativaWiFi = 0;
const unsigned long intervaloTentativaWiFi = 15000;

unsigned long ultimaTentativaEthernetDHCP = 0;
const unsigned long intervaloTentativaEthernetDHCP = 10000;

// ==================================================
// ESTRUTURA DOS DADOS
// ==================================================

struct DadosAmbientais {
  float temperatura;
  float umidadeAr;
  int umidadeSolo;
  int luminosidade;
  int chuva;
};

DadosAmbientais dados;

// ==================================================
// CONTROLE DO SPI COMPARTILHADO
// ==================================================

void prepararSPIParaSD() {
  digitalWrite(PINO_W5500_CS, HIGH);
}

void prepararSPIParaEthernet() {
  digitalWrite(PINO_SD_CS, HIGH);
}

void prepararRedeParaMQTT() {
  if (conexaoAtual == CONEXAO_ETHERNET) {
    prepararSPIParaEthernet();
  } else {
    digitalWrite(PINO_SD_CS, HIGH);
    digitalWrite(PINO_W5500_CS, HIGH);
  }
}

// ==================================================
// FUNCOES AUXILIARES
// ==================================================

bool ipValido(IPAddress ip) {
  return !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
}

const char* nomeConexao(TipoConexao conexao) {
  switch (conexao) {
    case CONEXAO_ETHERNET:
      return "Ethernet";
    case CONEXAO_WIFI:
      return "Wi-Fi";
    default:
      return "Offline";
  }
}

// ==================================================
// RESET DO W5500
// ==================================================

void reiniciarW5500() {
  pinMode(PINO_W5500_RST, OUTPUT);

  digitalWrite(PINO_W5500_RST, LOW);
  delay(200);

  digitalWrite(PINO_W5500_RST, HIGH);
  delay(500);
}

// ==================================================
// INICIALIZACAO DO microSD
// ==================================================

void iniciarMicroSD() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("INICIANDO microSD");
  Serial.println("====================================");

  prepararSPIParaSD();

  if (!SD.begin(PINO_SD_CS, SPI)) {
    Serial.println("Falha ao inicializar o microSD.");
    microSdOk = false;
    return;
  }

  microSdOk = true;
  Serial.println("microSD inicializado com sucesso.");

  if (!SD.exists(ARQUIVO_DADOS)) {
    File arquivo = SD.open(ARQUIVO_DADOS, FILE_WRITE);

    if (arquivo) {
      arquivo.println("timestamp_ms,temperatura,umidade_ar,umidade_solo,luminosidade,chuva");
      arquivo.close();

      Serial.println("Arquivo CSV criado com cabecalho.");
    } else {
      Serial.println("Erro ao criar arquivo CSV.");
    }
  }
}

// ==================================================
// SALVAR HISTORICO NO microSD
// ==================================================

void salvarNoMicroSD(const DadosAmbientais& leitura) {
  if (!microSdOk) {
    Serial.println("microSD indisponivel. Leitura nao salva.");
    return;
  }

  prepararSPIParaSD();

  File arquivo = SD.open(ARQUIVO_DADOS, FILE_APPEND);

  if (!arquivo) {
    Serial.println("Erro ao abrir arquivo CSV no microSD.");
    return;
  }

  arquivo.print(millis());
  arquivo.print(",");
  arquivo.print(leitura.temperatura, 2);
  arquivo.print(",");
  arquivo.print(leitura.umidadeAr, 2);
  arquivo.print(",");
  arquivo.print(leitura.umidadeSolo);
  arquivo.print(",");
  arquivo.print(leitura.luminosidade);
  arquivo.print(",");
  arquivo.println(leitura.chuva);

  arquivo.close();

  Serial.println("Leitura salva no microSD.");
}

// ==================================================
// SALVAR PACOTE PENDENTE NO microSD
// ==================================================

void salvarPendenteMQTT(const char* json) {
  if (!microSdOk) {
    Serial.println("microSD indisponivel. Pendente nao salvo.");
    return;
  }

  prepararSPIParaSD();

  File arquivo = SD.open(ARQUIVO_PENDENTES, FILE_APPEND);

  if (!arquivo) {
    Serial.println("Erro ao abrir arquivo de pendentes.");
    return;
  }

  arquivo.println(json);
  arquivo.close();

  Serial.println("Pacote salvo como pendente no microSD.");
}

// ==================================================
// INICIALIZACAO DA ETHERNET
// ==================================================

// Esta funcao usa a mesma logica do teste isolado que funcionou:
// reset fisico do W5500 -> SPI.begin -> Ethernet.init -> Ethernet.begin(mac)
// Evitamos bloquear o projeto com Ethernet.hardwareStatus(), pois no seu modulo
// esse teste retornou falso antes da inicializacao DHCP funcionar corretamente.

bool tentarObterIPEthernet() {
  prepararSPIParaEthernet();

  Ethernet.init(PINO_W5500_CS);

  Serial.println("Procurando endereco IP via DHCP...");

  if (Ethernet.begin(mac) == 0) {
    ethernetComIP = false;
    Serial.println("Falha ao obter IP via DHCP.");
    return false;
  }

  delay(1000);

  ethernetComIP = true;

  Serial.println("Ethernet conectada!");

  Serial.print("IP Ethernet: ");
  Serial.println(Ethernet.localIP());

  Serial.print("Mascara de rede: ");
  Serial.println(Ethernet.subnetMask());

  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());

  Serial.print("Servidor DNS: ");
  Serial.println(Ethernet.dnsServerIP());

  Serial.print("IP do broker MQTT: ");
  Serial.println(ipBroker);

  if (Ethernet.linkStatus() == LinkON) {
    Serial.println("Status do cabo Ethernet: CONECTADO");
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Status do cabo Ethernet: DESCONECTADO");
  } else {
    Serial.println("Status do cabo Ethernet: DESCONHECIDO");
  }

  return true;
}

void iniciarEthernet() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("INICIANDO ETHERNET W5500");
  Serial.println("====================================");

  prepararSPIParaEthernet();

  // Garante que o microSD nao esteja selecionado durante a inicializacao da Ethernet.
  digitalWrite(PINO_SD_CS, HIGH);
  digitalWrite(PINO_W5500_CS, HIGH);

  reiniciarW5500();

  // Reaplica a configuracao SPI exatamente como no teste isolado aprovado.
  SPI.begin(
    PINO_SPI_SCK,
    PINO_SPI_MISO,
    PINO_SPI_MOSI,
    PINO_W5500_CS
  );

  Ethernet.init(PINO_W5500_CS);

  tentarObterIPEthernet();
}

// ==================================================
// VERIFICACAO DE ETHERNET
// ==================================================

bool ethernetDisponivel() {
  prepararSPIParaEthernet();

  // Nao usamos Ethernet.hardwareStatus() aqui porque ele retornou
  // "nao encontrado" no seu teste anterior, mesmo com o W5500 funcionando.
  // A verificacao pratica sera: cabo/link + IP valido por DHCP.

  if (Ethernet.linkStatus() == LinkOFF) {
    if (ethernetComIP) {
      Serial.println("Cabo Ethernet desconectado.");
    }

    ethernetComIP = false;
    return false;
  }

  if (!ethernetComIP || !ipValido(Ethernet.localIP())) {
    unsigned long tempoAtual = millis();

    if (tempoAtual - ultimaTentativaEthernetDHCP >= intervaloTentativaEthernetDHCP) {
      ultimaTentativaEthernetDHCP = tempoAtual;
      return tentarObterIPEthernet();
    }

    return false;
  }

  return true;
}

// ==================================================
// CONEXAO WI-FI RESERVA
// ==================================================

bool conectarWiFiReserva() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  unsigned long tempoAtual = millis();

  if (tempoAtual - ultimaTentativaWiFi < intervaloTentativaWiFi) {
    return false;
  }

  ultimaTentativaWiFi = tempoAtual;

  Serial.println();
  Serial.println("====================================");
  Serial.println("TENTANDO WI-FI RESERVA");
  Serial.println("====================================");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssidWiFi, senhaWiFi);

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 10000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi reserva conectado.");

    Serial.print("IP Wi-Fi: ");
    Serial.println(WiFi.localIP());

    return true;
  }

  Serial.println("Falha ao conectar no Wi-Fi reserva.");
  return false;
}

// ==================================================
// TROCA DO CLIENTE MQTT
// ==================================================

void selecionarClienteMQTT(TipoConexao novaConexao) {
  if (conexaoAtual == novaConexao) {
    return;
  }

  if (clienteMQTT.connected()) {
    clienteMQTT.disconnect();
  }

  if (novaConexao == CONEXAO_ETHERNET) {
    clienteMQTT.setClient(clienteEthernet);
    Serial.println("MQTT configurado para usar Ethernet.");
  } else if (novaConexao == CONEXAO_WIFI) {
    clienteMQTT.setClient(clienteWiFi);
    Serial.println("MQTT configurado para usar Wi-Fi reserva.");
  } else {
    Serial.println("MQTT sem conexao de rede disponivel.");
  }

  clienteMQTT.setServer(ipBroker, portaMQTT);
  conexaoAtual = novaConexao;
  falhasMQTTConsecutivas = 0;
}

// ==================================================
// ATUALIZACAO DA CONEXAO PRINCIPAL / RESERVA
// ==================================================

void atualizarConexao() {
  bool ethernetOk = ethernetDisponivel();

  if (wifiReservaPorFalhaEthernet) {
    if (millis() - inicioFallbackWiFi >= tempoRetornoParaEthernet) {
      wifiReservaPorFalhaEthernet = false;
      Serial.println("Nova tentativa de retorno para Ethernet habilitada.");
    }
  }

  if (ethernetOk && !wifiReservaPorFalhaEthernet) {
    if (conexaoAtual != CONEXAO_ETHERNET) {
      Serial.println();
      Serial.println("Ethernet disponivel. Usando Ethernet como conexao principal.");

      selecionarClienteMQTT(CONEXAO_ETHERNET);

      if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        Serial.println("Wi-Fi reserva desconectado. Ethernet reassumiu a prioridade.");
      }
    }

    return;
  }

  if (!ethernetOk) {
    Serial.println("Ethernet indisponivel. Tentando usar Wi-Fi reserva.");
  } else if (wifiReservaPorFalhaEthernet) {
    Serial.println("Ethernet com falhas MQTT recentes. Mantendo Wi-Fi reserva temporariamente.");
  }

  if (WiFi.status() == WL_CONNECTED || conectarWiFiReserva()) {
    if (conexaoAtual != CONEXAO_WIFI) {
      Serial.println("Usando Wi-Fi como conexao reserva.");
      selecionarClienteMQTT(CONEXAO_WIFI);
    }

    return;
  }

  if (conexaoAtual != CONEXAO_OFFLINE) {
    Serial.println("Sem Ethernet e sem Wi-Fi. Gateway em modo offline.");
  }

  selecionarClienteMQTT(CONEXAO_OFFLINE);
}

// ==================================================
// CONEXAO MQTT
// ==================================================

bool conectarMQTT() {
  if (conexaoAtual == CONEXAO_OFFLINE) {
    return false;
  }

  if (clienteMQTT.connected()) {
    return true;
  }

  unsigned long tempoAtual = millis();

  if (tempoAtual - ultimaTentativaMQTT < intervaloReconexaoMQTT) {
    return false;
  }

  ultimaTentativaMQTT = tempoAtual;

  prepararRedeParaMQTT();

  Serial.println();
  Serial.print("Conectando ao Mosquitto via ");
  Serial.print(nomeConexao(conexaoAtual));
  Serial.print(" em ");
  Serial.print(ipBroker);
  Serial.print(":");
  Serial.println(portaMQTT);

  uint64_t chipID = ESP.getEfuseMac();

  char idCliente[50];

  snprintf(
    idCliente,
    sizeof(idCliente),
    "ESP32-Gateway-%04X",
    (uint16_t)(chipID >> 32)
  );

  bool conectado = clienteMQTT.connect(
    idCliente,
    topicoStatus,
    0,
    true,
    "offline"
  );

  if (conectado) {
    Serial.print("MQTT conectado via ");
    Serial.println(nomeConexao(conexaoAtual));

    clienteMQTT.publish(
      topicoStatus,
      "online",
      true
    );

    falhasMQTTConsecutivas = 0;
    return true;
  }

  Serial.print("Falha na conexao MQTT via ");
  Serial.print(nomeConexao(conexaoAtual));
  Serial.print(". Codigo: ");
  Serial.println(clienteMQTT.state());

  falhasMQTTConsecutivas++;

  if (
    conexaoAtual == CONEXAO_ETHERNET &&
    falhasMQTTConsecutivas >= limiteFalhasMQTTEthernet
  ) {
    Serial.println("Limite de falhas MQTT pela Ethernet atingido.");
    Serial.println("Ativando Wi-Fi reserva temporariamente.");

    wifiReservaPorFalhaEthernet = true;
    inicioFallbackWiFi = millis();

    selecionarClienteMQTT(CONEXAO_OFFLINE);
    atualizarConexao();
  }

  return false;
}

// ==================================================
// VALIDACAO DOS DADOS
// ==================================================

bool dadosValidos(const DadosAmbientais& leitura) {
  if (leitura.temperatura < -40.0 || leitura.temperatura > 80.0) {
    return false;
  }

  if (leitura.umidadeAr < 0.0 || leitura.umidadeAr > 100.0) {
    return false;
  }

  if (leitura.umidadeSolo < 0 || leitura.umidadeSolo > 1023) {
    return false;
  }

  if (leitura.luminosidade < 0 || leitura.luminosidade > 1023) {
    return false;
  }

  if (leitura.chuva < 0 || leitura.chuva > 1023) {
    return false;
  }

  return true;
}

// ==================================================
// MONTAGEM DO JSON
// ==================================================

void montarJson(const DadosAmbientais& leitura, char* json, size_t tamanhoJson) {
  snprintf(
    json,
    tamanhoJson,
    "{\"temperatura\":%.1f,"
    "\"umidade_ar\":%.1f,"
    "\"umidade_solo\":%d,"
    "\"luminosidade\":%d,"
    "\"chuva\":%d}",
    leitura.temperatura,
    leitura.umidadeAr,
    leitura.umidadeSolo,
    leitura.luminosidade,
    leitura.chuva
  );
}

// ==================================================
// PUBLICACAO MQTT
// ==================================================

bool publicarDadosMQTT(const DadosAmbientais& leitura, const char* json) {
  if (!clienteMQTT.connected()) {
    Serial.println("MQTT desconectado. Pacote nao publicado.");
    return false;
  }

  prepararRedeParaMQTT();

  bool sucesso = true;

  char valor[20];

  snprintf(valor, sizeof(valor), "%.1f", leitura.temperatura);
  sucesso = sucesso && clienteMQTT.publish(topicoTemperatura, valor, true);

  snprintf(valor, sizeof(valor), "%.1f", leitura.umidadeAr);
  sucesso = sucesso && clienteMQTT.publish(topicoUmidadeAr, valor, true);

  snprintf(valor, sizeof(valor), "%d", leitura.umidadeSolo);
  sucesso = sucesso && clienteMQTT.publish(topicoUmidadeSolo, valor, true);

  snprintf(valor, sizeof(valor), "%d", leitura.luminosidade);
  sucesso = sucesso && clienteMQTT.publish(topicoLuminosidade, valor, true);

  snprintf(valor, sizeof(valor), "%d", leitura.chuva);
  sucesso = sucesso && clienteMQTT.publish(topicoChuva, valor, true);

  sucesso = sucesso && clienteMQTT.publish(topicoDados, json, true);

  if (sucesso) {
    Serial.print("Dados publicados no MQTT via ");
    Serial.println(nomeConexao(conexaoAtual));
    Serial.println(json);

    falhasMQTTConsecutivas = 0;
  } else {
    Serial.println("Erro ao publicar um ou mais topicos MQTT.");

    falhasMQTTConsecutivas++;

    if (
      conexaoAtual == CONEXAO_ETHERNET &&
      falhasMQTTConsecutivas >= limiteFalhasMQTTEthernet
    ) {
      Serial.println("Falhas de publicacao pela Ethernet. Wi-Fi reserva sera tentado.");
      wifiReservaPorFalhaEthernet = true;
      inicioFallbackWiFi = millis();
    }
  }

  return sucesso;
}

// ==================================================
// REENVIO DOS PACOTES PENDENTES
// ==================================================

void reenviarPendentesMQTT() {
  if (!microSdOk) {
    return;
  }

  if (conexaoAtual == CONEXAO_OFFLINE) {
    return;
  }

  if (!clienteMQTT.connected()) {
    return;
  }

  prepararSPIParaSD();

  if (!SD.exists(ARQUIVO_PENDENTES)) {
    return;
  }

  File pendentes = SD.open(ARQUIVO_PENDENTES, FILE_READ);

  if (!pendentes) {
    Serial.println("Erro ao abrir arquivo de pendentes para leitura.");
    return;
  }

  File temporario = SD.open(ARQUIVO_TEMP, FILE_WRITE);

  if (!temporario) {
    Serial.println("Erro ao criar arquivo temporario de pendentes.");
    pendentes.close();
    return;
  }

  Serial.println();
  Serial.println("====================================");
  Serial.println("REENVIANDO PACOTES PENDENTES");
  Serial.println("====================================");

  int reenviados = 0;
  int mantidos = 0;

  while (true) {
    prepararSPIParaSD();

    if (!pendentes.available()) {
      break;
    }

    String linha = pendentes.readStringUntil('\n');
    linha.trim();

    if (linha.length() == 0) {
      continue;
    }

    prepararRedeParaMQTT();

    bool enviado = clienteMQTT.publish(
      topicoDados,
      linha.c_str(),
      true
    );

    if (enviado) {
      reenviados++;

      Serial.println("Pendente reenviado:");
      Serial.println(linha);

      delay(100);
    } else {
      prepararSPIParaSD();

      temporario.println(linha);
      mantidos++;

      Serial.println("Falha ao reenviar pendente. Mantendo no microSD.");

      falhasMQTTConsecutivas++;
    }
  }

  prepararSPIParaSD();

  pendentes.close();
  temporario.close();

  SD.remove(ARQUIVO_PENDENTES);

  if (mantidos > 0) {
    SD.rename(ARQUIVO_TEMP, ARQUIVO_PENDENTES);
  } else {
    SD.remove(ARQUIVO_TEMP);
  }

  Serial.print("Reenvio finalizado. Reenviados: ");
  Serial.print(reenviados);
  Serial.print(" | Ainda pendentes: ");
  Serial.println(mantidos);
}

// ==================================================
// IMPRESSAO DOS DADOS NO SERIAL
// ==================================================

void imprimirDados(const DadosAmbientais& leitura) {
  Serial.println("====================================");
  Serial.println("PACOTE AMBIENTAL VALIDO");
  Serial.println("====================================");

  Serial.print("Temperatura: ");
  Serial.print(leitura.temperatura);
  Serial.println(" C");

  Serial.print("Umidade do ar: ");
  Serial.print(leitura.umidadeAr);
  Serial.println(" %");

  Serial.print("Umidade do solo: ");
  Serial.println(leitura.umidadeSolo);

  Serial.print("Luminosidade: ");
  Serial.println(leitura.luminosidade);

  Serial.print("Chuva: ");
  Serial.println(leitura.chuva);
}

// ==================================================
// PROCESSAMENTO DA LINHA RECEBIDA VIA UART
// ==================================================

void processarLinhaUART(const char* linha) {
  Serial.println();
  Serial.print("UART recebida: ");
  Serial.println(linha);

  DadosAmbientais novaLeitura;

  int quantidadeLida = sscanf(
    linha,
    "DADOS,%f,%f,%d,%d,%d",
    &novaLeitura.temperatura,
    &novaLeitura.umidadeAr,
    &novaLeitura.umidadeSolo,
    &novaLeitura.luminosidade,
    &novaLeitura.chuva
  );

  if (quantidadeLida != 5) {
    Serial.println("Pacote UART invalido.");
    return;
  }

  if (!dadosValidos(novaLeitura)) {
    Serial.println("Pacote rejeitado: valores fora do intervalo.");
    return;
  }

  dados = novaLeitura;

  imprimirDados(dados);

  char json[200];
  montarJson(dados, json, sizeof(json));

  atualizarConexao();

  bool mqttDisponivel = conectarMQTT();
  bool publicado = false;

  if (mqttDisponivel) {
    publicado = publicarDadosMQTT(dados, json);
  }

  if (!publicado) {
    salvarPendenteMQTT(json);
  }

  salvarNoMicroSD(dados);
}

// ==================================================
// LEITURA NAO BLOQUEANTE DA UART
// ==================================================

void receberUART() {
  while (SerialSensores.available()) {
    char caractere = SerialSensores.read();

    if (caractere == '\r') {
      continue;
    }

    if (caractere == '\n') {
      if (indiceLinha > 0) {
        linhaUART[indiceLinha] = '\0';

        processarLinhaUART(linhaUART);

        indiceLinha = 0;
      }

      continue;
    }

    if (indiceLinha < sizeof(linhaUART) - 1) {
      linhaUART[indiceLinha] = caractere;
      indiceLinha++;
    } else {
      indiceLinha = 0;
      Serial.println("Erro: linha UART muito grande.");
    }
  }
}

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PINO_W5500_CS, OUTPUT);
  pinMode(PINO_SD_CS, OUTPUT);

  digitalWrite(PINO_W5500_CS, HIGH);
  digitalWrite(PINO_SD_CS, HIGH);

  SPI.begin(
    PINO_SPI_SCK,
    PINO_SPI_MISO,
    PINO_SPI_MOSI,
    PINO_W5500_CS
  );

  Serial.println();
  Serial.println("====================================");
  Serial.println("GATEWAY AMBIENTAL ESP32");
  Serial.println("UART + W5500 + Wi-Fi reserva + MQTT + microSD");
  Serial.println("Prioridade: Ethernet > Wi-Fi > microSD");
  Serial.println("====================================");

  SerialSensores.begin(
    9600,
    SERIAL_8N1,
    PINO_UART_RX,
    PINO_UART_TX
  );

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  iniciarEthernet();

  iniciarMicroSD();

  clienteMQTT.setServer(
    ipBroker,
    portaMQTT
  );

  atualizarConexao();
  conectarMQTT();
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  if (conexaoAtual == CONEXAO_ETHERNET) {
    prepararSPIParaEthernet();
    Ethernet.maintain();
  }

  if (millis() - ultimoCheckConexao >= intervaloCheckConexao) {
    ultimoCheckConexao = millis();
    atualizarConexao();
  }

  conectarMQTT();

  if (clienteMQTT.connected()) {
    clienteMQTT.loop();
  }

  receberUART();

  if (millis() - ultimoReenvioPendentes >= intervaloReenvioPendentes) {
    ultimoReenvioPendentes = millis();
    reenviarPendentesMQTT();
  }
}
