#include "../port/linux/src/xinput_sdl.c"
#include <assert.h>
struct fake_pad { uint32_t id; int rank; bool held; };
static struct fake_pad fake[4]; static int fake_count; static unsigned char fake_split;
unsigned char pc_menu_split_players(void) { return fake_split; }
SDL_JoystickID *SDLCALL SDL_GetGamepads(int *count) { int i; SDL_JoystickID *ids=SDL_malloc((fake_count+1)*sizeof(*ids)); *count=fake_count; for(i=0;i<fake_count;i++)ids[i]=fake[i].id;ids[fake_count]=0;return ids; }
SDL_Gamepad *SDLCALL SDL_GetGamepadFromID(SDL_JoystickID id) { int i;for(i=0;i<fake_count;i++)if(fake[i].id==id)return (SDL_Gamepad*)&fake[i];return NULL; }
SDL_GamepadType SDLCALL SDL_GetGamepadType(SDL_Gamepad *g) { return ((struct fake_pad*)g)->rank==0 ? SDL_GAMEPAD_TYPE_PS5 : SDL_GAMEPAD_TYPE_STANDARD; }
Uint16 SDLCALL SDL_GetGamepadVendor(SDL_Gamepad *g) { return ((struct fake_pad*)g)->rank==3 ? 0x28de : 0; }
Uint16 SDLCALL SDL_GetGamepadProduct(SDL_Gamepad *g) { return ((struct fake_pad*)g)->rank==3 ? 0x1205 : 0; }
const char *SDLCALL SDL_GetGamepadName(SDL_Gamepad *g) { return ((struct fake_pad*)g)->rank==2 ? "Steam Virtual Gamepad" : "Physical controller"; }
bool SDLCALL SDL_GetGamepadButton(SDL_Gamepad *g,SDL_GamepadButton b) { (void)b;return ((struct fake_pad*)g)->held; }
Sint16 SDLCALL SDL_GetGamepadAxis(SDL_Gamepad *g,SDL_GamepadAxis a) { (void)g;(void)a;return 0; }
static void set(int n,uint32_t a,int ar,uint32_t b,int br,uint32_t c,int cr) { fake_count=n;fake[0]=(struct fake_pad){a,ar,false};fake[1]=(struct fake_pad){b,br,false};fake[2]=(struct fake_pad){c,cr,false}; }
static void changes(DWORD expected_in,DWORD expected_out) { DWORD in,out;XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD,&in,&out);assert(in==expected_in);assert(out==expected_out);assert(!(out&1)); }
int main(void) {
 DWORD packet;
 set(2,2,3,3,0,0,0);changes(3,0);assert(logical_gamepad_ids[0]==3 && logical_gamepad_ids[1]==2);
 packet=controllers[0].packet_number;
 set(2,3,0,2,3,0,0);changes(0,0);assert(controllers[0].packet_number==packet);
 set(3,9,2,2,3,3,0);changes(4,0);assert(logical_gamepad_ids[0]==3 && logical_gamepad_ids[1]==2 && logical_gamepad_ids[2]==9);
 controllers[0].previous.wButtons=123;
 set(2,9,2,2,3,0,0);changes(0,4);assert(logical_gamepad_ids[0]==9);assert(controllers[0].previous.wButtons==0);
 set(1,2,3,0,0,0,0);changes(0,2);assert(logical_gamepad_ids[0]==2);
 fake_split=1;changes(2,0);assert(logical_gamepad_ids[1]==2 && logical_gamepad_ids[0]==0);
 set(1,12,0,0,0,0,0);fake[0].held=true;changes(0,2);assert(logical_gamepad_ids[0]==12 && !lone_gamepad_split);
 fake[0].held=false;changes(2,0);assert(logical_gamepad_ids[1]==12);
 set(2,12,0,2,3,0,0);changes(2,2);assert(logical_gamepad_ids[0]==12 && logical_gamepad_ids[1]==2);
 puts("PASS: production XInput identity events, permanent KBM port, stable split ports, delayed virtual, held-source replacement");return 0;
}
