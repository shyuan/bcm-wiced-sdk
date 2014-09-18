/*
 * Copyright 2014, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

/**
 * @file
 *
 * Bluetooth SmartBridge Console Test Application
 *
 * The console application is an interactive Bluetooth networking
 * diagnostic and test tool controlled via a console and serial
 * interface.
 *
 * The application provides commands to access various bluetooth
 * functions including Bluetooth Smart functions (as part of the
 * WICED SmartBridge API).
 *
 * To use the application, build and download the app to a WICED
 * Bluetooth-capable evaluation board (such as BCM9WCDPLUS114) then
 * connect to the UART with a terminal program using comm
 * parameters: 115200 8N1. A command prompt appears.
 *
 * The list of available commands may be obtained in several ways.
 * Pressing the tab key twice prints a list similar to that shown
 * below. Alternately, entering ? or help prints a detailed list
 * with information describing each command.
 *
 * The console application supports tab completion and maintains
 * a history of commands. Command history is accessible with the
 * up-arrow key.
 *
 * WARNING : The Console App is a test application and necessarily calls
 * some internal WICED functions that are not part of the standard
 * WICED API located in the <WICED-SDK>/include directory. As such, the
 * Console App is generally not a good application to base new applications
 * upon.
 *
 */

#include "wiced.h"
#include "console.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wiced_utilities.h"
#include "wiced_platform.h"
#include "wifi_config_dct.h"
#include "reboot/reboot.h"

#define MAX_LINE_LENGTH  128
#define DELIMIT ((char*) " ")

#define MAX_ARGUMENTS      18

extern int start_smartbridge               ( int argc, char *argv[] );
extern int stop_smartbridge                ( int argc, char *argv[] );
extern int auto_test_smartbridge           ( int argc, char *argv[] );
extern int connect                         ( int argc, char *argv[] );
extern int disconnect                      ( int argc, char *argv[] );
extern int scan_connect_direct_adv         ( int argc, char *argv[] );
extern int scan                            ( int argc, char *argv[] );
extern int status                          ( int argc, char *argv[] );
extern int clear_bond_info_dct             ( int argc, char *argv[] );
extern int enable_attribute_cache          ( int argc, char *argv[] );
extern int disable_attribute_cache         ( int argc, char *argv[] );
extern int print_attribute_cache           ( int argc, char *argv[] );
extern int write_attribute_cache           ( int argc, char *argv[] );
extern int enable_notification             ( int argc, char *argv[] );
extern int disable_notification            ( int argc, char *argv[] );
extern int add_whitelist                   ( int argc, char *argv[] );
extern int remove_whitelist                ( int argc, char *argv[] );
extern int clear_whitelist                 ( int argc, char *argv[] );
extern int get_whitelist_size              ( int argc, char *argv[] );
extern int discover_all_services           ( int argc, char *argv[] );
extern int discover_services_by_uuid       ( int argc, char *argv[] );
extern int find_included_services          ( int argc, char *argv[] );
extern int discover_all_chars_in_a_service ( int argc, char *argv[] );
extern int discover_chars_by_uuid          ( int argc, char *argv[] );
extern int discover_all_char_descriptors   ( int argc, char *argv[] );
extern int read_char_descriptor            ( int argc, char *argv[] );
extern int read_long_char_descriptor       ( int argc, char *argv[] );
extern int write_char_descriptor           ( int argc, char *argv[] );
extern int write_long_char_descriptor      ( int argc, char *argv[] );
extern int read_char_value                 ( int argc, char *argv[] );
extern int read_long_char_value            ( int argc, char *argv[] );
extern int read_char_values_by_uuid        ( int argc, char *argv[] );
extern int write_char_value                ( int argc, char *argv[] );
extern int write_long_char_value           ( int argc, char *argv[] );
extern int dump_packet                     ( int argc, char *argv[] );
extern int stack_trace                     ( int argc, char *argv[] );
extern int set_tx_power                    ( int argc, char *argv[] );

static void history_load_line              ( uint32_t line, char* buffer );
static void history_store_line             ( char* buffer );
static uint32_t history_get_num_lines      ( void );
static wiced_bool_t console_process_esc_seq( char c );
static cmd_err_t console_parse_cmd         ( char* line );
static void console_load_line              ( uint32_t new_line );
static void console_insert_char            ( char c );
static void console_remove_char            ( void );

/* forward declarations */
static void console_do_home      ( void );
static void console_do_end       ( void );
static void console_do_up        ( void );
static void console_do_down      ( void );
static void console_do_left      ( void );
static void console_do_right     ( void );
static void console_do_delete    ( void );
static void console_do_backspace ( void );
static void console_do_tab       ( void );
static cmd_err_t console_do_enter( void );
static cmd_err_t console_do_newline_without_command_repeat( void );


static void send_char( char c );
static void send_str( char* s );
static void send_charstr( char* s );

/* API Functions */
/* for general use these are the only things you need to worry about */
static void console_init(const command_t *command_table, uint32_t line_len, char *buffer, uint32_t history_len, char *history_buffer);
static cmd_err_t console_process_char(char c);
#if 0
static cmd_err_t console_process_char_with_tab_complete(char c);
#endif
static int help_command( int argc, char* argv[] );



