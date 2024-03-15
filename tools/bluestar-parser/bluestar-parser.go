package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/antchfx/htmlquery"
	mqtt "github.com/eclipse/paho.mqtt.golang"
)

type query struct {
	url     string
	towards string
}

type journey struct {
	Towards           string
	DeparturesSummary string
}

var f mqtt.MessageHandler = func(client mqtt.Client, msg mqtt.Message) {
	fmt.Printf("TOPIC: %s\n", msg.Topic())
	fmt.Printf("MSG: %s\n", msg.Payload())
	if msg.Topic() == "matrix_display/panel/brightness/set" {
		brightness, _ = strconv.Atoi(string(msg.Payload()))
	}
}

var brightness int = 0

func downloadURL(url string) (string, error) {
	cmd := exec.Command("curl", "-s", url)
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	err := cmd.Run()
	if err != nil {
		return "", err
	}
	fmt.Println(stderr.String())
	return stdout.String(), nil
}

func main() {
	mqtt.DEBUG = log.New(os.Stdout, "", 0)
	mqtt.ERROR = log.New(os.Stdout, "", 0)

	brokerPtr := flag.String("broker", "", "broker url, such as tcp://IP:1883")
	flag.Parse()

	if *brokerPtr == "" {
		panic("No broker given")
	}

	mqttUsername := os.Getenv("MQTT_USERNAME")
	mqttPassword := os.Getenv("MQTT_PASSWORD")

	if mqttUsername == "" || mqttPassword == "" {
		panic("MQTT_USERNAME and MQTT_PASSWORD must be set in env")
	}

	opts := mqtt.NewClientOptions().AddBroker(*brokerPtr).SetClientID("bluestar-parser")
	opts.SetKeepAlive(2 * time.Second)
	opts.SetDefaultPublishHandler(f)
	opts.SetPingTimeout(1 * time.Second)
	opts.SetUsername(mqttUsername)
	opts.SetPassword(mqttPassword)

	c := mqtt.NewClient(opts)
	if token := c.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}
	if token := c.Subscribe("matrix_display/panel/brightness/set", 1, nil); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	var extractionRe = regexp.MustCompile(`Destination - (.*?)\. Departure time - (.*?)\.`)

	for {
		var queries = []query{
			{
				url:     "https://www.bluestarbus.co.uk/stops/1900HA080331",
				towards: "Hythe",
			},
			{
				url:     "https://www.bluestarbus.co.uk/stops/1900HA080333",
				towards: "So'ton",
			},
		}

		var journies []journey

		for _, q := range queries {
			towards := "To: " + q.towards

			fmt.Printf("Brightness %d\n", brightness)
			if brightness > 0 {
				text, err := downloadURL(q.url)
				if err != nil {
					panic(err)
				}
				doc, err := htmlquery.Parse(strings.NewReader(text))
				if err != nil {
					panic(err)
				}
				// Find all news item.
				list := htmlquery.Find(doc, "/html/body/div/div/div/ol/li/a/p")
				var departures []string

				for i, n := range list {
					m := extractionRe.FindStringSubmatch(htmlquery.InnerText(n))
					shortDeparture := strings.Replace(m[2], " mins", "m", -1)
					if len(m) > 0 && i < 3 {
						departures = append(departures, shortDeparture)
					}
				}
				journies = append(journies, journey{
					Towards:           towards,
					DeparturesSummary: strings.Join(departures, " "),
				})
			} else {
				journies = append(journies, journey{
					Towards:           towards,
					DeparturesSummary: "Unknown",
				})
			}
		}
		b, _ := json.Marshal(journies)
		token := c.Publish("matrix_display/transport", 0, true, b)
		token.Wait()

		time.Sleep(150 * time.Second)
	}
}
