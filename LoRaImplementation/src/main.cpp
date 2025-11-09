#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include <ArduinoJson.h>

// ===== OLED (I2C) =====
#define OLED_SDA   17
#define OLED_SCL   18
#define OLED_RST   21
#define OLED_ADDR  0x3C
#define SCREEN_W   128
#define SCREEN_H   64

// ===== Vext control =====
#define VEXT_CTRL  36
#define VEXT_ON_LOW 1

// ===== LoRa (SX1262) pins =====
#define LORA_CS    8
#define LORA_SCK   9
#define LORA_MOSI  10
#define LORA_MISO  11
#define LORA_RST   12
#define LORA_BUSY  13
#define LORA_DIO1  14    // IRQ: DIO1 

// ===== Botón PRG (GPIO0) =====
#define BTN_PRG    0

// ===== LED de RX =====
#define LED_PIN    35

// ===== App =====
#define NODE_ID    8 
uint8_t DEST_ID = 0;                 // 0=broadcast
#define JSON_BUF_SIZE 256

// ===== Parámetros LoRa =====
#define LORA_FREQ_MHZ   915.0
#define LORA_BW_KHZ     125.0
#define LORA_SF         9        
#define LORA_CR         7
#define LORA_PREAMBLE   10
#define LORA_PWR_DBM    10        
#define LORA_SYNC       RADIOLIB_SX126X_SYNC_WORD_PUBLIC

// ===== ACKs / Latencia =====
#define MAX_NODES         8
#define ACK_WINDOW_MS     24000
#define LED_RX_PULSE_MS   120

// ===== Mesh Multi-Hop =====
#define DEFAULT_TTL       5      // Saltos máximos permitidos
#define MID_CACHE_SIZE    20     // Tamaño del caché de MIDs vistos
#define RELAY_DELAY_MIN   50     // Delay mínimo antes de retransmitir (ms)
#define RELAY_DELAY_MAX   200    // Delay máximo antes de retransmitir (ms)

// ===== Simulación de topología de red =====
// Define qué nodos están en "alcance directo" de este nodo
#define SIMULATE_TOPOLOGY false   // true = solo acepta de vecinos, false = acepta de todos

#if NODE_ID == 1
  const uint8_t neighbors[] = {2, 3};        // Node 1 ve a 2, 3
  const uint8_t numNeighbors = 2;
#elif NODE_ID == 2
  const uint8_t neighbors[] = {1, 3, 6};     // Node 2 ve a 1, 3, 6
  const uint8_t numNeighbors = 3;
#elif NODE_ID == 3
  const uint8_t neighbors[] = {1, 2, 4, 6, 7, 8};  // Node 3 ve a 1, 2, 4, 6, 7, 8 (hub)
  const uint8_t numNeighbors = 6;
#elif NODE_ID == 4
  const uint8_t neighbors[] = {3, 5, 8};     // Node 4 ve a 3, 5, 8
  const uint8_t numNeighbors = 3;
#elif NODE_ID == 5
  const uint8_t neighbors[] = {4, 8};        // Node 5 ve a 4, 8
  const uint8_t numNeighbors = 2;
#elif NODE_ID == 6
  const uint8_t neighbors[] = {2, 3, 7};     // Node 6 ve a 2, 3, 7
  const uint8_t numNeighbors = 3;
#elif NODE_ID == 7
  const uint8_t neighbors[] = {3, 6, 8};     // Node 7 ve a 3, 6, 8
  const uint8_t numNeighbors = 3;
#elif NODE_ID == 8
  const uint8_t neighbors[] = {3, 4, 5, 7};  // Node 8 ve a 3, 4, 5, 7
  const uint8_t numNeighbors = 4;
#else
  const uint8_t neighbors[] = {1, 2, 3, 4, 5, 6, 7, 8}; // Default: todos
  const uint8_t numNeighbors = 8;
#endif

// Delayed ACK (simple fixed-delay scheduling per node)
struct DelayedAck {
  bool active;
  uint32_t txTime;
  uint16_t seq;
  uint32_t mid;
  uint8_t dest;
  String path;  // Path para source routing del ACK
};

DelayedAck delayedAck = { false, 0, 0, 0, 0, "" };