const command_t* console_command_table;

static char line_buffer[MAX_LINE_LENGTH];
static char history_buffer_storage[MAX_LINE_LENGTH * 20];

/* default error strings */
static const char* const console_default_error_strings[] = {
    (char*) "OK",
    (char*) "Unknown Error",
    (char*) "Unknown Command",
    (char*) "Insufficient Arguments",
    (char*) "Too Many Arguments",
    (char*) "Bad Address Value",
    (char*) "No Command Entered"
};

/* console config with defaults */
static const char* const *console_error_strings = console_default_error_strings;
static char* console_bell_string = (char*) "\a";
static char* console_delimit_string = (char*) " ";
static char* console_prompt_string = (char*) "> ";
static uint32_t console_max_params = 16;

/* console config specified by init */
static uint32_t console_line_len;
static char* console_buffer = NULL;

/* console state */
static uint32_t console_cursor_position;
static uint32_t console_current_line;
static wiced_bool_t console_in_esc_seq;
static char console_esc_seq_last_char;

/* history config specified by init */
static char* history_buffer;
static uint32_t history_length;

/* history state */
static uint32_t history_num_lines;
static uint32_t history_newest_line;

#ifdef USE_CUSTOM_COMMANDS
#include "custom.h"
#endif /* ifdef   CUSTOM_COMMANDS */

