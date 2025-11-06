/*
 * INSEN Controller Client - Java Example
 * madebybunnyrce
 * This example demonstrates how to connect to and interact with
 * the INSEN controller system using Java.
 * madebybunnyrce
 * Dependencies: jSerialComm library for cross-platform serial communication
 */

import com.fazecast.jSerialComm.*; // madebybunnyrce
import java.io.*; // madebybunnyrce
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

public class InsenControllerClient { // madebybunnyrce
    
    public static class ControllerState { // madebybunnyrce
        public int id; // madebybunnyrce
        public int leftStickX, leftStickY;
        public int rightStickX, rightStickY;
        public int leftTrigger, rightTrigger;
        public int buttons;
        public int dpad;
        public int battery;
        public long timestamp;
        
        public ControllerState(int id) {
            this.id = id;
            this.timestamp = System.currentTimeMillis();
        }
        
        public List<String> getButtonNames() {
            List<String> pressedButtons = new ArrayList<>();
            Map<Integer, String> buttonMap = Map.of(
                0x01, "A", 0x02, "B", 0x04, "X", 0x08, "Y",
                0x10, "LB", 0x20, "RB", 0x40, "SELECT", 0x80, "START",
                0x100, "HOME", 0x200, "LSB", 0x400, "RSB"
            );
            
            for (Map.Entry<Integer, String> entry : buttonMap.entrySet()) {
                if ((buttons & entry.getKey()) != 0) {
                    pressedButtons.add(entry.getValue());
                }
            }
            
            return pressedButtons;
        }
        
        @Override // madebybunnyrce
        public String toString() { // madebybunnyrce
            return String.format("Controller %d: L:(%6d,%6d) R:(%6d,%6d) Buttons: %s Battery: %d%%", // madebybunnyrce
                id, leftStickX, leftStickY, rightStickX, rightStickY, 
                getButtonNames(), battery);
        }
    }
    
    @FunctionalInterface // madebybunnyrce
    public interface InputCallback { // madebybunnyrce
        void onInput(ControllerState state); // madebybunnyrce
    }
    
    private SerialPort serialPort;
    private String portName;
    private int baudRate;
    private boolean isConnected;
    private Map<Integer, ControllerState> controllers;
    private InputCallback inputCallback;
    private AtomicBoolean monitoring;
    private ExecutorService executorService;
    
    public InsenControllerClient(String portName, int baudRate) {
        this.portName = portName;
        this.baudRate = baudRate;
        this.controllers = new ConcurrentHashMap<>();
        this.monitoring = new AtomicBoolean(false);
        this.executorService = Executors.newCachedThreadPool();
    }
    
    public boolean connect() {
        try {
            // Find the serial port
            SerialPort[] ports = SerialPort.getCommPorts();
            SerialPort targetPort = null;
            
            for (SerialPort port : ports) {
                if (port.getSystemPortName().equals(portName) || 
                    port.getDescriptivePortName().contains(portName)) {
                    targetPort = port;
                    break;
                }
            }
            
            if (targetPort == null) {
                // Try to open by name directly
                targetPort = SerialPort.getCommPort(portName);
            }
            
            if (targetPort == null) {
                System.err.println("Port " + portName + " not found");
                return false;
            }
            
            serialPort = targetPort;
            serialPort.setBaudRate(baudRate);
            serialPort.setNumDataBits(8);
            serialPort.setNumStopBits(SerialPort.ONE_STOP_BIT);
            serialPort.setParity(SerialPort.NO_PARITY);
            serialPort.setComPortTimeouts(SerialPort.TIMEOUT_READ_SEMI_BLOCKING, 1000, 0);
            
            if (!serialPort.openPort()) {
                System.err.println("Failed to open port " + portName);
                return false;
            }
            
            isConnected = true;
            System.out.println("Connected to INSEN device on " + portName);
            
            // Get initial device info
            Thread.sleep(100);
            getDeviceInfo();
            
            return true;
            
        } catch (Exception e) {
            System.err.println("Connection error: " + e.getMessage());
            return false;
        }
    }
    
    public void disconnect() {
        stopMonitoring();
        
        if (isConnected && serialPort != null) {
            serialPort.closePort();
            isConnected = false;
            System.out.println("Disconnected from INSEN device");
        }
        
        executorService.shutdown();
        try {
            if (!executorService.awaitTermination(2, TimeUnit.SECONDS)) {
                executorService.shutdownNow();
            }
        } catch (InterruptedException e) {
            executorService.shutdownNow();
        }
    }
    
    public String sendCommand(String command) throws IOException {
        if (!isConnected) {
            throw new IOException("Device not connected");
        }
        
        String fullCommand = command + "\r\n";
        byte[] commandBytes = fullCommand.getBytes();
        
        int bytesWritten = serialPort.writeBytes(commandBytes, commandBytes.length);
        if (bytesWritten != commandBytes.length) {
            throw new IOException("Failed to write complete command");
        }
        
        // Read response
        byte[] buffer = new byte[1024];
        int bytesRead = serialPort.readBytes(buffer, buffer.length);
        
        if (bytesRead > 0) {
            String response = new String(buffer, 0, bytesRead).trim();
            return response;
        }
        
        return "";
    }
    
