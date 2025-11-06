"""
INSEN Controller Client - Python Example
#madebybunnyrce

This example demonstrates how to connect to and interact with
the INSEN controller system using Python.
#madebybunnyrce - python is actually decent for this shit
"""

import serial #madebybunnyrce - pyserial for serial port bullshit
import threading #madebybunnyrce - threading cause python is slow as fuck
import time #madebybunnyrce - time functions for delays and timestamps
import json #madebybunnyrce - json parsing cause everything uses json now
from typing import Dict, List, Optional, Callable #madebybunnyrce - type hints cause python needs them
from dataclasses import dataclass #madebybunnyrce - dataclass decorator is pretty neat


@dataclass # dataclass decorator - saves writing boilerplate constructor shit
class ControllerState:
    """Data class representing controller state - all the controller data in one place"""
    id: int # controller id - which controller this is
    left_stick_x: int # left analog stick x axis - left/right movement
    left_stick_y: int # left analog stick y axis - up/down movement 
    right_stick_x: int # right analog stick x axis - camera horizontal
    right_stick_y: int # right analog stick y axis - camera vertical
    left_trigger: int # left trigger value - how hard pressed
    right_trigger: int # right trigger value - analog trigger bullshit
    buttons: int # button bitfield - all buttons packed into int
    dpad: int # dpad state - directional pad input
    battery: int # battery level - cause wireless controllers die
    timestamp: float # timestamp when state was captured - for timing shit


