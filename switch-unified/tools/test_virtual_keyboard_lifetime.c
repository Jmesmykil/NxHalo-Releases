/* Compile the actual production implementation into this isolated test.
 * --gc-sections discards unused renderer and platform functions. */
#include "../source/interface/virtual_keyboard.c"

#include <stdlib.h>

static struct virtual_keyboard_key test_keys[NUMBER_OF_CONFIGURABLE_VIRTUAL_KEYS];
static struct virtual_keyboard_definition test_keyboard;
static boolean lookup_missing;
static long error_count;
static long flush_count;

long tag_loaded(long group_tag, const char *name)
{
	(void)name;
	if (group_tag == VIRTUAL_KEYBOARD_TAG)
		return lookup_missing ? NONE : 0x10001;
	return 0x10002;
}

void *tag_get(long group_tag, long tag_index)
{
	if (group_tag == VIRTUAL_KEYBOARD_TAG && tag_index == 0x10001)
		return &test_keyboard;
	return NULL;
}

void error(short priority, const char *format, ...)
{
	(void)priority;
	(void)format;
	error_count++;
}

void event_manager_flush(void)
{
	flush_count++;
}

static void require(boolean condition, char const *message)
{
	if (!condition)
	{
		exit(1);
	}
}

/* Models the old missing-lookup branch and old return expression without
 * dereferencing freed memory: the cached non-NULL pointer falsely succeeds. */
static boolean legacy_initialize_reports_success_on_missing_lookup(void)
{
	long index = tag_loaded(VIRTUAL_KEYBOARD_TAG, "ui\\english");
	if (index != NONE)
		virtual_keyboard_globals.keyboard = virtual_keyboard_definition_get(index);
	return virtual_keyboard_globals.keyboard != NULL;
}

int main(void)
{
	wchar_t buffer[4] = { L'o', L'l', L'd', 0 };
	struct virtual_keyboard_key *key = &test_keys[_vkey_a];
	long index;

	for (index = 0; index < NUMBER_OF_CONFIGURABLE_VIRTUAL_KEYS; index++)
		test_keys[index] = (struct virtual_keyboard_key){0};
	test_keyboard.keys.count = NUMBER_OF_CONFIGURABLE_VIRTUAL_KEYS;
	test_keyboard.keys.address = test_keys;
	key->character = L'a';
	key->shift_character = L'A';
	key->caps_character = L'A';
	key->symbols_character = L'@';
	key->shift_caps_character = L'A';
	key->shift_symbols_character = L'#';
	key->caps_symbols_character = L'@';

	lookup_missing = FALSE;
	require(virtual_keyboard_initialize(), "valid keyboard initializes");
	require(virtual_keyboard_globals.keyboard == &test_keyboard, "valid tag pointer bound");
	require(virtual_keyboard_get_character(_vkey_a) == L'a', "normal key character");
	virtual_keyboard_globals.shift_active = TRUE;
	require(virtual_keyboard_get_character(_vkey_a) == L'A', "shift character");
	virtual_keyboard_globals.shift_active = FALSE;
	virtual_keyboard_globals.caps_active = TRUE;
	require(virtual_keyboard_get_character(_vkey_a) == L'A', "caps character");
	virtual_keyboard_globals.symbols_active = TRUE;
	require(virtual_keyboard_get_character(_vkey_a) == L'@', "caps plus symbols character");

	virtual_keyboard_globals.active = TRUE;
	virtual_keyboard_globals.text_buffer = buffer;
	virtual_keyboard_globals.cursor = buffer + 3;
	virtual_keyboard_globals.buffer_size = sizeof(buffer);
	virtual_keyboard_dispose();
	require(virtual_keyboard_globals.keyboard == NULL &&
		!virtual_keyboard_globals.active &&
		virtual_keyboard_globals.text_buffer == NULL &&
		virtual_keyboard_globals.cursor == NULL &&
		virtual_keyboard_globals.buffer_size == 0,
		"dispose clears tag and caller-buffer pointers before unload");
	require(flush_count == 1, "dispose flushes pending keyboard events");

	require(virtual_keyboard_initialize(), "rebind before missing-tag case");
	lookup_missing = TRUE;
	require(legacy_initialize_reports_success_on_missing_lookup(),
		"baseline stale-success reproduced without dereferencing memory");
	require(!virtual_keyboard_initialize(), "production initializer rejects missing tag after a prior bind");
	require(virtual_keyboard_globals.keyboard == NULL &&
		!virtual_keyboard_globals.active,
		"failed rebind cannot retain the previous definition pointer");

	lookup_missing = FALSE;
	test_keyboard.keys.count = NUMBER_OF_CONFIGURABLE_VIRTUAL_KEYS - 1;
	test_keyboard.keys.address = test_keys;
	require(!virtual_keyboard_initialize(), "short key block rejected");
	require(virtual_keyboard_globals.keyboard == NULL, "short block leaves no cached pointer");

	test_keyboard.keys.count = NUMBER_OF_CONFIGURABLE_VIRTUAL_KEYS;
	test_keyboard.keys.address = NULL;
	require(!virtual_keyboard_initialize(), "null key block rejected");
	require(virtual_keyboard_globals.keyboard == NULL, "null block leaves no cached pointer");

	test_keyboard.keys.address = test_keys;
	require(virtual_keyboard_initialize(), "valid keyboard reinitializes after invalid tag data");
	virtual_keyboard_globals.active = TRUE;
	test_keyboard.keys.address = NULL;
	require(virtual_keyboard_get_character(_vkey_a) == 0x7F,
		"getter fails closed rather than reading invalid key data");
	require(virtual_keyboard_globals.keyboard == NULL &&
		!virtual_keyboard_globals.active,
		"invalid getter deactivates keyboard and clears stale pointers");

	test_keyboard.keys.address = test_keys;
	require(virtual_keyboard_initialize(), "valid keyboard reinitializes");
	virtual_keyboard_globals.active = TRUE;
	virtual_keyboard_globals.row = VIRTUAL_KEYBOARD_ROW_COUNT;
	require(virtual_keyboard_get_current_character() == 0x7F, "invalid row fails closed");
	require(!virtual_keyboard_globals.active, "invalid layout deactivates keyboard");

	return error_count == 5 ? 0 : 2;
}