    public ControllerState parseControllerInput(String response) throws Exception {
        if (!response.startsWith(">>> ")) {
            throw new Exception("Invalid response format");
        }
        
        String data = response.substring(4);
        String[] parts = data.split("\\|");
        
        if (parts.length >= 8 && parts[0].equals("INPUT")) {
            int id = Integer.parseInt(parts[1]);
            ControllerState state = new ControllerState(id);
            
            // Parse stick values
            String[] leftCoords = parts[2].split(",");
            state.leftStickX = Integer.parseInt(leftCoords[0]);
            state.leftStickY = Integer.parseInt(leftCoords[1]);
            
            String[] rightCoords = parts[3].split(",");
            state.rightStickX = Integer.parseInt(rightCoords[0]);
            state.rightStickY = Integer.parseInt(rightCoords[1]);
            
            String[] triggerVals = parts[4].split(",");
            state.leftTrigger = Integer.parseInt(triggerVals[0]);
            state.rightTrigger = Integer.parseInt(triggerVals[1]);
            
            state.buttons = Integer.parseUnsignedInt(parts[5].replace("0x", ""), 16);
            state.dpad = Integer.parseInt(parts[6]);
            state.battery = Integer.parseInt(parts[7]);
            
            controllers.put(id, state);
            return state;
        }
        
        throw new Exception("Failed to parse controller input");
    }
    
    public void getDeviceInfo() {
        try {
            String response = sendCommand("INFO");
            System.out.println("Device Info: " + response);
        } catch (IOException e) {
            System.err.println("Failed to get device info: " + e.getMessage());
        }
    }
    
    public void getStatus() {
        try {
            String response = sendCommand("STATUS");
            System.out.println("Status: " + response);
        } catch (IOException e) {
            System.err.println("Failed to get status: " + e.getMessage());
        }
    }
    
    public void listControllers() {
        try {
            String response = sendCommand("LIST");
            System.out.println("Controllers: " + response);
        } catch (IOException e) {
            System.err.println("Failed to list controllers: " + e.getMessage());
        }
    }
    
    public ControllerState getControllerInput(int controllerId) {
        try {
            String response = sendCommand("GET " + controllerId);
            return parseControllerInput(response);
        } catch (Exception e) {
            // Silently handle parsing errors for continuous monitoring
            return null;
        }
    }
    
    public void setInputCallback(InputCallback callback) {
        this.inputCallback = callback;
    }
    
    public void startMonitoring(int controllerId, int fps) {
        if (monitoring.get()) {
            System.out.println("Monitoring already active");
            return;
        }
        
        monitoring.set(true);
        long interval = 1000 / fps;
        
        executorService.submit(() -> {
            while (monitoring.get()) {
                try {
                    ControllerState state = getControllerInput(controllerId);
                    if (state != null && inputCallback != null) {
                        inputCallback.onInput(state);
                    }
                    Thread.sleep(interval);
                } catch (InterruptedException e) {
                    break;
                } catch (Exception e) {
                    // Continue monitoring despite errors
                }
            }
        });
        
        System.out.println("Started monitoring controller " + controllerId + " at " + fps + " FPS");
    }
    
    public void stopMonitoring() {
        if (monitoring.get()) {
            monitoring.set(false);
            System.out.println("Stopped monitoring");
        }
    }
    
    public Map<Integer, ControllerState> getControllers() {
        return new HashMap<>(controllers);
    }
    
    // Example callback implementation
    public static class ExampleCallback implements InputCallback {
        @Override
        public void onInput(ControllerState state) {
            // Only print when there's significant input or button presses
            boolean significantInput = Math.abs(state.leftStickX) > 5000 ||
                                     Math.abs(state.leftStickY) > 5000 ||
                                     Math.abs(state.rightStickX) > 5000 ||
                                     Math.abs(state.rightStickY) > 5000 ||
                                     state.buttons != 0;
            
            if (significantInput) {
                System.out.println(state);
            }
        }
    }
    
    // Example usage
    public static void main(String[] args) {
        System.out.println("INSEN Controller Client - Java Example");
        
        // Create controller instance (adjust port as needed)
        String portName;
        String osName = System.getProperty("os.name").toLowerCase();
        
        if (osName.contains("windows")) {
            portName = "COM3";  // Windows
        } else if (osName.contains("mac")) {
            portName = "/dev/tty.usbserial-*";  // macOS
        } else {
            portName = "/dev/ttyUSB0";  // Linux
        }
        
        InsenControllerClient controller = new InsenControllerClient(portName, 115200);
        
        try {
            // Connect to device
            if (!controller.connect()) {
                System.err.println("Failed to connect to device");
                System.exit(1);
            }
            
            // Get device information
            Thread.sleep(1000);
            controller.getStatus();
            controller.listControllers();
            
            // Set up input callback
            controller.setInputCallback(new ExampleCallback());
            
            // Start monitoring at 60 FPS
            controller.startMonitoring(0, 60);
            
            System.out.println("Monitoring controller input for 30 seconds...");
            System.out.println("Press Ctrl+C to stop early");
            
            // Run for 30 seconds
            Thread.sleep(30000);
            
        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
        } finally {
            controller.disconnect();
            System.out.println("Shutting down...");
        }
    }
}