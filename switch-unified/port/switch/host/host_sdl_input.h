/* SDL2 -> SDL3 3.4.16 input ABI used by the ILP32 guest.
 * tools/android_build.py pins this SDL3 version. Only pointer-free events
 * consumed by the platform input layer are bridged; never copy SDL2_Event.
 * Writes use memcpy so the guest buffer need not have host alignment.
 */
#ifndef HOST_SDL_INPUT_H
#define HOST_SDL_INPUT_H

#include <SDL2/SDL.h>
#include <stdint.h>
#include <string.h>

#define GUEST_SDL_EVENT_SIZE 128

/* SDL3 reserves ID 0; SDL2's first instance can be 0. Never expose a
 * transient device index as an ID. Apply this bijection to every path. */
static inline uint32_t guest_gamepad_id(SDL_JoystickID instance)
{
	return instance < 0 ? 0 : (uint32_t)instance + 1;
}

static inline SDL_JoystickID host_gamepad_id(uint32_t id)
{
	return id == 0 || id > (uint32_t)INT32_MAX + 1 ? -1 :
		(SDL_JoystickID)(id - 1);
}

static inline int host_gamepad_index(uint32_t id)
{
	SDL_JoystickID instance = host_gamepad_id(id);
	int count = SDL_NumJoysticks();
	int index;

	if (instance < 0)
		return -1;
	for (index = 0; index < count; index++)
		if (SDL_JoystickGetDeviceInstanceID(index) == instance &&
		    SDL_IsGameController(index))
			return index;
	return -1;
}

static inline int guest_gamepads(uint32_t *ids, int capacity)
{
	int count = 0, index, devices = SDL_NumJoysticks();

	if (!ids || capacity <= 0)
		return 0;
	for (index = 0; index < devices && count < capacity; index++)
	{
		uint32_t id = guest_gamepad_id(SDL_JoystickGetDeviceInstanceID(index));
		if (id && SDL_IsGameController(index))
			ids[count++] = id;
	}
	return count;
}

static inline SDL_GameControllerButton host_gamepad_button(int button)
{
	/* SDL3 SOUTH/EAST/WEST/NORTH are physical positions. SDL2's A/B/X/Y
	 * mapping has the same positions, including on Nintendo controllers.
	 * SDL2 PADDLE1..4 mean Xbox P1/P3/P2/P4, matching SDL3's hand order. */
	static const SDL_GameControllerButton buttons[] = {
		SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
		SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
		SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_GUIDE,
		SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_LEFTSTICK,
		SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
		SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_DPAD_UP,
		SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
		SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_MISC1,
		SDL_CONTROLLER_BUTTON_PADDLE1, SDL_CONTROLLER_BUTTON_PADDLE2,
		SDL_CONTROLLER_BUTTON_PADDLE3, SDL_CONTROLLER_BUTTON_PADDLE4,
		SDL_CONTROLLER_BUTTON_TOUCHPAD
	};
	return button >= 0 && button < (int)(sizeof(buttons) / sizeof(buttons[0])) ?
		buttons[button] : SDL_CONTROLLER_BUTTON_INVALID;
}

static inline int guest_gamepad_button(Uint8 button)
{
	int guest;
	for (guest = 0; guest < 21; guest++)
		if (host_gamepad_button(guest) == button)
			return guest;
	return -1;
}

static inline int guest_gamepad_type(SDL_GameControllerType type)
{
	switch (type)
	{
	case SDL_CONTROLLER_TYPE_XBOX360: return 2;
	case SDL_CONTROLLER_TYPE_XBOXONE: return 3;
	case SDL_CONTROLLER_TYPE_PS3: return 4;
	case SDL_CONTROLLER_TYPE_PS4: return 5;
	case SDL_CONTROLLER_TYPE_PS5: return 6;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return 7;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return 8;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return 9;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return 10;
	default: return 1; /* SDL3 STANDARD: valid controller, no named SDL3 type */
	}
}

static inline void guest_event_u32(void *event, unsigned offset, uint32_t value)
{
	memcpy((uint8_t *)event + offset, &value, sizeof(value));
}

static inline void guest_event_float(void *event, unsigned offset, float value)
{
	memcpy((uint8_t *)event + offset, &value, sizeof(value));
}

