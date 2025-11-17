#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h> // Lembre-se de instalar esta biblioteca

// --- Configurações de Wi-Fi ---
const char* ssid = "NOME_DA_SUA_REDE";
const char* password = "SENHA_DA_SUA_REDE";

// --- Configurações MQTT ---
const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);
const char* topic = "meu-semaforo-smart-city/estado"; // Nome do "grupo"

// --- Pinos ---
const int s1_verde = 18;
const int s1_amarelo = 19;
const int s1_vermelho = 21;
const int pino_ldr = 34; // Pino de leitura analógica

// --- Variáveis de Controle ---
WebServer server(80);
int valorLDR = 0;
bool modoNoturnoAtivo = false;
int limiteLuz = 400; // ATENÇÃO: Ajuste este valor conforme a luz da sua sala
unsigned long tempoAnterior = 0;
int estadoSemaforo = 0;
String estadoAtualParaPublicar = "";

void setup() {
  Serial.begin(115200);
  setupPinos();
  conectarWiFi();
  
  // Configura o servidor MQTT
  client.setServer(mqtt_server, 1883); // Porta padrão do MQTT
  
  // Inicia o servidor Web (da Parte 2)
  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient(); // Mantém a interface web ativa

  // Garante que está conectado ao MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop(); // Mantém o MQTT ativo

  // Leitura do Sensor
  valorLDR = analogRead(pino_ldr);
  modoNoturnoAtivo = (valorLDR < limiteLuz);

  // Lógica principal
  if (modoNoturnoAtivo) {
    executarModoNoturno();
    estadoAtualParaPublicar = "noite";
  } else {
    executarCicloNormal();
    // O estado (ex: "s1_verde") é definido dentro da função
  }
  
  // Publica o estado no Tópico MQTT
  // (Vamos publicar apenas se o estado mudar, para evitar spam)
  static String ultimoEstadoPublicado = "";
  if (estadoAtualParaPublicar != ultimoEstadoPublicado) {
    client.publish(topic, estadoAtualParaPublicar.c_str());
    ultimoEstadoPublicado = estadoAtualParaPublicar;
  }

  delay(100); // Pequena pausa para estabilidade
}

// --- Funções de Conexão ---
void conectarWiFi() {
  Serial.print("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  Serial.print("IP da Interface Web: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT Broker...");
    // ID único para este cliente
    if (client.connect("ESP32_Mestre_LDR")) {
      Serial.println("Conectado!");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

// --- Funções do Semáforo 1 ---
void executarModoNoturno() {
  // Amarelo piscante
  digitalWrite(s1_verde, LOW); digitalWrite(s1_vermelho, LOW);
  int estado = (millis() / 500) % 2; // Pisca a cada 500ms
  digitalWrite(s1_amarelo, estado);
}

void executarCicloNormal() {
  unsigned long tempoAtual = millis();
  
  switch (estadoSemaforo) {
    case 0: // S1 Verde
      configuraLuzesS1(HIGH, LOW, LOW);
      estadoAtualParaPublicar = "s1_verde";
      if (tempoAtual - tempoAnterior > 5000) { estadoSemaforo = 1; tempoAnterior = tempoAtual; }
      break;
    case 1: // S1 Amarelo
      configuraLuzesS1(LOW, HIGH, LOW);
      estadoAtualParaPublicar = "s1_amarelo";
      if (tempoAtual - tempoAnterior > 2000) { estadoSemaforo = 2; tempoAnterior = tempoAtual; }
      break;
    case 2: // S1 Vermelho (dá vez ao S2)
      configuraLuzesS1(LOW, LOW, HIGH);
      estadoAtualParaPublicar = "s1_vermelho";
      // Tempo de S2 (verde 5s + amarelo 2s) = 7s
      if (tempoAtual - tempoAnterior > 7000) { estadoSemaforo = 0; tempoAnterior = tempoAtual; }
      break;
  }
}

// --- Funções de Setup/Auxiliares ---
void setupPinos() {
  pinMode(s1_verde, OUTPUT); pinMode(s1_amarelo, OUTPUT); pinMode(s1_vermelho, OUTPUT);
  pinMode(pino_ldr, INPUT);
}
void configuraLuzesS1(int v, int a, int r) {
  digitalWrite(s1_verde, v); digitalWrite(s1_amarelo, a); digitalWrite(s1_vermelho, r);
}

// --- Interface Web (HTML) - CORRIGIDA ---
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='2'>"; // Atualiza a cada 2s
  html += "<style>body{font-family: Arial; text-align: center; margin-top: 50px;}";
  html += ".card{box-shadow: 0 4px 8px 0 rgba(0,0,0,0.2); padding: 20px; width: 300px; margin: auto;}";
  html += "</style></head><body>";
  
  html += "<div class='card'><h1>🚦 Smart City Control (Mestre)</h1>";
  html += "<p>Luminosidade (LDR): <strong>" + String(valorLDR) + "</strong></p>";
  
  if(modoNoturnoAtivo) {
    html += "<h2 style='color:orange;'>🌙 MODO NOTURNO</h2>";
  } else {
    html += "<h2 style='color:green;'>☀️ TRÁFEGO NORMAL</h2>";
  }
  
  html += "<p>Estado Publicado: <strong>" + estadoAtualParaPublicar + "</strong></p>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}