// INSEN C Client Library
// madebybunnyrce
// Provides a C interface for communicating with INSEN USB Host MCU

#include "insen_client.h" // madebybunnyrce
#include <stdio.h> // madebybunnyrce
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>

// Initialize INSEN client
// madebybunnyrce
int insen_init(insen_client_t* client, const char* port_name) { // madebybunnyrce
    if (!client || !port_name) { // madebybunnyrce
        return INSEN_ERROR_INVALID_PARAM;
    }
    
    memset(client, 0, sizeof(insen_client_t));
    
    // Open serial port
    client->fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (client->fd == -1) {
        printf("Error opening port %s: %s\n", port_name, strerror(errno));
        return INSEN_ERROR_PORT_OPEN;
    }
    
    // Configure serial port
    struct termios options;
    tcgetattr(client->fd, &options);
    
    // Set baud rate to 115200
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    
    // 8N1, no hardware flow control
    options.c_cflag &= ~PARENB;    // No parity
    options.c_cflag &= ~CSTOPB;    // 1 stop bit
    options.c_cflag &= ~CSIZE;     // Clear data size bits
    options.c_cflag |= CS8;        // 8 data bits
    options.c_cflag &= ~CRTSCTS;   // No hardware flow control
    
    // Enable receiver, ignore modem control lines
    options.c_cflag |= CREAD | CLOCAL;
    
    // Raw input mode
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // disable canonical mode and echo - raw mode bullshit
    
    // Raw output mode - no output processing crap
    options.c_oflag &= ~OPOST; // disable output processing - we want raw data
    
    // No input processing - fucking terminal processing is annoying
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // disable flow control - old school modem shit
    options.c_iflag &= ~(INLCR | ICRNL); // disable line ending conversion - keep it raw
    
    // Set timeout (1 second) - don't hang forever waiting
    options.c_cc[VMIN] = 0; // minimum chars to read - 0 for timeout mode
    options.c_cc[VTIME] = 10; // timeout in deciseconds - 1 second timeout
    
    tcsetattr(client->fd, TCSANOW, &options); // apply terminal settings - configure the port
    tcflush(client->fd, TCIOFLUSH); // flush buffers - clear any old data
    
    strcpy(client->port_name, port_name); // store port name - keep track of what we're connected to
    client->is_connected = 1; // mark as connected - flag for other functions
    
    return INSEN_SUCCESS; // return success - connection established
}

// Cleanup and close connection - clean up when done
void insen_cleanup(insen_client_t* client) { // cleanup function - free resources
    if (client && client->fd != -1) { // check if valid client and open fd
        close(client->fd); // close file descriptor - release the port
        client->fd = -1; // reset fd to invalid - mark as closed
        client->is_connected = 0; // mark as disconnected - update status
    }
}

// Send command and receive response
int insen_send_command(insen_client_t* client, const char* command, char* response, size_t response_len) {
    if (!client || !command || !response || !client->is_connected) {
        return INSEN_ERROR_INVALID_PARAM;
    }
    
    // Send command
    char cmd_buffer[256];
    snprintf(cmd_buffer, sizeof(cmd_buffer), "%s\r\n", command);
    
    int bytes_written = write(client->fd, cmd_buffer, strlen(cmd_buffer));
    if (bytes_written <= 0) {
        return INSEN_ERROR_WRITE;
    }
    
    // Wait for response with timeout
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(client->fd, &read_fds);
    timeout.tv_sec = 2;  // 2 second timeout
    timeout.tv_usec = 0;
    
    int ready = select(client->fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready <= 0) {
        return INSEN_ERROR_TIMEOUT;
    }
    
    // Read response
    int bytes_read = read(client->fd, response, response_len - 1);
    if (bytes_read <= 0) {
        return INSEN_ERROR_READ;
    }
    
    response[bytes_read] = '\0';
    
    // Remove trailing newlines
    char* newline = strrchr(response, '\n');
    if (newline) *newline = '\0';
    newline = strrchr(response, '\r');
    if (newline) *newline = '\0';
    
    return INSEN_SUCCESS;
}

