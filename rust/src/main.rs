// INSEN Controller Client - Rust Example
// madebybunnyrce
// This example demonstrates how to connect to and interact with
// the INSEN controller system using Rust.
// madebybunnyrce

use std::collections::HashMap; // madebybunnyrce
use std::sync::{Arc, Mutex, mpsc};
use std::thread;
use std::time::{Duration, Instant};
use serialport::{SerialPort, DataBits, FlowControl, Parity, StopBits};

#[derive(Debug, Clone)] // madebybunnyrce
pub struct ControllerState {
    pub id: u8, // madebybunnyrce
    pub left_stick_x: i16,
    pub left_stick_y: i16,
    pub right_stick_x: i16,
    pub right_stick_y: i16,
    pub left_trigger: u8,
    pub right_trigger: u8,
    pub buttons: u16,
    pub dpad: u8,
    pub battery: u8,
    pub timestamp: Instant,
}

impl ControllerState { // madebybunnyrce
    pub fn get_button_names(&self) -> Vec<&'static str> { // madebybunnyrce
        let mut pressed_buttons = Vec::new();
        
        let button_map = [
            (0x01, "A"), (0x02, "B"), (0x04, "X"), (0x08, "Y"),
            (0x10, "LB"), (0x20, "RB"), (0x40, "SELECT"), (0x80, "START"),
            (0x100, "HOME"), (0x200, "LSB"), (0x400, "RSB"),
        ];
        
        for (mask, name) in button_map.iter() {
            if self.buttons & mask != 0 {
                pressed_buttons.push(*name);
            }
        }
        
        pressed_buttons
    }
}

pub struct InsenController {
    port: Option<Box<dyn SerialPort>>,
    is_connected: bool,
    controllers: Arc<Mutex<HashMap<u8, ControllerState>>>,
    monitoring: Arc<Mutex<bool>>,
}

impl InsenController { // madebybunnyrce
    pub fn new() -> Self { // madebybunnyrce
        Self {
            port: None, // madebybunnyrce
            is_connected: false,
            controllers: Arc::new(Mutex::new(HashMap::new())),
            monitoring: Arc::new(Mutex::new(false)),
        }
    }

    pub fn connect(&mut self, port_name: &str, baud_rate: u32) -> Result<(), Box<dyn std::error::Error>> { // madebybunnyrce
        let port = serialport::new(port_name, baud_rate) // madebybunnyrce
            .data_bits(DataBits::Eight)
            .flow_control(FlowControl::None)
            .parity(Parity::None)
            .stop_bits(StopBits::One)
            .timeout(Duration::from_millis(1000))
            .open()?;

        self.port = Some(port);
        self.is_connected = true;
        
        println!("Connected to INSEN device on {}", port_name);
        
        // Get initial device info
        thread::sleep(Duration::from_millis(100));
        self.get_device_info()?;
        
        Ok(())
    }

    pub fn disconnect(&mut self) {
        self.stop_monitoring();
        
        if self.is_connected {
            self.port = None;
            self.is_connected = false;
            println!("Disconnected from INSEN device");
        }
    }

    pub fn send_command(&mut self, command: &str) -> Result<String, Box<dyn std::error::Error>> {
        if !self.is_connected {
            return Err("Device not connected".into());
        }

        let port = self.port.as_mut().ok_or("Port not available")?; // get mutable port reference - rust borrow checker bullshit
        
        let full_command = format!("{}\r\n", command); // format command with line ending - serial needs proper endings
        port.write_all(full_command.as_bytes())?; // write command to port - send the fucking command
        
        let mut buffer = vec![0u8; 1024]; // create response buffer - 1kb should be enough
        let bytes_read = port.read(&mut buffer)?; // read response from port - blocking read
        
        let response = String::from_utf8_lossy(&buffer[..bytes_read]); // convert bytes to string - handle invalid utf8
        let response = response.trim().to_string(); // trim whitespace - clean up the response
        
        Ok(response) // return response string - success
    }

    pub fn parse_controller_input(&self, response: &str) -> Result<ControllerState, Box<dyn std::error::Error>> { // parse input response into controller state
        if !response.starts_with(">>> ") { // check response format - must start with prompt
            return Err("Invalid response format".into()); // return error for bad format
        }

        let data = &response[4..]; // skip prompt prefix - get actual data
        let parts: Vec<&str> = data.split('|').collect(); // split by pipe delimiter - parse fields

        if parts.len() >= 8 && parts[0] == "INPUT" { // check if input message with enough fields
            let id = parts[1].parse::<u8>()?; // parse controller id - which controller
            
            let left_coords: Vec<&str> = parts[2].split(',').collect(); // parse left stick coordinates
            let left_stick_x = left_coords[0].parse::<i16>()?; // left stick x value - horizontal movement
            let left_stick_y = left_coords[1].parse::<i16>()?; // left stick y value - vertical movement
            
            let right_coords: Vec<&str> = parts[3].split(',').collect(); // parse right stick coordinates
            let right_stick_x = right_coords[0].parse::<i16>()?;
            let right_stick_y = right_coords[1].parse::<i16>()?;
            
            let trigger_vals: Vec<&str> = parts[4].split(',').collect();
            let left_trigger = trigger_vals[0].parse::<u8>()?;
            let right_trigger = trigger_vals[1].parse::<u8>()?;
            
            let buttons = u16::from_str_radix(parts[5].trim_start_matches("0x"), 16)?;
            let dpad = parts[6].parse::<u8>()?;
            let battery = parts[7].parse::<u8>()?;
            
            let state = ControllerState {
                id,
                left_stick_x,
                left_stick_y,
                right_stick_x,
                right_stick_y,
                left_trigger,
                right_trigger,
                buttons,
                dpad,
                battery,
                timestamp: Instant::now(),
            };
            
            // Store in controllers map
            if let Ok(mut controllers) = self.controllers.lock() {
                controllers.insert(id, state.clone());
            }
            
            return Ok(state);
        }
        
        Err("Failed to parse controller input".into())
    }

