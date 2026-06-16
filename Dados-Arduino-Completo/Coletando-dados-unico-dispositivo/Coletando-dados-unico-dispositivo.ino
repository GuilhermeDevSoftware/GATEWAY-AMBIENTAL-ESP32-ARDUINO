#include <DHT.h>

// ==================================================
// CONFIGURAÇÃO DO DHT11
// ==================================================

#define PINO_DHT 2
#define TIPO_DHT DHT11

DHT dht(PINO_DHT, TIPO_DHT);

// ==================================================
// PINOS ANALÓGICOS
// ==================================================

#define PINO_SOLO A0
#define PINO_LDR A1
#define PINO_CHUVA A3

// ==================================================
// TEMPORIZAÇÃO
// ==================================================

const unsigned long INTERVALO_LEITURA = 2000;

unsigned long tempoAnterior = 0;

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(9600);

  dht.begin();

  pinMode(PINO_SOLO, INPUT);
  pinMode(PINO_LDR, INPUT);
  pinMode(PINO_CHUVA, INPUT);

  Serial.println("Gateway ambiental - Arduino iniciado");
  Serial.println("Aguardando leituras...");
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  unsigned long tempoAtual = millis();

  if (tempoAtual - tempoAnterior >= INTERVALO_LEITURA) {
    tempoAnterior = tempoAtual;

    realizarLeitura();
  }
}

// ==================================================
// LEITURA DOS SENSORES
// ==================================================

void realizarLeitura() {
  float temperatura = dht.readTemperature();
  float umidadeAr = dht.readHumidity();

  int umidadeSolo = analogRead(PINO_SOLO);
  int luminosidade = analogRead(PINO_LDR);
  int chuva = analogRead(PINO_CHUVA);

  bool dhtValido = true;

  if (isnan(temperatura) || isnan(umidadeAr)) {
    dhtValido = false;
  }

  if (!dhtValido) {
    Serial.println("ERRO,DHT11");
    return;
  }

  enviarDados(
    temperatura,
    umidadeAr,
    umidadeSolo,
    luminosidade,
    chuva);
}

// ==================================================
// ENVIO PELA UART
// ==================================================

void enviarDados(
  float temperatura,
  float umidadeAr,
  int umidadeSolo,
  int luminosidade,
  int chuva) {
  Serial.print("DADOS,");

  Serial.print(temperatura, 1);
  Serial.print(",");

  Serial.print(umidadeAr, 1);
  Serial.print(",");

  Serial.print(umidadeSolo);
  Serial.print(",");

  Serial.print(luminosidade);
  Serial.print(",");

  Serial.println(chuva);
}