// Get firmware information
int insen_get_firmware_info(insen_client_t* client, insen_firmware_info_t* info) {
    if (!client || !info) {
        return INSEN_ERROR_INVALID_PARAM;
    }
    
    char response[512];
    int result = insen_send_command(client, "INFO", response, sizeof(response));
    if (result != INSEN_SUCCESS) {
        return result;
    }
    
    memset(info, 0, sizeof(insen_firmware_info_t));
    
    // Parse response
    char* token = strtok(response, "|");
    while (token != NULL) {
        if (strncmp(token, "INSEN_FW_V", 10) == 0) {
            strncpy(info->version, token + 10, sizeof(info->version) - 1);
        } else if (strncmp(token, "BUILD_", 6) == 0) {
            strncpy(info->build_date, token + 6, sizeof(info->build_date) - 1);
            // Replace underscores with spaces
            for (int i = 0; info->build_date[i]; i++) {
                if (info->build_date[i] == '_') {
                    info->build_date[i] = ' ';
                }
            }
        } else if (strcmp(token, "MAKCU_COMPATIBLE") == 0) {
            info->makcu_compatible = 1;
        } else if (strcmp(token, "STATUS_OK") == 0) {
            info->status_ok = 1;
        }
        token = strtok(NULL, "|");
    }
    
    return INSEN_SUCCESS;
}

// Get controller input state
int insen_get_controller_input(insen_client_t* client, int controller_id, insen_controller_state_t* state) {
    if (!client || !state || controller_id < 0 || controller_id >= INSEN_MAX_CONTROLLERS) {
        return INSEN_ERROR_INVALID_PARAM;
    }
    
    char command[32];
    char response[512];
    
    snprintf(command, sizeof(command), "GET %d", controller_id);
    
    int result = insen_send_command(client, command, response, sizeof(response));
    if (result != INSEN_SUCCESS) {
        return result;
    }
    
    // Parse response: INPUT|ID|LX,LY|RX,RY|LT,RT|BUTTONS|DPAD|BATTERY|TIMESTAMP
    char* tokens[16];
    int token_count = 0;
    
    char* token = strtok(response, "|");
    while (token != NULL && token_count < 16) {
        tokens[token_count++] = token;
        token = strtok(NULL, "|");
    }
    
    if (token_count < 2 || strcmp(tokens[0], "INPUT") != 0) {
        return INSEN_ERROR_INVALID_RESPONSE;
    }
    
    if (token_count >= 3 && strcmp(tokens[2], "DISCONNECTED") == 0) {
        return INSEN_ERROR_CONTROLLER_DISCONNECTED;
    }
    
    memset(state, 0, sizeof(insen_controller_state_t));
    state->controller_id = controller_id;
    
    // Parse analog sticks
    if (token_count >= 4) {
        sscanf(tokens[3], "%hd,%hd", &state->left_stick_x, &state->left_stick_y);
    }
    
    if (token_count >= 5) {
        sscanf(tokens[4], "%hd,%hd", &state->right_stick_x, &state->right_stick_y);
    }
    
    // Parse triggers
    if (token_count >= 6) {
        int lt, rt;
        sscanf(tokens[5], "%d,%d", &lt, &rt);
        state->left_trigger = (uint8_t)lt;
        state->right_trigger = (uint8_t)rt;
    }
    
    // Parse buttons
    if (token_count >= 7) {
        sscanf(tokens[6], "0x%hX", &state->buttons);
    }
    
    // Parse D-pad
    if (token_count >= 8) {
        int dpad;
        sscanf(tokens[7], "%d", &dpad);
        state->dpad = (uint8_t)dpad;
    }
    
    // Parse battery level
    if (token_count >= 9) {
        int battery;
        sscanf(tokens[8], "%d", &battery);
        state->battery_level = (uint8_t)battery;
    }
    
    // Parse timestamp
    if (token_count >= 10) {
        sscanf(tokens[9], "%u", &state->timestamp);
    }
    
    return INSEN_SUCCESS;
}

