#include "../port/linux/game/nxhalo_custom_content.c"

static int test_failures;
#define assert(expression) do { if (!(expression)) test_failures++; } while (0)
static void test_zero(void *buffer, unsigned long size) { unsigned char *p = buffer; while (size--) *p++ = 0; }
static void test_copy(char *destination, char const *source) { while ((*destination++ = *source++)) { } }

static struct weapon_definition fake_definition;
static struct unit_datum fake_unit;
static struct object_datum fake_sword_object;
static struct object_datum fake_old_object;
static char fake_tag_name[128];
static boolean tag_present;
static boolean spawn_succeeds;
static boolean add_succeeds;
static boolean deleted_object;
static long fake_weapon_object_index = 102;
static long fake_old_weapon_object_index = 88;
static long fake_unit_index = 42;

void tag_iterator_new(struct tag_iterator *iterator, long group_tag)
{
	iterator->absolute_index = 0;
	iterator->group_tag = group_tag;
}

long tag_iterator_next(struct tag_iterator *iterator)
{
	if (iterator->absolute_index++ == 0 && tag_present)
		return 1;
	return NONE;
}

void *tag_get(long group_tag, long tag_index)
{
	(void)group_tag;
	return tag_index == 1 ? &fake_definition : NULL;
}

char *tag_get_name(long tag_index)
{
	return tag_index == 1 ? fake_tag_name : NULL;
}

void object_placement_data_new(struct object_placement_data *data, long definition_index, long owner_object_index)
{
	test_zero(data, sizeof(*data));
	data->definition_index = definition_index;
	data->owner_object_index = owner_object_index;
}

long object_new(struct object_placement_data *data)
{
	(void)data;
	return spawn_succeeds ? fake_weapon_object_index : NONE;
}

boolean unit_add_weapon_to_inventory(long unit_index, long weapon_index, long is_starting_weapon)
{
	short slot;
	(void)is_starting_weapon;
	assert(unit_index == fake_unit_index);
	if (!add_succeeds)
		return FALSE;
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		fake_unit.unit.weapon_object_indices[slot] = NONE;
	fake_unit.unit.weapon_object_indices[0] = weapon_index;
	return TRUE;
}

void object_delete(long object_index)
{
	assert(object_index == fake_weapon_object_index);
	deleted_object = TRUE;
}

void *object_get_and_verify_type(long object_index, unsigned long valid_type_flags)
{
	(void)valid_type_flags;
	if (object_index == fake_unit_index)
		return &fake_unit;
	if (object_index == fake_weapon_object_index)
		return &fake_sword_object;
	if (object_index == fake_old_weapon_object_index)
		return &fake_old_object;
	return NULL;
}

unsigned long csstrlen(const char *s) { unsigned long n=0; while (s[n]) n++; return n; }

int _stricmp(char const *a, char const *b)
{
	while (*a && *b) { char x=*a++, y=*b++; if (x >= 'A' && x <= 'Z') x += 'a'-'A'; if (y >= 'A' && y <= 'Z') y += 'a'-'A'; if (x != y) return (unsigned char)x - (unsigned char)y; }
	return (unsigned char)*a - (unsigned char)*b;
}

static void reset_case(void)
{
	test_zero(&fake_definition, sizeof(fake_definition));
	test_zero(&fake_unit, sizeof(fake_unit));
	test_zero(&fake_sword_object, sizeof(fake_sword_object));
	test_zero(&fake_old_object, sizeof(fake_old_object));
	test_copy(fake_tag_name, "weapons\\energy sword\\energy sword");
	fake_definition.weapon.melee_attack_damage.index = 3;
	fake_definition.weapon.interface_definition.first_person_model.index = 4;
	fake_definition.weapon.interface_definition.first_person_animations.index = 5;
	fake_sword_object.definition_index = 1;
	fake_old_object.definition_index = 7;
	fake_unit.unit.weapon_object_indices[0] = fake_old_weapon_object_index;
	fake_unit.unit.desired_weapon_index = NONE;
	tag_present = TRUE;
	spawn_succeeds = TRUE;
	add_succeeds = TRUE;
	deleted_object = FALSE;
}

int main(void)
{
	reset_case();
	tag_present = FALSE;
	assert(!nxhalo_zombie_sword_available());
	assert(!nxhalo_give_zombie_sword(fake_unit_index));
	assert(fake_unit.unit.weapon_object_indices[0] == fake_old_weapon_object_index);

	reset_case();
	fake_definition.weapon.interface_definition.first_person_animations.index = NONE;
	assert(!nxhalo_zombie_sword_available());
	assert(!nxhalo_give_zombie_sword(fake_unit_index));
	assert(fake_unit.unit.weapon_object_indices[0] == fake_old_weapon_object_index);

	reset_case();
	test_copy(fake_tag_name, "custom\\weapons\\energy_blade\\energy_blade");
	assert(nxhalo_zombie_sword_available());
	spawn_succeeds = FALSE;
	assert(!nxhalo_give_zombie_sword(fake_unit_index));
	assert(fake_unit.unit.weapon_object_indices[0] == fake_old_weapon_object_index);

	reset_case();
	assert(nxhalo_zombie_sword_available());
	add_succeeds = FALSE;
	assert(!nxhalo_give_zombie_sword(fake_unit_index));
	assert(fake_unit.unit.weapon_object_indices[0] == fake_old_weapon_object_index);
	assert(deleted_object);

	reset_case();
	assert(nxhalo_give_zombie_sword(fake_unit_index));
	assert(fake_unit.unit.weapon_object_indices[0] == fake_weapon_object_index);
	assert(fake_unit.unit.desired_weapon_index == 0);
	return test_failures;
}
