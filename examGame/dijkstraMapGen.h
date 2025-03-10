#pragma once
#include <vector>
#include <flecs.h>
#include "ecsTypes.h"

constexpr float invalid_tile_value = 1e5f;

namespace dmaps
{
  void gen_player_approach_map(flecs::world &ecs, std::vector<float> &map);
  void gen_player_flee_map(flecs::world &ecs, std::vector<float> &map);
  void gen_hive_pack_map(flecs::world &ecs, std::vector<float> &map);
  void process_dmap(std::vector<float>& map, const DungeonData& dd);
  void init_tiles(std::vector<float>& map, const DungeonData& dd);
  void process_dmap_split(std::vector<float>& map, const DungeonData& dd, const size_t split_x, const size_t split_y, const size_t split_width);
};

