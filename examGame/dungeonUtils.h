#pragma once
#include "ecsTypes.h"
#include <flecs.h>

namespace dungeon
{
  constexpr char wall = '#';
  constexpr char floor = ' ';
  constexpr char exit = 'e';
  constexpr char spawner = 's';

  Position find_walkable_tile(flecs::world &ecs);
  bool is_tile_walkable(flecs::world &ecs, Position pos);
};
