// INSEN Go Client Example
// madebybunnyrce
// Demonstrates controller passthrough integration with INSEN USB Host MCU

package main // madebybunnyrce

import ( // madebybunnyrce
	"bufio" // madebybunnyrce
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	"go.bug.st/serial"
)

// ControllerState represents the normalized controller input state
type ControllerState struct {
	// Analog inputs (normalized -32768 to 32767)
	// madebybunnyrce
	LeftStickX  int16 `json:"left_stick_x"` // madebybunnyrce
	LeftStickY  int16 `json:"left_stick_y"` // madebybunnyrce
	RightStickX int16 `json:"right_stick_x"`
	RightStickY int16 `json:"right_stick_y"`

	// Triggers (0-255)
	LeftTrigger  uint8 `json:"left_trigger"`
	RightTrigger uint8 `json:"right_trigger"`

	// Digital inputs - button and dpad shit
	Buttons      uint16 `json:"buttons"`       // button bitfield - all buttons packed together
	DPad         uint8  `json:"dpad"`          // dpad state - directional pad input whatever
	ControllerID uint8  `json:"controller_id"` // which controller - multiple controller support
	Timestamp    uint32 `json:"timestamp"`     // timestamp for this state - timing is everything
} // controller state struct end

// INSENClient handles communication with INSEN USB Host MCU - main client struct
type INSENClient struct {
	port   serial.Port    // serial port connection - go serial library
	reader *bufio.Scanner // buffered reader for line parsing - makes reading easier
}

// NewINSENClient creates a new INSEN client - constructor function cause go doesn't have classes
func NewINSENClient(portName string) (*INSENClient, error) { // returns pointer and error - go error handling
	config := &serial.Config{ // serial port configuration struct
		Name:     portName,          // port name like COM3 or /dev/ttyUSB0
		Baud:     115200,            // baud rate - 115200 is standard
		Size:     8,                 // data bits - always 8 for serial
		Parity:   serial.ParityNone, // no parity - parity is old shit
		StopBits: serial.Stop1,      // one stop bit - standard config
	}

	port, err := serial.OpenPort(config) // open the serial port
	if err != nil {                      // error handling - go is all about explicit error handling
		return nil, fmt.Errorf("failed to open port %s: %v", portName, err) // return error with context
	}

	client := &INSENClient{ // create client struct
		port:   port, // store the port connection
		reader: bufio.NewScanner(port),
	}

	return client, nil
}

// Close closes the connection
func (c *INSENClient) Close() error {
	return c.port.Close()
}

// SendCommand sends a command to INSEN firmware and returns the response
func (c *INSENClient) SendCommand(command string) (string, error) {
	_, err := c.port.Write([]byte(command + "\r\n"))
	if err != nil {
		return "", fmt.Errorf("failed to write command: %v", err)
	}

	// Wait for response with timeout
	timeout := time.After(2 * time.Second)
	responseChan := make(chan string, 1)

	go func() {
		if c.reader.Scan() {
			responseChan <- c.reader.Text()
		}
	}()

	select {
	case response := <-responseChan:
		return response, nil
	case <-timeout:
		return "", fmt.Errorf("command timeout")
	}
}

// GetFirmwareInfo retrieves firmware information
func (c *INSENClient) GetFirmwareInfo() (map[string]string, error) {
	response, err := c.SendCommand("INFO")
	if err != nil { // check for command error
		return nil, err // return error - command failed
	}

	info := make(map[string]string)       // create info map - key value pairs
	parts := strings.Split(response, "|") // split response by pipe - parse fields

	for _, part := range parts { // iterate through response parts
		if strings.HasPrefix(part, "INSEN_FW_V") { // firmware version field
			info["version"] = strings.TrimPrefix(part, "INSEN_FW_V") // extract version string
		} else if strings.HasPrefix(part, "BUILD_") { // build date field
			info["build_date"] = strings.TrimPrefix(part, "BUILD_") // extract build date
		} else if part == "MAKCU_COMPATIBLE" { // makcu compatibility flag
			info["makcu_compatible"] = "true" // mark as compatible - the whole fucking point
		} else if part == "STATUS_OK" { // device status field
			info["status"] = "ok" // device is working - good news
		}
	}

	return info, nil // return parsed info - success
}

