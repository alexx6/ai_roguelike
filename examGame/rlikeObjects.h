#pragma once
#include <flecs.h>
#include "raylib.h"
#include "ecsTypes.h"

flecs::entity create_monster(flecs::world &ecs, Position pos, Color col, const char *texture_src);
void create_player(flecs::world &ecs, Position pos, const char *texture_src);
flecs::entity create_spawner(flecs::world &ecs, Position &pos);

struct MonsterSpawner
{
  float timeToSpawn;
  float timeBetweenSpawns;
};

