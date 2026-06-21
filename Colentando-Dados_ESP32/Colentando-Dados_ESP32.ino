#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <SD.h>

// ==================================================
// CONFIGURAÇÃO DOS PINOS
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
// CONFIGURAÇÃO DO microSD
// ==================================================

bool microSdOk = false;

const char* ARQUIVO_DADOS      = "/dados_gateway.csv";
const char* ARQUIVO_PENDENTES  = "/pendentes_mqtt.txt";
const char* ARQUIVO_TEMP       = "/pendentes_tmp.txt";

// ==================================================
// CONFIGURAÇÃO DA ETHERNET
// ==================================================

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// ==================================================
// CONFIGURAÇÃO DO MQTT
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
PubSubClient clienteMQTT(clienteEthernet);

// ==================================================
// CONFIGURAÇÃO DA UART
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
// INICIALIZAÇÃO DO microSD
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
// SALVAR HISTÓRICO NO microSD
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
// INICIALIZAÇÃO DA ETHERNET
// ==================================================

void iniciarEthernet() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("INICIANDO ETHERNET W5500");
  Serial.println("====================================");

  prepararSPIParaEthernet();

  Ethernet.init(PINO_W5500_CS);

  reiniciarW5500();

  Serial.println("Obtendo endereco IP por DHCP...");

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Falha ao obter IP por DHCP.");
    Serial.println("Verifique o cabo de rede e o roteador.");
    return;
  }

  delay(1000);

  Serial.print("IP do ESP32: ");
  Serial.println(Ethernet.localIP());

  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());

  Serial.print("Mascara: ");
  Serial.println(Ethernet.subnetMask());

  Serial.print("IP do broker MQTT: ");
  Serial.println(ipBroker);
}

// ==================================================
// CONEXÃO MQTT
// ==================================================

void conectarMQTT() {
  if (clienteMQTT.connected()) {
    return;
  }

  unsigned long tempoAtual = millis();

  if (tempoAtual - ultimaTentativaMQTT < intervaloReconexaoMQTT) {
    return;
  }

  ultimaTentativaMQTT = tempoAtual;

  prepararSPIParaEthernet();

  Serial.println();
  Serial.print("Conectando ao Mosquitto em ");
  Serial.print(ipBroker);
  Serial.print(":");
  Serial.println(portaMQTT);

  uint64_t chipID = ESP.getEfuseMac();

  char idCliente[40];

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
    Serial.println("MQTT conectado!");

    clienteMQTT.publish(
      topicoStatus,
      "online",
      true
    );
  } else {
    Serial.print("Falha na conexao MQTT. Codigo: ");
    Serial.println(clienteMQTT.state());
  }
}

// ==================================================
// VALIDAÇÃO DOS DADOS
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
// PUBLICAÇÃO MQTT
// ==================================================

bool publicarDadosMQTT(const DadosAmbientais& leitura, const char* json) {
  if (!clienteMQTT.connected()) {
    Serial.println("MQTT desconectado. Pacote nao publicado.");
    return false;
  }

  prepararSPIParaEthernet();

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
    Serial.println("Dados publicados no MQTT:");
    Serial.println(json);
  } else {
    Serial.println("Erro ao publicar um ou mais topicos MQTT.");
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

  while (pendentes.available()) {
    String linha = pendentes.readStringUntil('\n');
    linha.trim();

    if (linha.length() == 0) {
      continue;
    }

    prepararSPIParaEthernet();

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
    }
  }

  pendentes.close();
  temporario.close();

  prepararSPIParaSD();

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
// IMPRESSÃO DOS DADOS NO SERIAL
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

  bool publicado = publicarDadosMQTT(dados, json);

  if (!publicado) {
    salvarPendenteMQTT(json);
  }

  salvarNoMicroSD(dados);
}

// ==================================================
// LEITURA NÃO BLOQUEANTE DA UART
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
    PINO_SPI_MOSI
  );

  Serial.println();
  Serial.println("====================================");
  Serial.println("GATEWAY AMBIENTAL ESP32");
  Serial.println("UART + W5500 + MQTT + microSD");
  Serial.println("====================================");

  SerialSensores.begin(
    9600,
    SERIAL_8N1,
    PINO_UART_RX,
    PINO_UART_TX
  );

  iniciarMicroSD();

  iniciarEthernet();

  clienteMQTT.setServer(
    ipBroker,
    portaMQTT
  );

  conectarMQTT();
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  Ethernet.maintain();

  conectarMQTT();

  clienteMQTT.loop();

  receberUART();

  if (millis() - ultimoReenvioPendentes >= intervaloReenvioPendentes) {
    ultimoReenvioPendentes = millis();
    reenviarPendentesMQTT();
  }
}