class InsenController:
    """INSEN Controller Client for Python"""
    
    # Button definitions
    BUTTONS = {
        0x01: 'A', 0x02: 'B', 0x04: 'X', 0x08: 'Y',
        0x10: 'LB', 0x20: 'RB', 0x40: 'SELECT', 0x80: 'START',
        0x100: 'HOME', 0x200: 'LSB', 0x400: 'RSB'
    }
    
    def __init__(self, port: str = 'COM3', baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.serial_conn: Optional[serial.Serial] = None
        self.is_connected = False
        self.controllers: Dict[int, ControllerState] = {}
        self.input_callback: Optional[Callable] = None
        self._monitoring = False
        self._monitor_thread = None
        
    def connect(self) -> bool:
        """Connect to INSEN device"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1
            )
            self.is_connected = True
            print(f"Connected to INSEN device on {self.port}")
            
            # Get initial device info
            time.sleep(0.1)  # Wait for device to be ready
            self.get_device_info()
            
            return True
            
        except serial.SerialException as e:
            print(f"Failed to connect: {e}")
            return False
    
    def disconnect(self) -> None:
        """Disconnect from device"""
        self.stop_monitoring()
        
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            self.is_connected = False
            print("Disconnected from INSEN device")
    
    def send_command(self, command: str) -> Optional[str]:
        """Send command to device and return response"""
        if not self.is_connected or not self.serial_conn:
            raise ConnectionError("Device not connected")
        
        try:
            # Send command
            self.serial_conn.write((command + '\r\n').encode())
            
            # Read response
            response = self.serial_conn.readline().decode().strip()
            
            if response.startswith('>>> '):
                return response[4:]  # Remove prefix
            
            return response
            
        except serial.SerialException as e:
            print(f"Communication error: {e}")
            return None
    
    def parse_controller_input(self, response: str) -> Optional[ControllerState]:
        """Parse controller input response"""
        parts = response.split('|')
        
        if len(parts) >= 8 and parts[0] == 'INPUT':
            try:
                controller_id = int(parts[1])
                left_x, left_y = map(int, parts[2].split(','))
                right_x, right_y = map(int, parts[3].split(','))
                left_trigger, right_trigger = map(int, parts[4].split(','))
                buttons = int(parts[5], 16)
                dpad = int(parts[6])
                battery = int(parts[7])
                
                state = ControllerState(
                    id=controller_id,
                    left_stick_x=left_x,
                    left_stick_y=left_y,
                    right_stick_x=right_x,
                    right_stick_y=right_y,
                    left_trigger=left_trigger,
                    right_trigger=right_trigger,
                    buttons=buttons,
                    dpad=dpad,
                    battery=battery,
                    timestamp=time.time()
                )
                
                self.controllers[controller_id] = state
                return state
                
            except (ValueError, IndexError) as e:
                print(f"Error parsing controller input: {e}")
                
        return None
    
    def get_button_names(self, button_mask: int) -> List[str]:
        """Convert button bitmask to list of button names"""
        pressed_buttons = []
        for mask, name in self.BUTTONS.items():
            if button_mask & mask:
                pressed_buttons.append(name)
        return pressed_buttons
    
    def get_device_info(self) -> Optional[Dict]:
        """Get device information"""
        response = self.send_command('INFO')
        if response and response.startswith('INSEN_FW_V'):
            parts = response.split('|')
            info = {
                'version': parts[0].replace('INSEN_FW_V', ''),
                'build': parts[1].replace('BUILD_', ''),
                'controllers': int(parts[2].replace('CONTROLLERS_', '')),
                'status': parts[4] if len(parts) > 4 else 'UNKNOWN'
            }
            print(f"Device Info: {info}")
            return info
        return None
    
    def get_status(self) -> Optional[Dict]:
        """Get device status"""
        response = self.send_command('STATUS')
        if response and response.startswith('STATUS'):
            print(f"Status: {response}")
            return {'raw': response}
        return None
    
    def list_controllers(self) -> List[str]:
        """List connected controllers"""
        response = self.send_command('LIST')
        if response and response.startswith('CONTROLLERS'):
            controllers = response.split('|')[1:]
            print(f"Connected controllers: {controllers}")
            return controllers
        return []
    
    def get_controller_input(self, controller_id: int = 0) -> Optional[ControllerState]:
        """Get input from specific controller"""
        response = self.send_command(f'GET {controller_id}')
        if response:
            return self.parse_controller_input(response)
        return None
    
    def set_input_callback(self, callback: Callable[[ControllerState], None]) -> None:
        """Set callback function for controller input"""
        self.input_callback = callback
    
    def _monitor_loop(self, controller_id: int, interval: float) -> None:
        """Internal monitoring loop"""
        while self._monitoring:
            state = self.get_controller_input(controller_id)
            if state and self.input_callback:
                self.input_callback(state)
            time.sleep(interval)
    
    def start_monitoring(self, controller_id: int = 0, fps: int = 60) -> None:
        """Start continuous input monitoring"""
        if self._monitoring:
            print("Monitoring already active")
            return
        
        self._monitoring = True
        interval = 1.0 / fps
        
        self._monitor_thread = threading.Thread(
            target=self._monitor_loop,
            args=(controller_id, interval)
        )
        self._monitor_thread.daemon = True
        self._monitor_thread.start()
        
        print(f"Started monitoring controller {controller_id} at {fps} FPS")
    
    def stop_monitoring(self) -> None:
        """Stop input monitoring"""
        if self._monitoring:
            self._monitoring = False
            if self._monitor_thread:
                self._monitor_thread.join(timeout=1.0)
            print("Stopped monitoring")


def example_callback(state: ControllerState) -> None:
    """Example callback function for processing controller input"""
    
    # Only print when there's significant input or button presses
    significant_input = (
        abs(state.left_stick_x) > 5000 or 
        abs(state.left_stick_y) > 5000 or
        abs(state.right_stick_x) > 5000 or 
        abs(state.right_stick_y) > 5000 or
        state.buttons != 0
    )
    
    if significant_input:
        controller = InsenController('', 0)  # Dummy for button parsing
        pressed_buttons = controller.get_button_names(state.buttons)
        
        print(f"Controller {state.id}: "
              f"L:({state.left_stick_x:6},{state.left_stick_y:6}) "
              f"R:({state.right_stick_x:6},{state.right_stick_y:6}) "
              f"Buttons: {pressed_buttons} "
              f"Battery: {state.battery}%")


def main():
    """Example usage of the INSEN Controller Client"""
    
    # Create controller instance (adjust port as needed)
    controller = InsenController(port='COM3')  # Windows
    # controller = InsenController(port='/dev/ttyUSB0')  # Linux
    # controller = InsenController(port='/dev/tty.usbserial-*')  # macOS
    
    try:
        # Connect to device
        if not controller.connect():
            print("Failed to connect to device")
            return
        
        # Get device information
        time.sleep(1)
        controller.get_status()
        controller.list_controllers()
        
        # Set up input callback
        controller.set_input_callback(example_callback)
        
        # Start monitoring at 60 FPS
        controller.start_monitoring(controller_id=0, fps=60)
        
        print("Monitoring controller input for 30 seconds...")
        print("Press Ctrl+C to stop early")
        
        # Run for 30 seconds
        try:
            time.sleep(30)
        except KeyboardInterrupt:
            print("\nStopping...")
        
    except Exception as e:
        print(f"Error: {e}")
    
    finally:
        controller.disconnect()


if __name__ == "__main__":
    main()