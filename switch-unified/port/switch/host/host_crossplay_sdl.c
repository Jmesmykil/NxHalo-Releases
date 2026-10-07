#include "host.h"
#include <SDL.h>
#include <switch.h>
#include <stdio.h>
#include <string.h>
int host_sdl_set_clipboard_text(const char *text) { return text && SDL_SetClipboardText(text) == 0; }
void host_sdl_get_clipboard_text(char *buffer, uint32_t size) {
    char *text;
    if (!buffer || !size) return;
    buffer[0] = 0; text = SDL_GetClipboardText();
    if (text) { strncpy(buffer, text, size - 1); buffer[size - 1] = 0; SDL_free(text); }
}
int host_sdl_show_toast(const char *message, int duration, int gravity, int x, int y) {
    (void)duration; (void)gravity; (void)x; (void)y;
    if (message) host_logf(3, "[toast] %s", message);
    return 0;
}
int host_sdl_show_simple_message_box(uint32_t flags, const char *title, const char *message) {
    return SDL_ShowSimpleMessageBox(flags, title, message, NULL) == 0;
}

void host_sdl_scancode_name(int scancode, char *buffer, uint32_t size) {
    if (buffer && size) snprintf(buffer, size, "%s", SDL_GetScancodeName((SDL_Scancode)scancode));
}
int host_sdl_scancode_from_name(const char *name) { return (int)SDL_GetScancodeFromName(name); }
int host_sdl_show_message_box(uint32_t flags, const char *title, const char *message, int count,
                             const uint32_t *button_flags, const int *ids, const uint32_t *texts) {
    if (count < 0 || count > 8 || (count > 0 && (!button_flags || !ids || !texts))) return -1;
    /* Guest pointers are 32-bit mapped addresses, not host SDL object
     * handles. SDL consumes the text during this blocking call; none escape. */
    SDL_MessageBoxButtonData buttons[8];
    for (int i = 0; i < count; i++) {
        buttons[i].flags = button_flags[i]; buttons[i].buttonid = ids[i];
        buttons[i].text = (const char *)(uintptr_t)texts[i];
    }
    SDL_MessageBoxData data = { flags, NULL, title, message, count, buttons, NULL };
    int choice = -1;
    return SDL_ShowMessageBox(&data, &choice) == 0 ? choice : -1;
}
int host_sdl_open_url(const char *url) {
    WebCommonConfig config;
    if (!url) return 0;
    if (strncmp(url, "https://", 8)) {
        SDL_SetError("Switch browser supports HTTPS links only; use the SD card import menu for local files");
        return 0;
    }
    Result rc = webPageCreate(&config, url);
    if (R_SUCCEEDED(rc)) rc = webConfigShow(&config, NULL);
    if (R_FAILED(rc)) host_logf(3, "[browser] web applet failed: 0x%x", rc);
    return R_SUCCEEDED(rc);
}