// Delayed ACK Relay (para retransmitir ACKs con delay y evitar colisiones)
struct DelayedAckRelay {
  bool active;
  uint32_t txTime;
  uint8_t from;      // Nodo origen del ACK
  uint8_t nextHop;   // A quién enviar
  uint16_t seq;
  uint32_t mid;
};

DelayedAckRelay delayedAckRelay = { false, 0, 0, 0, 0, 0 };

// Caché de MIDs para prevenir duplicados
uint32_t seenMids[MID_CACHE_SIZE];
int midCacheIndex = 0;

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RST);

// SPI dedicado
SPIClass spiLoRa(HSPI);
Module sx1262Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, spiLoRa);
SX1262 radio(&sx1262Module);

bool radioReady = false;
char jsonBuf[JSON_BUF_SIZE];

// PRG debounce/flanco
bool btnPrev = true;           // HIGH = suelto
uint32_t btnTs = 0;
const uint16_t BTN_DB_MS = 40;
bool armed = true;

// Flag de RX por interrupción DIO1 
volatile bool rxFlag = false;
void IRAM_ATTR onRadioRx() { rxFlag = true; }

// LED pulse en RX
bool ledOn = false;
uint32_t ledTs = 0;

// Seguimiento de TX/ACKs
uint16_t txSeq = 0;                    // secuencia de TX local
uint32_t txMid = 0;                    // ID único de mensaje actual
bool waitingAcks = false;              // si está en ventana de recogida
uint32_t txStartMs = 0;                // inicio para calcular RTT
uint32_t ackWindowDeadline = 0;        // fin de ventana

// rttMs[i] = tiempo RTT en ms para nodo i (1..8), -1 = no recibido
int32_t rttMs[MAX_NODES + 1];          

// gen de MID único (simple y suficiente)
uint32_t nextMid() {
  static uint32_t ctr = 0xA5A50000u ^ ((uint32_t)NODE_ID << 16) ^ (uint32_t)millis();
  ctr += 0x00010001u;  // salta con patrón que evita repetir en corto
  return ctr;
}

// Caché de MIDs para prevenir retransmisiones duplicadas
bool hasSeen(uint32_t mid) {
  for (int i = 0; i < MID_CACHE_SIZE; i++) {
    if (seenMids[i] == mid) return true;
  }
  return false;
}

void rememberMid(uint32_t mid) {
  seenMids[midCacheIndex] = mid;
  midCacheIndex = (midCacheIndex + 1) % MID_CACHE_SIZE;
}

// Verificar si un nodo está en nuestra lista de vecinos
bool isNeighbor(uint8_t nodeId) {
  if (!SIMULATE_TOPOLOGY) return true;  // Si no simulamos, aceptar todos
  for (uint8_t i = 0; i < numNeighbors; i++) {
    if (neighbors[i] == nodeId) return true;
  }
  return false;
}


// OLED helpers
void vextOn() {
  pinMode(VEXT_CTRL, OUTPUT);
#if VEXT_ON_LOW
  digitalWrite(VEXT_CTRL, LOW);
#else
  digitalWrite(VEXT_CTRL, HIGH);
#endif
}
void vextOff() {
#if VEXT_ON_LOW
  digitalWrite(VEXT_CTRL, HIGH);
#else
  digitalWrite(VEXT_CTRL, LOW);
#endif
}

void oledPrintLines(const char* l1 = nullptr, const char* l2 = nullptr,
                    const char* l3 = nullptr, const char* l4 = nullptr,
                    const char* l5 = nullptr, const char* l6 = nullptr) {
  // Contar cuántas líneas no nulas tenemos
  int lineCount = 0;
  if (l1 && l1[0]) lineCount++;
  if (l2 && l2[0]) lineCount++;
  if (l3 && l3[0]) lineCount++;
  if (l4 && l4[0]) lineCount++;
  if (l5 && l5[0]) lineCount++;
  if (l6 && l6[0]) lineCount++;
  
  // Calcular posición Y inicial para centrar verticalmente
  // Cada línea ocupa 8 píxeles en textSize=1
  const int lineHeight = 8;
  int totalHeight = lineCount * lineHeight;
  int startY = (SCREEN_H - totalHeight) / 2;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, startY);
  if (l1 && l1[0]) display.println(l1);
  if (l2 && l2[0]) display.println(l2);
  if (l3 && l3[0]) display.println(l3);
  if (l4 && l4[0]) display.println(l4);
  if (l5 && l5[0]) display.println(l5);
  if (l6 && l6[0]) display.println(l6);
  display.display();
}

