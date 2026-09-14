#include "log.h"
#include "../framebuffer/framebuffer_console.h"
#include "../drivers/serial/serial.h"

#include <stdarg.h>

#define LOG_MAX_LINES 10000
#define LOG_LINE_LENGTH 180

static char log_lines[LOG_MAX_LINES][LOG_LINE_LENGTH];
static int log_line_count = 0;
static int console_initialized = 0;

void log_init(void) {
    log_line_count = 0;
}

void kernel_log(const char* text, ...) {
    if (log_line_count >= LOG_MAX_LINES) {
        for (int i = 1; i < LOG_MAX_LINES; i++) {
            for (int j = 0; j < LOG_LINE_LENGTH; j++) {
                log_lines[i - 1][j] = log_lines[i][j];
            }
        }

        log_line_count = LOG_MAX_LINES - 1;
    }

    int text_position = 0;
    int output_position = 0;

    va_list args;
    va_start(args, text);

    while (text[text_position] != '\0' && output_position < LOG_LINE_LENGTH - 1) {
        if (text[text_position] != '%') {
            log_lines[log_line_count][output_position] = text[text_position];
            text_position++;
            output_position++;
            continue;
        }

        text_position++;

        if (text[text_position] == 'u') {
            uint32_t number = va_arg(args, uint32_t);
            char number_buffer[11];
            int number_length = 0;

            if (number == 0) {
                number_buffer[number_length++] = '0' + (number % 10);
                number /= 10;
            }
            else{
                while (number > 0) {
                    number_buffer[number_length++] = '0' + (number % 10);
                    number /= 10;
                }
            }

            for (int i = number_length - 1; i >= 0; i--) {
                if (output_position >= LOG_LINE_LENGTH - 1) {
                    break;
                }

                log_lines[log_line_count][output_position++] = number_buffer[i];
            }
        }
        if (text[text_position] == 'x') {
            uint32_t number = va_arg(args, uint32_t);
            char hex_buffer[8];
            int hex_length = 0;

            if (output_position < LOG_LINE_LENGTH - 1) {
                log_lines[log_line_count][output_position++] = '0';
            }

            if (output_position < LOG_LINE_LENGTH - 1) {
                log_lines[log_line_count][output_position++] = 'x';
            }

            if (number == 0) {
                if (output_position < LOG_LINE_LENGTH - 1) {
                    log_lines[log_line_count][output_position++] = '0';
                }
            }
            else{
                while (number > 0 && hex_length < 8) {
                    uint32_t digit = number & 0xF;

                    if (digit < 10) {
                        hex_buffer[hex_length++] = '0' + digit;
                    }
                    else {
                        hex_buffer[hex_length++] = 'a' + (digit - 10);
                    }

                    number >>= 4;
                }
            }

            for (int i = hex_length - 1; i >= 0; i--) {
                if (output_position >= LOG_LINE_LENGTH - 1) {
                    break;
                }

                log_lines[log_line_count][output_position++] = hex_buffer[i];
            }
        }
        else if (text[text_position] == 'd') {
            int value = va_arg(args, int);

            if (value == 0) {
                if (output_position < LOG_LINE_LENGTH - 1) {
                    log_lines[log_line_count][output_position++] = '0';
                }
            }

            if (value < 0) {
                if (output_position < LOG_LINE_LENGTH - 1) {
                    log_lines[log_line_count][output_position++] = '-';
                }

                value = -value;
            }

            char digits[12];
            int digit_count = 0;

            while (value > 0 && digit_count < 11) {
                digits[digit_count++] = '0' + (value % 10);
                value /= 10;
            }

            while (digit_count > 0) {
                if (output_position >= LOG_LINE_LENGTH -1) {
                    break;
                }

                log_lines[log_line_count][output_position++] = digits[--digit_count];
            }
        }
        else if (text[text_position] == 's') {
            char* string = va_arg(args, char*);

            for (int i = 0; string[i] != '\0'; i++) {
                if (output_position >= LOG_LINE_LENGTH - 1) {
                    break;
                }

                log_lines[log_line_count][output_position++] = string[i];
            }
        }
        else if (text[text_position] == '%') {
            log_lines[log_line_count][output_position++] = '%';
        }

        text_position++;
    }

    va_end(args);

    if (console_initialized && output_position > 0) {
        framebuffer_console_print(log_lines[log_line_count]);
        framebuffer_console_print("\n");
    }

    log_lines[log_line_count][output_position] = '\0';
    log_line_count++;
}

void log_init_console(void) {
    framebuffer_console_init();
    console_initialized = 1;
}

void log_disable_console(void) {
    console_initialized = 0;
}

int log_count(void) {
    return log_line_count;
}

const char* log_get_line(int index) {
    if (index < 0) {
        return "";
    }
    else if (index >= log_line_count) {
        return "";
    }
    else {
        return log_lines[index];
    }
}

void log_dump_serial(void) {
    serial_write_string("==== KERNEL LOG DUMP START ====\n");

    for (int i = 0; i < log_line_count; i++) {
        serial_write_string(log_lines[i]);
        serial_write_string("\n");
    }

    serial_write_string("==== KERNEL LOG DUMP END ====\n");
}