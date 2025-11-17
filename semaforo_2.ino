#include <WiFi.h>
#include <PubSubClient.h> // <-- Biblioteca Nova

// --- Configurações de Wi-Fi ---
const char* ssid = "Inteli.Iot"; // TEM QUE SER A MESMA REDE
const char* password = "%(Yk(sxGMtvFEs.3";

// --- Configurações MQTT ---
const char* mqtt_server = "10.128.0.156"; // <-- COLOQUE O IP DO SEU NOTEBOOK AQUIWiFiClient espClient;

WiFiClient espClient;
PubSubClient client(espClient);
const char* topic = "meu-semaforo-smart-city/estado"; // MESMO "grupo"

// --- Pinos ---
const int s2_verde = 25;
const int s2_amarelo = 26;
const int s2_vermelho = 27;

// --- Variáveis de Tempo (para o Amarelo do S2) ---
unsigned long tempoAmarelo = 0;
bool s2_no_amarelo = false;

void setup() {
  Serial.begin(9600);
  pinMode(s2_verde, OUTPUT); 
  pinMode(s2_amarelo, OUTPUT); 
  pinMode(s2_vermelho, OUTPUT);
  
  conectarWiFi();
  
  // Configura o servidor MQTT
  client.setServer(mqtt_server, 1883);
  // Define a função que será chamada quando uma msg chegar
  client.setCallback(callbackMQTT); 
}

void loop() {
  // Garante que está conectado ao MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop(); // Mantém o MQTT ativo
  
  // Lógica de tempo para o S2 Amarelo (para não usar delay)
  if (s2_no_amarelo && (millis() - tempoAmarelo > 2000)) {
    // Se o S1 está vermelho, o S2 pode ficar verde
    configuraLuzesS2(LOW, LOW, HIGH); 
    s2_no_amarelo = false;
  }
}

// --- Funções de Conexão ---
void conectarWiFi() {
  Serial.print("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT Broker...");
    if (client.connect("ESP32_Escravo_S2")) {
      Serial.println("Conectado!");
      // Inscreve-se no tópico para ouvir o S1
      client.subscribe(topic);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

// ★★★ ESTA É A FUNÇÃO MAIS IMPORTANTE ★★★
// Chamada automaticamente quando uma mensagem chega no Tópico
void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  
  // Converte a mensagem em String
  String mensagem = "";
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  Serial.println(mensagem);

  // Lógica de reação do Semáforo 2
  if (mensagem == "s1_verde" || mensagem == "s1_amarelo") {
    // Se S1 está verde ou amarelo, S2 TEM que estar vermelho
    configuraLuzesS2(LOW, LOW, HIGH);
    s2_no_amarelo = false;
  } 
  else if (mensagem == "s1_vermelho") {
    // S1 ficou vermelho. S2 pode iniciar seu ciclo.
    // Lógica Invertida: S2 fica VERDE
    // (Vamos pular o amarelo piscante e ir direto pra verde por simplicidade)
    
    // ATUALIZAÇÃO: Vamos fazer o ciclo certo
    // S2 estava vermelho, agora fica verde
    configuraLuzesS2(HIGH, LOW, LOW);
    
    /* // Lógica mais complexa (opcional):
    // Se S1 acabou de ficar vermelho, S2 deve ficar Verde.
    // O Mestre controla o tempo de S2 ficar Amarelo.
    // Para simplificar, vamos fazer o ciclo aqui
    configuraLuzesS2(HIGH, LOW, LOW); // S2 Verde
    //... (o código do mestre teria que mandar "s2_amarelo_agora")
    //... para manter simples, o Mestre já deu 7s de S1_Vermelho (5s verde + 2s amarelo)
    */
  }
  else if (mensagem == "noite") {
    // Modo noturno: pisca amarelo
    int estado = (millis() / 500) % 2;
    configuraLuzesS2(LOW, estado, LOW);
  }
}

void configuraLuzesS2(int v, int a, int r) {
  digitalWrite(s2_verde, v); digitalWrite(s2_amarelo, a); digitalWrite(s2_vermelho, r);
}