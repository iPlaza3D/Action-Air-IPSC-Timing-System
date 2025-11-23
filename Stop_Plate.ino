#include <WiFi.h>
#include <WiFiUdp.h>

// ============================================================================
// CONFIGURACIÓN CLIENTE
// ============================================================================
const char* ssid = "AIRSOFT Stage 01";
const char* password = "12345678";
WiFiUDP udp;
const uint16_t PORT = 4210;
const char* SERVER_IP = "255.255.255.255"; // Broadcast

const int BUTTON_PIN = 0; // Botón BOOT para simular hit
const int LED_PIN = 2;    // LED interno para feedback

// Variables de simulación
uint16_t sequenceNumber = 0;
bool lastButtonState = HIGH;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  
  // Configurar pines
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  // Conectar a WiFi
  WiFi.begin(ssid, password);
  Serial.print("🔌 Conectando a WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // LED parpadeante
  }
  
  Serial.println("\n✅ Conectado a: " + String(ssid));
  Serial.println("📡 IP: " + WiFi.localIP().toString());
  
  digitalWrite(LED_PIN, HIGH); // LED encendido = conectado
  udp.begin(PORT);
  
  Serial.println("🎯 Simulador de Plato LISTO");
  Serial.println("👉 Presiona BOOT para enviar HIT al stage");
  Serial.println("🔘 O envía comandos por Serial:");
  Serial.println("   'hit' = Simular impacto");
  Serial.println("   'stop' = Finalizar stage");
  Serial.println("   'sync' = Solicitar sync");
}

// ============================================================================
// ENVIAR MENSAJE HIT
// ============================================================================
void sendHit(int sensorId) {
  uint32_t currentTime = micros();
  char message[64];
  
  snprintf(message, sizeof(message), "HIT:%d:%lu:%d", sensorId, currentTime, sequenceNumber++);
  
  udp.beginPacket(SERVER_IP, PORT);
  udp.write((uint8_t*)message, strlen(message));
  udp.endPacket();
  
  Serial.printf("🎯 HIT enviado: ID=%d, Time=%lu, Seq=%d\n", sensorId, currentTime, sequenceNumber-1);
  
  // Feedback visual
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
}

// ============================================================================
// ENVIAR MENSAJE STOP
// ============================================================================
void sendStop() {
  uint32_t currentTime = micros();
  char message[64];
  
  snprintf(message, sizeof(message), "HIT:99:%lu:%d", currentTime, sequenceNumber++);
  
  udp.beginPacket(SERVER_IP, PORT);
  udp.write((uint8_t*)message, strlen(message));
  udp.endPacket();
  
  Serial.println("🛑 STOP enviado - Finalizando stage");
  
  // Feedback visual especial
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
  }
}

// ============================================================================
// SOLICITAR SYNC
// ============================================================================
void requestSync() {
  char message[32] = "SYNC_REQUEST";
  
  udp.beginPacket(SERVER_IP, PORT);
  udp.write((uint8_t*)message, strlen(message));
  udp.endPacket();
  
  Serial.println("⏱️ Sync solicitado");
}

// ============================================================================
// PROCESAR COMANDOS SERIAL
// ============================================================================
void processSerialCommand() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "hit" || command == "h") {
      sendHit(1); // Sensor ID 1
    }
    else if (command == "stop" || command == "s") {
      sendStop();
    }
    else if (command == "sync" || command == "r") {
      requestSync();
    }
    else if (command.startsWith("hit")) {
      // Formato: "hit 5" para ID específico
      int id = command.substring(4).toInt();
      if (id > 0) sendHit(id);
    }
    else {
      Serial.println("❌ Comando desconocido. Usa: hit, stop, sync");
    }
  }
}

// ============================================================================
// DETECTAR BOTÓN
// ============================================================================
void checkButton() {
  bool currentState = digitalRead(BUTTON_PIN);
  
  if (lastButtonState == HIGH && currentState == LOW) {
    // Botón presionado
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) { // Confirmar
      sendHit(99); // Enviar hit con ID 99
    }
  }
  
  lastButtonState = currentState;
}

// ============================================================================
// ESCUCHAR RESPUESTAS
// ============================================================================
void listenForResponses() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packet[64];
    int len = udp.read(packet, 64);
    if (len > 0) {
      packet[len] = '\0';
      
      Serial.print("📥 Respuesta recibida: ");
      Serial.println(packet);
      
      if (strncmp(packet, "ACK:", 4) == 0) {
        Serial.println("✅ ACK confirmado por el stage");
      }
      else if (strncmp(packet, "SYNC:", 5) == 0) {
        uint32_t syncTime = strtoul(packet + 5, NULL, 10);
        Serial.printf("⏱️ SYNC recibido: %lu\n", syncTime);
      }
    }
  }
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
  checkButton();        // Verificar botón físico
  processSerialCommand(); // Procesar comandos serial
  listenForResponses(); // Escuchar respuestas
  
  delay(10);
}