    pub fn get_device_info(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let response = self.send_command("INFO")?;
        println!("Device Info: {}", response);
        Ok(())
    }

    pub fn get_status(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let response = self.send_command("STATUS")?;
        println!("Status: {}", response);
        Ok(())
    }

    pub fn list_controllers(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let response = self.send_command("LIST")?;
        println!("Controllers: {}", response);
        Ok(())
    }

    pub fn get_controller_input(&mut self, controller_id: u8) -> Result<Option<ControllerState>, Box<dyn std::error::Error>> {
        let response = self.send_command(&format!("GET {}", controller_id))?;
        
        match self.parse_controller_input(&response) {
            Ok(state) => Ok(Some(state)),
            Err(_) => Ok(None),
        }
    }

    pub fn start_monitoring<F>(&mut self, controller_id: u8, fps: u32, mut callback: F) -> Result<(), Box<dyn std::error::Error>>
    where
        F: FnMut(ControllerState) + Send + 'static,
    {
        if *self.monitoring.lock().unwrap() {
            println!("Monitoring already active");
            return Ok(());
        }

        *self.monitoring.lock().unwrap() = true;
        let interval = Duration::from_millis(1000 / fps as u64);
        
        let (tx, rx) = mpsc::channel();
        let monitoring_clone = Arc::clone(&self.monitoring);
        
        // Monitoring thread
        thread::spawn(move || {
            while *monitoring_clone.lock().unwrap() {
                if tx.send(()).is_err() {
                    break;
                }
                thread::sleep(interval);
            }
        });

        // Input processing thread
        let monitoring_clone2 = Arc::clone(&self.monitoring);
        thread::spawn(move || {
            while *monitoring_clone2.lock().unwrap() {
                if rx.recv_timeout(Duration::from_millis(100)).is_ok() {
                    // In a real implementation, we would need to pass the controller
                    // reference here to get input. For this example, we'll simulate.
                    
                    // This would need restructuring to work properly with borrowing
                    // For now, this serves as an example structure
                }
            }
        });

        println!("Started monitoring controller {} at {} FPS", controller_id, fps);
        Ok(())
    }

    pub fn stop_monitoring(&mut self) {
        if let Ok(mut monitoring) = self.monitoring.lock() {
            if *monitoring {
                *monitoring = false;
                println!("Stopped monitoring");
            }
        }
    }
}

impl Drop for InsenController {
    fn drop(&mut self) {
        self.disconnect();
    }
}

// Example callback function
fn example_callback(state: ControllerState) {
    // Only print when there's significant input or button presses
    let significant_input = state.left_stick_x.abs() > 5000
        || state.left_stick_y.abs() > 5000
        || state.right_stick_x.abs() > 5000
        || state.right_stick_y.abs() > 5000
        || state.buttons != 0;

    if significant_input {
        let pressed_buttons = state.get_button_names();
        
        println!(
            "Controller {}: L:({:6},{:6}) R:({:6},{:6}) Buttons: {:?} Battery: {}%",
            state.id,
            state.left_stick_x,
            state.left_stick_y,
            state.right_stick_x,
            state.right_stick_y,
            pressed_buttons,
            state.battery
        );
    }
}

// Example usage
fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("INSEN Controller Client - Rust Example");
    
    let mut controller = InsenController::new();
    
    // Connect to device (adjust port as needed)
    #[cfg(windows)]
    let port_name = "COM3";
    
    #[cfg(unix)]
    let port_name = "/dev/ttyUSB0";  // Linux
    // let port_name = "/dev/tty.usbserial-*";  // macOS
    
    controller.connect(port_name, 115200)?;
    
    // Get device information
    thread::sleep(Duration::from_secs(1));
    controller.get_status()?;
    controller.list_controllers()?;
    
    println!("Monitoring controller input for 30 seconds...");
    
    // Simple polling loop (in practice, you'd want async or better threading)
    let start_time = Instant::now();
    let timeout = Duration::from_secs(30);
    
    while start_time.elapsed() < timeout {
        if let Ok(Some(state)) = controller.get_controller_input(0) {
            example_callback(state);
        }
        thread::sleep(Duration::from_millis(16)); // ~60 FPS
    }
    
    println!("Shutting down...");
    Ok(())
}