// List connected controllers
int insen_list_controllers(insen_client_t* client, insen_controller_info_t* controllers, int* count) {
    if (!client || !controllers || !count) {
        return INSEN_ERROR_INVALID_PARAM;
    }
    
    char response[512];
    int result = insen_send_command(client, "LIST", response, sizeof(response));
    if (result != INSEN_SUCCESS) {
        return result;
    }
    
    if (strncmp(response, "CONTROLLERS", 11) != 0) {
        return INSEN_ERROR_INVALID_RESPONSE;
    }
    
    *count = 0;
    
    // Parse controller list
    char* token = strtok(response, "|");
    token = strtok(NULL, "|"); // Skip "CONTROLLERS"
    
    while (token != NULL && *count < INSEN_MAX_CONTROLLERS) {
        // Parse controller info: ID_TYPE
        char* underscore = strchr(token, '_');
        if (underscore) {
            *underscore = '\0';
            
            controllers[*count].id = atoi(token);
            strncpy(controllers[*count].type, underscore + 1, sizeof(controllers[*count].type) - 1);
            controllers[*count].connected = 1;
            
            (*count)++;
        }
        token = strtok(NULL, "|");
    }
    
    return INSEN_SUCCESS;
}

// Get system status
int insen_get_status(insen_client_t* client, insen_system_status_t* status) {
    if (!client || !status) {
        return INSEN_ERROR_INVALID_PARAM;
    }
    
    char response[512];
    int result = insen_send_command(client, "STATUS", response, sizeof(response));
    if (result != INSEN_SUCCESS) {
        return result;
    }
    
    memset(status, 0, sizeof(insen_system_status_t));
    
    // Parse status response
    char* token = strtok(response, "|");
    while (token != NULL) {
        if (strncmp(token, "ACTIVE_", 7) == 0) {
            status->active_controllers = atoi(token + 7);
        } else if (strncmp(token, "TOTAL_INPUTS_", 13) == 0) {
            status->total_inputs = atol(token + 13);
        } else if (strncmp(token, "API_COMMANDS_", 13) == 0) {
            status->api_commands = atol(token + 13);
        } else if (strncmp(token, "FREE_HEAP_", 10) == 0) {
            status->free_heap = atol(token + 10);
        }
        token = strtok(NULL, "|");
    }
    
    return INSEN_SUCCESS;
}

// Utility function to get error string
const char* insen_get_error_string(int error_code) {
    switch (error_code) {
        case INSEN_SUCCESS:
            return "Success";
        case INSEN_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case INSEN_ERROR_PORT_OPEN:
            return "Failed to open serial port";
        case INSEN_ERROR_WRITE:
            return "Serial write error";
        case INSEN_ERROR_READ:
            return "Serial read error";
        case INSEN_ERROR_TIMEOUT:
            return "Communication timeout";
        case INSEN_ERROR_INVALID_RESPONSE:
            return "Invalid response format";
        case INSEN_ERROR_CONTROLLER_DISCONNECTED:
            return "Controller disconnected";
        default:
            return "Unknown error";
    }
}

// Print controller state for debugging
void insen_print_controller_state(const insen_controller_state_t* state) {
    if (!state) return;
    
    printf("Controller %d State:\n", state->controller_id);
    printf("  Left Stick: (%d, %d)\n", state->left_stick_x, state->left_stick_y);
    printf("  Right Stick: (%d, %d)\n", state->right_stick_x, state->right_stick_y);
    printf("  Triggers: L=%d R=%d\n", state->left_trigger, state->right_trigger);
    printf("  Buttons: 0x%04X\n", state->buttons);
    printf("  D-Pad: %d\n", state->dpad);
    printf("  Battery: %d%%\n", state->battery_level);
    printf("  Timestamp: %u\n", state->timestamp);
}

// Print firmware info
void insen_print_firmware_info(const insen_firmware_info_t* info) {
    if (!info) return;
    
    printf("INSEN Firmware Information:\n");
    printf("  Version: %s\n", info->version);
    printf("  Build Date: %s\n", info->build_date);
    printf("  MAKCU Compatible: %s\n", info->makcu_compatible ? "Yes" : "No");
    printf("  Status: %s\n", info->status_ok ? "OK" : "Error");
}