#define PINO_SOLO A0

void setup() {
  Serial.begin(9600);

  Serial.println("Teste do sensor de umidade do solo");
}

void loop() {
  int valorSolo = analogRead(PINO_SOLO);

  Serial.print("Valor bruto: ");
  Serial.println(valorSolo);

  if (valorSolo < 400) {
    Serial.println("Solo muito umido");
  }
  else if (valorSolo < 700) {
    Serial.println("Solo com umidade intermediaria");
  }
  else {
    Serial.println("Solo seco");
  }

  Serial.println("--------------------");

  delay(1000);
}