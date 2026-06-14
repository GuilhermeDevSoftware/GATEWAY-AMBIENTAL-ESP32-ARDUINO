#include <Arduino.h>

// ==================================================
// CONFIGURAÇÃO UART
// ==================================================

#define PINO_RX_ARDUINO 16
#define PINO_TX_ARDUINO 17

#define VELOCIDADE_MONITOR 115200
#define VELOCIDADE_ARDUINO 9600

HardwareSerial SerialArduino(2);

// ==================================================
// ESTRUTURA DOS DADOS
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
// BUFFER DE RECEPÇÃO
// ==================================================

const int TAMANHO_BUFFER = 120;

char bufferRecepcao[TAMANHO_BUFFER];
int indiceBuffer = 0;

// ==================================================
// MONITORAMENTO DA COMUNICAÇÃO
// ==================================================

const unsigned long TEMPO_LIMITE_COMUNICACAO = 6000;

unsigned long ultimoPacoteValido = 0;

unsigned long pacotesValidos = 0;
unsigned long pacotesInvalidos = 0;

bool recebeuPrimeiroPacote = false;
bool falhaComunicacao = false;

// ==================================================
// PROTÓTIPOS
// ==================================================

void receberUART();
void processarLinha(char *linha);
bool interpretarPacote(char *linha);
bool validarDados(const DadosAmbientais &dadosRecebidos);
void mostrarDados();
void verificarComunicacao();

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(VELOCIDADE_MONITOR);

  delay(1000);

  SerialArduino.begin(
    VELOCIDADE_ARDUINO,
    SERIAL_8N1,
    PINO_RX_ARDUINO,
    PINO_TX_ARDUINO
  );

  Serial.println();
  Serial.println("====================================");
  Serial.println("Gateway ambiental - ESP32 iniciado");
  Serial.println("====================================");
  Serial.println("UART do Arduino:");
  Serial.println("RX2: GPIO 16");
  Serial.println("Velocidade: 9600 baud");
  Serial.println();
  Serial.println("Aguardando dados do Arduino...");
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  receberUART();
  verificarComunicacao();
}

// ==================================================
// RECEPÇÃO UART NÃO BLOQUEANTE
// ==================================================

void receberUART() {
  while (SerialArduino.available() > 0) {
    char caractere = SerialArduino.read();

    // Ignora o caractere de retorno de carro
    if (caractere == '\r') {
      continue;
    }

    // Final da mensagem
    if (caractere == '\n') {
      if (indiceBuffer > 0) {
        bufferRecepcao[indiceBuffer] = '\0';

        processarLinha(bufferRecepcao);

        indiceBuffer = 0;
      }

      continue;
    }

    // Armazena o caractere se ainda houver espaço
    if (indiceBuffer < TAMANHO_BUFFER - 1) {
      bufferRecepcao[indiceBuffer] = caractere;
      indiceBuffer++;
    } else {
      Serial.println("ERRO: buffer UART excedido.");

      indiceBuffer = 0;
      pacotesInvalidos++;
    }
  }
}

// ==================================================
// PROCESSAMENTO DA LINHA RECEBIDA
// ==================================================

void processarLinha(char *linha) {
  Serial.print("UART recebida: ");
  Serial.println(linha);

  // Mensagem de erro enviada pelo Arduino
  if (strcmp(linha, "ERRO,DHT11") == 0) {
    Serial.println("ALERTA: falha na leitura do sensor DHT11.");
    Serial.println();

    pacotesInvalidos++;
    return;
  }

  // Ignora mensagens de inicialização do Arduino
  if (strncmp(linha, "DADOS,", 6) != 0) {
    Serial.println("Mensagem informativa ignorada.");
    Serial.println();
    return;
  }

  if (interpretarPacote(linha)) {
    pacotesValidos++;

    ultimoPacoteValido = millis();
    recebeuPrimeiroPacote = true;

    if (falhaComunicacao) {
      Serial.println("COMUNICAÇÃO RESTABELECIDA.");

      falhaComunicacao = false;
    }

    mostrarDados();
  } else {
    pacotesInvalidos++;

    Serial.println("ERRO: pacote DADOS inválido.");
    Serial.println();
  }
}