static const command_t commands[] = {
    { (char*) "?",                               help_command,                    0, DELIMIT, NULL, NULL, NULL},
    { (char*) "help",                            help_command,                    0, DELIMIT, NULL, (char*) "[<command> [<example_num>]]",                                                "Print help message or command example."},
    REBOOT_COMMANDS
    { (char*) "start_smartbridge",               start_smartbridge,               0, DELIMIT, NULL,  NULL ,                                                                               "Start SmartBridge"},
    { (char*) "stop_smartbridge",                stop_smartbridge,                0, DELIMIT, NULL,  NULL ,                                                                               "Stop SmartBridge"},
    { (char*) "auto_test_smartbridge",           auto_test_smartbridge,           1, DELIMIT, NULL, (char*) "<iteration>",                                                                "Start SmartBridge auto-test"},
    { (char*) "dump_packet",                     dump_packet,                     1, DELIMIT, NULL, (char*) "<enable_packet_dump>",                                                       "Enable/disable packet dump."},
    { (char*) "stack_trace",                     stack_trace,                     1, DELIMIT, NULL, (char*) "<enable_stack_stace>",                                                       "Enable/disable printing of stack debug trace."},
    { (char*) "connect",                         connect,                         4, DELIMIT, NULL, (char*) "<socket_number> <device_address> <address_type> <enable_pairing> [passkey]", "Establish a SmartBridge connection with a remote device."},
    { (char*) "disconnect",                      disconnect,                      1, DELIMIT, NULL, (char*) "<socket_number>",                                                            "Disconnect a SmartBridge connection with a remote device."},
    { (char*) "set_tx_power",                    set_tx_power,                    2, DELIMIT, NULL, (char*) "<socket_number> <transmit_power_dbm>",                                       "Set transmit power during a connection."},
    { (char*) "scan_connect_direct_adv",         scan_connect_direct_adv,         3, DELIMIT, NULL, (char*) "<socket_number> <enable_pairing> <scan_duration>",                           "Scan for a Directed Advertisement and connect ot the remote device sending the advertisement"},
    { (char*) "scan",                            scan,                            2, DELIMIT, NULL, (char*) "<duration_second> <filter_policy>",                                          "Scan for Bluetooth Smart devices in range."},
    { (char*) "status",                          status,                          0, DELIMIT, NULL,  NULL,                                                                                "Get all Bluetooth SmartBridge sockets' status."},
    { (char*) "clear_bond_info_dct",             clear_bond_info_dct,             0, DELIMIT, NULL,  NULL,                                                                                "Clear Bond Info DCT"},
    { (char*) "enable_attribute_cache",          enable_attribute_cache,          1, DELIMIT, NULL, (char*) "<number_of_cache>",                                                          "Enable Attribute Cache"},
    { (char*) "disable_attribute_cache",         disable_attribute_cache,         0, DELIMIT, NULL,  NULL,                                                                                "Disable Attribute Cache"},
    { (char*) "enable_notification",             enable_notification,             1, DELIMIT, NULL, (char*) "<socket_number>",                                                            "Enable Attribute Cache notifications"},
    { (char*) "disable_notification",            disable_notification,            1, DELIMIT, NULL, (char*) "<socket_number>",                                                            "Enable Attribute Cache notifications"},
    { (char*) "print_attribute_cache",           print_attribute_cache,           1, DELIMIT, NULL, (char*) "<socket_number>",                                                            "Print all attributes in the cache for the socket specified."},
    { (char*) "write_attribute_cache",           write_attribute_cache,           3, DELIMIT, NULL, (char*) "<socket_number> <handle> <value>",                                           "Write attribute to both the cache and remote device"},
    { (char*) "add_whitelist",                   add_whitelist,                   2, DELIMIT, NULL, (char*) "<device_address> <address_type>",                                            "Add a device to the whitelist"},
    { (char*) "remove_whitelist",                remove_whitelist,                2, DELIMIT, NULL, (char*) "<device_address> <address_type>",                                            "Remove a device to the whitelist"},
    { (char*) "clear_whitelist",                 clear_whitelist,                 0, DELIMIT, NULL,  NULL,                                                                                "Clear whitelist"},
    { (char*) "get_whitelist_size",              get_whitelist_size,              0, DELIMIT, NULL,  NULL,                                                                                "Get whitelist size"},
    { (char*) "discover_all_services",           discover_all_services,           1, DELIMIT, NULL, (char*) "<socket_number> <address_type>",                                             "Send GATT discover all primary services.             \n\t  Example: discover_all_services 0"},
    { (char*) "discover_services_by_uuid",       discover_services_by_uuid,       2, DELIMIT, NULL, (char*) "<socket_number> <uuid>",                                                     "Send GATT discover primary services by UUID.         \n\t  Example: discover_services_by_uuid 0 1800"},
    { (char*) "find_included_services",          find_included_services,          3, DELIMIT, NULL, (char*) "<socket_number> <start_handle> <end_handle>",                                "Send GATT find included services.                    \n\t  Example: find_included_services 2 10"},
    { (char*) "discover_all_chars_in_a_service", discover_all_chars_in_a_service, 3, DELIMIT, NULL, (char*) "<socket_number> <start_handle> <end_handle>",                                "Send GATT discover all characteristics in a service. \n\t  Example: discover_all_chars_in_a_service 2 10"},
    { (char*) "discover_chars_by_uuid",          discover_chars_by_uuid,          4, DELIMIT, NULL, (char*) "<socket_number> <uuid> <start_handle> <end_handle>",                         "Send GATT discover characteristics by UUID.          \n\t  Example: discover_chars_by_uuid 2A29 1 100"},
    { (char*) "discover_all_char_descriptors",   discover_all_char_descriptors,   3, DELIMIT, NULL, (char*) "<socket_number> <start_handle> <end_handle>",                                "Send GATT discover all characteristic descriptors.   \n\t  Example: discover_all_char_descriptors 2 10"},
    { (char*) "read_char_descriptor",            read_char_descriptor,            3, DELIMIT, NULL, (char*) "<socket_number> <handle> <uuid>",                                            "Send GATT read characteristic descriptor.            \n\t  Example: read_char_descriptor 10 2A00"},
    { (char*) "read_long_char_descriptor",       read_long_char_descriptor,       3, DELIMIT, NULL, (char*) "<socket_number> <handle> <uuid>",                                            "Send GATT read long characteristic descriptor.       \n\t  Example: read_long_char_descriptor 10 2A00"},
    { (char*) "write_char_descriptor",           write_char_descriptor,           3, DELIMIT, NULL, (char*) "<socket_number> <handle> <value>",                                           "Send GATT write characteristic descriptor.           \n\t  Example: write_char_descriptor 10 ABCD"},
    { (char*) "write_long_char_descriptor",      write_long_char_descriptor,      3, DELIMIT, NULL, (char*) "<socket_number> <handle> <value>",                                           "Send GATT write long characteristic descriptor.      \n\t  Example: write_char_descriptor 10 FEEDBEADFBEADFEED"},
    { (char*) "read_char_value",                 read_char_value,                 3, DELIMIT, NULL, (char*) "<socket_number> <handle> <uuid>",                                            "Send GATT read characteristic value.                 \n\t  Example: read_char_value 10 2A00"},
    { (char*) "read_long_char_value",            read_long_char_value,            3, DELIMIT, NULL, (char*) "<socket_number> <handle> <uuid>",                                            "Send GATT read long characteristic value.            \n\t  Example: read_long_char_value 10 2A00"},
    { (char*) "read_char_values_by_uuid",        read_char_values_by_uuid,        2, DELIMIT, NULL, (char*) "<socket_number> <uuid>",                                                     "Send GATT read values by UUID.                       \n\t  Example: read_char_values_by_uuid 2A00"},
    { (char*) "write_char_value",                write_char_value,                3, DELIMIT, NULL, (char*) "<socket_number> <handle> <value>",                                           "Send GATT write characteristic value.                \n\t  Example: write_char_value 10 ABCD"},
    { (char*) "write_long_char_value",           write_long_char_value,           3, DELIMIT, NULL, (char*) "<socket_number> <handle> <value>",                                           "Send GATT write long characteristic value.           \n\t  Example: write_long_char_value 10 FEEDBEADFBEADFEED"},

    CMD_TABLE_END
};

static wiced_bool_t in_process_char = WICED_FALSE;

/* console state */
static wiced_bool_t console_in_tab_tab = WICED_FALSE;


static void (*tab_handling_func)( void ) = console_do_tab;
static cmd_err_t (*newline_handling_func)( void ) = console_do_newline_without_command_repeat;


void app_main( void )
{
    /* turn off buffers, so IO occurs immediately */
    setvbuf( stdin, NULL, _IONBF, 0 );
    setvbuf( stdout, NULL, _IONBF, 0 );
    setvbuf( stderr, NULL, _IONBF, 0 );

    printf( "Bluetooth SmartBridge console app\n" );

    console_init( commands, MAX_LINE_LENGTH, line_buffer, 20, history_buffer_storage);

    while ( 1 )
    {
        console_process_char( getchar( ) );
    }
}

