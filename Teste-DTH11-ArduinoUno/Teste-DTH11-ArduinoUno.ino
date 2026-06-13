#include <DHT.h>

#define PINO_DHT 2
#define TIPO_DHT DHT11

DHT dht(PINO_DHT, TIPO_DHT);

void setup() {
  Serial.begin(9600);
  dht.begin();

  Serial.println("Teste do sensor DHT11");
}

void loop() {
  delay(2000);

  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  if (isnan(umidade) || isnan(temperatura)) {
    Serial.println("Erro ao ler o DHT11.");
    return;
  }

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  Serial.print("Umidade do ar: ");
  Serial.print(umidade);
  Serial.println(" %");

  Serial.println("--------------------");
}