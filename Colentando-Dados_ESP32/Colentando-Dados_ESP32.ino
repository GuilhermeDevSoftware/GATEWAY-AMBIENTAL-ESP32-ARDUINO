#include <SPI.h>
#include <Ethernet.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SD.h>
#include "esp_task_wdt.h"
#include "esp_system.h"

#if __has_include("esp_arduino_version.h")
  #include "esp_arduino_version.h"
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

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
// CONFIGURACAO DO WATCHDOG
// ==================================================

// Tempo maximo sem o loop principal alimentar o watchdog.
// Se o firmware travar por mais tempo que isso, o ESP32 reinicia.
#define TEMPO_WATCHDOG_SEGUNDOS 20

// Envie a letra T pelo Monitor Serial para simular um travamento.
// Depois do teste, pode trocar para false.
#define HABILITAR_TESTE_WATCHDOG true

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
// SERVIDORES WEB DO GATEWAY
// ==================================================

EthernetServer servidorWebEthernet(80);
WiFiServer servidorWebWiFi(80);

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
// DIAGNOSTICO DO SISTEMA
// ==================================================

bool existeLeituraValida = false;
unsigned long totalPacotesRecebidos = 0;
unsigned long totalErros = 0;
unsigned long ultimaLeituraValidaMs = 0;

bool watchdogAtivo = false;
String motivoUltimoReset = "Nao identificado";

// Mantem a contagem mesmo depois de um reset do ESP32.
// Essa contagem zera quando o ESP32 perde energia.
RTC_DATA_ATTR unsigned long totalResetsWatchdog = 0;

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
// PAGINA WEB DO GATEWAY
// ==================================================

