/*
 * INSEN Controller Client - C++ Example
 * //madebybunnyrce
 * This example demonstrates how to connect to and interact with
 * the INSEN controller system using C++.
 * //madebybunnyrce
 * Dependencies: 
 * - Windows: Requires no additional libraries (uses Windows API)
 * - Linux: Requires no additional libraries (uses POSIX serial)
 * - Cross-platform: Can use libserial or boost::asio
 * //madebybunnyrce
 */

#include <iostream> //madebybunnyrce
#include <string> //madebybunnyrce
#include <vector> //madebybunnyrce
#include <map> //madebybunnyrce
#include <thread>
#include <chrono>
#include <functional>
#include <sstream>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace insen {

struct ControllerState {
    int id;
    int left_stick_x, left_stick_y;
    int right_stick_x, right_stick_y;
    int left_trigger, right_trigger;
    uint16_t buttons;
    uint8_t dpad;
    uint8_t battery;
    std::chrono::steady_clock::time_point timestamp;
};

class Controller {
private:
    std::string port_name;
    int baud_rate;
    bool is_connected;
    std::map<int, ControllerState> controllers;
    std::function<void(const ControllerState&)> input_callback;
    std::atomic<bool> monitoring;
    std::thread monitor_thread;

#ifdef _WIN32
    HANDLE serial_handle;
#else
    int serial_fd;
#endif

    // Button name mapping
    const std::map<uint16_t, std::string> button_names = {
        {0x01, "A"}, {0x02, "B"}, {0x04, "X"}, {0x08, "Y"},
        {0x10, "LB"}, {0x20, "RB"}, {0x40, "SELECT"}, {0x80, "START"},
        {0x100, "HOME"}, {0x200, "LSB"}, {0x400, "RSB"}
    };

public:
    Controller(const std::string& port = "COM3", int baudrate = 115200)
        : port_name(port), baud_rate(baudrate), is_connected(false), monitoring(false) {
#ifdef _WIN32
        serial_handle = INVALID_HANDLE_VALUE;
#else
        serial_fd = -1;
#endif
    }

    ~Controller() {
        disconnect();
    }

    bool connect() {
        try {
#ifdef _WIN32
            // Windows implementation
            serial_handle = CreateFileA(
                port_name.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );

            if (serial_handle == INVALID_HANDLE_VALUE) {
                std::cerr << "Failed to open port " << port_name << std::endl;
                return false;
            }

            DCB dcb = {};
            dcb.DCBlength = sizeof(dcb);
            
            if (!GetCommState(serial_handle, &dcb)) {
                std::cerr << "Failed to get comm state" << std::endl;
                CloseHandle(serial_handle);
                return false;
            }

            dcb.BaudRate = baud_rate;
            dcb.ByteSize = 8;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;

            if (!SetCommState(serial_handle, &dcb)) {
                std::cerr << "Failed to set comm state" << std::endl;
                CloseHandle(serial_handle);
                return false;
            }

            COMMTIMEOUTS timeouts = {};
            timeouts.ReadIntervalTimeout = 100;
            timeouts.ReadTotalTimeoutConstant = 1000;
            timeouts.ReadTotalTimeoutMultiplier = 0;
            SetCommTimeouts(serial_handle, &timeouts);

#else
            // Linux/Unix implementation
            serial_fd = open(port_name.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
            
            if (serial_fd < 0) {
                std::cerr << "Failed to open port " << port_name << std::endl;
                return false;
            }

            struct termios tty;
            if (tcgetattr(serial_fd, &tty) != 0) {
                std::cerr << "Failed to get terminal attributes" << std::endl;
                close(serial_fd);
                return false;
            }

            cfsetospeed(&tty, B115200);
            cfsetispeed(&tty, B115200);

            tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
            tty.c_iflag &= ~IGNBRK;
            tty.c_lflag = 0;
            tty.c_oflag = 0;
            tty.c_cc[VMIN] = 0;
            tty.c_cc[VTIME] = 10;

            tty.c_iflag &= ~(IXON | IXOFF | IXANY);
            tty.c_cflag |= (CLOCAL | CREAD);
            tty.c_cflag &= ~(PARENB | PARODD);
            tty.c_cflag &= ~CSTOPB;
            tty.c_cflag &= ~CRTSCTS;

            if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
                std::cerr << "Failed to set terminal attributes" << std::endl;
                close(serial_fd);
                return false;
            }
#endif

            is_connected = true;
            std::cout << "Connected to INSEN device on " << port_name << std::endl;
            
            // Get device info
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            getDeviceInfo();
            
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
            return false;
        }
    }