void oledShowIdle() {
  char a[32], b[32];
  snprintf(a, sizeof(a), "Nodo ID: %d", NODE_ID);
  snprintf(b, sizeof(b), "Destino: %d", DEST_ID);
  oledPrintLines(a, b, "RX listo...");
}

// Recorta strings para caber en 128 px (aprox 21-22 chars en textSize=1)
String trimForOLED(const String& s, uint8_t maxChars = 22) {
  if (s.length() <= maxChars) return s;
  return s.substring(0, maxChars - 3) + "...";
}

// Tabla compacta con encabezado de unidades
void oledShowAckTable() {
  char h1[22], h2[22], r1[22], r2[22], r3[22], r4[22];
  snprintf(h1, sizeof(h1), "ID %d -> %d", NODE_ID, DEST_ID);
  snprintf(h2, sizeof(h2), "SEQ %u  RTT [ms]", (unsigned)txSeq);

  auto cell = [](int32_t v, char* out, size_t n){
    if (v < 0) snprintf(out, n, "-");
    else       snprintf(out, n, "%ld", (long)v);
  };

  char c1[6], c2[6], c3[6], c4[6], c5[6], c6[6], c7[6], c8[6];
  cell(rttMs[1], c1, sizeof(c1));
  cell(rttMs[2], c2, sizeof(c2));
  cell(rttMs[3], c3, sizeof(c3));
  cell(rttMs[4], c4, sizeof(c4));
  cell(rttMs[5], c5, sizeof(c5));
  cell(rttMs[6], c6, sizeof(c6));
  cell(rttMs[7], c7, sizeof(c7));
  cell(rttMs[8], c8, sizeof(c8));

  // Columnas fijas: 1:xxxx 2:xxxx
  snprintf(r1, sizeof(r1), "1:%-4s 2:%-4s", c1, c2);
  snprintf(r2, sizeof(r2), "3:%-4s 4:%-4s", c3, c4);
  snprintf(r3, sizeof(r3), "5:%-4s 6:%-4s", c5, c6);
  snprintf(r4, sizeof(r4), "7:%-4s 8:%-4s", c7, c8);

  oledPrintLines(h1, h2, r1, r2, r3, r4);
}

// Pantalla del receptor: muestra emisor, payload y mid
void oledShowRxDetail(int from, const String& payload, uint32_t mid, uint8_t hops) {
  char l1[22], l3[22], l4[22];
  snprintf(l1, sizeof(l1), "RX de: %d", from);
  String p = "msg: " + trimForOLED(payload, 22 - 5);
  snprintf(l3, sizeof(l3), "mid: %lu", (unsigned long)mid);
  
  // Mostrar si llegó por relay (multi-hop)
  if (hops > 0) {
    snprintf(l4, sizeof(l4), "hops: %d (relay)", hops);
    oledPrintLines(l1, p.c_str(), l3, l4);
  } else {
    snprintf(l4, sizeof(l4), "hops: %d (directo)", hops);
    oledPrintLines(l1, p.c_str(), l3, l4);
  }
}

void reviveOLEDIfNeeded() {
  static uint32_t last = 0;
  if (millis() - last < 2000) return;
  last = millis();
  Wire.beginTransmission(OLED_ADDR);
  if (Wire.endTransmission() != 0) {
    vextOn();
    delay(50);
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    oledPrintLines("OLED reinit");
  }
}

