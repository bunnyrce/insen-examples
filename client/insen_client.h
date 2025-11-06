// INSEN Client Library Header
// madebybunnyrce
// C interface for communicating with INSEN USB Host MCU

#ifndef INSEN_CLIENT_H // madebybunnyrce
#define INSEN_CLIENT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Constants
#define INSEN_MAX_CONTROLLERS 4
#define INSEN_MAX_PORT_NAME 64
#define INSEN_MAX_TYPE_NAME 32
#define INSEN_MAX_VERSION_LEN 32
#define INSEN_MAX_BUILD_DATE_LEN 64

// Error codes
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

// Button definitions (bitmask)
#define INSEN_BTN_A           (1 << 0)
#define INSEN_BTN_B           (1 << 1)
#define INSEN_BTN_X           (1 << 2)
#define INSEN_BTN_Y           (1 << 3)
#define INSEN_BTN_LB          (1 << 4)
#define INSEN_BTN_RB          (1 << 5)
#define INSEN_BTN_SELECT      (1 << 6)
#define INSEN_BTN_START       (1 << 7)
#define INSEN_BTN_HOME        (1 << 8)
#define INSEN_BTN_LSB         (1 << 9)  // Left stick button
#define INSEN_BTN_RSB         (1 << 10) // Right stick button
#define INSEN_BTN_TOUCHPAD    (1 << 11)
#define INSEN_BTN_MUTE        (1 << 12)

// D-Pad definitions
#define INSEN_DPAD_NEUTRAL    0
#define INSEN_DPAD_UP         1
#define INSEN_DPAD_UP_RIGHT   2
#define INSEN_DPAD_RIGHT      3
#define INSEN_DPAD_DOWN_RIGHT 4
#define INSEN_DPAD_DOWN       5
#define INSEN_DPAD_DOWN_LEFT  6
#define INSEN_DPAD_LEFT       7
#define INSEN_DPAD_UP_LEFT    8

// Structure definitions
typedef struct {
    int fd;                                    // File descriptor for serial port
    char port_name[INSEN_MAX_PORT_NAME];      // Port name (e.g., "/dev/ttyUSB0", "COM3")
    int is_connected;                         // Connection status
} insen_client_t;

typedef struct {
    // Analog inputs (normalized -32768 to 32767)
    int16_t left_stick_x;
    int16_t left_stick_y;
    int16_t right_stick_x;
    int16_t right_stick_y;
    
    // Triggers (0-255)
    uint8_t left_trigger;
    uint8_t right_trigger;
    
    // Digital inputs
    uint16_t buttons;        // Bitmask of pressed buttons
    uint8_t dpad;           // D-pad state
    
    // Metadata
    uint8_t controller_id;   // Controller ID (0-3)
    uint8_t battery_level;   // Battery level (0-100%)
    uint32_t timestamp;      // Timestamp from firmware
} insen_controller_state_t;

typedef struct {
    int id;                                   // Controller ID
    char type[INSEN_MAX_TYPE_NAME];          // Controller type (e.g., "XBOX_ONE", "PS4")
    int connected;                           // Connection status
} insen_controller_info_t;

typedef struct {
    char version[INSEN_MAX_VERSION_LEN];     // Firmware version
    char build_date[INSEN_MAX_BUILD_DATE_LEN]; // Build date
    int makcu_compatible;                    // MAKCU compatibility flag
    int status_ok;                          // Overall status
} insen_firmware_info_t;

typedef struct {
    int active_controllers;                  // Number of active controllers
    uint32_t total_inputs;                  // Total input events processed
    uint32_t api_commands;                  // Total API commands received
    uint32_t free_heap;                     // Free heap memory in bytes
} insen_system_status_t;

// Function prototypes

/**
 * Initialize INSEN client and open serial connection
 * @param client Client structure to initialize
 * @param port_name Serial port name (e.g., "/dev/ttyUSB0", "COM3")
 * @return INSEN_SUCCESS on success, error code on failure
 */
int insen_init(insen_client_t* client, const char* port_name);

/**
 * Cleanup and close connection
 * @param client Client structure to cleanup
 */
void insen_cleanup(insen_client_t* client);

/**
 * Send raw command to INSEN firmware
 * @param client Initialized client
 * @param command Command string to send
 * @param response Buffer to store response
 * @param response_len Size of response buffer
 * @return INSEN_SUCCESS on success, error code on failure
 */
int insen_send_command(insen_client_t* client, const char* command, char* response, size_t response_len);

/**
 * Get firmware information
 * @param client Initialized client
 * @param info Structure to store firmware info
 * @return INSEN_SUCCESS on success, error code on failure
 */
int insen_get_firmware_info(insen_client_t* client, insen_firmware_info_t* info);

/**
 * Get controller input state
 * @param client Initialized client
 * @param controller_id Controller ID (0-3)
 * @param state Structure to store controller state
 * @return INSEN_SUCCESS on success, error code on failure
 */
int insen_get_controller_input(insen_client_t* client, int controller_id, insen_controller_state_t* state);

/**
 * List connected controllers
 * @param client Initialized client
 * @param controllers Array to store controller info
 * @param count Pointer to store number of controllers found
 * @return INSEN_SUCCESS on success, error code on failure
 */
int insen_list_controllers(insen_client_t* client, insen_controller_info_t* controllers, int* count);

/**
 * Get system status
 * @param client Initialized client
 * @param status Structure to store system status
 * @return INSEN_SUCCESS on success, error code on failure
 */
int insen_get_status(insen_client_t* client, insen_system_status_t* status);

/**
 * Get error string for error code
 * @param error_code Error code from other functions
 * @return Human-readable error string
 */
const char* insen_get_error_string(int error_code);

/**
 * Print controller state for debugging
 * @param state Controller state to print
 */
void insen_print_controller_state(const insen_controller_state_t* state);

/**
 * Print firmware info for debugging
 * @param info Firmware info to print
 */
void insen_print_firmware_info(const insen_firmware_info_t* info);

#ifdef __cplusplus
}
#endif

#endif // INSEN_CLIENT_H