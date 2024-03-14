package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"time"
	"unicode"

	"github.com/antchfx/htmlquery"
	mqtt "github.com/eclipse/paho.mqtt.golang"
)

func removeSpace(s string) string {
	rr := make([]rune, 0, len(s))
	for _, r := range s {
		if !unicode.IsSpace(r) {
			rr = append(rr, r)
		}
	}
	return string(rr)
}

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
	if msg.Topic() == "matrix_display/brightness" {
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
	opts := mqtt.NewClientOptions().AddBroker("tcp://10.52.0.2:1883").SetClientID("bluestar-parser")
	opts.SetKeepAlive(2 * time.Second)
	opts.SetDefaultPublishHandler(f)
	opts.SetPingTimeout(1 * time.Second)
	opts.SetUsername("mqttuser")
	opts.SetPassword("mqttpassword")

	c := mqtt.NewClient(opts)
	if token := c.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}
	if token := c.Subscribe("matrix_display/brightness", 1, nil); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	var extractionRe = regexp.MustCompile(`Destination - (.*?)\. Arrival time - (.*?)\.`)

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

			fmt.Printf("Brightness %\n", brightness)
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
