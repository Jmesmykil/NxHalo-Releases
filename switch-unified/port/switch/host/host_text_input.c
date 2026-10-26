/* Switch applet text entry. All guest pointers stay explicit ILP32 values. */
#include "host.h"
#include <switch.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

int host_text_input(uint32_t initial_address, uint32_t output_address, uint32_t capacity)
{
    char initial[1024], output[1024];
    SwkbdConfig keyboard;
    size_t length;
    Result result;
    if (capacity < 2 || capacity > sizeof(output) ||
        !host_low_owns(output_address, capacity) || !initial_address)
        return -1;
    for (length = 0; length < sizeof(initial); ++length) {
        if (initial_address > UINT32_MAX - length ||
            !host_low_owns(initial_address + (uint32_t)length, 1)) return -1;
        initial[length] = *(const char *)(uintptr_t)(initial_address + length);
        if (!initial[length]) break;
    }
    if (length == sizeof(initial)) return -1;
    result = swkbdCreate(&keyboard, 0);
    if (R_FAILED(result)) {
        printf("[text_input] swkbdCreate failed: 0x%08x\n", result);
        return -1;
    }
    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetType(&keyboard, SwkbdType_QWERTY);
    swkbdConfigSetHeaderText(&keyboard, "NxHalo custom content");
    swkbdConfigSetGuideText(&keyboard, "Enter a map name, locator link, or SD file path");
    swkbdConfigSetInitialText(&keyboard, initial);
    swkbdConfigSetStringLenMax(&keyboard, capacity > 501 ? 500 : capacity - 1);
    swkbdConfigSetStringLenMin(&keyboard, 1);
    memset(output, 0, sizeof(output));
    result = swkbdShow(&keyboard, output, capacity);
    swkbdClose(&keyboard);
    /* libnx reports the keyboard's nonzero CloseResult (Back/Cancel) here. */
    if (result == MAKERESULT(Module_Libnx, LibnxError_LibAppletBadExit)) {
        printf("[text_input] keyboard closed without accepted input\n");
        return 0;
    }
    if (R_FAILED(result)) {
        printf("[text_input] swkbdShow failed: 0x%08x\n", result);
        return -1;
    }
    output[capacity - 1] = 0;
    if (!output[0]) return 0;
    host_memory_watch_prepare_write(output_address, (uint32_t)strlen(output) + 1);
    memcpy((void *)(uintptr_t)output_address, output, strlen(output) + 1);
    return 1;
}
