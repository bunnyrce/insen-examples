// INSEN C Client Example
// madebybunnyrce
// Demonstrates controller passthrough integration with INSEN USB Host MCU

#include <stdio.h> // madebybunnyrce
#include <stdlib.h> // madebybunnyrce
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "insen_client.h"

static volatile int running = 1;

// Signal handler for graceful shutdown
// madebybunnyrce
void signal_handler(int sig) { // madebybunnyrce
    printf("\nShutting down...\n");
    running = 0;
}

// Print button names for debugging
void print_button_state(uint16_t buttons) {
    printf("Buttons: ");
    if (buttons & INSEN_BTN_A) printf("A ");
    if (buttons & INSEN_BTN_B) printf("B ");
    if (buttons & INSEN_BTN_X) printf("X ");
    if (buttons & INSEN_BTN_Y) printf("Y ");
    if (buttons & INSEN_BTN_LB) printf("LB ");
    if (buttons & INSEN_BTN_RB) printf("RB ");
    if (buttons & INSEN_BTN_SELECT) printf("SELECT ");
    if (buttons & INSEN_BTN_START) printf("START ");
    if (buttons & INSEN_BTN_HOME) printf("HOME ");
    if (buttons & INSEN_BTN_LSB) printf("LSB ");
    if (buttons & INSEN_BTN_RSB) printf("RSB ");
    if (buttons == 0) printf("None");
    printf("\n");
}

// Print D-pad state
void print_dpad_state(uint8_t dpad) {
    const char* dpad_names[] = {
        "Neutral", "Up", "Up-Right", "Right", "Down-Right",
        "Down", "Down-Left", "Left", "Up-Left"
    };
    
    if (dpad < 9) {
        printf("D-Pad: %s\n", dpad_names[dpad]);
    } else {
        printf("D-Pad: Unknown (%d)\n", dpad);
    }
}

// Monitor controllers continuously
void monitor_controllers(insen_client_t* client) {
    printf("Starting controller monitoring... Press Ctrl+C to stop\n");
    printf("=======================================================\n");
    
    while (running) {
        // Get list of connected controllers
        insen_controller_info_t controllers[INSEN_MAX_CONTROLLERS];
        int count = 0;
        
        int result = insen_list_controllers(client, controllers, &count);
        if (result != INSEN_SUCCESS) {
            printf("Error listing controllers: %s\n", insen_get_error_string(result));
            usleep(100000); // 100ms
            continue;
        }
        
        // Poll each controller
        for (int i = 0; i < count; i++) {
            insen_controller_state_t state;
            result = insen_get_controller_input(client, controllers[i].id, &state);
            
            if (result == INSEN_SUCCESS) {
                // Clear screen and show current state
                printf("\033[2J\033[H"); // ANSI clear screen
                printf("INSEN Controller Monitor - Controller %d (%s)\n", 
                       controllers[i].id, controllers[i].type);
                printf("================================================\n");
                
                printf("Left Stick:  X=%6d Y=%6d\n", state.left_stick_x, state.left_stick_y);
                printf("Right Stick: X=%6d Y=%6d\n", state.right_stick_x, state.right_stick_y);
                printf("Triggers:    L=%3d     R=%3d\n", state.left_trigger, state.right_trigger);
                print_button_state(state.buttons);
                print_dpad_state(state.dpad);
                printf("Battery:     %d%%\n", state.battery_level);
                printf("Timestamp:   %u\n", state.timestamp);
                
                // Visual representation of sticks
                printf("\nStick Visualization:\n");
                printf("Left:  [%c%c%c]\n", 
                       state.left_stick_x < -10000 ? '<' : ' ',
                       (state.left_stick_x > -10000 && state.left_stick_x < 10000) ? 'o' : ' ',
                       state.left_stick_x > 10000 ? '>' : ' ');
                printf("Right: [%c%c%c]\n", 
                       state.right_stick_x < -10000 ? '<' : ' ',
                       (state.right_stick_x > -10000 && state.right_stick_x < 10000) ? 'o' : ' ',
                       state.right_stick_x > 10000 ? '>' : ' ');
                
                fflush(stdout);
            } else if (result == INSEN_ERROR_CONTROLLER_DISCONNECTED) {
                printf("Controller %d disconnected\n", controllers[i].id);
            } else {
                printf("Error reading controller %d: %s\n", 
                       controllers[i].id, insen_get_error_string(result));
            }
        }
        
        usleep(10000); // 10ms = 100Hz polling
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        printf("Example: %s /dev/ttyUSB0\n", argv[0]);
        printf("         %s COM3\n", argv[0]);
        return 1;
    }
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    const char* port_name = argv[1];
    printf("Connecting to INSEN USB Host MCU on %s...\n", port_name);
    
    // Initialize INSEN client
    insen_client_t client;
    int result = insen_init(&client, port_name);
    if (result != INSEN_SUCCESS) {
        printf("Failed to initialize client: %s\n", insen_get_error_string(result));
        return 1;
    }
    
    printf("✓ Connected successfully!\n");
    
    // Get firmware information
    insen_firmware_info_t firmware_info;
    result = insen_get_firmware_info(&client, &firmware_info);
    if (result == INSEN_SUCCESS) {
        printf("\n=== INSEN Firmware Information ===\n");
        insen_print_firmware_info(&firmware_info);
    } else {
        printf("Failed to get firmware info: %s\n", insen_get_error_string(result));
    }
    
    // Get system status
    insen_system_status_t status;
    result = insen_get_status(&client, &status);
    if (result == INSEN_SUCCESS) {
        printf("\n=== System Status ===\n");
        printf("Active Controllers: %d\n", status.active_controllers);
        printf("Total Inputs: %u\n", status.total_inputs);
        printf("API Commands: %u\n", status.api_commands);
        printf("Free Heap: %u bytes\n", status.free_heap);
    } else {
        printf("Failed to get system status: %s\n", insen_get_error_string(result));
    }
    
    // List connected controllers
    printf("\n=== Connected Controllers ===\n");
    insen_controller_info_t controllers[INSEN_MAX_CONTROLLERS];
    int count = 0;
    
    result = insen_list_controllers(&client, controllers, &count);
    if (result == INSEN_SUCCESS) {
        if (count == 0) {
            printf("No controllers connected. Please connect a controller to the INSEN USB Host port.\n");
        } else {
            for (int i = 0; i < count; i++) {
                printf("Controller %d: %s (%s)\n", 
                       controllers[i].id, 
                       controllers[i].type,
                       controllers[i].connected ? "Connected" : "Disconnected");
            }
            
            // Start monitoring if controllers are connected
            printf("\n");
            monitor_controllers(&client);
        }
    } else {
        printf("Failed to list controllers: %s\n", insen_get_error_string(result));
    }
    
    // Cleanup
    insen_cleanup(&client);
    printf("Disconnected from INSEN device.\n");
    
    return 0;
}