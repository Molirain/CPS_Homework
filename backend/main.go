package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"sync"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

// DeviceShadow 定义核心设备影子状态模型
type DeviceShadow struct {
	Mode       string  `json:"mode"`        // "auto" 或 "manual"
	Power      string  `json:"power"`       // "on" 或 "off"
	BlindAngle int     `json:"blind_angle"` // 0-180
	LightLevel int     `json:"light_level"` // 0, 1, 2, 3
	Lux        float64 `json:"lux"`         // 实时光照度
	Human      bool    `json:"human"`       // 实时人体检测
	Timestamp  int64   `json:"timestamp"`
}

var (
	// 全局设备状态与互斥锁
	currentShadow DeviceShadow
	shadowMutex   sync.RWMutex

	// global MQTT client
	mqttClient mqtt.Client

	// WebSocket 升级器（允许所有跨域请求以便开发调试）
	upgrader = websocket.Upgrader{
		CheckOrigin: func(r *http.Request) bool {
			return true
		},
	}

	// 记录所有活跃的 WS 连接，用于广播
	wsClients = make(map[*websocket.Conn]bool)
	wsMutex   sync.Mutex
)

func main() {
	// 1. 初始化并连接 MQTT
	initMQTT()

	// 2. 初始化 Gin 引擎
	r := gin.Default()

	// 3. 注册 WebSocket 路由
	r.GET("/ws", wsHandler)

	// 4. 启动 HTTP 服务
	fmt.Println("Server is running on :8080")
	if err := r.Run(":8080"); err != nil {
		log.Fatal("Server failed: ", err)
	}
}

// initMQTT 初始化 MQTT 连接并监听设备上报消息
func initMQTT() {
	opts := mqtt.NewClientOptions().
		AddBroker("tcp://localhost:1883").
		SetClientID("golang_backend_server").
		SetCleanSession(true)

	// 收到 device/up 消息时的回调函数
	opts.SetDefaultPublishHandler(func(client mqtt.Client, msg mqtt.Message) {
		log.Printf("Received MQTT message on %s: %s\n", msg.Topic(), msg.Payload())
		
		var updatedShadow DeviceShadow
		if err := json.Unmarshal(msg.Payload(), &updatedShadow); err != nil {
			log.Println("MQTT payload unmarshal error:", err)
			return
		}

		// 更新全局状态
		shadowMutex.Lock()
		currentShadow = updatedShadow
		shadowMutex.Unlock()

		// 向所有前端 Web UI 广播最新状态
		broadcastToWS(updatedShadow)
	})

	opts.OnConnect = func(c mqtt.Client) {
		log.Println("MQTT Connected, Subscribing to device/up...")
		if token := c.Subscribe("device/up", 0, nil); token.Wait() && token.Error() != nil {
			log.Println("Subscribe error:", token.Error())
		}
	}

	mqttClient = mqtt.NewClient(opts)
	if token := mqttClient.Connect(); token.Wait() && token.Error() != nil {
		log.Printf("MQTT Connect error: %v (Waiting for broker...)\n", token.Error())
	}
}

// wsHandler 处理前端的 WebSocket 连接
func wsHandler(c *gin.Context) {
	conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		log.Println("WebSocket Upgrade error:", err)
		return
	}
	defer conn.Close()

	// 注册新连接并下发当前状态
	wsMutex.Lock()
	wsClients[conn] = true
	wsMutex.Unlock()

	shadowMutex.RLock()
	initData, _ := json.Marshal(currentShadow)
	shadowMutex.RUnlock()
	
	// 连接成功后先推送一次最新状态
	_ = conn.WriteMessage(websocket.TextMessage, initData)

	// 监听前端发来的指令控制
	for {
		_, msg, err := conn.ReadMessage()
		if err != nil {
			log.Println("WebSocket Read error/Close:", err)
			wsMutex.Lock()
			delete(wsClients, conn)
			wsMutex.Unlock()
			break
		}

		log.Printf("Received WS control message: %s\n", msg)

		var cmdShadow DeviceShadow
		if err := json.Unmarshal(msg, &cmdShadow); err != nil {
			log.Println("WS payload error:", err)
			continue
		}

		// 补齐最新时间戳
		cmdShadow.Timestamp = time.Now().Unix()

		shadowMutex.Lock()
		currentShadow = cmdShadow
		shadowMutex.Unlock()

		// 序列化后通过 MQTT 下发给设备端
		payload, _ := json.Marshal(cmdShadow)
		if mqttClient.IsConnected() {
			mqttClient.Publish("device/down", 0, false, payload)
		}

		// 也需将最新状态广播给其他所有可能的已连接的前端 Web UI，保持多设备同步
		broadcastToWS(cmdShadow)
	}
}

// broadcastToWS 向所有活跃的 WS 连接广播 JSON 状态
func broadcastToWS(shadow DeviceShadow) {
	data, err := json.Marshal(shadow)
	if err != nil {
		return
	}

	wsMutex.Lock()
	defer wsMutex.Unlock()
	for client := range wsClients {
		if err := client.WriteMessage(websocket.TextMessage, data); err != nil {
			log.Println("WS Broadcast error:", err)
			client.Close()
			delete(wsClients, client)
		}
	}
}