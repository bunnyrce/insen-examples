/**
 * INSEN Controller Client - JavaScript/Node.js Example
 * //madebybunnyrce
 * This example demonstrates how to connect to and interact with
 * the INSEN controller system using JavaScript/Node.js.
 * //madebybunnyrce - fucking javascript everywhere these days
 */

const { SerialPort } = require('serialport'); //madebybunnyrce - serialport lib for comms
const { ReadlineParser } = require('@serialport/parser-readline'); //madebybunnyrce - line parser cause serial is messy

class InsenController { // main controller class - oop bullshit in js
    constructor(portPath = 'COM3', baudRate = 115200) { // constructor with defaults
        this.port = null; // serial port object - null until connected
        this.parser = null; // data parser - null initially
        this.portPath = portPath; // com port path - windows style
        this.baudRate = baudRate; // baud rate - 115200 is decent
        this.isConnected = false; // connection status - false by default
        this.controllers = new Map(); // controller state map - es6 maps are nice
    }

    // Connect to INSEN device - async cause serial is slow
    async connect() {
        try { // try catch cause serial connections fail all the time
            this.port = new SerialPort({ // create serial port instance
                path: this.portPath, // com port path
                baudRate: this.baudRate // connection speed
            });

            this.parser = this.port.pipe(new ReadlineParser({ delimiter: '\r\n' })); // pipe to line parser
            
            // Handle incoming data
            this.parser.on('data', (data) => {
                this.handleResponse(data);
            });

            // Handle connection events
            this.port.on('open', () => {
                console.log('Connected to INSEN device');
                this.isConnected = true;
                this.getDeviceInfo();
            });

            this.port.on('error', (err) => {
                console.error('Serial port error:', err);
                this.isConnected = false;
            });

            this.port.on('close', () => {
                console.log('Disconnected from INSEN device'); // device unplugged or some shit
                this.isConnected = false; // update connection status - no longer connected
            });

        } catch (error) { // catch connection errors - serial ports fail all the time
            console.error('Failed to connect:', error); // log the error - figure out what went wrong
            throw error; // re-throw error - let caller handle it
        }
    }

    // Send command to INSEN device - write serial data
    sendCommand(command) {
        if (!this.isConnected || !this.port) { // check if connected first - can't send without connection
            throw new Error('Device not connected'); // error if not connected - obvious
        }

        return new Promise((resolve, reject) => { // promise based cause async shit
            this.port.write(command + '\r\n', (err) => { // write command with line ending - serial needs proper endings
                if (err) { // check for write errors
                    reject(err); // reject promise on error - something fucked up
                } else {
                    resolve(); // resolve on success - command sent
                }
            });
        });
    }

    // Handle responses from device
    handleResponse(data) {
        if (data.startsWith('>>> ')) {
            const response = data.substring(4);
            console.log('Device response:', response);
            
            // Parse different response types
            if (response.startsWith('INSEN_FW_V')) {
                this.parseDeviceInfo(response);
            } else if (response.startsWith('INPUT|')) {
                this.parseControllerInput(response);
            } else if (response.startsWith('CONTROLLERS')) {
                this.parseControllerList(response);
            }
        }
    }

    // Parse device information
    parseDeviceInfo(response) {
        const parts = response.split('|');
        const info = {
            version: parts[0].replace('INSEN_FW_V', ''),
            build: parts[1].replace('BUILD_', ''),
            controllers: parseInt(parts[2].replace('CONTROLLERS_', '')), // extract controller count - how many connected
            status: parts[4] // device status - running idle whatever
        };
        console.log('Device Info:', info); // log device info - debug output
    }