// ==================================================
// INTERPRETAÇÃO DO PACOTE
// ==================================================

bool interpretarPacote(char *linha) {
  DadosAmbientais dadosRecebidos;

  int quantidadeConvertida = sscanf(
    linha,
    "DADOS,%f,%f,%d,%d,%d",
    &dadosRecebidos.temperatura,
    &dadosRecebidos.umidadeAr,
    &dadosRecebidos.umidadeSolo,
    &dadosRecebidos.luminosidade,
    &dadosRecebidos.chuva
  );

  // Deve encontrar exatamente cinco valores
  if (quantidadeConvertida != 5) {
    return false;
  }

  dadosRecebidos.valido = validarDados(dadosRecebidos);

  if (!dadosRecebidos.valido) {
    return false;
  }

  // Atualiza os dados globais somente se o pacote for válido
  dados = dadosRecebidos;

  return true;
}

// ==================================================
// VALIDAÇÃO DOS VALORES
// ==================================================

bool validarDados(const DadosAmbientais &dadosRecebidos) {
  if (dadosRecebidos.temperatura < -40.0 ||
      dadosRecebidos.temperatura > 80.0) {
    Serial.println("Temperatura fora do intervalo permitido.");
    return false;
  }

  if (dadosRecebidos.umidadeAr < 0.0 ||
      dadosRecebidos.umidadeAr > 100.0) {
    Serial.println("Umidade do ar fora do intervalo permitido.");
    return false;
  }

  if (dadosRecebidos.umidadeSolo < 0 ||
      dadosRecebidos.umidadeSolo > 1023) {
    Serial.println("Umidade do solo fora do intervalo permitido.");
    return false;
  }

  if (dadosRecebidos.luminosidade < 0 ||
      dadosRecebidos.luminosidade > 1023) {
    Serial.println("Luminosidade fora do intervalo permitido.");
    return false;
  }

  if (dadosRecebidos.chuva < 0 ||
      dadosRecebidos.chuva > 1023) {
    Serial.println("Sensor de chuva fora do intervalo permitido.");
    return false;
  }

  return true;
}

// ==================================================
// EXIBIÇÃO DOS DADOS
// ==================================================

void mostrarDados() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("PACOTE AMBIENTAL VÁLIDO");
  Serial.println("====================================");

  Serial.print("Temperatura: ");
  Serial.print(dados.temperatura, 1);
  Serial.println(" °C");

  Serial.print("Umidade do ar: ");
  Serial.print(dados.umidadeAr, 1);
  Serial.println(" %");

  Serial.print("Umidade do solo: ");
  Serial.println(dados.umidadeSolo);

  Serial.print("Luminosidade: ");
  Serial.println(dados.luminosidade);

  Serial.print("Chuva: ");
  Serial.println(dados.chuva);

  Serial.println("------------------------------------");

  Serial.print("Pacotes válidos: ");
  Serial.println(pacotesValidos);

  Serial.print("Pacotes inválidos: ");
  Serial.println(pacotesInvalidos);

  Serial.println("====================================");
  Serial.println();
}

// ==================================================
// DETECÇÃO DE PERDA DE COMUNICAÇÃO
// ==================================================

void verificarComunicacao() {
  if (!recebeuPrimeiroPacote) {
    return;
  }

  unsigned long tempoSemDados = millis() - ultimoPacoteValido;

  if (tempoSemDados > TEMPO_LIMITE_COMUNICACAO &&
      !falhaComunicacao) {
    falhaComunicacao = true;

    Serial.println();
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("ALERTA: comunicação UART perdida.");
    Serial.println("Nenhum pacote válido por 6 segundos.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println();
  }
}