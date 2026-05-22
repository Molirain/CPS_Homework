package main

import (
"database/sql"
"encoding/json"
"fmt"
"log"
"net/http"
"os"
"strconv"
"sync"
"time"

mqtt "github.com/eclipse/paho.mqtt.golang"
"github.com/gin-gonic/gin"
"github.com/gorilla/websocket"
_ "modernc.org/sqlite"
)

// Full Device Shadow
type DeviceShadow struct {
// Core
Mode       string  `json:"mode"`
Power      string  `json:"power"`
BlindAngle int     `json:"blind_angle"`
LightLevel int     `json:"light_level"`
Lux        float64 `json:"lux"`
Human      bool    `json:"human"`
Timestamp  int64   `json:"timestamp"`
// Climate
Temperature    float64 `json:"temperature"`
Humidity       float64 `json:"humidity"`
CO2            int     `json:"co2"`
OutdoorTemp    float64 `json:"outdoor_temp"`
OutdoorWeather string  `json:"outdoor_weather"`
HvacMode       string  `json:"hvac_mode"`
HvacSetpoint   float64 `json:"hvac_setpoint"`
HvacFan        string  `json:"hvac_fan"`
// Security
DoorClosed          bool `json:"door_closed"`
WindowLivingClosed  bool `json:"window_living_closed"`
WindowKitchenClosed bool `json:"window_kitchen_closed"`
CamerasOnline       int  `json:"cameras_online"`
}

var (
currentShadow DeviceShadow
shadowMutex   sync.RWMutex

mqttClient mqtt.Client
mqttOpts   *mqtt.ClientOptions

db *sql.DB

upgrader = websocket.Upgrader{CheckOrigin: func(r *http.Request) bool { return true }}

wsClients = make(map[*websocket.Conn]bool)
wsMutex   sync.Mutex
)

func getEnv(key, fallback string) string {
if v := os.Getenv(key); v != "" {
return v
}
return fallback
}

func getEnvBool(key string, fallback bool) bool {
v := os.Getenv(key)
if v == "" {
return fallback
}
b, err := strconv.ParseBool(v)
if err != nil {
return fallback
}
return b
}

func mqttBrokerURL() string {
host := getEnv("MQTT_HOST", "localhost")
useTLS := getEnvBool("MQTT_TLS", false)
port := "1883"
if useTLS {
port = getEnv("MQTT_PORT", "8883")
} else {
port = getEnv("MQTT_PORT", "1883")
}
scheme := "tcp"
if useTLS {
scheme = "ssl"
}
return fmt.Sprintf("%s://%s:%s", scheme, host, port)
}

// ─── SQLite ───

func initDB() {
var err error
dbDir := getEnv("SQLITE_DIR", "/data")
dbPath := getEnv("SQLITE_PATH", "/data/lumina.db")
os.MkdirAll(dbDir, 0755)

db, err = sql.Open("sqlite", dbPath+"?_journal_mode=WAL&_busy_timeout=5000")
if err != nil {
log.Fatalf("Failed to open SQLite: %v", err)
}
db.SetMaxOpenConns(1)

_, err = db.Exec(`CREATE TABLE IF NOT EXISTS device_shadow (
id INTEGER PRIMARY KEY CHECK (id = 1),
payload TEXT NOT NULL,
updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
)`)
if err != nil {
log.Fatalf("SQLite schema init failed: %v", err)
}
loadState()
}

func loadState() {
var payload string
err := db.QueryRow("SELECT payload FROM device_shadow WHERE id = 1").Scan(&payload)
if err == sql.ErrNoRows {
log.Println("No saved state, using defaults")
return
}
if err != nil {
log.Printf("WARN: loadState: %v", err)
return
}
var s DeviceShadow
if json.Unmarshal([]byte(payload), &s) != nil {
return
}
shadowMutex.Lock()
currentShadow = s
shadowMutex.Unlock()
log.Println("Restored state from SQLite")
}

func saveState() {
shadowMutex.RLock()
data, _ := json.Marshal(currentShadow)
shadowMutex.RUnlock()
db.Exec("INSERT OR REPLACE INTO device_shadow (id, payload, updated_at) VALUES (1, ?, CURRENT_TIMESTAMP)", string(data))
}

// ─── MQTT ───

func initMQTT() {
mqttOpts = mqtt.NewClientOptions().
AddBroker(mqttBrokerURL()).
SetClientID("lumina_backend_" + fmt.Sprint(time.Now().UnixNano())).
SetCleanSession(true).
SetKeepAlive(30 * time.Second).
SetPingTimeout(10 * time.Second).
SetConnectRetry(true).
SetConnectRetryInterval(5 * time.Second).
SetMaxReconnectInterval(30 * time.Second).
SetAutoReconnect(true)

if user := os.Getenv("MQTT_USERNAME"); user != "" {
mqttOpts.SetUsername(user)
mqttOpts.SetPassword(os.Getenv("MQTT_PASSWORD"))
}

mqttOpts.SetDefaultPublishHandler(func(client mqtt.Client, msg mqtt.Message) {
log.Printf("[MQTT <-] %s: %s", msg.Topic(), string(msg.Payload()))
if msg.Topic() != "device/up" {
return
}
var incoming DeviceShadow
if json.Unmarshal(msg.Payload(), &incoming) != nil {
return
}
shadowMutex.Lock()
mergeShadow(&currentShadow, &incoming)
shadowMutex.Unlock()
saveState()
shadowMutex.RLock()
broadcastToWS(currentShadow)
shadowMutex.RUnlock()
})

mqttOpts.OnConnect = func(c mqtt.Client) {
log.Println("[MQTT] Connected, subscribing device/up")
c.Subscribe("device/up", 0, nil)
}
mqttOpts.OnConnectionLost = func(c mqtt.Client, err error) {
log.Printf("[MQTT] Lost: %v", err)
}
mqttOpts.OnReconnecting = func(c mqtt.Client, opts *mqtt.ClientOptions) {
log.Println("[MQTT] Reconnecting...")
}

mqttClient = mqtt.NewClient(mqttOpts)
go func() {
if token := mqttClient.Connect(); token.Wait() && token.Error() != nil {
log.Printf("[MQTT] Initial connect failed: %v (auto-retry enabled)", token.Error())
}
}()
}

