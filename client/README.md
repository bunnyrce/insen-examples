# INSEN Client Examples

This directory contains client libraries and examples for communicating with the INSEN ESP32-S3 USB Host MCU firmware.

## Overview

The INSEN Controller Passthrough System provides a serial API for external applications to communicate with the ESP32-S3 USB Host MCU. This allows you to:

- Read controller input in real-time
- Monitor system status
- Get firmware information
- List connected controllers

## Available Clients

### C Client Library (`insen_client.c/h`)

A complete C library for communicating with INSEN firmware.

**Features:**
- Cross-platform serial communication (Linux, macOS, Windows)
- Full API coverage (INFO, STATUS, LIST, GET commands)
- Error handling and timeout management
- Structured data types for all responses
- Debug utilities

**Building:**
```bash
# Build example program
make

# Build library only  
make lib

# Install system-wide (Unix)
sudo make install

# Debug build
make debug
```

**Usage:**
```bash
# Linux/macOS
./insen_example /dev/ttyUSB0

# Windows
./insen_example.exe COM3
```

### Go Client Example (`main.go`)

A Go implementation demonstrating controller monitoring.

**Note:** This example requires the `go.bug.st/serial` package. To use:

```bash
# Initialize Go module
go mod init insen-client
go get go.bug.st/serial

# Run example
go run main.go /dev/ttyUSB0
```

## API Reference

The INSEN firmware supports the following serial commands:

### INFO
Returns firmware information.
```
> INFO
< INSEN_FW_V1.2.0|BUILD_Dec_15_2024_14:30:25|MAKCU_COMPATIBLE|STATUS_OK
```

### STATUS  
Returns system status.
```
> STATUS
< STATUS|ACTIVE_1|TOTAL_INPUTS_12345|API_COMMANDS_67|FREE_HEAP_234567
```

### LIST
Lists connected controllers.
```
> LIST
< CONTROLLERS|0_XBOX_ONE|1_PS4
```

### GET <id>
Gets controller input for specified ID.
```
> GET 0
< INPUT|0|-1234,5678|890,-2345|128,64|0x000F|3|85|1234567
```

Response format:
- Controller ID
- Left stick X,Y
- Right stick X,Y  
- Left trigger, Right trigger
- Button bitmask (hex)
- D-pad state
- Battery level
- Timestamp

### VERSION
Returns firmware version.
```
> VERSION
< VERSION|1.2.0|MAKCU_COMPATIBLE
```

### HELP
Lists available commands.
```
> HELP
< COMMANDS|INFO,STATUS,LIST,GET,HELP,RESET,VERSION
```

## Button Bitmask

The button bitmask uses the following bit assignments:

```c
#define INSEN_BTN_A           (1 << 0)   // 0x0001
#define INSEN_BTN_B           (1 << 1)   // 0x0002
#define INSEN_BTN_X           (1 << 2)   // 0x0004
#define INSEN_BTN_Y           (1 << 3)   // 0x0008
#define INSEN_BTN_LB          (1 << 4)   // 0x0010
#define INSEN_BTN_RB          (1 << 5)   // 0x0020
#define INSEN_BTN_SELECT      (1 << 6)   // 0x0040
#define INSEN_BTN_START       (1 << 7)   // 0x0080
#define INSEN_BTN_HOME        (1 << 8)   // 0x0100
#define INSEN_BTN_LSB         (1 << 9)   // 0x0200
#define INSEN_BTN_RSB         (1 << 10)  // 0x0400
#define INSEN_BTN_TOUCHPAD    (1 << 11)  // 0x0800
#define INSEN_BTN_MUTE        (1 << 12)  // 0x1000
```

## D-Pad States

```c
#define INSEN_DPAD_NEUTRAL    0
#define INSEN_DPAD_UP         1
#define INSEN_DPAD_UP_RIGHT   2
#define INSEN_DPAD_RIGHT      3
#define INSEN_DPAD_DOWN_RIGHT 4
#define INSEN_DPAD_DOWN       5
#define INSEN_DPAD_DOWN_LEFT  6
#define INSEN_DPAD_LEFT       7
#define INSEN_DPAD_UP_LEFT    8
```

