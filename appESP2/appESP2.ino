#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>

// ===== CONFIGURAÇÕES MQTT (HIVEMQ) =====
const char* mqtt_server = "207e698bbf4b4a0bb7ba88c530587131.s1.eu.hivemq.cloud";
const int mqtt_port = 8883; 
const char* mqtt_user = "meu_esp32";
const char* mqtt_pass = "@Senha123";

const char* topic_cmd = "moto/comando";
const char* topic_state = "moto/estado";
const char* topic_ip = "moto/ip";

WiFiClientSecure espClient;
PubSubClient client(espClient);
WebServer server(80);
Preferences preferences;

// ===== VARIÁVEIS DE ESTADO =====
String ssid = "";
String password = "";
const uint8_t pinIgnicao = 23; 
const uint8_t pinPartida = 22; 
const uint8_t ledPin = 2;
bool ignicaoLigada = false;
bool modoConfig = false;

void atualizarEstadoIgnicao(bool novoEstado) {
  ignicaoLigada = novoEstado;
  digitalWrite(pinIgnicao, ignicaoLigada);
  digitalWrite(ledPin, ignicaoLigada);
  if (client.connected()) {
    client.publish(topic_state, ignicaoLigada ? "LIGADA" : "DESLIGADA", true);
  }
}

void sequenciaPartida() {
  Serial.println(">>> PASSO 1: Ligando Ignição...");
  atualizarEstadoIgnicao(HIGH); 
  delay(2000); 
  Serial.println(">>> PASSO 2: Acionando Partida...");
  digitalWrite(pinPartida, HIGH); 
  delay(1500);                   
  digitalWrite(pinPartida, LOW);  
  Serial.println(">>> MOTO PRONTA!");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (msg == "LIGAR") { if (!ignicaoLigada) sequenciaPartida(); }
  else if (msg == "DESLIGAR") { atualizarEstadoIgnicao(LOW); }
}

void reconnect() {
  if (modoConfig) return;
  while (!client.connected()) {
    Serial.print("Conectando ao HiveMQ...");
    espClient.setInsecure();
    String clientId = "Moto_ESP32_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Conectado!");
      client.subscribe(topic_cmd);
      client.publish(topic_ip, WiFi.localIP().toString().c_str(), true); // Envia IP para o App
      client.publish(topic_state, ignicaoLigada ? "LIGADA" : "DESLIGADA", true);
    } else {
      Serial.print("Falha rc="); Serial.print(client.state());
      delay(5000);
    }
  }
}

// ===== PORTAL DE CONFIGURAÇÃO COM VALIDAÇÃO =====
void handleConfig() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<style>body{background:#121212;color:white;font-family:sans-serif;text-align:center;padding:20px;}";
  html += "input{width:90%;padding:15px;margin:10px 0;border-radius:8px;border:none;font-size:16px;}";
  html += "button{width:90%;padding:15px;background:#ff9900;border:none;border-radius:8px;font-size:18px;font-weight:bold;cursor:pointer;}</style></head>";
  html += "<body><h1>Configurar WiFi</h1><p>Digite os dados da sua rede:</p>";
  html += "<form action='/save' method='POST'>";
  html += "<input type='text' name='s' placeholder='Nome da Rede (SSID)' required><br>";
  html += "<input type='password' name='p' placeholder='Senha' required><br>";
  html += "<button type='submit'>VALIDAR E CONECTAR</button></form></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  String s = server.arg("s"); s.trim();
  String p = server.arg("p"); p.trim();
  
  server.send(200, "text/html", "<h1>Validando...</h1><p>Aguarde enquanto testamos a conexão com " + s + "</p>");
  
  WiFi.begin(s.c_str(), p.c_str());
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 20) { delay(500); t++; }

  if (WiFi.status() == WL_CONNECTED) {
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", s);
    preferences.putString("pass", p);
    preferences.end();
    delay(1000);
    ESP.restart();
  } else {
    // Se falhar, volta para o AP para o usuário tentar de novo
    WiFi.softAP("CONFIGURAR_MOTO_ST"); 
  }
}

void home() {
  if (modoConfig) { handleConfig(); return; }
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>";
  h += "body{background:#121212;color:white;font-family:Arial;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;}";
  h += ".card{background:#1e1e1e;border-radius:40px;padding:30px;width:320px;text-align:center;box-shadow:0 10px 30px rgba(0,0,0,0.8); border: 2px solid #333;}";
  h += "h1{color:#ff9900; letter-spacing: 2px;}";
  h += ".pwr{width:100%;height:150px;border-radius:75px;border:none;font-size:1.8em;font-weight:bold;cursor:pointer;transition:0.2s;text-transform:uppercase;}";
  h += ".on{background:linear-gradient(145deg, #27ae60, #2ecc71);color:white;box-shadow:0 8px #1e8449;}";
  h += ".off{background:linear-gradient(145deg, #c0392b, #e74c3c);color:white;box-shadow:0 8px #922b21;}";
  h += ".pwr:active{transform:translateY(4px);box-shadow:0 4px #000;}";
  h += "</style></head><body><div class='card'><h1>🏍️ MOTO START</h1>";
  h += "<button class='pwr " + String(ignicaoLigada?"on":"off") + "' id='p' onclick='t()'>" + String(ignicaoLigada?"LIGADA":"PARTIDA") + "</button>";
  h += "<br><br><a href='/config' style='color:#666;text-decoration:none;font-size:12px;'>Alterar WiFi</a>";
  h += "</div><script>function t(){fetch('/t').then(r=>r.json()).then(d=>{location.reload();});}</script></body></html>";
  server.send(200, "text/html", h);
}

void apiToggle() { 
  if (!ignicaoLigada) sequenciaPartida();
  else atualizarEstadoIgnicao(LOW);
  server.send(200, "application/json", "{\"e\":" + String(ignicaoLigada) + "}"); 
}

void setup() {
  Serial.begin(115200);
  pinMode(pinIgnicao, OUTPUT); pinMode(pinPartida, OUTPUT); pinMode(ledPin, OUTPUT);
  digitalWrite(pinIgnicao, LOW); digitalWrite(pinPartida, LOW); digitalWrite(ledPin, LOW);

  preferences.begin("wifi-config", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("pass", "");
  preferences.end();

  if (ssid != "") {
    WiFi.begin(ssid.c_str(), password.c_str());
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 30) { delay(500); Serial.print("."); count++; }
  }

  if (WiFi.status() != WL_CONNECTED) {
    modoConfig = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CONFIGURAR_MOTO_ST");
    Serial.println("\nModo Config: 192.168.4.1");
  } else {
    Serial.println("\nConectado: " + WiFi.localIP().toString());
    configTime(0, 0, "pool.ntp.org");
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
  }

  server.on("/", home);
  server.on("/config", handleConfig);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/t", apiToggle);
  server.begin();
}

void loop() {
  server.handleClient();
  if (!modoConfig) {
    if (!client.connected()) reconnect();
    client.loop();
  }
}
