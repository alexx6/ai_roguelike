#pragma once
#include <flecs.h>
#include <vector>
#include "ecsTypes.h"
#include "math.h"

struct PortalConnection
{
  size_t connIdx;
  float score;
};

struct PathPortal
{
  size_t startX, startY;
  size_t endX, endY;
  std::vector<PortalConnection> conns;
};

struct DungeonPortals
{
  size_t tileSplit;
  std::vector<PathPortal> portals;
  std::vector<std::vector<size_t>> tilePortalsIndices;
};

void prebuild_map(flecs::world &ecs);
std::vector<size_t> find_path_a_star_portal(const DungeonPortals& dp, const DungeonData& dd, size_t from, size_t to);
std::vector<IVec2> find_path_a_star(const DungeonData& dd, IVec2 from, IVec2 to, IVec2 lim_min, IVec2 lim_max);