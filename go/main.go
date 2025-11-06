package main //madebybunnyrce

/*
 * INSEN Controller Client - Go Example
 * //madebybunnyrce
 * This example demonstrates how to connect to and interact with
 * the INSEN controller system using Go.
 * //madebybunnyrce
 * DEPENDENCIES:
 * This code requires the external package "go.bug.st/serial" for serial communication.
 * //madebybunnyrce
 * To install dependencies, run:
 *   go mod init insen-controller-client
 *   go get go.bug.st/serial@v1.6.2
 * //madebybunnyrce
 */

import ( //madebybunnyrce
	"bufio"   //madebybunnyrce
	"fmt"     //madebybunnyrce
	"log"     //madebybunnyrce
	"runtime" //madebybunnyrce
	"strconv" //madebybunnyrce
	"strings" //madebybunnyrce
	"sync"    //madebybunnyrce
	"time"    //madebybunnyrce

	"go.bug.st/serial" //madebybunnyrce
) //madebybunnyrce

// ControllerState represents the state of a controller
type ControllerState struct {
	ID           int       `json:"id"`
	LeftStickX   int       `json:"left_stick_x"`
	LeftStickY   int       `json:"left_stick_y"`
	RightStickX  int       `json:"right_stick_x"`
	RightStickY  int       `json:"right_stick_y"`
	LeftTrigger  int       `json:"left_trigger"`
	RightTrigger int       `json:"right_trigger"`
	Buttons      uint16    `json:"buttons"`
	DPad         uint8     `json:"dpad"`
	Battery      uint8     `json:"battery"`
	Timestamp    time.Time `json:"timestamp"`
}

// GetButtonNames returns a slice of pressed button names
func (cs *ControllerState) GetButtonNames() []string {
	buttonMap := map[uint16]string{
		0x01: "A", 0x02: "B", 0x04: "X", 0x08: "Y",
		0x10: "LB", 0x20: "RB", 0x40: "SELECT", 0x80: "START",
		0x100: "HOME", 0x200: "LSB", 0x400: "RSB",
	}

	var pressedButtons []string
	for mask, name := range buttonMap {
		if cs.Buttons&mask != 0 {
			pressedButtons = append(pressedButtons, name)
		}
	}
	return pressedButtons
}

// InsenController represents the INSEN controller client
type InsenController struct {
	port          serial.Port // External dependency
	portName      string
	baudRate      int
	isConnected   bool
	controllers   map[int]*ControllerState
	controllerMux sync.RWMutex
	monitoring    bool
	monitorMux    sync.Mutex
	inputCallback func(*ControllerState)
}

// NewInsenController creates a new INSEN controller client
func NewInsenController(portName string, baudRate int) *InsenController {
	return &InsenController{
		portName:    portName,
		baudRate:    baudRate,
		controllers: make(map[int]*ControllerState),
	}
}

// Connect establishes connection to the INSEN device
func (ic *InsenController) Connect() error {
	mode := &serial.Mode{
		BaudRate: ic.baudRate,
		Parity:   serial.NoParity,
		DataBits: 8,
		StopBits: serial.OneStopBit,
	}

	port, err := serial.Open(ic.portName, mode)
	if err != nil {
		return fmt.Errorf("failed to open port %s: %v", ic.portName, err)
	}

	ic.port = port
	ic.isConnected = true

	fmt.Printf("Connected to INSEN device on %s\n", ic.portName)

	// Set read timeout
	if err := ic.port.SetReadTimeout(time.Second); err != nil {
		log.Printf("Warning: Could not set read timeout: %v", err)
	}

	time.Sleep(100 * time.Millisecond)
	if err := ic.GetDeviceInfo(); err != nil {
		log.Printf("Warning: Failed to get device info: %v", err)
	}
	return nil
}