String ipParaTexto(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

String formatarUptime() {
  unsigned long segundos = millis() / 1000;

  unsigned long dias = segundos / 86400;
  segundos %= 86400;

  unsigned long horas = segundos / 3600;
  segundos %= 3600;

  unsigned long minutos = segundos / 60;
  segundos %= 60;

  String texto = "";

  if (dias > 0) {
    texto += String(dias) + "d ";
  }

  if (horas < 10) texto += "0";
  texto += String(horas) + ":";

  if (minutos < 10) texto += "0";
  texto += String(minutos) + ":";

  if (segundos < 10) texto += "0";
  texto += String(segundos);

  return texto;
}

String tempoDesdeUltimaLeitura() {
  if (!existeLeituraValida) {
    return "Aguardando primeira leitura";
  }

  unsigned long segundos = (millis() - ultimaLeituraValidaMs) / 1000;

  if (segundos < 60) {
    return String(segundos) + " s atras";
  }

  unsigned long minutos = segundos / 60;

  if (minutos < 60) {
    return String(minutos) + " min atras";
  }

  unsigned long horas = minutos / 60;
  return String(horas) + " h atras";
}

String classeStatus(bool ok) {
  if (ok) {
    return "ok";
  }

  return "erro";
}

String textoStatus(bool ok) {
  if (ok) {
    return "ONLINE";
  }

  return "OFFLINE";
}

String estadoEthernetTexto() {
  if (Ethernet.linkStatus() == LinkON && ethernetComIP && ipValido(Ethernet.localIP())) {
    return "Conectada";
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    return "Cabo desconectado";
  }

  return "Sem IP / desconhecida";
}

String estadoWiFiTexto() {
  if (WiFi.status() == WL_CONNECTED) {
    if (conexaoAtual == CONEXAO_WIFI) {
      return "Conectado como reserva ativa";
    }

    return "Conectado";
  }

  if (conexaoAtual == CONEXAO_ETHERNET) {
    return "Standby";
  }

  return "Desconectado";
}

String estadoMQTTTexto() {
  if (clienteMQTT.connected()) {
    return "Conectado";
  }

  return "Desconectado";
}

bool mqttOk() {
  return clienteMQTT.connected();
}

bool ethernetOkWeb() {
  return Ethernet.linkStatus() == LinkON && ethernetComIP && ipValido(Ethernet.localIP());
}

bool wifiOkWeb() {
  return WiFi.status() == WL_CONNECTED;
}

bool sistemaOnline() {
  return conexaoAtual != CONEXAO_OFFLINE && mqttOk();
}

// ==================================================
// WATCHDOG E DIAGNOSTICO DE RESET
// ==================================================

String obterMotivoReset() {
  esp_reset_reason_t motivo = esp_reset_reason();

  switch (motivo) {
    case ESP_RST_POWERON:
      return "Ligado por energia";

    case ESP_RST_SW:
      return "Reset por software";

    case ESP_RST_PANIC:
      return "Reset por panic/erro fatal";

    case ESP_RST_INT_WDT:
      return "Reset por Interrupt Watchdog";

    case ESP_RST_TASK_WDT:
      return "Reset por Task Watchdog";

    case ESP_RST_WDT:
      return "Reset por Watchdog";

    case ESP_RST_BROWNOUT:
      return "Reset por queda de tensao";

    case ESP_RST_DEEPSLEEP:
      return "Retorno de deep sleep";

    default:
      return "Motivo desconhecido";
  }
}

bool ultimoResetFoiPorWatchdog() {
  esp_reset_reason_t motivo = esp_reset_reason();

  return motivo == ESP_RST_TASK_WDT ||
         motivo == ESP_RST_INT_WDT ||
         motivo == ESP_RST_WDT;
}

void atualizarDiagnosticoReset() {
  motivoUltimoReset = obterMotivoReset();

  if (ultimoResetFoiPorWatchdog()) {
    totalResetsWatchdog++;
  }
}

void iniciarWatchdog() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("INICIANDO WATCHDOG");
  Serial.println("====================================");

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t configWatchdog = {
    .timeout_ms = TEMPO_WATCHDOG_SEGUNDOS * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };

  esp_err_t resultado = esp_task_wdt_init(&configWatchdog);

  if (resultado == ESP_ERR_INVALID_STATE) {
    resultado = esp_task_wdt_reconfigure(&configWatchdog);
  }
#else
  esp_err_t resultado = esp_task_wdt_init(TEMPO_WATCHDOG_SEGUNDOS, true);
#endif

  if (resultado != ESP_OK && resultado != ESP_ERR_INVALID_STATE) {
    watchdogAtivo = false;

    Serial.print("Falha ao configurar Watchdog. Codigo: ");
    Serial.println(resultado);

    return;
  }

  esp_err_t resultadoAdd = esp_task_wdt_add(NULL);

  if (resultadoAdd == ESP_OK || resultadoAdd == ESP_ERR_INVALID_STATE) {
    watchdogAtivo = true;

    Serial.print("Watchdog ativo. Timeout: ");
    Serial.print(TEMPO_WATCHDOG_SEGUNDOS);
    Serial.println(" segundos.");
  } else {
    watchdogAtivo = false;

    Serial.print("Falha ao adicionar loop ao Watchdog. Codigo: ");
    Serial.println(resultadoAdd);
  }
}

void alimentarWatchdog() {
  if (watchdogAtivo) {
    esp_task_wdt_reset();
  }
}

void testarWatchdogSerial() {
#if HABILITAR_TESTE_WATCHDOG
  if (Serial.available()) {
    char comando = Serial.read();

    if (comando == 'T' || comando == 't') {
      Serial.println();
      Serial.println("====================================");
      Serial.println("TESTE DO WATCHDOG");
      Serial.println("====================================");
      Serial.println("Travamento proposital iniciado.");
      Serial.println("O ESP32 deve reiniciar quando o timeout do watchdog for atingido.");

      while (true) {
        // Travamento proposital.
        // Nao chame alimentarWatchdog() aqui.
      }
    }
  }
#endif
}

void enviarCabecalhoHTML(Client& cliente) {
  cliente.println("HTTP/1.1 200 OK");
  cliente.println("Content-Type: text/html; charset=utf-8");
  cliente.println("Cache-Control: no-store");
  cliente.println("Connection: close");
  cliente.println();
}

