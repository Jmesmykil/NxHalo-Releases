from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text()


def test_profile_24_is_exact_and_legacy_default_stays_21():
    header = read("port/include/halo_network_profile.h")
    rows = [tuple(map(int, match)) for match in re.findall(r"X\((\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\)", header)]
    assert rows == [
        (11, 11, 11, 0, 0),
        (12, 11, 12, 0, 1),
        (13, 11, 13, 0, 1),
        (14, 11, 14, 0, 1),
        (15, 11, 15, 0, 1),
        (16, 11, 16, 0, 1),
        (17, 11, 17, 0, 1),
        (18, 11, 18, 0, 1),
        (19, 11, 19, 0, 1),
        (20, 11, 20, 0, 1),
        (21, 21, 21, 1, 1),
        (24, 24, 24, 1, 1),
    ]
    client = read("source/networking/network_client_manager.c")
    assert "configured == 11 || configured == 20 || configured == 21 || configured == 24" in client
    assert '_config_integer, "21"' in read("port/linux/src/port_config.c")
    assert "#define HALO_PORT_NETWORK_VERSION 24" in read("port/linux/include/halo_port_limits.h")


def test_profile24_map_namespace_is_bounded_and_exact():
    client = read("source/networking/network_client_manager.c")
    family = read("port/linux/game/map_families.c")
    server = read("source/networking/network_server_manager.c")
    assert "network_profile_active_version() != 24" in client
    assert "strlen(leaf) >= 64" in client
    assert 'custom_maps\\\\' in family
    assert "length >= size || length >= MAP_FAMILY_FILE_LENGTH" in family
    assert "length > 63" in server and "length + 12 >= destination_size" in server
    assert "NETWORK_GAME_MAP_NAME_LENGTH = 0x80" in server


def test_profile24_schema_and_vehicle_changes_do_not_rewrite_legacy_layouts():
    validator = read("port/linux/game/tag_validate.c")
    schemas = read("port/linux/game/tag_schema_scenario.c") + read("port/linux/game/tag_schema_models.c")
    assert "network_profile_active_version() == 24 && ce_map_cache_version == 609" in validator
    assert "TAG_SCHEMA_CE_BLOCK(struct scenario, vehicles" in schemas
    assert "TAG_SCHEMA_CE_BLOCK(struct animation_graph, unit_seats" in schemas
    assert "TAG_SCHEMA_CE_BLOCK(struct animation_graph, animations" in schemas
    assert "VARIANT_VEHICLE_SET_PC = 0xFE" in read("source/game/game_engine.h")
    engine = read("source/game/game_engine.c")
    assert engine.count("network_profile_active_version() == 24") >= 2


if __name__ == "__main__":
    test_profile_24_is_exact_and_legacy_default_stays_21()
    test_profile24_map_namespace_is_bounded_and_exact()
    test_profile24_schema_and_vehicle_changes_do_not_rewrite_legacy_layouts()
    print("PASS: v24 profile, map namespace, schema, and vehicle source checks")
