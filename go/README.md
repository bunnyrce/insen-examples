# INSEN Controller Client - Go Example

This Go example demonstrates how to connect to and interact with the INSEN controller system.

## Prerequisites

- Go 1.19 or higher
- Serial port access permissions

## Installation

1. Navigate to the go example directory:
```bash
cd examples/go
```

2. Initialize the module and download dependencies:
```bash
go mod tidy
```

## Usage

1. **Adjust the port name** in `main.go` for your system:
   - Windows: `COM3`, `COM4`, etc.
   - Linux: `/dev/ttyUSB0`, `/dev/ttyACM0`, etc.  
   - macOS: `/dev/tty.usbserial-*`, `/dev/tty.usbmodem*`, etc.

2. **Run the example**:
```bash
go run main.go
```

3. **Build executable**:
```bash
go build -o insen_client main.go
./insen_client
```

## Features

- Cross-platform serial communication using `go.bug.st/serial`
- Concurrent monitoring with goroutines
- Thread-safe controller state management
- Automatic OS detection for default port selection
- Robust error handling and recovery
- Real-time input processing at configurable FPS

## Troubleshooting

### Permission Issues (Linux/macOS)
```bash
sudo usermod -a -G dialout $USER  # Linux
sudo dscl . append /Groups/wheel GroupMembership $(whoami)  # macOS
```
Log out and back in after running.

### Port Not Found
- Check available ports: `ls /dev/tty*` (Unix) or Device Manager (Windows)
- Ensure INSEN device is connected and recognized
- Try different port names based on your system

### Build Issues
- Ensure Go 1.19+ is installed: `go version`
- Clean module cache: `go clean -modcache`
- Re-download dependencies: `go mod download`

## API Usage

```go
// Create controller instance
controller := NewInsenController("COM3", 115200)

// Connect to device
if err := controller.Connect(); err != nil {
    log.Fatal(err)
}
defer controller.Disconnect()

// Set up input callback
controller.SetInputCallback(func(state *ControllerState) {
    fmt.Printf("Input: %+v\n", state)
})

// Start monitoring at 60 FPS
controller.StartMonitoring(0, 60)
```

## Protocol Commands

- `INFO` - Get firmware version and device info
- `STATUS` - Get current device status  
- `LIST` - List connected controllers
- `GET <id>` - Get input from controller ID

## Dependencies

- `go.bug.st/serial v1.6.2` - Cross-platform serial port library