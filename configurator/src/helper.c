#include "include/helper.h"

static __uint16_t spinner_loop = 0;
static char spinner_chars[] = {'\\', '|', '/', '-'};

#ifdef _DEBUG
#include <ctype.h>

void dump_hex(const void *data, size_t size)
{
    locate(0, 24);
    const unsigned char *byte_data = data;
    // Print hex values
    for (size_t i = 0; i < size; i++)
    {
        printf("%02x ", byte_data[i]);
    }

    printf("  "); // Some space between hex and chars

    // Print characters
    for (size_t i = 0; i < size; i++)
    {
        // Check if the character is printable; if not, print a dot
        char ch = isprint(byte_data[i]) ? byte_data[i] : '.';
        printf("%c", ch);
    }
}
#endif

// Function to convert a character to lowercase
char to_lowercase(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + 32;
    }
    return c;
}

int get_number_within_range(char *prompt, __uint8_t num_items, __uint8_t first_value, char cancel_char, char save_char)
{
    char input[3];
    int number;

    while (1)
    {
        // Prompt the user
        printf(prompt);

        // Get the input as a string (this is to handle empty input)
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            return -1; // Use -1 or another value to indicate that no valid number was received
        }

        // Check if the string is empty or contains only the newline character
        int i = 0;
        while (input[i] != '\0')
        {
            if (input[i] != '\n' && input[i] != ' ')
            {
                break;
            }
            i++;
        }

        if (save_char != '\0')
        {
            if (to_lowercase(input[i]) == to_lowercase(save_char))
            {
                return num_items + first_value; // If returned the number of elements plus first_value, then save command was selected
            }
        }

        if (cancel_char != '\0')
        {
            if (to_lowercase(input[i]) == to_lowercase(cancel_char))
            {
                return -1; // Use -1 or another value to indicate that no valid number was received
            }
        }
        else
        {
            if (input[i] == '\0' || input[i] == '\n')
            {
                return -1; // Use -1 or another value to indicate that no valid number was received
            }
        }

        // Convert the string input to an integer
        if (sscanf(input, "%d", &number) == 1)
        {
            if (number >= first_value && number <= num_items)
            {
                // If the number is within the desired range, return the number
                return number;
            }
        }

        // If out of range or not a valid number, print an error message
        printf("Invalid input! Please enter a number between %d and %d.\n", first_value, num_items);
    }
}

int send_command(__uint16_t command, void *payload, __uint16_t payload_size)
{
    if (payload_size % 2 != 0)
    {
        payload_size++;
    }
    __uint32_t rom3_address = ROM3_MEMORY_START;
    __uint8_t command_header = *((volatile __uint8_t *)(rom3_address + PROTOCOL_HEADER));
    __uint8_t command_code = *((volatile __uint8_t *)(rom3_address + command));
    __uint8_t command_payload_size = *((volatile __uint8_t *)(rom3_address + payload_size)); // Always even!

#ifdef _DEBUG
    printf("ROM3 memory address: 0x%08X\r\n", rom3_address);
    printf("Payload size: %d\r\n", payload_size);
    printf("Command header: 0x%02X\r\n", command_header);
    printf("Command code: 0x%02X\r\n", command_code);
    printf("Command payload size: 0x%02X\r\n", command_payload_size);
#endif

    if ((payload_size > 0) && (payload != NULL))
    {

        for (__uint8_t i = 0; i < payload_size; i += 2)
        {
            __uint8_t value = *((volatile __uint8_t *)(rom3_address + *((__uint16_t *)(payload + i))));
#ifdef _DEBUG
            printf("Payload[%i]: 0x%04X / 0x%02X\r\n", i, *((__uint16_t *)(payload + i)), value);
#endif
        }
    }
    return 0;
}

void sleep_seconds(__uint8_t seconds, bool silent)
{
    for (__uint8_t j = 0; j < seconds; j++)
    {
        // Assuming PAL system; for NTSC, use 60.
        for (__uint8_t i = 0; i < 50; i++)
        {
            if (!silent)
                spinner(1);
            Vsync(); // Wait for VBL interrupt
        }
    }
}

void spinner(__uint16_t spinner_update_frequency)
{
    if (spinner_loop % spinner_update_frequency == 0)
    {
        printf("\b%c", spinner_chars[(spinner_loop / spinner_update_frequency) % 4]);
    }
    spinner_loop++;
}

void please_wait(char *message, __uint8_t seconds)
{
    printf(message); // Show the message
    printf(" ");     // Leave a space for the spinner
    sleep_seconds(seconds, false);
}

void please_wait_silent(__uint8_t seconds)
{
    sleep_seconds(seconds, true);
}

char *read_files_from_memory(__uint8_t *memory_location)
{
    __uint8_t *current_ptr = memory_location;
    __uint16_t total_size = 0;

    // Calculate total size required
    while (1)
    {
        // If we encounter two consecutive 0x00 bytes, it means we're at the end of the list
        if (*current_ptr == 0x00 && *(current_ptr + 1) == 0x00)
        {
            total_size += 2; // For two consecutive null characters
            break;
        }

        total_size++;
        current_ptr++;
    }

    // Allocate memory for the file list
    char *output_array = (char *)malloc(total_size * sizeof(char));
    if (!output_array)
    {
        // Allocation failed
        return NULL;
    }

    // Copy the memory content to the output array
    for (int i = 0; i < total_size; i++)
    {
        output_array[i] = memory_location[i];
    }

    return output_array;
}

