#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

// ==================================================
// CONFIGURAÇÃO DO W5500
// ==================================================

#define PINO_W5500_CS   5
#define PINO_W5500_RST  26

#define PINO_SPI_SCK    18
#define PINO_SPI_MISO   19
#define PINO_SPI_MOSI   23

// Endereço MAC local do W5500.
// Pode ser qualquer endereço válido que não esteja sendo usado na rede.
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// ==================================================
// CONFIGURAÇÃO DO MQTT
// ==================================================

// Atenção: os números devem ser separados por vírgulas.
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

#define PINO_UART_RX 16
#define PINO_UART_TX 17

HardwareSerial SerialSensores(2);

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

// Buffer que receberá uma linha da UART.
char linhaUART[100];
uint8_t indiceLinha = 0;

// Controle da tentativa de reconexão MQTT.
unsigned long ultimaTentativaMQTT = 0;
const unsigned long intervaloReconexaoMQTT = 5000;

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
// INICIALIZAÇÃO DA ETHERNET
// ==================================================

void iniciarEthernet() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("INICIANDO ETHERNET W5500");
  Serial.println("====================================");

  SPI.begin(
    PINO_SPI_SCK,
    PINO_SPI_MISO,
    PINO_SPI_MOSI,
    PINO_W5500_CS
  );

  Ethernet.init(PINO_W5500_CS);

  reiniciarW5500();

  Serial.println("Obtendo endereco IP por DHCP...");

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Falha ao obter IP por DHCP.");
    Serial.println("Verifique o cabo e o roteador.");
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

  Serial.println();
  Serial.print("Conectando ao Mosquitto em ");
  Serial.print(ipBroker);
  Serial.print(":");
  Serial.println(portaMQTT);

  // Cria um identificador único utilizando o chip do ESP32.
  uint64_t chipID = ESP.getEfuseMac();

  char idCliente[40];

  snprintf(
    idCliente,
    sizeof(idCliente),
    "ESP32-Gateway-%04X",
    (uint16_t)(chipID >> 32)
  );

  /*
    Configura também uma última vontade MQTT.

    Se o ESP32 perder a conexão abruptamente, o broker
    publicará "offline" no tópico de status.
  */
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
// VALIDAÇÃO DOS SENSORES
// ==================================================

bool dadosValidos(const DadosAmbientais& leitura) {
  if (leitura.temperatura < -40.0 ||
      leitura.temperatura > 80.0) {
    return false;
  }

  if (leitura.umidadeAr < 0.0 ||
      leitura.umidadeAr > 100.0) {
    return false;
  }

  if (leitura.umidadeSolo < 0 ||
      leitura.umidadeSolo > 1023) {
    return false;
  }

  if (leitura.luminosidade < 0 ||
      leitura.luminosidade > 1023) {
    return false;
  }

  if (leitura.chuva < 0 ||
      leitura.chuva > 1023) {
    return false;
  }

  return true;
}

// ==================================================
// PUBLICAÇÃO MQTT
// ==================================================

void publicarDadosMQTT(const DadosAmbientais& leitura) {
  if (!clienteMQTT.connected()) {
    Serial.println("MQTT desconectado. Pacote nao publicado.");
    return;
  }

  char valor[20];

  // Temperatura
  snprintf(
    valor,
    sizeof(valor),
    "%.1f",
    leitura.temperatura
  );

  clienteMQTT.publish(
    topicoTemperatura,
    valor,
    true
  );

  // Umidade do ar
  snprintf(
    valor,
    sizeof(valor),
    "%.1f",
    leitura.umidadeAr
  );

  clienteMQTT.publish(
    topicoUmidadeAr,
    valor,
    true
  );

  // Umidade do solo
  snprintf(
    valor,
    sizeof(valor),
    "%d",
    leitura.umidadeSolo
  );

  clienteMQTT.publish(
    topicoUmidadeSolo,
    valor,
    true
  );

  // Luminosidade
  snprintf(
    valor,
    sizeof(valor),
    "%d",
    leitura.luminosidade
  );

  clienteMQTT.publish(
    topicoLuminosidade,
    valor,
    true
  );

  // Chuva
  snprintf(
    valor,
    sizeof(valor),
    "%d",
    leitura.chuva
  );

  clienteMQTT.publish(
    topicoChuva,
    valor,
    true
  );

  // Publicação de todos os sensores em JSON.
  char json[200];

  snprintf(
    json,
    sizeof(json),
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

  bool publicado = clienteMQTT.publish(
    topicoDados,
    json,
    true
  );

  if (publicado) {
    Serial.println("Dados publicados no MQTT:");
    Serial.println(json);
  } else {
    Serial.println("Erro ao publicar pacote MQTT.");
  }
}

// ==================================================
// PROCESSAMENTO DA LINHA RECEBIDA
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

  Serial.println("====================================");
  Serial.println("PACOTE AMBIENTAL VALIDO");
  Serial.println("====================================");

  Serial.print("Temperatura: ");
  Serial.print(dados.temperatura);
  Serial.println(" C");

  Serial.print("Umidade do ar: ");
  Serial.print(dados.umidadeAr);
  Serial.println(" %");

  Serial.print("Umidade do solo: ");
  Serial.println(dados.umidadeSolo);

  Serial.print("Luminosidade: ");
  Serial.println(dados.luminosidade);

  Serial.print("Chuva: ");
  Serial.println(dados.chuva);

  publicarDadosMQTT(dados);
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
      // Evita ultrapassar o tamanho do buffer.
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

  Serial.println();
  Serial.println("====================================");
  Serial.println("GATEWAY AMBIENTAL ESP32");
  Serial.println("UART + W5500 + MQTT");
  Serial.println("====================================");

  SerialSensores.begin(
    9600,
    SERIAL_8N1,
    PINO_UART_RX,
    PINO_UART_TX
  );

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
  // Mantém a concessão DHCP.
  Ethernet.maintain();

  // Tenta reconectar caso o MQTT seja desconectado.
  conectarMQTT();

  // Mantém a comunicação MQTT funcionando.
  clienteMQTT.loop();

  // Recebe e processa os sensores enviados pelo Arduino.
  receberUART();
}