## Error Handling

The C library uses the following error codes:

```c
typedef enum {
    INSEN_SUCCESS = 0,
    INSEN_ERROR_INVALID_PARAM = -1,
    INSEN_ERROR_PORT_OPEN = -2,
    INSEN_ERROR_WRITE = -3,
    INSEN_ERROR_READ = -4,
    INSEN_ERROR_TIMEOUT = -5,
    INSEN_ERROR_INVALID_RESPONSE = -6,
    INSEN_ERROR_CONTROLLER_DISCONNECTED = -7
} insen_error_t;
```

Use `insen_get_error_string()` to get human-readable error messages.

## Example Output

```
Connecting to INSEN USB Host MCU on /dev/ttyUSB0...
✓ Connected successfully!

=== INSEN Firmware Information ===
INSEN Firmware Information:
  Version: 1.2.0
  Build Date: Dec 15 2024 14:30:25
  MAKCU Compatible: Yes
  Status: OK

=== System Status ===
Active Controllers: 1
Total Inputs: 12345
API Commands: 67
Free Heap: 234567 bytes

=== Connected Controllers ===
Controller 0: XBOX_ONE (Connected)

INSEN Controller Monitor - Controller 0 (XBOX_ONE)
================================================
Left Stick:  X= -1234 Y=  5678
Right Stick: X=   890 Y= -2345
Triggers:    L=128     R= 64
Buttons: A B LB 
D-Pad: Down-Right
Battery:     85%
Timestamp:   1234567

Stick Visualization:
Left:  [< o ]
Right: [ o>]
```

## Testing

### Virtual Serial Ports (Linux)

For testing without hardware:

```bash
# Create virtual port pair
make test-virtual

# Use the displayed port names
./insen_example /dev/pts/N
```

### Hardware Testing

1. Connect ESP32-S3 running INSEN firmware to USB
2. Connect Xbox/PlayStation controller to ESP32-S3 USB Host port
3. Identify the serial port (usually `/dev/ttyUSB0` or `COM3`)
4. Run the client example

## Integration

### Using the C Library

```c
#include "insen_client.h"

int main() {
    insen_client_t client;
    
    // Initialize
    if (insen_init(&client, "/dev/ttyUSB0") != INSEN_SUCCESS) {
        return 1;
    }
    
    // Get controller input
    insen_controller_state_t state;
    if (insen_get_controller_input(&client, 0, &state) == INSEN_SUCCESS) {
        printf("Left stick: %d, %d\n", state.left_stick_x, state.left_stick_y);
    }
    
    // Cleanup
    insen_cleanup(&client);
    return 0;
}
```

### Cross-Platform Considerations

- **Linux**: Ports are typically `/dev/ttyUSB0`, `/dev/ttyACM0`
- **macOS**: Ports are typically `/dev/cu.usbserial-*`, `/dev/cu.usbmodem*`  
- **Windows**: Ports are typically `COM1`, `COM3`, etc.

## Dependencies

### C Library
- Standard C library (C99 or later)
- POSIX serial I/O functions (Linux/macOS)
- Windows.h (Windows builds)

### Go Example
- Go 1.19 or later
- `go.bug.st/serial` package

## Building on Different Platforms

### Linux/macOS
```bash
make
```

### Windows (MinGW)
```bash
gcc -o insen_example.exe example.c insen_client.c
```

### Windows (Visual Studio)
```cmd
cl /Fe:insen_example.exe example.c insen_client.c
```

## Troubleshooting

### Common Issues

1. **Port Access Denied**
   - Add user to `dialout` group (Linux)
   - Run as administrator (Windows)

2. **No Response from Device**
   - Check port name and baud rate
   - Verify INSEN firmware is running
   - Check USB connections

3. **Timeout Errors**
   - Increase timeout in client code
   - Check for electrical interference
   - Verify stable power supply

### Debug Mode

Build with debug information:
```bash
make debug
```

This enables additional logging and error checking.

## License

This client library is part of the INSEN Controller Passthrough System.
See the main project README for license information.