__uint8_t get_file_count(char *file_array)
{
    __uint8_t count = 0;
    char *current_ptr = file_array;

    while (*current_ptr)
    { // as long as we don't hit the double null terminator
        count++;

        // skip past the current filename to the next
        while (*current_ptr)
            current_ptr++;
        current_ptr++; // skip the null terminator for the current filename
    }

    return count;
}

char *print_file_at_index(char *current_ptr, __uint8_t index, int num_columns)
{

    __uint8_t current_index = 0;
    while (*current_ptr)
    { // As long as we don't hit the double null terminator
        if (current_index == index)
        {
            int chars_printed = 0; // To keep track of how many characters are printed

            while (*current_ptr)
            {
                putchar(*current_ptr);
                current_ptr++;
                chars_printed++;
            }

            // If num_columns is provided, fill the rest of the line with spaces
            if (num_columns > 0)
            {
                for (; chars_printed < num_columns; chars_printed++)
                {
                    putchar(' ');
                }
            }

            putchar('\r');
            putchar('\n');
            return current_ptr;
        }

        // Skip past the current filename to the next
        while (*current_ptr)
            current_ptr++;
        current_ptr++; // Skip the null terminator for the current filename

        current_index++;
    }

    printf("No file found at index %d\r\n", index);
    return current_ptr;
}

int display_paginated_content(char *file_array, int num_files, int page_size, char *item_name)
{
    void highlight_and_print(char *file_array, __uint8_t index, __uint8_t offset, int current_line, int num_columns, bool highlight)
    {
        locate(0, current_line + 2 + index - offset);
        if (highlight)
            printf("\033p"); // Enter reverse video mode (VT52)
        printf("\033K");     // Erase to end of line (VT52)
        print_file_at_index(file_array, index, num_columns);
        if (highlight)
            printf("\033q"); // Exit reverse video mode (VT52)
    }

    int selected_rom = -1;
    int page_number = 0; // Page number starts at 0
    int current_index = 0;
    locate(0, 22);
    printf("Use [UP] and [DOWN] arrows to select. [LEFT] and [RIGHT] to paginate.\r\n");
    printf("Press [ENTER] to load it. [ESC] to return to main menu.");

    while (selected_rom < 0)
    {
        int start_index = page_number * page_size;
        int end_index = start_index + page_size - 1;
        int max_page = (num_files / page_size);

        if (start_index >= num_files)
        {
            printf("No content for this page number!\r\n");
            return -1;
        }

        if (end_index >= num_files)
        {
            end_index = num_files - 1;
        }

        char *current_ptr = file_array;
        int index = 0;
        int current_line = 2;
        locate(0, current_line);
        printf("\033K"); // Erase to end of line (VT52)
        printf("%s found: %d. ", item_name, num_files);
        printf("Page %d of %d\r\n", page_number + 1, max_page + 1);
        locate(0, current_line + 1);
        printf("\033K"); // Erase to end of line (VT52)
        while (index <= num_files)
        {
            if ((start_index <= index) && (index <= end_index))
            {
                // Print the index number
                locate(0, current_line + 2 + index - page_size * page_number);
                current_ptr = print_file_at_index(file_array, index, 80);
                index++;
            }
            else
            {
                // Skip past the current filename to the next
                while (*current_ptr != 0x00)
                    current_ptr++;
                current_ptr++; // skip the null terminator for the current filename
                index++;
            }
        }
        for (int i = index; i <= page_size * (page_number + 1); i++)
        {
            // THe -1 at the begining it's because of the previous increment
            locate(0, -1 + current_line + 2 + i - (page_size * page_number));
            printf("\033K"); // Erase to end of line (VT52)
        }

        long key;
        bool change_page = false;
        while ((selected_rom < 0) && (!change_page))
        {
            highlight_and_print(file_array, current_index, page_size * page_number, current_line, 80, true);
            key = Crawcin();
            switch (key)
            {
            case KEY_UP_ARROW:
                if (current_index > start_index)
                {
                    highlight_and_print(file_array, current_index, page_size * page_number, current_line, 80, false);
                    current_index = current_index - 1;
                }
                break;

            case KEY_DOWN_ARROW:
                if (current_index < end_index)
                {
                    highlight_and_print(file_array, current_index, page_size * page_number, current_line, 80, false);
                    current_index = current_index + 1;
                }
                break;
            case KEY_LEFT_ARROW:
                if (page_number > 0)
                {
                    page_number--;
                    current_index = page_number * page_size;
                    change_page = true;
                }
                break;
            case KEY_RIGHT_ARROW:
                if (page_number < max_page)
                {
                    page_number++;
                    current_index = page_number * page_size;
                    change_page = true;
                }
                break;
            case KEY_ENTER:
                selected_rom = current_index + 1;
                return selected_rom;
            case KEY_ESC:
                return -1;
            default:
                // Handle other keys
                break;
            }

#ifdef _DEBUG
            locate(0, 20);
            printf("Key pressed: 0x%04X\r\n", key);
            locate(0, 21);
            printf("Current index: %d\r\n", current_index);
#endif
        }
    }
}

void print_centered(const char *str, int screen_width)
{
    int len = strlen(str);
    if (len >= screen_width)
    {
        printf("%s", str);
    }
    else
    {
        int padding = (screen_width - len) / 2; // calculate padding needed
        for (int i = 0; i < padding; i++)
        {
            printf(" "); // print padding
        }
        printf("%s", str);
    }
}