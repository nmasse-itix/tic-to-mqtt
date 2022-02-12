package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"reflect"
	"strings"
	"sync"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	diff "github.com/r3labs/diff/v2"
	"go.bug.st/serial"
	yaml "gopkg.in/yaml.v3"
)

const (
	MQTT_QOS_0 = 0
	MQTT_QOS_1 = 1
	MQTT_QOS_2 = 2
)

type TicData struct {
	Value     string `json:"val"`
	Timestamp int64  `json:"ts"`
}

func main() {
	//mqtt.ERROR = log.New(os.Stdout, "[ERROR] ", 0)
	//mqtt.CRITICAL = log.New(os.Stdout, "[CRIT] ", 0)
	//mqtt.WARN = log.New(os.Stdout, "[WARN]  ", 0)
	//mqtt.DEBUG = log.New(os.Stdout, "[DEBUG]  ", 0)

	var portName string
	var mqttUri string
	var mqttUsername string
	var mqttPassword string
	var mqttClientId string
	var mqttClient mqtt.Client

	flag.StringVar(&portName, "p", "/dev/ttyUSB1", "Serial port to use")
	flag.StringVar(&mqttUri, "s", "", "MQTT Server URI (tcp://server:port or ssl://server:port)")
	flag.StringVar(&mqttUsername, "u", "", "MQTT Username")
	flag.StringVar(&mqttPassword, "w", "", "MQTT Password")
	flag.StringVar(&mqttClientId, "c", "esptic-e2e-test", "MQTT Client ID")
	flag.Parse()

	cases := flag.Args()
	if len(cases) == 0 || len(cases) > 1 {
		fmt.Println("Usage: e2e [options] test_case")
		fmt.Println()
		fmt.Println("Options:")
		flag.PrintDefaults()
		fmt.Println()
		fmt.Println("Available test cases:")
		for _, testCase := range testCases {
			fmt.Printf("- %s\n", testCase.Name)
		}
		os.Exit(1)
	}
	testcaseName := cases[0]

	if mqttUri != "" {
		opts := mqtt.NewClientOptions()
		opts.AddBroker(mqttUri)
		opts.SetAutoReconnect(false)
		opts.SetConnectRetry(false)
		opts.SetOrderMatters(false)
		opts.SetCleanSession(true)
		opts.SetClientID(mqttClientId)
		if mqttUsername != "" {
			opts.SetUsername(mqttUsername)
		}
		if mqttPassword != "" {
			opts.SetPassword(mqttPassword)
		}
		mqttClient = mqtt.NewClient(opts)
		fmt.Printf("Connecting to MQTT server %s...\n", mqttUri)
		ct := mqttClient.Connect()
		if !ct.WaitTimeout(30 * time.Second) {
			fmt.Println("Timeout waiting for MQTT server!")
			os.Exit(1)
		}
		if ct.Error() != nil {
			fmt.Println(ct.Error())
			os.Exit(1)
		}
	}

	for _, testCase := range testCases {
		var mut sync.Mutex
		if testCase.Name != testcaseName {
			continue
		}

		fmt.Printf("Running test case %s...\n", testCase.Name)

		var mode serial.Mode
		var modeName string
		if testCase.Mode == TIC_MODE_HISTORIQUE {
			mode = serial.Mode{
				BaudRate: 1200,
				DataBits: 7,
				Parity:   serial.EvenParity,
				StopBits: serial.OneStopBit,
			}
			modeName = "historique"
		} else if testCase.Mode == TIC_MODE_STANDARD {
			mode = serial.Mode{
				BaudRate: 9600,
				DataBits: 7,
				Parity:   serial.EvenParity,
				StopBits: serial.OneStopBit,
			}
			modeName = "standard"
		} else {
			panic("Unknown mode")
		}

		fmt.Printf("Opening port %s with mode %s...\n", portName, modeName)

		port, err := serial.Open(portName, &mode)
		if err != nil {
			panic(err)
		}
		defer port.Close()

		var mqttObservedValues map[string][]string = make(map[string][]string, len(testCase.Expected))
		if mqttUri != "" {
			var topics map[string]byte = make(map[string]byte, len(testCase.Expected))
			for topic := range testCase.Expected {
				topics[topic] = MQTT_QOS_1
			}
			st := mqttClient.SubscribeMultiple(topics, func(c mqtt.Client, m mqtt.Message) {
				if m.Retained() {
					return
				}
				var data TicData
				err := json.Unmarshal(m.Payload(), &data)
				if err != nil {
					fmt.Printf("Cannot unmarshal JSON payload '%s'\n", string(m.Payload()))
					return
				}

				mut.Lock()
				defer mut.Unlock()
				values, ok := mqttObservedValues[m.Topic()]
				if ok {
					mqttObservedValues[m.Topic()] = append(values, data.Value)
				} else {
					mqttObservedValues[m.Topic()] = []string{data.Value}
				}
			})
			st.Wait() // Happy path: there won't be a timeout...
		}

		for i, frame := range testCase.Sent {
			fmt.Printf("Sending trame %d...\n", i)
			var b bytes.Buffer
			b.WriteByte(0x02)
			for _, info := range frame {
				b.WriteString(fmt.Sprintf("\n%s\r", info))
			}
			b.WriteByte(0x03)
			buffer := b.Bytes()
			n, err := port.Write(buffer)
			if err != nil {
				panic(err)
			}
			if n != len(buffer) {
				panic("Cannot send bytes to serial port!")
			}
			// Can be any value between 16.7 and 33.4 ms
			time.Sleep(33 * time.Millisecond)
		}

		fmt.Println("Waiting for MQTT messages...")
		success := false
		for i := 0; i < 15; i++ {
			mut.Lock()
			ok := reflect.DeepEqual(mqttObservedValues, testCase.Expected)
			mut.Unlock()
			if ok {
				success = true
				break
			}

			// there may be ongoing MQTT messages waiting to be sent/delivered
			// wait one second and try again
			time.Sleep(time.Second)
		}

		if !success {
			changelog, err := diff.Diff(testCase.Expected, mqttObservedValues)
			if err != nil {
				fmt.Println(err)
				os.Exit(1)
			}
			fmt.Println("TEST FAILED!")
			fmt.Println()
			fmt.Println("Changelog:")
			fmt.Println()
			for _, change := range changelog {
				fmt.Printf("%s: %s: %s -> %s\n", change.Type, strings.Join(change.Path, "."), str(change.From), str(change.To))
			}
			fmt.Println()
			fmt.Println("Expected:")
			b, _ := yaml.Marshal(testCase.Expected)
			fmt.Println(string(b))
			fmt.Println()
			fmt.Println("Got:")
			b, _ = yaml.Marshal(mqttObservedValues)
			fmt.Println(string(b))
			fmt.Println()
			os.Exit(1)
		}

		if mqttUri != "" {
			var topics []string = make([]string, len(testCase.Expected))
			i := 0
			for topic := range testCase.Expected {
				topics[i] = topic
				i++
			}
			ut := mqttClient.Unsubscribe(topics...)
			ut.Wait() // Happy path: there won't be a timeout...
		}

		fmt.Println("TEST SUCCEEDED!")

		break
	}

	if mqttClient.IsConnected() {
		mqttClient.Disconnect(0)
	}

	fmt.Println("Done.")
}

func str(s interface{}) string {
	if s == nil {
		return "nil"
	}

	return fmt.Sprintf("%s", s)
}