// --------- Mensajería JSON con Mesh Multi-Hop ---------
String makeMsg(uint8_t from, uint8_t to, const char* type,
               const char* payload, uint16_t seq, uint32_t mid,
               uint8_t ttl = DEFAULT_TTL, uint8_t hops = 0) {
  StaticJsonDocument<JSON_BUF_SIZE> doc;
  doc["from"] = from;
  doc["to"]   = to;
  doc["type"] = type ? type : "ping";
  if (payload) doc["payload"] = payload;
  doc["seq"]  = seq;
  doc["mid"]  = mid;    // ID único de mensaje
  doc["ttl"]  = ttl;    // Time To Live (saltos máximos)
  doc["hops"] = hops;   // Saltos ya realizados
  
  // Path: array con la ruta (para debugging)
  JsonArray path = doc.createNestedArray("path");
  path.add(from);
  
  size_t n = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
  return String(jsonBuf, n);
}

bool parseMsg(const String& s, int &from, int &to, String &type,
              String &payload, int &seq, uint32_t &mid,
              int &ttl, int &hops, String &path) {
  StaticJsonDocument<JSON_BUF_SIZE> doc;
  auto err = deserializeJson(doc, s);
  if (err) return false;
  
  from = doc["from"] | -1;
  to   = doc["to"]   | -1;
  type = (const char*)doc["type"]    ? (const char*)doc["type"]    : "";
  payload = (const char*)doc["payload"] ? (const char*)doc["payload"] : "";
  seq  = doc["seq"] | -1;
  mid  = doc["mid"] | 0u;
  ttl  = doc["ttl"] | DEFAULT_TTL;
  hops = doc["hops"] | 0;
  
  // Parsear path como string "[1,2,3]"
  path = "";
  if (doc.containsKey("path")) {
    JsonArray pathArray = doc["path"];
    path = "[";
    for (size_t i = 0; i < pathArray.size(); i++) {
      if (i > 0) path += ",";
      path += String((int)pathArray[i]);
    }
    path += "]";
  }
  
  return (from >= 0 && to >= 0);
}

// -----------------------------------

// ---- Función para retransmitir mensajes (Mesh Multi-Hop) ----
void relayMessage(const String& originalMsg, int ttl, int hops, 
                  int from, int to, const String& type, const String& payload,
                  int seq, uint32_t mid) {
  // Decrementar TTL y aumentar hops
  ttl--;
  hops++;
  
  Serial.printf("[RELAY] Retransmitiendo msg mid=%lu, ttl=%d, hops=%d\n", 
                (unsigned long)mid, ttl, hops);
  
  // Parsear el mensaje original para modificarlo
  StaticJsonDocument<JSON_BUF_SIZE> doc;
  deserializeJson(doc, originalMsg);
  
  // Actualizar TTL y hops
  doc["ttl"] = ttl;
  doc["hops"] = hops;
  
  // Agregar este nodo a la ruta
  JsonArray path = doc["path"];
  path.add(NODE_ID);
  
  // Serializar el mensaje modificado
  char relayBuf[JSON_BUF_SIZE];
  size_t n = serializeJson(doc, relayBuf, sizeof(relayBuf));
  String relayMsg = String(relayBuf, n);
  
  // Pequeño delay aleatorio para evitar colisiones
  delay(random(RELAY_DELAY_MIN, RELAY_DELAY_MAX));
  
  // Transmitir
  radio.clearDio1Action();
  radio.standby();
  int16_t tx = radio.transmit(relayMsg.c_str());
  if (tx == RADIOLIB_ERR_NONE) {
    Serial.println("[RELAY] Retransmisión exitosa");
  } else {
    Serial.printf("[RELAY] Error en retransmisión: %d\n", tx);
  }
  radio.setDio1Action(onRadioRx);
  radio.startReceive();
}

// -----------------------------------

// --- Simple fixed-delay ACK scheduler (2000ms por nodo) ---
uint32_t fixedDelayForNode(uint8_t nodeId) {
  if (nodeId < 1) nodeId = 1;
  return (uint32_t)nodeId * 2000u;  // 2s, 4s, 6s, 8s, etc.
}