/*!
 ******************************************************************************
 * Load a line of history out of the store into the supplied buffer.
 * To change the way the command history is stored, for example to move it to NVM
 * or flash, patch the 3 History functions.
 *
 * @param[in] line    The number of the line to be loaded.
 * @param[in] buffer  Buffer to load the line into.
 *
 * @return  void
 */

static void history_load_line( uint32_t line, char* buffer )
{
    if ( ( line >= history_num_lines ) )
    {
        buffer[0] = 0;
    }
    else
    {
        uint32_t actual_line = ( history_newest_line + history_length - line ) % history_length;
        strncpy( buffer, &history_buffer[actual_line * console_line_len], console_line_len );
    }
}

/*!
 ******************************************************************************
 * Store a command line into the history. To change the way the command history is
 * stored, for example to move it to NVM or flash, patch the 3 History functions.
 *
 * @param[in] buffer  The line to be stored.
 *
 * @return  void
 */

static void history_store_line( char* buffer )
{
    if ( history_length > 0 )
    {
        history_newest_line = ( history_newest_line + 1 ) % history_length;
        strncpy( &history_buffer[history_newest_line * console_line_len], buffer, console_line_len );
        if ( history_num_lines < history_length )
        {
            history_num_lines++;
        }
    }
}

/*!
 ******************************************************************************
 * Ask the history store how many lines of history are currently stored.
 * To change the way the command history is stored, for example to move it to NVM
 * or flash, patch the 3 History functions.
 *
 * @return  The number of currently stored lines.
 */

static uint32_t history_get_num_lines( void )
{
    return history_num_lines;
}

/*!
 ******************************************************************************
 * Print a single character.
 *
 * @param[in] c  The string to be printed.
 *
 * @return  void
 */

static void send_char( char c )
{
    printf( "%c", c );
}

/*!
 ******************************************************************************
 * Print a string.
 *
 * @param[in] s  The string to be printed.
 *
 * @return  void
 */

static void send_str( char* s )
{
    printf( "%s", s );
}

/*!
 ******************************************************************************
 * Print a string character at a time (gets around eCos not printing strings with non printable characters).
 *
 * @param[in] s  The string to be printed.
 *
 * @return  void
 */

static void send_charstr( char* s )
{
    while ( *s != 0 )
    {
        printf( "%c", *s );
        s++;
    }
}

/*!
 ******************************************************************************
 * Initialise the console functionality.
 *
 * @param[in] command_table      The table of commands that the console will dispatch.
 * @param[in] line_len           Maximum length of an input line.
 * @param[in] buffer             A buffer of line_len size to hold the current command line.
 * @param[in] history_len        Number of lines of command history to retain.
 * @param[in] history_buffer_ptr A buffer of line_len * history_len size to hold the lines of command history, this is optional.
 *
 * @return    void
 */

static void console_init( const command_t* command_table, uint32_t line_len, char* buffer, uint32_t history_len, char* history_buffer_ptr )
{
    console_command_table = command_table;
    console_line_len = line_len;
    console_buffer = buffer;

    console_buffer[0] = 0;
    console_cursor_position = 0;
    console_current_line = 0;
    console_in_esc_seq = WICED_FALSE;
    console_esc_seq_last_char = ' ';

    history_buffer = history_buffer_ptr;
    history_length = history_len;
    history_num_lines = 0;
    history_newest_line = 0;

    send_str( console_prompt_string );
}

/*!
 ******************************************************************************
 * Load a line from the history into the current line and update the screen.
 * This calls HistoryLoadLine to interface with the history store.
 *
 * @param[in] new_line The index of the line to load from the history.
 *
 * @return    void
 */

static void console_load_line( uint32_t new_line )
{
    uint32_t i;
    uint32_t old_len;
    uint32_t new_len;

    old_len = strlen( console_buffer );

    if ( new_line > 0 )
    {
        history_load_line( new_line - 1, console_buffer );
    }
    else
    {
        console_buffer[0] = 0;
    }
    new_len = strlen( console_buffer );

    /* seek to the start of the line */
    send_char('\r');

    /* print out the prompt and new line */
    send_str( console_prompt_string );
    send_str( console_buffer );

    /* write spaces over the rest of the old line */
    for ( i = new_len; i < old_len; i++ )
    {
        send_char( ' ' );
    }

    /* then move back the same amount */
    for ( i = new_len; i < old_len; i++ )
    {
        send_char( '\b' );
    }

    console_current_line = new_line;

    /* position the cursor at the end of the line */
    console_cursor_position = new_len;
}

/*!
 ******************************************************************************
 * Insert a character into the current line at the cursor position and update the screen.
 *
 * @param[in] c  The character to be inserted.
 *
 * @return    void
 */

