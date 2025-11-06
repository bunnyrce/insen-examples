# INSEN Controller Examples

This directory contains example client implementations for the INSEN Controller system in various programming languages.

## Available Examples

### JavaScript/Node.js (`javascript/`)
- **File**: `insen-client.js`
- **Dependencies**: `serialport` package
- **Setup**: `npm install`
- **Run**: `node insen-client.js`

Features:
- Serial communication with INSEN device
- Real-time controller input monitoring
- Event-driven architecture with callbacks
- Cross-platform compatibility

### Python (`python/`)
- **File**: `insen_client.py` 
- **Dependencies**: `pyserial`
- **Setup**: `pip install -r requirements.txt`
- **Run**: `python insen_client.py`

Features:
- Object-oriented design with dataclasses
- Threading for non-blocking monitoring
- Type hints for better code clarity
- Comprehensive error handling

### C++ (`cpp/`)
- **File**: `insen_client.cpp`
- **Dependencies**: None (uses platform APIs)
- **Setup**: Use CMake or compile directly
- **Build**: 
  ```bash
  mkdir build && cd build
  cmake ..
  make
  ```

Features:
- Cross-platform serial communication
- Windows API and POSIX support
- Modern C++17 features
- Memory-safe design

### C (`c/`)
- **File**: `insen_client.c`
- **Dependencies**: None (uses platform APIs) 
- **Setup**: Use provided Makefile
- **Build**: `make`

Features:
- Minimal dependencies
- Direct system API usage
- High performance
- Embedded-friendly code

### Go (`go/`)
- **File**: `main.go`
- **Dependencies**: `go.bug.st/serial`
- **Setup**: `go mod tidy`
- **Run**: `go run main.go`

Features:
- Concurrent design with goroutines
- Type-safe structures
- Cross-platform serial support
- Built-in JSON serialization

### Rust (`rust/`)
- **File**: `src/main.rs`
- **Dependencies**: `serialport` crate
- **Setup**: `cargo build`
- **Run**: `cargo run`

Features:
- Memory safety without garbage collection
- Error handling with Result types
- Cross-platform serial communication
- Modern language features

### Java (`java/`)
- **File**: `InsenControllerClient.java`
- **Dependencies**: `jSerialComm` library
- **Setup**: `mvn install` (if using Maven)
- **Run**: `java InsenControllerClient`

Features:
- Object-oriented design
- Cross-platform compatibility
- Thread-safe concurrent access
- Maven build configuration

## Common Features

All examples provide:

1. **Device Connection**: Establish serial communication with INSEN controller
2. **Command Interface**: Send commands and receive responses
3. **Input Monitoring**: Real-time controller input processing
4. **Button Mapping**: Convert button bitmasks to readable names
5. **Error Handling**: Robust error management and recovery
6. **Cross-Platform**: Support for Windows, Linux, and macOS

## Usage Pattern

Each example follows a similar pattern:

```
1. Create controller client instance
2. Connect to INSEN device via serial port
3. Get device information and status
4. Set up input callback for processing
5. Start monitoring controller input
6. Process input in real-time
7. Clean shutdown and disconnect
```

## Serial Port Configuration

- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None

## Port Names by Platform

- **Windows**: `COM3`, `COM4`, etc.
- **Linux**: `/dev/ttyUSB0`, `/dev/ttyACM0`, etc.
- **macOS**: `/dev/tty.usbserial-*`, `/dev/tty.usbmodem*`, etc.

## Command Reference

- `INFO` - Get firmware version and device info
- `STATUS` - Get current device status
- `LIST` - List connected controllers
- `GET <id>` - Get input from controller ID
- `HELP` - Show available commands

## Controller Input Format

Input responses follow this format:
```
>>> INPUT|<id>|<left_x>,<left_y>|<right_x>,<right_y>|<left_trigger>,<right_trigger>|<buttons>|<dpad>|<battery>
```

Example:
```
>>> INPUT|0|-1234,5678|890,-2345|128,64|0x0105|2|87
```

## Button Mapping

- `0x01` - A Button
- `0x02` - B Button  
- `0x04` - X Button
- `0x08` - Y Button
- `0x10` - Left Bumper
- `0x20` - Right Bumper
- `0x40` - Select/Back
- `0x80` - Start/Menu
- `0x100` - Home/Xbox
- `0x200` - Left Stick Button
- `0x400` - Right Stick Button

## Getting Started

1. Choose your preferred programming language
2. Navigate to the corresponding directory
3. Install dependencies as listed above
4. Modify the port name for your system
5. Run the example
6. Connect your controller and see the input!

For more information, see the individual README files in each language directory.