void enviarCabecalhoJSON(Client& cliente) {
  cliente.println("HTTP/1.1 200 OK");
  cliente.println("Content-Type: application/json; charset=utf-8");
  cliente.println("Cache-Control: no-store");
  cliente.println("Connection: close");
  cliente.println();
}

void enviarCSSDashboard(Client& cliente) {
  cliente.println("<style>");
  cliente.println(":root{--bg:#0f172a;--panel:#111827;--card:#1e293b;--card2:#162033;--border:#334155;--text:#e5e7eb;--muted:#94a3b8;--ok:#22c55e;--warn:#facc15;--err:#ef4444;--blue:#38bdf8;}");
  cliente.println("*{box-sizing:border-box}body{margin:0;font-family:Arial,Helvetica,sans-serif;background:linear-gradient(135deg,#0f172a,#020617);color:var(--text)}");
  cliente.println(".wrap{width:min(1180px,94%);margin:0 auto;padding:24px 0 36px}.top{display:flex;justify-content:space-between;gap:16px;align-items:center;margin-bottom:22px;padding:18px 20px;border:1px solid var(--border);background:rgba(17,24,39,.92);border-radius:18px;box-shadow:0 20px 50px rgba(0,0,0,.25)}");
  cliente.println(".brand h1{margin:0;font-size:1.55rem;letter-spacing:.3px}.brand p{margin:6px 0 0;color:var(--muted);font-size:.95rem}.pill{display:inline-flex;align-items:center;gap:8px;padding:8px 12px;border-radius:999px;font-weight:700;border:1px solid var(--border);background:#0b1220}.dot{width:10px;height:10px;border-radius:50%;display:inline-block}.dot.ok{background:var(--ok);box-shadow:0 0 14px var(--ok)}.dot.warn{background:var(--warn);box-shadow:0 0 14px var(--warn)}.dot.erro{background:var(--err);box-shadow:0 0 14px var(--err)}");
  cliente.println(".grid{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin-bottom:16px}.grid.sensors{grid-template-columns:repeat(5,1fr)}.card{background:linear-gradient(180deg,var(--card),var(--card2));border:1px solid var(--border);border-radius:16px;padding:16px;box-shadow:0 14px 34px rgba(0,0,0,.22)}.card h2{font-size:.82rem;text-transform:uppercase;letter-spacing:.12em;color:var(--muted);margin:0 0 12px}.value{font-size:1.8rem;font-weight:800;margin:0}.unit{font-size:.95rem;color:var(--muted);font-weight:600}.sub{margin-top:8px;color:var(--muted);font-size:.9rem;word-break:break-word}.statusText{font-size:1.25rem;font-weight:800;margin:0}.okText{color:var(--ok)}.warnText{color:var(--warn)}.errText{color:var(--err)}.blueText{color:var(--blue)}");
  cliente.println(".section{margin:22px 0 12px;display:flex;justify-content:space-between;align-items:end;gap:12px}.section h2{margin:0;font-size:1.05rem}.section span{color:var(--muted);font-size:.9rem}.table{width:100%;border-collapse:collapse}.table td{padding:10px 0;border-bottom:1px solid rgba(51,65,85,.75)}.table tr:last-child td{border-bottom:0}.table td:first-child{color:var(--muted)}.table td:last-child{text-align:right;font-weight:700}.actions{display:flex;gap:10px;flex-wrap:wrap}.btn{display:inline-block;padding:11px 14px;border-radius:12px;text-decoration:none;color:var(--text);border:1px solid var(--border);background:#0b1220;font-weight:700}.btn.primary{border-color:#0ea5e9;background:rgba(14,165,233,.14);color:#7dd3fc}.footer{margin-top:18px;color:var(--muted);font-size:.85rem;text-align:center}");
  cliente.println("@media(max-width:900px){.grid,.grid.sensors{grid-template-columns:repeat(2,1fr)}.top{align-items:flex-start;flex-direction:column}.pill{align-self:flex-start}}@media(max-width:560px){.grid,.grid.sensors{grid-template-columns:1fr}.wrap{width:92%;padding-top:16px}.value{font-size:1.55rem}.table td:last-child{text-align:left}.table td{display:block;border-bottom:0;padding:5px 0}.table tr{display:block;border-bottom:1px solid rgba(51,65,85,.75);padding:8px 0}}");
  cliente.println("</style>");
}