    // Parse controller input data - decode the fucking protocol
    parseControllerInput(response) {
        const parts = response.split('|'); // split by pipe delimiter - standard format
        if (parts.length >= 8) { // make sure we have enough fields - prevent crashes
            const controllerId = parseInt(parts[1]); // controller id - which controller
            const [leftX, leftY] = parts[2].split(',').map(Number); // left stick coordinates - analog input
            const [rightX, rightY] = parts[3].split(',').map(Number); // right stick coordinates - camera control
            const [leftTrigger, rightTrigger] = parts[4].split(',').map(Number); // trigger values - analog triggers
            const buttons = parseInt(parts[5], 16); // button bitmask - hex encoded buttons
            const dpad = parseInt(parts[6]); // dpad state - directional input
            const battery = parseInt(parts[7]); // battery level - wireless controller power
            
            const controllerData = { // build controller object - structured data
                id: controllerId, // controller identifier
                leftStick: { x: leftX, y: leftY }, // left analog stick data
                rightStick: { x: rightX, y: rightY }, // right analog stick data
                triggers: { left: leftTrigger, right: rightTrigger }, // trigger data
                buttons: buttons, // button bitmask - all buttons
                dpad: dpad, // directional pad state
                battery: battery, // battery percentage
                timestamp: Date.now()
            };
            
            this.controllers.set(controllerId, controllerData);
            this.onControllerInput(controllerData);
        }
    }

    // Parse controller list
    parseControllerList(response) {
        const controllers = response.split('|').slice(1);
        console.log('Connected controllers:', controllers);
    }

    // Override this method to handle controller input
    onControllerInput(controllerData) {
        console.log(`Controller ${controllerData.id}:`, {
            leftStick: controllerData.leftStick,
            rightStick: controllerData.rightStick,
            buttonsPressed: this.getButtonNames(controllerData.buttons)
        });
    }

    // Convert button bitmask to button names
    getButtonNames(buttonMask) {
        const buttons = [];
        const buttonMap = {
            0x01: 'A', 0x02: 'B', 0x04: 'X', 0x08: 'Y',
            0x10: 'LB', 0x20: 'RB', 0x40: 'SELECT', 0x80: 'START',
            0x100: 'HOME', 0x200: 'LSB', 0x400: 'RSB'
        };

        for (const [mask, name] of Object.entries(buttonMap)) {
            if (buttonMask & parseInt(mask)) {
                buttons.push(name);
            }
        }
        return buttons;
    }

    // Get device information
    async getDeviceInfo() {
        await this.sendCommand('INFO');
    }

    // Get device status
    async getStatus() {
        await this.sendCommand('STATUS');
    }

    // List connected controllers
    async listControllers() {
        await this.sendCommand('LIST');
    }

    // Get input from specific controller
    async getControllerInput(controllerId) {
        await this.sendCommand(`GET ${controllerId}`);
    }

    // Start continuous input monitoring
    startMonitoring(controllerId = 0, intervalMs = 16) {
        this.monitoringInterval = setInterval(() => {
            this.getControllerInput(controllerId);
        }, intervalMs);
    }

    // Stop input monitoring
    stopMonitoring() {
        if (this.monitoringInterval) {
            clearInterval(this.monitoringInterval);
            this.monitoringInterval = null;
        }
    }

    // Disconnect from device
    async disconnect() {
        this.stopMonitoring();
        
        if (this.port && this.port.isOpen) {
            return new Promise((resolve) => {
                this.port.close((err) => {
                    if (err) console.error('Error closing port:', err);
                    resolve();
                });
            });
        }
    }
}

// Example usage
async function main() {
    const controller = new InsenController('COM3'); // Adjust port as needed
    
    try {
        await controller.connect();
        
        // Get device info
        await new Promise(resolve => setTimeout(resolve, 1000));
        await controller.getStatus();
        await controller.listControllers();
        
        // Start monitoring controller input
        console.log('Starting input monitoring...');
        controller.startMonitoring(0, 16); // 60fps monitoring
        
        // Run for 30 seconds
        setTimeout(async () => {
            console.log('Stopping monitoring...');
            controller.stopMonitoring();
            await controller.disconnect();
            process.exit(0);
        }, 30000);
        
    } catch (error) {
        console.error('Error:', error);
        process.exit(1);
    }
}

// Run example if this file is executed directly
if (require.main === module) {
    main();
}

module.exports = InsenController;