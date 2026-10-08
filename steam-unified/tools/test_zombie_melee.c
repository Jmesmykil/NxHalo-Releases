#include "../port/linux/game/nxhalo_custom_content.c"

static int failures;
#define CHECK(expression) do { if (!(expression)) failures++; } while (0)
static void zero(void *buffer, unsigned long size) { unsigned char *p = buffer; while (size--) *p++ = 0; }

static struct weapon_definition definitions[4];
static struct unit_datum test_unit;
static struct object_datum test_weapon_object;
static struct object_datum old_weapon_object;
static long fake_unit_index = 42;
static long fake_weapon_index = 102;
static long old_weapon_index = 88;
static boolean ball_present;
static boolean flag_present;
static boolean pistol_present;
static boolean spawn_succeeds;
static boolean add_succeeds;
static boolean deleted_weapon;

static boolean same_string(char const *a, char const *b)
{
	while (*a && *b && *a == *b) { a++; b++; }
	return *a == *b;
}

long tag_loaded(long group_tag, const char *name)
{
	(void)group_tag;
	if (same_string(name, "weapons\\ball\\ball") && ball_present) return 1;
	if (same_string(name, "weapons\\flag\\flag") && flag_present) return 2;
	if (same_string(name, "weapons\\pistol\\pistol") && pistol_present) return 3;
	return NONE;
}

void *tag_get(long group_tag, long tag_index)
{
	(void)group_tag;
	return tag_index > 0 && tag_index < 4 ? &definitions[tag_index] : NULL;
}

void object_placement_data_new(struct object_placement_data *data, long definition_index, long owner_object_index)
{
	zero(data, sizeof(*data));
	data->definition_index = definition_index;
	data->owner_object_index = owner_object_index;
}

long object_new(struct object_placement_data *data)
{
	if (!spawn_succeeds) return NONE;
	test_weapon_object.definition_index = data->definition_index;
	return fake_weapon_index;
}

boolean unit_add_weapon_to_inventory(long unit_index, long weapon_index, long is_starting_weapon)
{
	short slot;
	(void)is_starting_weapon;
	CHECK(unit_index == fake_unit_index);
	if (!add_succeeds) return FALSE;
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		test_unit.unit.weapon_object_indices[slot] = NONE;
	test_unit.unit.weapon_object_indices[0] = weapon_index;
	return TRUE;
}

void object_delete(long object_index)
{
	CHECK(object_index == fake_weapon_index);
	deleted_weapon = TRUE;
}

void *object_get_and_verify_type(long object_index, unsigned long valid_type_flags)
{
	(void)valid_type_flags;
	if (object_index == fake_unit_index) return &test_unit;
	if (object_index == fake_weapon_index) return &test_weapon_object;
	if (object_index == old_weapon_index) return &old_weapon_object;
	return NULL;
}

static void set_melee_refs(struct weapon_definition *weapon)
{
	weapon->weapon.melee_attack_damage.index = 3;
	weapon->weapon.interface_definition.first_person_model.index = 4;
	weapon->weapon.interface_definition.first_person_animations.index = 5;
}

static void reset_case(void)
{
	zero(definitions, sizeof(definitions));
	zero(&test_unit, sizeof(test_unit));
	zero(&test_weapon_object, sizeof(test_weapon_object));
	zero(&old_weapon_object, sizeof(old_weapon_object));
	set_melee_refs(&definitions[1]);
	set_melee_refs(&definitions[2]);
	set_melee_refs(&definitions[3]);
	old_weapon_object.definition_index = 7;
	test_unit.unit.weapon_object_indices[0] = old_weapon_index;
	test_unit.unit.desired_weapon_index = NONE;
	ball_present = TRUE;
	flag_present = FALSE;
	pistol_present = FALSE;
	spawn_succeeds = TRUE;
	add_succeeds = TRUE;
	deleted_weapon = FALSE;
}

int main(void)
{
	reset_case();
	ball_present = flag_present = pistol_present = FALSE;
	CHECK(!nxhalo_zombie_melee_weapon_available());
	CHECK(!nxhalo_give_zombie_melee_weapon(fake_unit_index));
	CHECK(test_unit.unit.weapon_object_indices[0] == old_weapon_index);

	reset_case();
	definitions[1].weapon.interface_definition.first_person_animations.index = NONE;
	pistol_present = TRUE;
	CHECK(nxhalo_zombie_melee_weapon_available());
	CHECK(nxhalo_give_zombie_melee_weapon(fake_unit_index));
	CHECK(test_weapon_object.definition_index == 3);
	CHECK(test_unit.unit.desired_weapon_index == 0);

	reset_case();
	spawn_succeeds = FALSE;
	CHECK(!nxhalo_give_zombie_melee_weapon(fake_unit_index));
	CHECK(test_unit.unit.weapon_object_indices[0] == old_weapon_index);

	reset_case();
	add_succeeds = FALSE;
	CHECK(!nxhalo_give_zombie_melee_weapon(fake_unit_index));
	CHECK(test_unit.unit.weapon_object_indices[0] == old_weapon_index);
	CHECK(deleted_weapon);

	reset_case();
	CHECK(nxhalo_give_zombie_melee_weapon(fake_unit_index));
	CHECK(test_weapon_object.definition_index == 1);
	CHECK(test_unit.unit.weapon_object_indices[0] == fake_weapon_index);
	CHECK(test_unit.unit.desired_weapon_index == 0);
	return failures;
}