    void disconnect() {
        stopMonitoring();
        
        if (is_connected) {
#ifdef _WIN32
            if (serial_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(serial_handle);
                serial_handle = INVALID_HANDLE_VALUE;
            }
#else
            if (serial_fd >= 0) {
                close(serial_fd);
                serial_fd = -1;
            }
#endif
            is_connected = false;
            std::cout << "Disconnected from INSEN device" << std::endl;
        }
    }

    std::string sendCommand(const std::string& command) {
        if (!is_connected) {
            throw std::runtime_error("Device not connected");
        }

        std::string full_command = command + "\r\n";
        
#ifdef _WIN32
        DWORD bytes_written;
        if (!WriteFile(serial_handle, full_command.c_str(), full_command.length(), &bytes_written, nullptr)) {
            throw std::runtime_error("Failed to write to serial port");
        }

        // Read response
        char buffer[1024] = {};
        DWORD bytes_read;
        if (ReadFile(serial_handle, buffer, sizeof(buffer) - 1, &bytes_read, nullptr)) {
            std::string response(buffer, bytes_read);
            // Remove trailing whitespace
            response.erase(response.find_last_not_of(" \r\n\t") + 1);
            return response;
        }
#else
        if (write(serial_fd, full_command.c_str(), full_command.length()) < 0) {
            throw std::runtime_error("Failed to write to serial port");
        }

        // Read response
        char buffer[1024] = {};
        ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            std::string response(buffer, bytes_read);
            // Remove trailing whitespace
            response.erase(response.find_last_not_of(" \r\n\t") + 1);
            return response;
        }
#endif

        return "";
    }

    bool parseControllerInput(const std::string& response, ControllerState& state) {
        if (response.length() < 4 || response.substr(0, 4) != ">>> ") {
            return false;
        }

        std::string data = response.substr(4);
        std::vector<std::string> parts;
        std::stringstream ss(data);
        std::string item;

        while (std::getline(ss, item, '|')) {
            parts.push_back(item);
        }

        if (parts.size() >= 8 && parts[0] == "INPUT") {
            try {
                state.id = std::stoi(parts[1]);
                
                // Parse stick values
                size_t comma = parts[2].find(',');
                state.left_stick_x = std::stoi(parts[2].substr(0, comma));
                state.left_stick_y = std::stoi(parts[2].substr(comma + 1));
                
                comma = parts[3].find(',');
                state.right_stick_x = std::stoi(parts[3].substr(0, comma));
                state.right_stick_y = std::stoi(parts[3].substr(comma + 1));
                
                comma = parts[4].find(',');
                state.left_trigger = std::stoi(parts[4].substr(0, comma));
                state.right_trigger = std::stoi(parts[4].substr(comma + 1));
                
                state.buttons = static_cast<uint16_t>(std::stoul(parts[5], nullptr, 16));
                state.dpad = static_cast<uint8_t>(std::stoi(parts[6]));
                state.battery = static_cast<uint8_t>(std::stoi(parts[7]));
                state.timestamp = std::chrono::steady_clock::now();
                
                controllers[state.id] = state;
                return true;
                
            } catch (const std::exception& e) {
                std::cerr << "Error parsing controller input: " << e.what() << std::endl;
            }
        }
        
        return false;
    }

    std::vector<std::string> getButtonNames(uint16_t button_mask) const {
        std::vector<std::string> pressed_buttons;
        
        for (const auto& [mask, name] : button_names) {
            if (button_mask & mask) {
                pressed_buttons.push_back(name);
            }
        }
        
        return pressed_buttons;
    }

    void getDeviceInfo() {
        try {
            std::string response = sendCommand("INFO");
            std::cout << "Device Info: " << response << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to get device info: " << e.what() << std::endl;
        }
    }

    void getStatus() {
        try {
            std::string response = sendCommand("STATUS");
            std::cout << "Status: " << response << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to get status: " << e.what() << std::endl;
        }
    }

    void listControllers() {
        try {
            std::string response = sendCommand("LIST");
            std::cout << "Controllers: " << response << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to list controllers: " << e.what() << std::endl;
        }
    }

    bool getControllerInput(int controller_id = 0) {
        try {
            std::string response = sendCommand("GET " + std::to_string(controller_id));
            ControllerState state;
            
            if (parseControllerInput(response, state)) {
                if (input_callback) {
                    input_callback(state);
                }
                return true;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to get controller input: " << e.what() << std::endl;
        }
        
        return false;
    }

    void setInputCallback(const std::function<void(const ControllerState&)>& callback) {
        input_callback = callback;
    }

    void startMonitoring(int controller_id = 0, int fps = 60) {
        if (monitoring.load()) {
            std::cout << "Monitoring already active" << std::endl;
            return;
        }

        monitoring.store(true);
        auto interval = std::chrono::milliseconds(1000 / fps);

        monitor_thread = std::thread([this, controller_id, interval]() {
            while (monitoring.load()) {
                getControllerInput(controller_id);
                std::this_thread::sleep_for(interval);
            }
        });

        std::cout << "Started monitoring controller " << controller_id 
                  << " at " << fps << " FPS" << std::endl;
    }

    void stopMonitoring() {
        if (monitoring.load()) {
            monitoring.store(false);
            if (monitor_thread.joinable()) {
                monitor_thread.join();
            }
            std::cout << "Stopped monitoring" << std::endl;
        }
    }
};

} // namespace insen