func mergeShadow(current, incoming *DeviceShadow) {
if incoming.Mode != "" { current.Mode = incoming.Mode }
if incoming.Power != "" { current.Power = incoming.Power }
// 0 是合法值(关机/夜间)，不能跳过
current.BlindAngle = incoming.BlindAngle
current.LightLevel = incoming.LightLevel
current.Lux = incoming.Lux
current.Human = incoming.Human
if incoming.Timestamp != 0 { current.Timestamp = incoming.Timestamp }
if incoming.Temperature != 0 { current.Temperature = incoming.Temperature }
if incoming.Humidity != 0 { current.Humidity = incoming.Humidity }
if incoming.CO2 != 0 { current.CO2 = incoming.CO2 }
if incoming.OutdoorTemp != 0 { current.OutdoorTemp = incoming.OutdoorTemp }
if incoming.OutdoorWeather != "" { current.OutdoorWeather = incoming.OutdoorWeather }
if incoming.HvacMode != "" { current.HvacMode = incoming.HvacMode }
if incoming.HvacSetpoint != 0 { current.HvacSetpoint = incoming.HvacSetpoint }
if incoming.HvacFan != "" { current.HvacFan = incoming.HvacFan }
current.DoorClosed = incoming.DoorClosed
current.WindowLivingClosed = incoming.WindowLivingClosed
current.WindowKitchenClosed = incoming.WindowKitchenClosed
if incoming.CamerasOnline != 0 { current.CamerasOnline = incoming.CamerasOnline }
}

// ─── WebSocket ───

func wsHandler(c *gin.Context) {
conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
if err != nil {
return
}

pongWait := 60 * time.Second
conn.SetReadDeadline(time.Now().Add(pongWait))
conn.SetPongHandler(func(string) error {
conn.SetReadDeadline(time.Now().Add(pongWait))
return nil
})

pingPeriod := 30 * time.Second
pingTicker := time.NewTicker(pingPeriod)
defer pingTicker.Stop()
done := make(chan struct{})
go func() {
for {
select {
case <-pingTicker.C:
if conn.WriteControl(websocket.PingMessage, []byte{}, time.Now().Add(10*time.Second)) != nil {
return
}
case <-done:
return
}
}
}()

defer conn.Close()
defer close(done)

wsMutex.Lock()
wsClients[conn] = true
wsMutex.Unlock()

shadowMutex.RLock()
initData, _ := json.Marshal(currentShadow)
shadowMutex.RUnlock()
conn.WriteMessage(websocket.TextMessage, initData)

log.Printf("[WS] Connected (total=%d)", len(wsClients))

for {
_, msg, err := conn.ReadMessage()
if err != nil {
if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseNormalClosure) {
log.Printf("[WS] Unexpected close: %v", err)
}
break
}
log.Printf("[WS <-] %s", string(msg))

var cmd DeviceShadow
if json.Unmarshal(msg, &cmd) != nil {
continue
}
cmd.Timestamp = time.Now().Unix()

shadowMutex.Lock()
mergeShadow(&currentShadow, &cmd)
shadowMutex.Unlock()

saveState()

if payload, err := json.Marshal(cmd); err == nil && mqttClient.IsConnected() {
mqttClient.Publish("device/down", 0, false, payload)
}
shadowMutex.RLock()
broadcastToWS(currentShadow)
shadowMutex.RUnlock()
}

wsMutex.Lock()
delete(wsClients, conn)
wsMutex.Unlock()
log.Printf("[WS] Disconnected (total=%d)", len(wsClients))
}

func broadcastToWS(shadow DeviceShadow) {
data, _ := json.Marshal(shadow)
wsMutex.Lock()
defer wsMutex.Unlock()
for client := range wsClients {
client.SetWriteDeadline(time.Now().Add(10 * time.Second))
if client.WriteMessage(websocket.TextMessage, data) != nil {
client.Close()
delete(wsClients, client)
}
}
}

// ─── Health ───

func healthHandler(c *gin.Context) {
mqttOK := mqttClient != nil && mqttClient.IsConnected()
c.JSON(http.StatusOK, gin.H{
"status":     "ok",
"mqtt":       mqttOK,
"ws_clients": len(wsClients),
"time":       time.Now().Unix(),
})
}

func main() {
log.SetFlags(log.LstdFlags | log.Lshortfile)
log.Println("Lumina Home Backend starting...")
initDB()
defer db.Close()
initMQTT()
gin.SetMode(gin.ReleaseMode)
r := gin.Default()
r.GET("/ws", wsHandler)
r.GET("/health", healthHandler)
port := getEnv("PORT", "8080")
log.Printf("Listening on :%s", port)
log.Fatal(r.Run(":" + port))
}