void scheduleDelayedAck(uint8_t from, uint16_t seq, uint32_t mid, const String& pathStr) {
  delayedAck.active = true;
  delayedAck.dest = from;
  delayedAck.seq = seq;
  delayedAck.mid = mid;
  delayedAck.path = pathStr;
  
  // Delay base + jitter aleatorio pequeño para evitar colisiones exactas
  uint32_t baseDelay = fixedDelayForNode((uint8_t)NODE_ID);
  uint32_t jitter = random(0, 200);  // 0-200ms de variación aleatoria
  delayedAck.txTime = millis() + baseDelay + jitter;
  
  Serial.printf("[DELAYED SCHED] dest=%d mid=%lu txIn=%lu (base=%lu+jitter=%lu) path=%s\n", 
                (int)from, (unsigned long)mid, 
                (unsigned long)(delayedAck.txTime - millis()),
                (unsigned long)baseDelay, (unsigned long)jitter,
                pathStr.c_str());
}

// forward declaration (blink helper is defined later)
void blinkLedOnRx();

// Obtener el siguiente salto en la ruta inversa para ACKs
// Path: [1,2,3,4] y estoy en nodo 3 -> devuelve 2 (el anterior)
int getNextHopForAck(const String& pathStr, uint8_t myId) {
  // Parsear el path [1,2,3,4]
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, pathStr);
  if (err) {
    Serial.printf("[ACK ROUTE] Error parseando path: %s\n", err.c_str());
    return -1;
  }
  
  JsonArray path = doc.as<JsonArray>();
  int pathSize = path.size();
  
  // Encontrar mi posición en el path
  for (int i = 0; i < pathSize; i++) {
    if (path[i] == myId) {
      // Si soy el primero (origen), no hay siguiente
      if (i == 0) return -1;
      // Devolver el nodo anterior (ruta inversa)
      return path[i - 1];
    }
  }
  
  // No me encontré en el path
  return -1;
}

void processDelayedAck() {
  if (!delayedAck.active) return;
  if ((int32_t)(millis() - delayedAck.txTime) < 0) return;

  // transmit ACK now (indicate with LED)
  blinkLedOnRx();
  radio.clearDio1Action();
  radio.standby();
  
  // Determinar el siguiente salto en la ruta inversa
  int nextHop = getNextHopForAck(delayedAck.path, NODE_ID);
  uint8_t ackDestination;
  
  if (nextHop > 0) {
    // Hay ruta inversa: enviar ACK al siguiente nodo en el path
    ackDestination = (uint8_t)nextHop;
    Serial.printf("[ACK ROUTE] Enviando ACK a nodo %d (siguiente en ruta inversa)\n", nextHop);
  } else {
    // No hay ruta o soy el origen: enviar directo al destino final
    ackDestination = delayedAck.dest;
    Serial.printf("[ACK ROUTE] Enviando ACK directo a destino %d\n", delayedAck.dest);
  }
  
  String ack = makeMsg(NODE_ID, ackDestination, "ack", "ok", delayedAck.seq, delayedAck.mid);
  int16_t st = radio.transmit(ack.c_str());
  Serial.printf("[DELAYED TX] to=%d mid=%lu st=%d\n", ackDestination, (unsigned long)delayedAck.mid, (int)st);
  delayedAck.active = false;
  delayedAck.path = "";
  radio.setDio1Action(onRadioRx);
  radio.startReceive();
}

// Programar un relay de ACK con delay para evitar colisiones
void scheduleDelayedAckRelay(uint8_t from, uint8_t nextHop, uint16_t seq, uint32_t mid) {
  delayedAckRelay.active = true;
  delayedAckRelay.from = from;
  delayedAckRelay.nextHop = nextHop;
  delayedAckRelay.seq = seq;
  delayedAckRelay.mid = mid;
  
  // Delay aleatorio pequeño (100-300ms) para evitar colisiones entre relays
  uint32_t relayDelay = random(100, 300);
  delayedAckRelay.txTime = millis() + relayDelay;
  
  Serial.printf("[ACK RELAY SCHED] from=%d to=%d mid=%lu delay=%lums\n", 
                from, nextHop, (unsigned long)mid, (unsigned long)relayDelay);
}

void processDelayedAckRelay() {
  if (!delayedAckRelay.active) return;
  if ((int32_t)(millis() - delayedAckRelay.txTime) < 0) return;

  Serial.printf("[ACK RELAY TX] Retransmitiendo ACK de nodo %d hacia nodo %d\n", 
                delayedAckRelay.from, delayedAckRelay.nextHop);
  
  radio.clearDio1Action();
  radio.standby();
  
  String relayAck = makeMsg(delayedAckRelay.from, delayedAckRelay.nextHop, 
                            "ack", "ok", delayedAckRelay.seq, delayedAckRelay.mid);
  int16_t st = radio.transmit(relayAck.c_str());
  Serial.printf("[ACK RELAY TX] st=%d\n", (int)st);
  
  delayedAckRelay.active = false;
  radio.setDio1Action(onRadioRx);
  radio.startReceive();
}