static void console_insert_char( char c )
{
    uint32_t i;
    uint32_t len = strlen( console_buffer );

    /* move the end of the line out to make space */
    for ( i = len + 1; i > console_cursor_position; i-- )
    {
        console_buffer[i] = console_buffer[i - 1];
    }

    /* insert the character */
    len++;
    console_buffer[console_cursor_position] = c;

    /* print out the modified part of the ConsoleBuffer */
    send_str( &console_buffer[console_cursor_position] );

    /* move the cursor back to where it's supposed to be */
    console_cursor_position++;
    for ( i = len; i > console_cursor_position; i-- )
    {
        send_char( '\b' );
    }
}

/*!
 ******************************************************************************
 * Remove a character from the current line at the cursor position and update the screen.
 *
 * @return    void
 */

static void console_remove_char( void )
{
    uint32_t i;
    uint32_t len = strlen( console_buffer );

    /* back the rest of the line up a character */
    for ( i = console_cursor_position; i < len; i++ )
    {
        console_buffer[i] = console_buffer[i + 1];
    }
    len--;

    /* print out the modified part of the ConsoleBuffer */
    send_str( &console_buffer[console_cursor_position] );

    /* overwrite the extra character at the end */
    send_charstr( (char*) " \b" );

    /* move the cursor back to where it's supposed to be */
    for ( i = len; i > console_cursor_position; i-- )
    {
        send_char( '\b' );
    }
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing a goto home operation.
 *
 * @return  void
 */

static void console_do_home( void )
{
    console_cursor_position = 0;
    send_char('\r');
    send_str(console_prompt_string);
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing an goto end operation.
 *
 * @return  void
 */

static void console_do_end( void )
{
    send_str( &console_buffer[console_cursor_position] );
    console_cursor_position = strlen( console_buffer );
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing an up cursor operation.
 *
 * @return  void
 */

static void console_do_up( void )
{
    if ( console_current_line < history_get_num_lines( ) )
    {
        console_load_line( console_current_line + 1 );
    }
    else
    {
        send_charstr( console_bell_string );
    }
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing a down cursor operation.
 *
 * @return  void
 */

static void console_do_down( void )
{
    if ( console_current_line > 0 )
    {
        console_load_line( console_current_line - 1 );
    }
    else
    {
        send_charstr( console_bell_string );
    }
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing a left cursor operation.
 *
 * @return  void
 */

void console_do_left( void )
{
    if ( console_cursor_position > 0 )
    {
        send_char( '\b' );
        console_cursor_position--;
    }
    else
    {
        send_charstr( console_bell_string );
    }
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing a right cursor operation.
 *
 * @return  void
 */

void console_do_right( void )
{
    if ( console_cursor_position < strlen( console_buffer ) )
    {
        send_char( console_buffer[console_cursor_position] );
        console_cursor_position++;
    }
    else
    {
        send_charstr( console_bell_string );
    }
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing a delete operation.
 *
 * @return  void
 */

void console_do_delete( void )
{
    if ( console_cursor_position < strlen( console_buffer ) )
    {
        console_current_line = 0;
        console_remove_char( );
    }
    else
    {
        send_charstr( console_bell_string );
    }
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing a backspace operation.
 *
 * @return  void
 */

void console_do_backspace( void )
{
    if ( console_cursor_position > 0 )
    {
        console_current_line = 0;
        console_cursor_position--;
        send_char( '\b' );
        console_remove_char( );
    }
    else
    {
        send_charstr( console_bell_string );
    }
}

/*!
 ******************************************************************************
 * Handle a character or escape sequence representing a new line operation.
 *
 * If the current line is not empty then it's contents will be parsed and invoked.
 * If the current line is empty then the last command will be reinvoked.
 *
 * @return    Console error code indicating if the command was parsed and run correctly.
 */

cmd_err_t console_do_enter( void )
{
    cmd_err_t err = ERR_CMD_OK;

    console_current_line = 0;
    send_str( (char*) "\n" );

    if ( strlen( console_buffer ) ) /* if theres something in the buffer */
    {
        if ( strlen( console_buffer ) > strspn( console_buffer, console_delimit_string ) ) /* and it's not just delimit characters */
        {
            /* store it and invoke the command parser */
            history_store_line( console_buffer );
            err = console_parse_cmd( console_buffer );
        }
    }
    else
    {
        if ( history_get_num_lines( ) ) /* if theres a previous history line */
        {
            /* load it and invoke the command parser */
            history_load_line( 0, console_buffer );
            err = console_parse_cmd( console_buffer );
        }
    }

    /* prepare for a new line of entry */
    console_buffer[0] = 0;
    console_cursor_position = 0;
    send_str( console_prompt_string );

    return err;
}

/*!
 ******************************************************************************
 * Process escape sequences.
 *
 * This function doesn't process the sequences properly, but does handle all the common
 * ones. The ConsoleEscSeqLastChar variable is used to parse vt320 like sequences
 * without implementing sequence paramater parsing. In general the function doesn't
 * distinguish between the esc-[ and esc-O codes. Despite this the console functions are
 * setup to make sure that any failure doesn't produce non printable characters, so failures
 * should be soft.
 *
 * If you want to handle additional escape sequences then create a wraper patch that has
 * essentially the same code structure as this function and calls this one in its default case(s).
 *
 * @param[in] c  The incomming character.
 *
 * @return    Boolean indicating if more characters are needed to complete the sequence.
 */

wiced_bool_t console_process_esc_seq( char c )
{
    wiced_bool_t still_in_esc_seq = WICED_FALSE;

    switch ( c )
    {
        case ';':
        case '[':
        case 'O':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            still_in_esc_seq = WICED_TRUE;
            break;
        case 'A': /* up arrow */
            console_do_up( );
            break;
        case 'B': /* down arrow */
            console_do_down( );
            break;
        case 'C': /* right arrow */
            console_do_right( );
            break;
        case 'D': /* left arrow */
            console_do_left( );
            break;
        case 'F': /* end */
            console_do_end( );
            break;
        case 'H': /* home */
            console_do_home( );
            break;
        case '~': /* vt320 style control codes */
            switch ( console_esc_seq_last_char )
            {
                case '1': /* home */
                    console_do_home( );
                    break;
                case '3': /* delete */
                    console_do_delete( );
                    break;
                case '4': /* end */
                    console_do_end( );
                    break;
                default:
                    send_charstr( console_bell_string ); /* unknown escape sequence we are probably in trouble, beep and pretend it's going to be ok */
                    break;
            }
            break;
        default:
            send_charstr( console_bell_string ); /* unknown escape sequence we are probably in trouble, beep and pretend it's going to be ok */
            break;
    }

    console_esc_seq_last_char = c;

    return still_in_esc_seq;
}

/*!
 ******************************************************************************
 * Break an input line into command and arguments and invoke it.
 *
 * @param[in] line  The line of input.
 *
 * @return    Console error code indicating if the command was parsed correctly.
 */

cmd_err_t console_parse_cmd( char* line )
{
    const command_t* cmd_ptr = NULL;
    char* params[console_max_params];
    uint32_t param_cnt = 0;

    cmd_err_t err = ERR_CMD_OK;

    /* First call to strtok. */
    params[param_cnt++] = strtok( line, console_delimit_string );

    if ( params[0] == NULL ) /* no command entered */
    {
        err = ERR_NO_CMD;
    }
    else
    {
        /* find the command */
        for ( cmd_ptr = console_command_table; cmd_ptr->name != NULL; cmd_ptr++ )
        {
            if ( strcmp( params[0], cmd_ptr->name ) == 0 )
            {
                break;
            }
        }

        if ( cmd_ptr->name == NULL )
        {
            err = ERR_UNKNOWN_CMD;
        }
        else
        {
            /* determine argument delimit string */
            char* delimit;
            if ( cmd_ptr->delimit != NULL )
            {
                delimit = cmd_ptr->delimit;
            }
            else
            {
                delimit = console_delimit_string;
            }

            /* parse arguments */
            while ( ( ( params[param_cnt] = strtok( NULL, delimit ) ) != NULL ) )
            {
                param_cnt++;
                if ( param_cnt > ( console_max_params - 1 ) )
                {
                    err = ERR_TOO_MANY_ARGS;
                    break;
                }
            }

            /* check arguments */
            if ( ( param_cnt - 1 ) < cmd_ptr->arg_count )
            {
                err = ERR_INSUFFICENT_ARGS;
            }

            /* run command */
            if ( ( err == ERR_CMD_OK ) && ( cmd_ptr->command != NULL ) )
            {
                err = (cmd_err_t) cmd_ptr->command( param_cnt, params );
            }
        }

        /* process errors */
        if ( err != ERR_CMD_OK )
        {
            if ((err <=0) && (err > ERR_LAST_ERROR))
            {
                printf( "ERROR: %s\n", console_error_strings[-err] );
            }
            if ( err != ERR_UNKNOWN_CMD )
            {
                printf("Usage: %s %s\n",cmd_ptr->name, cmd_ptr->format);
            }
        }
    }

    return err;
}

/*!
 ******************************************************************************
 * Convert a hexidecimal string to an integer.
 *
 * @param[in] hex_str  The string containing the hex value.
 *
 * @return    The value represented by the string.
 */

int hex_str_to_int( const char* hex_str )
{
    int n = 0;
    uint32_t value = 0;
    int shift = 7;
    while ( hex_str[n] != '\0' && n < 8 )
    {
        if ( hex_str[n] > 0x21 && hex_str[n] < 0x40 )
        {
            value |= ( hex_str[n] & 0x0f ) << ( shift << 2 );
        }
        else if ( ( hex_str[n] >= 'a' && hex_str[n] <= 'f' ) || ( hex_str[n] >= 'A' && hex_str[n] <= 'F' ) )
        {
            value |= ( ( hex_str[n] & 0x0f ) + 9 ) << ( shift << 2 );
        }
        else
        {
            break;
        }
        n++;
        shift--;
    }

    return ( value >> ( ( shift + 1 ) << 2 ) );
}

/*!
 ******************************************************************************
 * Convert a decimal or hexidecimal string to an integer.
 *
 * @param[in] str  The string containing the value.
 *
 * @return    The value represented by the string.
 */

int str_to_int( const char* str )
{
    uint32_t val = 0;
    if ( strncmp( str, "0x", 2 ) == 0 )
    {
        val = hex_str_to_int( str + 2 );
    }
    else
    {
        val = atoi( str );
    }
    return val;
}

/*!
 ******************************************************************************
 * Convert an ip string to a uint32_t.
 *
 * @param[in] arg  The string containing the value.
 *
 * @return    The value represented by the string.
 */
uint32_t str_to_ip( char* arg )
{
    int temp[4];

    sscanf( arg, "%d.%d.%d.%d", &temp[0], &temp[1], &temp[2], &temp[3] );
    return temp[0] << 24 | temp[1] << 16 | temp[2] << 8 | temp[3];
}

/*!
 ******************************************************************************
 * Convert a security authentication type string to a wiced_security_t.
 *
 * @param[in] arg  The string containing the value.
 *
 * @return    The value represented by the string.
 */
wiced_security_t str_to_authtype( char* arg )
{
    if ( strcmp( arg, "open" ) == 0 )
    {
        return WICED_SECURITY_OPEN;
    }
    else if ( strcmp( arg, "wep" ) == 0 )
    {
        return WICED_SECURITY_WEP_PSK;
    }
    else if ( strcmp( arg, "wpa2aes" ) == 0 )
    {
        return WICED_SECURITY_WPA2_AES_PSK;
    }
    else if ( strcmp( arg, "wpa2" ) == 0 )
    {
        return WICED_SECURITY_WPA2_MIXED_PSK;
    }
    else if ( strcmp( arg, "wpa" ) == 0 )
    {
        return WICED_SECURITY_WPA_TKIP_PSK;
    }
    else
    {
        WPRINT_APP_INFO(("Bad auth type: '%s'\n", arg));
        return WICED_SECURITY_UNKNOWN;
    }
}

/*!
 ******************************************************************************
 * Convert a MAC string (xx:xx:xx:xx:xx) to a wiced_mac_t.
 *
 * @param[in] arg  The string containing the value.
 *
 * @return    The value represented by the string.
 */
wiced_mac_t str_to_mac( char* arg )
{
    wiced_mac_t mac = { { 0, 0, 0, 0, 0, 0 } };
    char* start = arg;
    char* end;
    int a = 0;
    do
    {
        end = strchr( start, ':' );
        if ( end != NULL )
        {
            *end = '\0';
        }
        mac.octet[a] = (uint8_t) hex_str_to_int( start );
        if ( end != NULL )
        {
            *end = ':';
            start = end + 1;
        }
        ++a;
    } while ( a < 6 && end != NULL );
    return mac;
}

/*!
 ******************************************************************************
 * Default help function.
 *
 * @param[in] argc  The number of arguments.
 * @param[in] argv  The arguments.
 *
 * @return    Console error code indicating if the command ran correctly.
 */

/* default help function */
int help_command( int argc, char* argv[] )
{
    const command_t* cmd_ptr;
    cmd_err_t err = ERR_CMD_OK;
    uint32_t eg_sel = 0;

    switch ( argc )
    {
        case 0:
        case 1:
            printf( "Console Commands:\n" );
            for ( cmd_ptr = console_command_table; cmd_ptr->name != NULL; cmd_ptr++ )
            {
                if ( ( cmd_ptr->format != NULL ) || ( cmd_ptr->brief != NULL ) )
                {
                    if ( cmd_ptr->format != NULL )
                    {
                        printf( "    %s%c%s\n", cmd_ptr->name, console_delimit_string[0], cmd_ptr->format );
                    }
                    else
                    {
                        printf( "    %s\n", cmd_ptr->name );
                    }

                    if ( cmd_ptr->brief != NULL )
                    {
                        printf( "        - %s\n", cmd_ptr->brief );
                    }
                }
            }
            break;
        default: /* greater than 2 */
            eg_sel = str_to_int( argv[2] );
            /* Disables Eclipse static analysis warning */
            /* Intentional drop through */
            /* no break */
        case 2:
            err = ERR_UNKNOWN_CMD;
            for ( cmd_ptr = console_command_table; cmd_ptr->name != NULL; cmd_ptr++ )
            {
                if ( strcmp( argv[1], cmd_ptr->name ) == 0 )
                {
                    if ( cmd_ptr->help_example != NULL )
                    {
                        err = cmd_ptr->help_example( argv[1], eg_sel );
                    }
                    else
                    {
                        err = ERR_CMD_OK;
                        printf( "No example available for %s\n\n", argv[1] );
                    }
                }
            }
            break;
    }

    return err;
}

/*!
 ******************************************************************************
 * Redraw the console prompt.
 *
 * This should be called to restore the console prompt after any asynchronous prints.
 *
 * @return    void
 */

void console_redraw( void )
{
    if ( !in_process_char )
    {
        /* print out the prompt and any entered characters */
        printf( "\n%s%s", console_prompt_string, console_buffer );
    }
}


#if 0
/*!
 ******************************************************************************
 * Process regular characters.
 *
 * Wrapper patch to ConsoleProcessChar for the purpose of doing tab completion.
 *
 * @param[in] c  The incomming character.
 *
 * @return    Console error code indicating if the character and any resulting command were processed correctly.
 */

cmd_err_t console_process_char_with_tab_complete( char c )
{
    tab_handling_func = console_do_tab;
    return console_process_char( c );
}
#endif
/*!
 ******************************************************************************
 * Handle a character or escape sequence representing a tab completion operation.
 *
 * @return  void
 */

void console_do_tab( void )
{
    uint32_t buf_len = strlen( console_buffer );
    const command_t* cmd_ptr = NULL;

    console_do_end( );

    if ( console_in_tab_tab == WICED_FALSE )
    {
        char *src = NULL;
        wiced_bool_t single_match = WICED_FALSE;
        uint32_t len = 0;

        /* for each where the buffer matches it's start */
        for ( cmd_ptr = console_command_table; cmd_ptr->name != NULL; cmd_ptr++ )
        {
            if ( strncmp( cmd_ptr->name, console_buffer, buf_len ) == 0 )
            {
                /* if we already have one or more matches then the completion is the longest common prefix */
                if ( src )
                {
                    single_match = WICED_FALSE;
                    uint32_t i = buf_len;
                    while ( ( i < len ) && ( src[i] == cmd_ptr->name[i] ) )
                    {
                        i++;
                    }
                    len = i;
                }
                /* for the first match the completion is the whole command */
                else
                {
                    single_match = WICED_TRUE;
                    src = cmd_ptr->name;
                    len = strlen( cmd_ptr->name );
                }
            }
        }

        /* if there is a valid completion then add it to the buffer */
        if ( src && ( ( len > strlen( console_buffer ) ) || single_match ) )
        {
            uint32_t i;
            for ( i = buf_len; i < len; i++ )
            {
                console_insert_char( src[i] );
            }
            if ( single_match )
            {
                console_insert_char( ' ' );
            }
        }
        else
        {
            console_in_tab_tab = WICED_TRUE;
        }
    }
    else
    {
        uint32_t cnt = 0;

        for ( cmd_ptr = console_command_table; cmd_ptr->name != NULL; cmd_ptr++ )
        {
            if ( ( strncmp( cmd_ptr->name, console_buffer, buf_len ) == 0 ) && ( strcmp( cmd_ptr->name, "" ) != 0 ) )
            {
                if ( ( cnt % 4 ) == 0 )
                {
                    send_str( (char*) "\n" );
                }
                else
                {
                    send_char( ' ' );
                }
                printf( "%-19.19s", cmd_ptr->name );
                cnt++;
            }
        }
        if ( cnt )
        {
            send_str( (char*) "\n" );
            send_str( console_prompt_string );
            send_str( console_buffer );
        }
        console_in_tab_tab = WICED_FALSE;
    }
}

/*!
 ******************************************************************************
 * Version of the console newline handler that does not repeat previous command
 * when newline pressed on empty line.
 */

cmd_err_t console_do_newline_without_command_repeat( void )
{
    cmd_err_t err = ERR_CMD_OK;

    if ( strlen( console_buffer ) ) /* if theres something in the buffer */
    {
        err = console_do_enter( );
    }
    else
    {
        /* prepare for a new line of entry */
        console_buffer[0] = 0;
        console_cursor_position = 0;
        send_str( (char*) "\n" );
        send_str( console_prompt_string );
    }
    return err;
}

/*!
 ******************************************************************************
 * Process regular characters.
 *
 * If you want to handle additional characters then create a wraper patch that has essentially
 * the same code structure as this function and calls this one in its default case(s).
 *
 * @param[in] c  The incoming character.
 *
 * @return    Console error code indicating if the character and any resulting command were processed correctly.
 */

cmd_err_t console_process_char( char c )
{
    cmd_err_t err = ERR_CMD_OK;
    in_process_char = WICED_TRUE;

    /* printf("ConsoleProcessChar\n"); */
    if ( console_in_esc_seq )
    {
        console_in_esc_seq = console_process_esc_seq( c );
    }
    else
    {
        /* printf("%lu %lu\n", (uint32_t)c, (uint32_t)'\b'); */
        switch ( c )
        {
            case 9: /* tab char */
                if ( tab_handling_func )
                {
                    tab_handling_func( );
                }
                break;
            case 10: /* line feed */
                /* ignore it */
                break;
            case '\r': /* newline */
                if ( newline_handling_func )
                {
                    err = newline_handling_func( );
                }
                break;
            case 27: /* escape char */
                console_in_esc_seq = WICED_TRUE;
                break;
            case '\b': /* backspace */
            case '\x7F': /* backspace */
                console_do_backspace( );
                break;
            case 16: /* ctrl-p */
                console_do_up( );
                break;
            case 14: /* ctrl-n */
                console_do_down( );
                break;
            case 2: /* ctrl-b */
                console_do_left( );
                break;
            case 6: /* ctrl-f */
                console_do_right( );
                break;
            case 1: /* ctrl-a */
                console_do_home( );
                break;
            case 5: /* ctrl-e */
                console_do_end( );
                break;
            case 4: /* ctrl-d */
                console_do_delete( );
                break;
            default:
                if ( ( c > 31 ) && ( c < 127 ) )
                { /* limit to printables */
                    if ( strlen( console_buffer ) + 1 < console_line_len )
                    {
                        console_current_line = 0;
                        console_insert_char( c );
                    }
                    else
                    {
                        send_charstr( console_bell_string );
                    }
                }
                else
                {
                    send_charstr( console_bell_string );
                }
                break;
        }
    }

    in_process_char = WICED_FALSE;
    return err;
}
