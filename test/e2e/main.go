package main

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"sort"
	"time"

	"go.bug.st/serial"
)

func main() {
	var portName string

	flag.StringVar(&portName, "p", "/dev/ttyUSB1", "Serial port to use")
	flag.Parse()

	wanted := flag.Args()
	if len(wanted) == 0 {
		fmt.Println("Usage: e2e [-t /dev/ttyUSBX ] test_case1 test_case2 ...")
		fmt.Println()
		fmt.Println("Available test cases:")
		for _, testCase := range testCases {
			fmt.Printf("- %s\n", testCase.Name)
		}
		os.Exit(1)
	}
	sort.Strings(wanted)
	for _, testCase := range testCases {
		i := sort.SearchStrings(wanted, testCase.Name)
		if i >= len(wanted) || wanted[i] != testCase.Name {
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

		for i, step := range testCase.Steps {
			fmt.Printf("Sending trame %d...\n", i)
			var b bytes.Buffer
			b.WriteByte(0x02)
			for _, info := range step.Sent {
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
	}

	fmt.Println("Done.")
}