// Example usage
void exampleCallback(const insen::ControllerState& state) {
    // Only print when there's significant input or button presses
    bool significant_input = (
        abs(state.left_stick_x) > 5000 ||
        abs(state.left_stick_y) > 5000 ||
        abs(state.right_stick_x) > 5000 ||
        abs(state.right_stick_y) > 5000 ||
        state.buttons != 0
    );

    if (significant_input) {
        insen::Controller controller;  // Temporary for button parsing
        auto pressed_buttons = controller.getButtonNames(state.buttons);
        
        std::cout << "Controller " << state.id << ": "
                  << "L:(" << state.left_stick_x << "," << state.left_stick_y << ") "
                  << "R:(" << state.right_stick_x << "," << state.right_stick_y << ") "
                  << "Buttons: ";
        
        for (const auto& button : pressed_buttons) {
            std::cout << button << " ";
        }
        
        std::cout << "Battery: " << static_cast<int>(state.battery) << "%" << std::endl;
    }
}

int main() {
    std::cout << "INSEN Controller Client - C++ Example" << std::endl;
    
    // Create controller instance (adjust port as needed)
#ifdef _WIN32
    insen::Controller controller("COM3");  // Windows
#else
    insen::Controller controller("/dev/ttyUSB0");  // Linux
    // insen::Controller controller("/dev/tty.usbserial-*");  // macOS
#endif
    
    try {
        // Connect to device
        if (!controller.connect()) {
            std::cerr << "Failed to connect to device" << std::endl;
            return 1;
        }
        
        // Get device information
        std::this_thread::sleep_for(std::chrono::seconds(1));
        controller.getStatus();
        controller.listControllers();
        
        // Set up input callback
        controller.setInputCallback(exampleCallback);
        
        // Start monitoring at 60 FPS
        controller.startMonitoring(0, 60);
        
        std::cout << "Monitoring controller input for 30 seconds..." << std::endl;
        std::cout << "Press Enter to stop early" << std::endl;
        
        // Run for 30 seconds or until user input
        auto start_time = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(30);
        
        while (std::chrono::steady_clock::now() - start_time < timeout) {
            // Check for user input (non-blocking would be better, but this is simple)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Shutting down..." << std::endl;
    return 0;
}