#define PINO_CHUVA A0

void setup() {
  Serial.begin(9600);

  Serial.println("Teste do sensor de chuva");
}

void loop() {
  int valorChuva = analogRead(PINO_CHUVA);

  Serial.print("Valor do sensor: ");
  Serial.println(valorChuva);

  if (valorChuva < 300) {
    Serial.println("Chuva forte ou placa muito molhada");
  }
  else if (valorChuva < 700) {
    Serial.println("Placa parcialmente molhada");
  }
  else {
    Serial.println("Sem chuva ou placa seca");
  }

  Serial.println("--------------------");

  delay(500);
}