void enviarCardStatus(Client& cliente, const char* titulo, const String& estado, const String& detalhe, const String& classe) {
  cliente.println("<div class='card'>");
  cliente.print("<h2>");
  cliente.print(titulo);
  cliente.println("</h2>");

  cliente.print("<p class='statusText ");
  if (classe == "ok") {
    cliente.print("okText");
  } else if (classe == "warn") {
    cliente.print("warnText");
  } else {
    cliente.print("errText");
  }
  cliente.print("'><span class='dot ");
  cliente.print(classe);
  cliente.print("'></span> ");
  cliente.print(estado);
  cliente.println("</p>");

  cliente.print("<div class='sub'>");
  cliente.print(detalhe);
  cliente.println("</div>");
  cliente.println("</div>");
}

void enviarCardSensor(Client& cliente, const char* titulo, const String& valor, const char* unidade) {
  cliente.println("<div class='card'>");
  cliente.print("<h2>");
  cliente.print(titulo);
  cliente.println("</h2>");
  cliente.print("<p class='value'>");
  cliente.print(valor);
  if (strlen(unidade) > 0) {
    cliente.print(" <span class='unit'>");
    cliente.print(unidade);
    cliente.print("</span>");
  }
  cliente.println("</p>");
  cliente.println("</div>");
}