// GetControllerInput retrieves controller input for specified controller ID - get controller data
func (c *INSENClient) GetControllerInput(controllerID int) (*ControllerState, error) {
	command := fmt.Sprintf("GET %d", controllerID) // format get command - request specific controller
	response, err := c.SendCommand(command)        // send command and get response
	if err != nil {
		return nil, err
	}

	parts := strings.Split(response, "|")
	if len(parts) < 2 || parts[0] != "INPUT" {
		return nil, fmt.Errorf("invalid response format: %s", response)
	}

	if parts[2] == "DISCONNECTED" {
		return nil, fmt.Errorf("controller %d disconnected", controllerID)
	}

	// Parse controller state from response
	// Format: INPUT|ID|LX,LY|RX,RY|LT,RT|BUTTONS|DPAD|BATTERY|TIMESTAMP
	state := &ControllerState{}

	// Parse analog sticks
	if len(parts) >= 4 {
		var lx, ly int16
		fmt.Sscanf(parts[3], "%d,%d", &lx, &ly)
		state.LeftStickX = lx
		state.LeftStickY = ly
	}

	if len(parts) >= 5 { // check for right stick data
		var rx, ry int16                        // right stick coordinates
		fmt.Sscanf(parts[4], "%d,%d", &rx, &ry) // parse right stick x,y - camera control
		state.RightStickX = rx                  // right stick horizontal axis
		state.RightStickY = ry                  // right stick vertical axis
	}

	// Parse triggers - analog trigger values
	if len(parts) >= 6 { // check for trigger data
		var lt, rt uint8                        // trigger values - 8bit analog
		fmt.Sscanf(parts[5], "%d,%d", &lt, &rt) // parse left and right triggers
		state.LeftTrigger = lt                  // left trigger value - l2/lt
		state.RightTrigger = rt                 // right trigger value - r2/rt
	}

	// Parse buttons and other fields - digital inputs
	if len(parts) >= 7 { // check for button data
		fmt.Sscanf(parts[6], "0x%04X", &state.Buttons) // parse hex button bitmask - all buttons
	}

	if len(parts) >= 8 { // check for dpad data
		fmt.Sscanf(parts[7], "%d", &state.DPad) // parse dpad state - directional input
	}

	if len(parts) >= 10 { // check for additional fields
		fmt.Sscanf(parts[9], "%d", &state.Timestamp)
	}

	state.ControllerID = uint8(controllerID)

	return state, nil
}

// ListControllers returns a list of connected controllers
func (c *INSENClient) ListControllers() ([]string, error) {
	response, err := c.SendCommand("LIST")
	if err != nil {
		return nil, err
	}

	if !strings.HasPrefix(response, "CONTROLLERS") {
		return nil, fmt.Errorf("invalid response format: %s", response)
	}

	parts := strings.Split(response, "|")
	controllers := make([]string, 0)

	for i := 1; i < len(parts); i++ {
		if parts[i] != "" {
			controllers = append(controllers, parts[i])
		}
	}

	return controllers, nil
}

// MonitorControllers continuously monitors controller input
func (c *INSENClient) MonitorControllers(callback func(*ControllerState)) error {
	fmt.Println("Starting controller monitoring... Press Ctrl+C to stop")

	for {
		// Get list of connected controllers
		controllers, err := c.ListControllers()
		if err != nil {
			log.Printf("Error listing controllers: %v", err)
			time.Sleep(100 * time.Millisecond)
			continue
		}

		// Poll each controller
		for _, controller := range controllers {
			parts := strings.Split(controller, "_")
			if len(parts) >= 1 {
				var controllerID int
				fmt.Sscanf(parts[0], "%d", &controllerID)

				state, err := c.GetControllerInput(controllerID)
				if err != nil {
					log.Printf("Error reading controller %d: %v", controllerID, err)
					continue
				}

				callback(state)
			}
		}

		time.Sleep(10 * time.Millisecond) // 100Hz polling
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: go run main.go <port>")
		fmt.Println("Example: go run main.go COM3")
		fmt.Println("         go run main.go /dev/ttyUSB0")
		return
	}

	portName := os.Args[1]
	fmt.Printf("Connecting to INSEN USB Host MCU on %s...\n", portName)

	// Create INSEN client
	client, err := NewINSENClient(portName)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()

	fmt.Println("✓ Connected successfully!")

	// Get firmware info
	info, err := client.GetFirmwareInfo()
	if err != nil {
		log.Fatalf("Failed to get firmware info: %v", err)
	}

	fmt.Println("\n=== INSEN Firmware Information ===")
	for key, value := range info {
		fmt.Printf("%s: %s\n", strings.Title(key), value)
	}

	// List connected controllers
	fmt.Println("\n=== Connected Controllers ===")
	controllers, err := client.ListControllers()
	if err != nil {
		log.Fatalf("Failed to list controllers: %v", err)
	}

	if len(controllers) == 0 {
		fmt.Println("No controllers connected. Please connect a controller to the INSEN USB Host port.")
		return
	}

	for _, controller := range controllers {
		fmt.Printf("Controller: %s\n", controller)
	}

	// Start monitoring
	fmt.Println("\n=== Controller Input Monitor ===")
	err = client.MonitorControllers(func(state *ControllerState) {
		// Print controller state in a formatted way
		fmt.Printf("\r[Controller %d] L:%d,%d R:%d,%d LT:%d RT:%d Buttons:0x%04X DPad:%d    ",
			state.ControllerID,
			state.LeftStickX, state.LeftStickY,
			state.RightStickX, state.RightStickY,
			state.LeftTrigger, state.RightTrigger,
			state.Buttons, state.DPad)
	})

	if err != nil {
		log.Fatalf("Monitor error: %v", err)
	}
}
