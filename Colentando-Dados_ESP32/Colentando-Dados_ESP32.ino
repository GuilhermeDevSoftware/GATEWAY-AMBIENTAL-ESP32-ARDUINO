#include <SPI.h>
#include <Ethernet.h>

// ==================================================
// UART ARDUINO -> ESP32
// ==================================================

#define PINO_RX_ARDUINO 16
#define PINO_TX_ARDUINO 17

HardwareSerial SerialArduino(2);

// ==================================================
// W5500
// ==================================================

#define PINO_SCK_W5500   18
#define PINO_MISO_W5500  19
#define PINO_MOSI_W5500  23
#define PINO_CS_W5500     5
#define PINO_RST_W5500   26

byte mac[] = {
  0x02, 0x47, 0x41, 0x54, 0x45, 0x01
};

// ==================================================
// ESTRUTURA DOS DADOS AMBIENTAIS
// ==================================================

struct DadosAmbientais {
  float temperatura;
  float umidadeAr;
  int umidadeSolo;
  int luminosidade;
  int chuva;
  bool valido;
};

DadosAmbientais dados;

// ==================================================
// VARIÁVEIS DE CONTROLE
// ==================================================

String linhaRecebida = "";

unsigned long momentoUltimoPacote = 0;
unsigned long momentoUltimaExibicao = 0;
unsigned long momentoUltimaVerificacaoEthernet = 0;

const unsigned long INTERVALO_EXIBICAO = 3000;
const unsigned long INTERVALO_ETHERNET = 5000;
const unsigned long TEMPO_LIMITE_UART = 10000;

// ==================================================
// RESET DO W5500
// ==================================================

void resetarW5500() {
  pinMode(PINO_RST_W5500, OUTPUT);

  digitalWrite(PINO_RST_W5500, LOW);
  delay(200);

  digitalWrite(PINO_RST_W5500, HIGH);
  delay(500);
}

// ==================================================
// INICIALIZAÇÃO DA ETHERNET
// ==================================================

void iniciarEthernet() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("INICIALIZANDO W5500");
  Serial.println("====================================");

  pinMode(PINO_CS_W5500, OUTPUT);
  digitalWrite(PINO_CS_W5500, HIGH);

  resetarW5500();

  SPI.begin(
    PINO_SCK_W5500,
    PINO_MISO_W5500,
    PINO_MOSI_W5500,
    PINO_CS_W5500
  );

  Ethernet.init(PINO_CS_W5500);

  Serial.println("Solicitando endereco IP...");

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Falha ao obter IP pelo DHCP.");

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("ERRO: W5500 nao detectado.");
    } else {
      Serial.println("W5500 detectado.");
      Serial.println("Verifique o cabo e o roteador.");
    }

    return;
  }

  delay(1000);

  Serial.println("Ethernet inicializada com sucesso.");

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());

  Serial.print("Mascara: ");
  Serial.println(Ethernet.subnetMask());

  Serial.print("DNS: ");
  Serial.println(Ethernet.dnsServerIP());
}

// ==================================================
// VALIDAÇÃO DOS DADOS
// ==================================================

bool validarDados(const DadosAmbientais &pacote) {
  if (pacote.temperatura < -20.0 ||
      pacote.temperatura > 80.0) {
    return false;
  }

  if (pacote.umidadeAr < 0.0 ||
      pacote.umidadeAr > 100.0) {
    return false;
  }

  if (pacote.umidadeSolo < 0 ||
      pacote.umidadeSolo > 1023) {
    return false;
  }

  if (pacote.luminosidade < 0 ||
      pacote.luminosidade > 1023) {
    return false;
  }

  if (pacote.chuva < 0 ||
      pacote.chuva > 1023) {
    return false;
  }

  return true;
}

// ==================================================
// PROCESSAMENTO DO PACOTE UART
//
// Formato esperado:
//
// DADOS,22.6,48.0,1023,462,374
// ==================================================