void enviarPaginaPrincipal(Client& cliente) {
  enviarCabecalhoHTML(cliente);

  String classeGeral = sistemaOnline() ? "ok" : (conexaoAtual != CONEXAO_OFFLINE ? "warn" : "erro");
  String textoGeral = sistemaOnline() ? "ONLINE" : (conexaoAtual != CONEXAO_OFFLINE ? "REDE SEM MQTT" : "OFFLINE");

  cliente.println("<!DOCTYPE html><html lang='pt-br'><head><meta charset='UTF-8'>");
  cliente.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  cliente.println("<meta http-equiv='refresh' content='5'>");
  cliente.println("<title>Gateway Ambiental ESP32</title>");
  enviarCSSDashboard(cliente);
  cliente.println("</head><body><main class='wrap'>");

  cliente.println("<header class='top'>");
  cliente.println("<div class='brand'>");
  cliente.println("<h1>Gateway Ambiental ESP32</h1>");
  cliente.println("<p>Painel de diagnostico e monitoramento do sistema IoT</p>");
  cliente.println("</div>");
  cliente.print("<div class='pill'><span class='dot ");
  cliente.print(classeGeral);
  cliente.print("'></span>");
  cliente.print(textoGeral);
  cliente.println("</div>");
  cliente.println("</header>");

  cliente.println("<section class='grid'>");

  String detalheEth = "IP: ";
  detalheEth += ipValido(Ethernet.localIP()) ? ipParaTexto(Ethernet.localIP()) : "indisponivel";
  enviarCardStatus(cliente, "Ethernet", estadoEthernetTexto(), detalheEth, ethernetOkWeb() ? "ok" : "erro");

  String detalheWiFi = "IP: ";
  detalheWiFi += wifiOkWeb() ? ipParaTexto(WiFi.localIP()) : "indisponivel";
  enviarCardStatus(cliente, "Wi-Fi reserva", estadoWiFiTexto(), detalheWiFi, wifiOkWeb() ? "ok" : (conexaoAtual == CONEXAO_ETHERNET ? "warn" : "erro"));

  String detalheMqtt = "Broker: ";
  detalheMqtt += ipParaTexto(ipBroker);
  detalheMqtt += ":";
  detalheMqtt += String(portaMQTT);
  enviarCardStatus(cliente, "MQTT", estadoMQTTTexto(), detalheMqtt, mqttOk() ? "ok" : "erro");

  cliente.println("</section>");

  cliente.println("<div class='section'><div><h2>Ultimas leituras ambientais</h2><span>");
  cliente.print(tempoDesdeUltimaLeitura());
  cliente.println("</span></div></div>");

  cliente.println("<section class='grid sensors'>");
  if (existeLeituraValida) {
    enviarCardSensor(cliente, "Temperatura", String(dados.temperatura, 1), "&deg;C");
    enviarCardSensor(cliente, "Umidade do ar", String(dados.umidadeAr, 1), "%");
    enviarCardSensor(cliente, "Umidade do solo", String(dados.umidadeSolo), "ADC");
    enviarCardSensor(cliente, "Luminosidade", String(dados.luminosidade), "ADC");
    enviarCardSensor(cliente, "Chuva", String(dados.chuva), "ADC");
  } else {
    enviarCardSensor(cliente, "Temperatura", "--", "&deg;C");
    enviarCardSensor(cliente, "Umidade do ar", "--", "%");
    enviarCardSensor(cliente, "Umidade do solo", "--", "ADC");
    enviarCardSensor(cliente, "Luminosidade", "--", "ADC");
    enviarCardSensor(cliente, "Chuva", "--", "ADC");
  }
  cliente.println("</section>");

  cliente.println("<div class='section'><div><h2>Diagnostico tecnico</h2><span>Informacoes internas do gateway</span></div></div>");
  cliente.println("<section class='grid'>");

  cliente.println("<div class='card'><h2>Sistema</h2><table class='table'>");
  cliente.print("<tr><td>Conexao em uso</td><td>");
  cliente.print(nomeConexao(conexaoAtual));
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Uptime</td><td>");
  cliente.print(formatarUptime());
  cliente.println("</td></tr>");
  cliente.print("<tr><td>microSD</td><td class='");
  cliente.print(microSdOk ? "okText" : "errText");
  cliente.print("'>");
  cliente.print(microSdOk ? "Operacional" : "Indisponivel");
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Watchdog</td><td class='");
  cliente.print(watchdogAtivo ? "okText" : "errText");
  cliente.print("'>");
  cliente.print(watchdogAtivo ? "Ativo" : "Inativo");
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Ultimo reset</td><td>");
  cliente.print(motivoUltimoReset);
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Resets por WDT</td><td>");
  cliente.print(totalResetsWatchdog);
  cliente.println("</td></tr>");
  cliente.println("</table></div>");

  cliente.println("<div class='card'><h2>Pacotes</h2><table class='table'>");
  cliente.print("<tr><td>Pacotes validos</td><td>");
  cliente.print(totalPacotesRecebidos);
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Erros detectados</td><td>");
  cliente.print(totalErros);
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Ultima leitura</td><td>");
  cliente.print(tempoDesdeUltimaLeitura());
  cliente.println("</td></tr>");
  cliente.println("</table></div>");

  cliente.println("<div class='card'><h2>Rede</h2><table class='table'>");
  cliente.print("<tr><td>Falhas MQTT</td><td>");
  cliente.print(falhasMQTTConsecutivas);
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Fallback Wi-Fi</td><td>");
  cliente.print(wifiReservaPorFalhaEthernet ? "Ativo" : "Inativo");
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Atualizacao</td><td>5 s</td></tr>");
  cliente.println("</table></div>");

  cliente.println("</section>");

  cliente.println("<div class='section'><div><h2>Operacao</h2><span>Acoes locais do painel</span></div></div>");
  cliente.println("<section class='card'>");
  cliente.println("<div class='actions'>");
  cliente.println("<a class='btn primary' href='/'>Atualizar painel</a>");
  cliente.println("<a class='btn' href='/config'>Configuracoes</a>");
  cliente.println("<a class='btn' href='/status'>Status JSON</a>");
  cliente.println("</div>");
  cliente.println("<div class='sub'>Interface local embarcada no ESP32. Nao depende de internet externa.</div>");
  cliente.println("</section>");

  cliente.println("<p class='footer'>Gateway Ambiental ESP32 | Ethernet W5500 | Wi-Fi reserva | MQTT | microSD | Watchdog</p>");
  cliente.println("</main></body></html>");
}