static inline int guest_input_event(void *event, const SDL_Event *host)
{
	uint8_t *bytes = event;
	uint32_t type, id;
	uint64_t timestamp = (uint64_t)host->common.timestamp * 1000000;
	int button;

	memset(event, 0, GUEST_SDL_EVENT_SIZE);
	switch (host->type)
	{
	case SDL_QUIT:
		type = 0x100;
		break;
	case SDL_WINDOWEVENT:
		if (host->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
			type = 0x20e;
		else if (host->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
			type = 0x20f;
		else
			return 0;
		guest_event_u32(event, 16, host->window.windowID);
		guest_event_u32(event, 20, host->window.data1);
		guest_event_u32(event, 24, host->window.data2);
		break;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		type = host->type == SDL_KEYDOWN ? 0x300 : 0x301;
		guest_event_u32(event, 16, host->key.windowID);
		/* which = 0: SDL2 doesn't identify the keyboard. */
		guest_event_u32(event, 24, host->key.keysym.scancode);
		guest_event_u32(event, 28, host->key.keysym.sym);
		memcpy(bytes + 32, &host->key.keysym.mod, sizeof(Uint16));
		bytes[36] = host->key.state == SDL_PRESSED;
		bytes[37] = host->key.repeat != 0;
		break;
	case SDL_MOUSEMOTION:
		type = 0x400;
		guest_event_u32(event, 16, host->motion.windowID);
		guest_event_u32(event, 20, host->motion.which);
		guest_event_u32(event, 24, host->motion.state);
		guest_event_float(event, 28, (float)host->motion.x);
		guest_event_float(event, 32, (float)host->motion.y);
		guest_event_float(event, 36, (float)host->motion.xrel);
		guest_event_float(event, 40, (float)host->motion.yrel);
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		type = host->type == SDL_MOUSEBUTTONDOWN ? 0x401 : 0x402;
		guest_event_u32(event, 16, host->button.windowID);
		guest_event_u32(event, 20, host->button.which);
		bytes[24] = host->button.button;
		bytes[25] = host->button.state == SDL_PRESSED;
		bytes[26] = host->button.clicks;
		guest_event_float(event, 28, (float)host->button.x);
		guest_event_float(event, 32, (float)host->button.y);
		break;
	case SDL_MOUSEWHEEL:
		type = 0x403;
		guest_event_u32(event, 16, host->wheel.windowID);
		guest_event_u32(event, 20, host->wheel.which);
		guest_event_float(event, 24, host->wheel.preciseX);
		guest_event_float(event, 28, host->wheel.preciseY);
		guest_event_u32(event, 32, host->wheel.direction);
		guest_event_float(event, 36, (float)host->wheel.mouseX);
		guest_event_float(event, 40, (float)host->wheel.mouseY);
		guest_event_u32(event, 44, host->wheel.x);
		guest_event_u32(event, 48, host->wheel.y);
		break;
	case SDL_CONTROLLERDEVICEADDED:
		/* SDL2 ADDED carries an index, unlike REMOVED/REMAPPED. */
		id = guest_gamepad_id(SDL_JoystickGetDeviceInstanceID(host->cdevice.which));
		if (!id)
			return 0; /* removed before the queued notification was consumed */
		type = 0x653;
		guest_event_u32(event, 16, id);
		break;
	case SDL_CONTROLLERDEVICEREMOVED:
	case SDL_CONTROLLERDEVICEREMAPPED:
		type = host->type == SDL_CONTROLLERDEVICEREMOVED ? 0x654 : 0x655;
		guest_event_u32(event, 16, guest_gamepad_id(host->cdevice.which));
		break;
	case SDL_CONTROLLERAXISMOTION:
		type = 0x650;
		guest_event_u32(event, 16, guest_gamepad_id(host->caxis.which));
		bytes[20] = host->caxis.axis; /* SDL2 and SDL3 axes have identical order */
		memcpy(bytes + 24, &host->caxis.value, sizeof(Sint16));
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		button = guest_gamepad_button(host->cbutton.button);
		if (button < 0)
			return 0;
		type = host->type == SDL_CONTROLLERBUTTONDOWN ? 0x651 : 0x652;
		guest_event_u32(event, 16, guest_gamepad_id(host->cbutton.which));
		bytes[20] = (uint8_t)button;
		bytes[21] = host->cbutton.state == SDL_PRESSED;
		break;
	default:
		return 0;
	}
	guest_event_u32(event, 0, type);
	memcpy(bytes + 8, &timestamp, sizeof(timestamp));
	return 1;
}

#endif