void setup() {
  Serial.begin(115200);
  delay(150);

  pinMode(BTN_PRG, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Pantalla / Vext
  vextOn(); delay(50);
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);
  delay(200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED FAIL");
    for(;;) delay(1000);
  }
  oledPrintLines("OLED OK", "Init LoRa...");

  // SPI y reset HW radio
  spiLoRa.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW);  delay(10);
  digitalWrite(LORA_RST, HIGH); delay(20);

  // Inicialización robusta (TCXO 1.8V y fallback XTAL)
  int16_t st = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                           LORA_SYNC, LORA_PREAMBLE, LORA_PWR_DBM,
                           1.8, true);
  if (st != RADIOLIB_ERR_NONE) {
    st = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                     LORA_SYNC, LORA_PREAMBLE, LORA_PWR_DBM,
                     0.0, false);
  }

  if (st == RADIOLIB_ERR_NONE) {
    radio.setDio2AsRfSwitch(true);
    radio.setDio1Action(onRadioRx);
    radio.startReceive();
    radioReady = true;
    oledShowIdle();
  } else {
    oledPrintLines("LoRa FAIL", String(st).c_str());
  }

  for (int i=1;i<=MAX_NODES;i++) rttMs[i] = -1;
  
  // Inicializar caché de MIDs para mesh
  for (int i=0; i<MID_CACHE_SIZE; i++) seenMids[i] = 0;
  
  Serial.println("Mesh Multi-Hop enabled!");
  Serial.printf("TTL: %d, Cache: %d MIDs\n", DEFAULT_TTL, MID_CACHE_SIZE);
}

void startAckWindow() {
  waitingAcks = true;
  txStartMs = millis();
  ackWindowDeadline = txStartMs + ACK_WINDOW_MS;
  for (int i=1;i<=MAX_NODES;i++) rttMs[i] = -1;
  oledShowAckTable();
}

void endAckWindowIfDue() {
  if (waitingAcks && (int32_t)(millis() - ackWindowDeadline) >= 0) {
    waitingAcks = false;
    oledShowAckTable(); // los faltantes quedan '-'
  }
}

void blinkLedOnRx() {
  digitalWrite(LED_PIN, HIGH);
  ledOn = true;
  ledTs = millis();
}

void serviceLedPulse() {
  if (ledOn && (millis() - ledTs >= LED_RX_PULSE_MS)) {
    digitalWrite(LED_PIN, LOW);
    ledOn = false;
  }
}

