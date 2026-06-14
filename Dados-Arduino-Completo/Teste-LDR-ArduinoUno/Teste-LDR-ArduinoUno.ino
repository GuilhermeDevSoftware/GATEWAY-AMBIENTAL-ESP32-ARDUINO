#define PINO_LDR A0

void setup() {
  Serial.begin(9600);

  Serial.println("Teste do sensor de luminosidade LDR");
}

void loop() {
  int luminosidade = analogRead(PINO_LDR);

  Serial.print("Valor do LDR: ");
  Serial.println(luminosidade);

  if (luminosidade < 300) {
    Serial.println("Ambiente escuro");
  }
  else if (luminosidade < 700) {
    Serial.println("Luminosidade intermediaria");
  }
  else {
    Serial.println("Ambiente claro");
  }

  Serial.println("--------------------");

  delay(500);
}