void enviarPaginaConfig(Client& cliente) {
  enviarCabecalhoHTML(cliente);

  cliente.println("<!DOCTYPE html><html lang='pt-br'><head><meta charset='UTF-8'>");
  cliente.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  cliente.println("<title>Configuracoes - Gateway Ambiental ESP32</title>");
  enviarCSSDashboard(cliente);
  cliente.println("</head><body><main class='wrap'>");

  cliente.println("<header class='top'><div class='brand'><h1>Configuracoes do Gateway</h1><p>Base visual para configuracoes futuras do produto</p></div><div class='pill'><span class='dot warn'></span>EM DESENVOLVIMENTO</div></header>");

  cliente.println("<section class='grid'>");
  cliente.println("<div class='card'><h2>MQTT</h2><table class='table'>");
  cliente.print("<tr><td>Broker atual</td><td>");
  cliente.print(ipParaTexto(ipBroker));
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Porta</td><td>");
  cliente.print(portaMQTT);
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Topico principal</td><td>");
  cliente.print(topicoDados);
  cliente.println("</td></tr></table></div>");

  cliente.println("<div class='card'><h2>Wi-Fi reserva</h2><table class='table'>");
  cliente.print("<tr><td>SSID configurado</td><td>");
  cliente.print(ssidWiFi);
  cliente.println("</td></tr>");
  cliente.print("<tr><td>Status</td><td>");
  cliente.print(estadoWiFiTexto());
  cliente.println("</td></tr>");
  cliente.println("<tr><td>Modo</td><td>Reserva</td></tr></table></div>");

  cliente.println("<div class='card'><h2>Planejado</h2><table class='table'>");
  cliente.println("<tr><td>Editar broker MQTT</td><td>Futuro</td></tr>");
  cliente.println("<tr><td>Alterar topicos</td><td>Futuro</td></tr>");
  cliente.println("<tr><td>Intervalo de envio</td><td>Futuro</td></tr>");
  cliente.println("<tr><td>Nome do dispositivo</td><td>Futuro</td></tr>");
  cliente.println("</table></div>");
  cliente.println("</section>");

  cliente.println("<section class='card'><div class='actions'><a class='btn primary' href='/'>Voltar ao painel</a><a class='btn' href='/status'>Ver JSON</a></div><div class='sub'>Nesta versao, a pagina e apenas informativa. Em uma etapa futura, ela pode salvar configuracoes no microSD.</div></section>");
  cliente.println("</main></body></html>");
}

void enviarStatusJSON(Client& cliente) {
  enviarCabecalhoJSON(cliente);

  cliente.print("{");
  cliente.print("\"sistema\":\"");
  cliente.print(sistemaOnline() ? "online" : "offline");
  cliente.print("\",");

  cliente.print("\"conexao_atual\":\"");
  cliente.print(nomeConexao(conexaoAtual));
  cliente.print("\",");

  cliente.print("\"ethernet\":\"");
  cliente.print(estadoEthernetTexto());
  cliente.print("\",");

  cliente.print("\"wifi\":\"");
  cliente.print(estadoWiFiTexto());
  cliente.print("\",");

  cliente.print("\"mqtt\":\"");
  cliente.print(estadoMQTTTexto());
  cliente.print("\",");

  cliente.print("\"ip_ethernet\":\"");
  cliente.print(ipParaTexto(Ethernet.localIP()));
  cliente.print("\",");

  cliente.print("\"ip_wifi\":\"");
  cliente.print(wifiOkWeb() ? ipParaTexto(WiFi.localIP()) : "indisponivel");
  cliente.print("\",");

  cliente.print("\"uptime\":\"");
  cliente.print(formatarUptime());
  cliente.print("\",");

  cliente.print("\"pacotes_validos\":");
  cliente.print(totalPacotesRecebidos);
  cliente.print(",");

  cliente.print("\"erros\":");
  cliente.print(totalErros);
  cliente.print(",");

  cliente.print("\"microsd\":\"");
  cliente.print(microSdOk ? "operacional" : "indisponivel");
  cliente.print("\",");

  cliente.print("\"watchdog\":\"");
  cliente.print(watchdogAtivo ? "ativo" : "inativo");
  cliente.print("\",");

  cliente.print("\"ultimo_reset\":\"");
  cliente.print(motivoUltimoReset);
  cliente.print("\",");

  cliente.print("\"resets_watchdog\":");
  cliente.print(totalResetsWatchdog);
  cliente.print(",");

  cliente.print("\"temperatura\":");
  if (existeLeituraValida) cliente.print(dados.temperatura, 1); else cliente.print("null");
  cliente.print(",");

  cliente.print("\"umidade_ar\":");
  if (existeLeituraValida) cliente.print(dados.umidadeAr, 1); else cliente.print("null");
  cliente.print(",");

  cliente.print("\"umidade_solo\":");
  if (existeLeituraValida) cliente.print(dados.umidadeSolo); else cliente.print("null");
  cliente.print(",");

  cliente.print("\"luminosidade\":");
  if (existeLeituraValida) cliente.print(dados.luminosidade); else cliente.print("null");
  cliente.print(",");

  cliente.print("\"chuva\":");
  if (existeLeituraValida) cliente.print(dados.chuva); else cliente.print("null");

  cliente.print("}");
}