// Disconnect closes the connection to the device
func (ic *InsenController) Disconnect() error {
	ic.StopMonitoring()

	if ic.isConnected && ic.port != nil {
		err := ic.port.Close()
		ic.isConnected = false
		fmt.Println("Disconnected from INSEN device")
		return err
	}
	return nil
}

// SendCommand sends a command to the device and returns the response
func (ic *InsenController) SendCommand(command string) (string, error) {
	if !ic.isConnected {
		return "", fmt.Errorf("device not connected")
	}

	fullCommand := command + "\r\n"
	n, err := ic.port.Write([]byte(fullCommand))
	if err != nil {
		return "", fmt.Errorf("failed to write command: %v", err)
	}
	if n != len(fullCommand) {
		return "", fmt.Errorf("incomplete write: wrote %d of %d bytes", n, len(fullCommand))
	}

	reader := bufio.NewReader(ic.port)
	response, err := reader.ReadString('\n')
	if err != nil {
		return "", fmt.Errorf("failed to read response: %v", err)
	}

	return strings.TrimSpace(response), nil
}

// ParseControllerInput parses controller input response
func (ic *InsenController) ParseControllerInput(response string) (*ControllerState, error) {
	if !strings.HasPrefix(response, ">>> ") {
		return nil, fmt.Errorf("invalid response format")
	}

	data := response[4:]
	parts := strings.Split(data, "|")

	if len(parts) < 8 || parts[0] != "INPUT" {
		return nil, fmt.Errorf("failed to parse controller input")
	}

	state := &ControllerState{}
	id, err := strconv.Atoi(parts[1])
	if err != nil {
		return nil, fmt.Errorf("invalid controller ID: %v", err)
	}
	state.ID = id

	leftCoords := strings.Split(parts[2], ",")
	if len(leftCoords) == 2 {
		state.LeftStickX, _ = strconv.Atoi(leftCoords[0])
		state.LeftStickY, _ = strconv.Atoi(leftCoords[1])
	}

	rightCoords := strings.Split(parts[3], ",")
	if len(rightCoords) == 2 {
		state.RightStickX, _ = strconv.Atoi(rightCoords[0])
		state.RightStickY, _ = strconv.Atoi(rightCoords[1])
	}

	triggerVals := strings.Split(parts[4], ",")
	if len(triggerVals) == 2 {
		state.LeftTrigger, _ = strconv.Atoi(triggerVals[0])
		state.RightTrigger, _ = strconv.Atoi(triggerVals[1])
	}

	buttonsStr := strings.TrimPrefix(parts[5], "0x")
	buttons, err := strconv.ParseUint(buttonsStr, 16, 16)
	if err != nil {
		return nil, fmt.Errorf("invalid buttons format: %v", err)
	}
	state.Buttons = uint16(buttons)

	dpad, _ := strconv.ParseUint(parts[6], 10, 8)
	state.DPad = uint8(dpad)

	battery, _ := strconv.ParseUint(parts[7], 10, 8)
	state.Battery = uint8(battery)
	state.Timestamp = time.Now()

	ic.controllerMux.Lock()
	ic.controllers[state.ID] = state
	ic.controllerMux.Unlock()

	return state, nil
}

// GetDeviceInfo retrieves device information
func (ic *InsenController) GetDeviceInfo() error {
	response, err := ic.SendCommand("INFO")
	if err != nil {
		return err
	}
	fmt.Printf("Device Info: %s\n", response)
	return nil
}

// GetStatus retrieves device status
func (ic *InsenController) GetStatus() error {
	response, err := ic.SendCommand("STATUS")
	if err != nil {
		return err
	}
	fmt.Printf("Status: %s\n", response)
	return nil
}

// ListControllers lists connected controllers
func (ic *InsenController) ListControllers() error {
	response, err := ic.SendCommand("LIST")
	if err != nil {
		return err
	}
	fmt.Printf("Controllers: %s\n", response)
	return nil
}