void processarLinha(String linha) {
  linha.trim();

  if (!linha.startsWith("DADOS,")) {
    Serial.print("Pacote desconhecido: ");
    Serial.println(linha);
    return;
  }

  DadosAmbientais novoPacote;

  int quantidadeLida = sscanf(
    linha.c_str(),
    "DADOS,%f,%f,%d,%d,%d",
    &novoPacote.temperatura,
    &novoPacote.umidadeAr,
    &novoPacote.umidadeSolo,
    &novoPacote.luminosidade,
    &novoPacote.chuva
  );

  if (quantidadeLida != 5) {
    Serial.print("Erro ao interpretar pacote: ");
    Serial.println(linha);
    return;
  }

  novoPacote.valido = validarDados(novoPacote);

  if (!novoPacote.valido) {
    Serial.print("Pacote fora dos limites: ");
    Serial.println(linha);
    return;
  }

  dados = novoPacote;
  momentoUltimoPacote = millis();
}

// ==================================================
// RECEPÇÃO UART
// ==================================================

void receberDadosArduino() {
  while (SerialArduino.available()) {
    char caractere = SerialArduino.read();

    if (caractere == '\n') {
      if (linhaRecebida.length() > 0) {
        processarLinha(linhaRecebida);
        linhaRecebida = "";
      }
    } else if (caractere != '\r') {
      linhaRecebida += caractere;

      if (linhaRecebida.length() > 150) {
        linhaRecebida = "";
        Serial.println("Pacote UART descartado: muito grande.");
      }
    }
  }
}

// ==================================================
// VERIFICAÇÃO DA ETHERNET
// ==================================================

void verificarEthernet() {
  if (millis() - momentoUltimaVerificacaoEthernet <
      INTERVALO_ETHERNET) {
    return;
  }

  momentoUltimaVerificacaoEthernet = millis();

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("ERRO: W5500 nao detectado.");
    return;
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("ALERTA: cabo Ethernet desconectado.");
    return;
  }
}

// ==================================================
// EXIBIÇÃO DOS DADOS
// ==================================================

void mostrarDados() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("GATEWAY AMBIENTAL");
  Serial.println("====================================");

  if (!dados.valido) {
    Serial.println("Aguardando dados validos do Arduino...");
  } else {
    unsigned long tempoSemPacote =
      millis() - momentoUltimoPacote;

    if (tempoSemPacote > TEMPO_LIMITE_UART) {
      Serial.println("UART: sem resposta do Arduino");
    } else {
      Serial.println("UART: Arduino conectado");
    }

    Serial.println("------------------------------------");

    Serial.print("Temperatura: ");
    Serial.print(dados.temperatura, 1);
    Serial.println(" C");

    Serial.print("Umidade do ar: ");
    Serial.print(dados.umidadeAr, 1);
    Serial.println(" %");

    Serial.print("Umidade do solo: ");
    Serial.println(dados.umidadeSolo);

    Serial.print("Luminosidade: ");
    Serial.println(dados.luminosidade);

    Serial.print("Chuva: ");
    Serial.println(dados.chuva);
  }

  Serial.println("------------------------------------");

  Serial.print("W5500: ");

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("nao detectado");
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("cabo desconectado");
  } else {
    Serial.println("conectado");

    Serial.print("IP: ");
    Serial.println(Ethernet.localIP());
  }

  Serial.println("====================================");
}

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);

  SerialArduino.begin(
    9600,
    SERIAL_8N1,
    PINO_RX_ARDUINO,
    PINO_TX_ARDUINO
  );

  dados.valido = false;

  delay(1000);

  Serial.println();
  Serial.println("Iniciando Gateway Ambiental...");

  iniciarEthernet();

  Serial.println();
  Serial.println("Aguardando pacotes do Arduino...");
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  receberDadosArduino();

  Ethernet.maintain();

  verificarEthernet();

  if (millis() - momentoUltimaExibicao >=
      INTERVALO_EXIBICAO) {

    momentoUltimaExibicao = millis();

    mostrarDados();
  }
}