void loop() {
  reviveOLEDIfNeeded();
  if (!radioReady) return;

  serviceLedPulse();
  endAckWindowIfDue();
  processDelayedAck();
  processDelayedAckRelay();  // Procesar relays de ACKs retrasados

  // --- Serial commands ---
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length()) {
      if (cmd.charAt(0) == 't') {
        int v = cmd.substring(1).toInt();
        if (v >= 0 && v <= 255) {
          DEST_ID = (uint8_t)v;
          Serial.printf("Target set to %d\n", DEST_ID);
          oledShowIdle();
        }
      } else if (cmd == "b") {
        DEST_ID = 0;
        Serial.println("Target set to BROADCAST (0)");
        oledShowIdle();
      } else if (cmd == "s") {
        Serial.printf("Current target: %d\n", DEST_ID);
        oledShowIdle();
      }
    }
  }

  // ---------- RX ----------
  if (rxFlag) {
    rxFlag = false;

    String str;
    int16_t rs = radio.readData(str);
    if (rs == RADIOLIB_ERR_NONE && str.length()) {
      Serial.printf("[RX] %s\n", str.c_str());

      int from, to, seq, ttl, hops; 
      String type, payload, path; 
      uint32_t mid;
      
      if (parseMsg(str, from, to, type, payload, seq, mid, ttl, hops, path)) {
        bool addressedToMe = (to == NODE_ID) || (to == 0);
        
        Serial.printf("[MESH] from=%d to=%d type=%s ttl=%d hops=%d path=%s\n",
                      from, to, type.c_str(), ttl, hops, path.c_str());

        // Solo verificar duplicados para mensajes tipo "ping" (no para ACKs)
        if (type == "ping") {
          // Verificar si ya vimos este mensaje (prevenir loops)
          if (hasSeen(mid)) {
            Serial.printf("[MESH] Mensaje duplicado mid=%lu, descartado\n", (unsigned long)mid);
            radio.startReceive();
            return;
          }
          
          // ===== SIMULACIÓN DE TOPOLOGÍA: Verificar vecindad =====
          // Si el mensaje tiene hops=0 (directo) y NO viene de un vecino permitido,
          // rechazarlo para forzar que venga por relay
          // IMPORTANTE: No recordar el MID si rechazamos por topología
          if (SIMULATE_TOPOLOGY && hops == 0 && !isNeighbor(from)) {
            Serial.printf("[TOPOLOGY] Mensaje directo (hops=0) de nodo %d rechazado - no es vecino\n", from);
            radio.startReceive();
            return;
          }
          
          // Recordar este MID solo DESPUÉS de pasar todas las validaciones
          rememberMid(mid);
        }

        if (type == "ping" && addressedToMe) {
          // ===== IMPORTANTE: No responder a nuestros propios mensajes =====
          if (from == NODE_ID) {
            Serial.printf("[MESH] Ignorando mi propio mensaje (from=%d)\n", from);
            radio.startReceive();
            return;
          }
          
          // Encender LED al recibir el mensaje principal (ping)
          blinkLedOnRx();
          // Mostrar detalle en receptor
          oledShowRxDetail(from, payload, mid, hops);

          // Si es broadcast, programar un ACK retrasado fijo por nodo
          if (to == 0) {
            scheduleDelayedAck((uint8_t)from, (uint16_t)seq, mid, path);
          } else {
            // Unicast: responder con pequeño delay usando el sistema de relay
            int nextHop = getNextHopForAck(path, NODE_ID);
            uint8_t ackDestination = (nextHop > 0) ? (uint8_t)nextHop : (uint8_t)from;
            
            Serial.printf("[ACK ROUTE] Programando ACK unicast to=%d (nextHop=%d)\n", ackDestination, nextHop);
            scheduleDelayedAckRelay(NODE_ID, ackDestination, (uint16_t)seq, mid);
            radio.startReceive();
          }
        }
        else if (type == "ack") {
          // Mostrar en serial lo que llega (ACK) siempre para depuración
          Serial.printf("[ACK] from=%d to=%d seq=%d mid=%lu hops=%d path=%s\n",
                        from, to, seq, (unsigned long)mid, hops, path.c_str());
          
          bool ackForMe = (to == NODE_ID);
          
          if (ackForMe) {
            // Este ACK es para mí
            // Si estoy esperando ACKs y el mid coincide, registra RTT
            if (waitingAcks && mid == txMid) {
              if (from >= 1 && from <= MAX_NODES) {
                int32_t rtt = (int32_t)(millis() - txStartMs);
                // Restar el delay artificial para mostrar tiempo real
                int32_t artificialDelay = fixedDelayForNode((uint8_t)from);
                int32_t realRtt = rtt - artificialDelay;
                if (rttMs[from] < 0) {
                  rttMs[from] = (realRtt > 0) ? realRtt : 0; // mostrar 0 si es negativo
                  oledShowAckTable();
                }
              }
            }
            radio.startReceive();
          } else {
            // Este ACK NO es para mí - verificar si debo retransmitirlo (source routing)
            // Buscar el siguiente salto en la ruta inversa del path original
            int nextHop = getNextHopForAck(path, NODE_ID);
            
            if (nextHop > 0) {
              // Soy parte del path - programar retransmisión del ACK con delay
              scheduleDelayedAckRelay((uint8_t)from, (uint8_t)nextHop, (uint16_t)seq, mid);
              radio.startReceive();
            } else {
              // No soy parte del path, ignorar
              Serial.printf("[ACK] No soy parte del path, ignorando ACK\n");
              radio.startReceive();
            }
          }
        } else {
          radio.startReceive();
        }
        
        // ===== LÓGICA DE RETRANSMISIÓN (MESH MULTI-HOP) =====
        // SOLO retransmitir mensajes tipo "ping", NO retransmitir ACKs
        // Retransmitir si:
        //   1. Es tipo "ping" (no ACKs)
        //   2. TTL > 0 (quedan saltos disponibles)
        //   3. NO estoy ya en el path (prevenir loops)
        //   4. Es broadcast (to=0) O no es para mí (unicast que no me corresponde)
        
        bool shouldRelay = false;
        if (type == "ping" && ttl > 0) {
          // NO retransmitir si soy el emisor original
          if (from == NODE_ID) {
            shouldRelay = false;
            Serial.printf("[MESH] No retransmito mi propio mensaje\n");
          } 
          // Para broadcasts (to=0), SIEMPRE retransmitir (flooding controlado)
          // Para unicast, solo retransmitir si NO es para mí
          else if (to == 0 || !addressedToMe) {
            shouldRelay = true;
          }
        }
        
        if (shouldRelay) {
          // Verificar que este nodo NO esté ya en el path (prevenir loops)
          // Path viene como "[1,2,3]" entonces buscamos ",NODE_ID," o "[NODE_ID," o ",NODE_ID]"
          bool inPath = false;
          String searchStr1 = "," + String(NODE_ID) + ",";  // en medio
          String searchStr2 = "[" + String(NODE_ID) + ",";  // al inicio
          String searchStr3 = "," + String(NODE_ID) + "]";  // al final
          String searchStr4 = "[" + String(NODE_ID) + "]";  // solo elemento
          
          if (path.indexOf(searchStr1) >= 0 || path.indexOf(searchStr2) >= 0 || 
              path.indexOf(searchStr3) >= 0 || path.indexOf(searchStr4) >= 0) {
            inPath = true;
          }
          
          if (!inPath) {
            relayMessage(str, ttl, hops, from, to, type, payload, seq, mid);
          } else {
            Serial.printf("[MESH] Loop detectado - yo (node %d) ya estoy en path %s\n", 
                         NODE_ID, path.c_str());
          }
        }
        
      } else {
        radio.startReceive();
      }
    } else {
      radio.startReceive();
    }
  }

  // ---------- TX con botón ----------
  bool btnNow = digitalRead(BTN_PRG); // HIGH = suelto, LOW = presionado
  uint32_t now = millis();

  if (now - btnTs > BTN_DB_MS) {
    if (btnPrev != btnNow) {
      btnPrev = btnNow;
      btnTs = now;

      if (btnNow == LOW && armed) {
        // Backoff corto
        delay((NODE_ID * 50) + (millis() & 0x7F));

        txSeq++;
        txMid = nextMid();
        String msg = makeMsg(NODE_ID, DEST_ID, "ping", "hello", txSeq, txMid);

        radio.clearDio1Action();
        radio.standby();
        int16_t tx = radio.transmit(msg.c_str());
        Serial.printf("[TX] %s (st=%d)\n", msg.c_str(), tx);
        radio.setDio1Action(onRadioRx);
        radio.startReceive();

        // Mostrar estado inicial y abrir ventana de ACKs (incluye MID)
        char l1[32], l2[32], l3[32];
        snprintf(l1, sizeof(l1), "TX %s", (tx == RADIOLIB_ERR_NONE) ? "OK" : "ERR");
        snprintf(l2, sizeof(l2), "ID %d -> %d  SEQ %u", NODE_ID, DEST_ID, (unsigned)txSeq);
        snprintf(l3, sizeof(l3), "mid: %lu", (unsigned long)txMid);
        oledPrintLines(l1, l2, l3, "Esperando ACKs...");

        waitingAcks = true;
        txStartMs = millis();
        ackWindowDeadline = txStartMs + ACK_WINDOW_MS;
        for (int i=1;i<=MAX_NODES;i++) rttMs[i] = -1;
        oledShowAckTable();

        armed = false;
      } else if (btnNow == HIGH) {
        armed = true;
      }
    }
  }
}