// GetControllerInput gets input from specific controller
func (ic *InsenController) GetControllerInput(controllerID int) (*ControllerState, error) {
	response, err := ic.SendCommand(fmt.Sprintf("GET %d", controllerID))
	if err != nil {
		return nil, err
	}
	return ic.ParseControllerInput(response)
}

// SetInputCallback sets the callback function for controller input
func (ic *InsenController) SetInputCallback(callback func(*ControllerState)) {
	ic.inputCallback = callback
}

// StartMonitoring starts continuous input monitoring
func (ic *InsenController) StartMonitoring(controllerID int, fps int) {
	ic.monitorMux.Lock()
	if ic.monitoring {
		ic.monitorMux.Unlock()
		fmt.Println("Monitoring already active")
		return
	}
	ic.monitoring = true
	ic.monitorMux.Unlock()

	interval := time.Duration(1000/fps) * time.Millisecond

	go func() {
		defer func() {
			if r := recover(); r != nil {
				log.Printf("Monitor goroutine panic: %v", r)
			}
		}()

		for {
			ic.monitorMux.Lock()
			monitoring := ic.monitoring
			ic.monitorMux.Unlock()

			if !monitoring {
				break
			}

			state, err := ic.GetControllerInput(controllerID)
			if err == nil && state != nil && ic.inputCallback != nil {
				ic.inputCallback(state)
			} else if err != nil {
				log.Printf("Error getting controller input: %v", err)
			}

			time.Sleep(interval)
		}
	}()

	fmt.Printf("Started monitoring controller %d at %d FPS\n", controllerID, fps)
}

// StopMonitoring stops input monitoring
func (ic *InsenController) StopMonitoring() {
	ic.monitorMux.Lock()
	defer ic.monitorMux.Unlock()
	if ic.monitoring {
		ic.monitoring = false
		fmt.Println("Stopped monitoring")
	}
}

// GetControllers returns a copy of the current controllers map
func (ic *InsenController) GetControllers() map[int]*ControllerState {
	ic.controllerMux.RLock()
	defer ic.controllerMux.RUnlock()

	controllers := make(map[int]*ControllerState)
	for id, state := range ic.controllers {
		stateCopy := *state
		controllers[id] = &stateCopy
	}
	return controllers
}

// Example callback function
func exampleCallback(state *ControllerState) {
	significantInput := abs(state.LeftStickX) > 5000 ||
		abs(state.LeftStickY) > 5000 ||
		abs(state.RightStickX) > 5000 ||
		abs(state.RightStickY) > 5000 ||
		state.Buttons != 0

	if significantInput {
		pressedButtons := state.GetButtonNames()
		fmt.Printf("Controller %d: L:(%6d,%6d) R:(%6d,%6d) Buttons: %v Battery: %d%%\n",
			state.ID,
			state.LeftStickX, state.LeftStickY,
			state.RightStickX, state.RightStickY,
			pressedButtons,
			state.Battery)
	}
}

// abs helper
func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}

func main() {
	fmt.Println("INSEN Controller Client - Go Example")

	var portName string
	switch runtime.GOOS {
	case "windows":
		portName = "COM3"
	case "darwin":
		portName = "/dev/tty.usbserial-0001"
	default:
		portName = "/dev/ttyUSB0"
	}

	controller := NewInsenController(portName, 115200)

	if err := controller.Connect(); err != nil {
		log.Fatalf("Failed to connect to device: %v", err)
	}
	defer controller.Disconnect()

	time.Sleep(1 * time.Second)
	if err := controller.GetStatus(); err != nil {
		log.Printf("Failed to get status: %v", err)
	}
	if err := controller.ListControllers(); err != nil {
		log.Printf("Failed to list controllers: %v", err)
	}

	controller.SetInputCallback(exampleCallback)
	controller.StartMonitoring(0, 60)

	fmt.Println("Monitoring controller input for 30 seconds...")
	fmt.Println("Press Ctrl+C to stop early")

	time.Sleep(30 * time.Second)

	fmt.Println("Shutting down...")
	controller.StopMonitoring()
}