void atenderClienteWeb(Client& cliente) {
  String requisicao = "";
  unsigned long inicio = millis();

  while (cliente.connected() && millis() - inicio < 800) {
    while (cliente.available()) {
      char c = cliente.read();
      requisicao += c;

      if (requisicao.endsWith("\r\n\r\n")) {
        break;
      }
    }

    if (requisicao.endsWith("\r\n\r\n")) {
      break;
    }
  }

  if (requisicao.indexOf("GET /config") >= 0) {
    enviarPaginaConfig(cliente);
  } else if (requisicao.indexOf("GET /status") >= 0) {
    enviarStatusJSON(cliente);
  } else {
    enviarPaginaPrincipal(cliente);
  }

  delay(1);
  cliente.stop();
}

void iniciarPaginaWeb() {
  servidorWebEthernet.begin();
  servidorWebWiFi.begin();

  Serial.println();
  Serial.println("====================================");
  Serial.println("SERVIDOR WEB INICIADO");
  Serial.println("====================================");

  Serial.print("Pagina web via Ethernet: http://");
  Serial.println(ipParaTexto(Ethernet.localIP()));

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Pagina web via Wi-Fi: http://");
    Serial.println(ipParaTexto(WiFi.localIP()));
  }

  Serial.println("Rotas: /  |  /config  |  /status");
}

void executarPaginaWeb() {
  if (ethernetComIP) {
    prepararSPIParaEthernet();
    EthernetClient clienteWebEthernet = servidorWebEthernet.available();

    if (clienteWebEthernet) {
      atenderClienteWeb(clienteWebEthernet);
    }
  }

  WiFiClient clienteWebWiFi = servidorWebWiFi.available();

  if (clienteWebWiFi) {
    atenderClienteWeb(clienteWebWiFi);
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
    totalErros++;
    return;
  }

  if (!dadosValidos(novaLeitura)) {
    Serial.println("Pacote rejeitado: valores fora do intervalo.");
    totalErros++;
    return;
  }

  dados = novaLeitura;
  existeLeituraValida = true;
  totalPacotesRecebidos++;
  ultimaLeituraValidaMs = millis();

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
      totalErros++;
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
  Serial.println("UART + W5500 + Wi-Fi reserva + MQTT + microSD + Watchdog");
  Serial.println("Prioridade: Ethernet > Wi-Fi > microSD");
  Serial.println("====================================");

  atualizarDiagnosticoReset();

  Serial.print("Motivo do ultimo reset: ");
  Serial.println(motivoUltimoReset);

  Serial.print("Resets por watchdog desde energizacao: ");
  Serial.println(totalResetsWatchdog);

  iniciarWatchdog();

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

  iniciarPaginaWeb();
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  testarWatchdogSerial();

  executarPaginaWeb();

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

  alimentarWatchdog();

  delay(10);
}
