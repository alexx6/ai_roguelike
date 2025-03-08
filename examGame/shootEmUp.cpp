#include <raylib.h>
#include "shootEmUp.h"
#include "ecsTypes.h"
#include "rlikeObjects.h"
#include "steering.h"
#include "dungeonGen.h"
#include "dungeonUtils.h"
#include "pathfinder.h"
#include "dijkstraMapGen.h"

constexpr float tile_size = 64.f;
const Position anchor = { 30.f, 30.0f };
std::vector<SteerDir> approachFlowMap;
std::vector<SteerDir> fleeFlowMap;

std::pair<int, UtilityActions> getApproachUtility() {
  return std::make_pair(50, ACTION_APPROACH);
}

std::pair<int, UtilityActions> getFleeUtility(float hp, int nearMonsters) {
  int utilityScore = 100 - hp - nearMonsters * 5;

  return std::make_pair(utilityScore, ACTION_FLEE);
}

UtilityActions selectUtilityAction(std::vector<std::pair<int, UtilityActions>> utilities) {
  int maxUtility = 0;
  int maxUtilityIndex = 0;

  for (size_t i = 0; i < utilities.size(); ++i)
  {
    if (utilities[i].first > maxUtility)
    {
      maxUtility = utilities[i].first;
      maxUtilityIndex = i;
    }
  }

  return utilities[maxUtilityIndex].second;
}

size_t find_max_valid_index(std::vector<float> map) 
{
  float maxValue = 0.f;
  size_t maxIndex = 0;

  for (size_t i = 0; i < map.size(); ++i)
  {
    const float v = map[i];
    if (v != invalid_tile_value && v > maxValue)
    {
      maxValue = v;
      maxIndex = i;
    }
  }

  return maxIndex;
}

std::vector<int> get_reachable_indexes(std::vector<float>& map, size_t tile, size_t width) 
{
  std::vector<int> reachableIndexes(9);
  std::fill(reachableIndexes.begin(), reachableIndexes.end(), -1);

  for (int j = 0; j < 3; ++j)
  {
    for (int i = 0; i < 3; ++i)
    {
      const int curTile = (i - 1) + (j - 1) * width + tile;

      if (curTile < 0 || curTile >= map.size() || curTile == tile)
      {
        continue;
      }

      if (map[curTile] < invalid_tile_value)
      {
        reachableIndexes[j * 3 + i] = curTile;
      }
    }
  }

  if (reachableIndexes[1] == -1 && reachableIndexes[3] == -1)
    reachableIndexes[0] = -1;

  if (reachableIndexes[1] == -1 && reachableIndexes[5] == -1)
    reachableIndexes[2] = -1;

  if (reachableIndexes[3] == -1 && reachableIndexes[7] == -1)
    reachableIndexes[6] = -1;

  if (reachableIndexes[5] == -1 && reachableIndexes[7] == -1)
    reachableIndexes[8] = -1;

  return reachableIndexes;
}

void update_player_approach_flowmap(flecs::world& ecs, std::vector<float>& map)
{
  approachFlowMap.resize(map.size());
  std::fill(approachFlowMap.begin(), approachFlowMap.end(), SteerDir());

  size_t mapWidth = 0;
  ecs.query<const DungeonData>().each([&](const DungeonData& dd)
  {
    mapWidth = dd.width;
  });

  for (size_t i = 0; i < map.size(); ++i) 
  {
    std::vector<int> reachableIndexes = get_reachable_indexes(map, i, mapWidth);
    int bestIndex = 0;

    for (int j = 0; j < 9; ++j)
    {
      if (reachableIndexes[j] < 0)
        continue;
      
      if (reachableIndexes[bestIndex] < 0 || map[reachableIndexes[j]] < map[reachableIndexes[bestIndex]])
        bestIndex = j;
    }

    approachFlowMap[i] = { float(bestIndex % 3 - 1), float(bestIndex / 3 - 1) };
  }
}

void update_player_flee_flowmap(flecs::world& ecs, std::vector<float>& map)
{
  fleeFlowMap.resize(map.size());
  std::fill(fleeFlowMap.begin(), fleeFlowMap.end(), SteerDir());
  size_t mapWidth = 0;

  //Flee to the farthest position from player (not necessary away from player at some moments)
  for (float& v : map)
    if (v < invalid_tile_value)
      v *= -1.2f;

  ecs.query<const DungeonData>().each([&](const DungeonData& dd)
  {
    mapWidth = dd.width;
    dmaps::process_dmap(map, dd);
  });
  //------------------------------------------------------------

  for (size_t i = 0; i < map.size(); ++i)
  {
    std::vector<int> reachableIndexes = get_reachable_indexes(map, i, mapWidth);
    int bestIndex = 0;

    for (int j = 0; j < 9; ++j)
    {
      if (reachableIndexes[j] < 0)
        continue;

      if (reachableIndexes[bestIndex] < 0 || map[reachableIndexes[j]] < map[reachableIndexes[bestIndex]])
        bestIndex = j;
    }

    fleeFlowMap[i] = { float(bestIndex % 3 - 1), float(bestIndex / 3 - 1) };
  }
}

void update_maps(flecs::world& ecs)
{
  std::vector<float> map;
  dmaps::gen_player_approach_map(ecs, map);

  update_player_approach_flowmap(ecs, map);
  update_player_flee_flowmap(ecs, map);
}

static void register_roguelike_systems(flecs::world &ecs, bool &needToRebuildLevel, size_t &difficulty)
{

  ecs.system<Velocity, const MoveSpeed, const IsPlayer>()
    .each([&](Velocity &vel, const MoveSpeed &ms, const IsPlayer)
    {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);
      vel.x = ((left ? -1.f : 0.f) + (right ? 1.f : 0.f));
      vel.y = ((up ? -1.f : 0.f) + (down ? 1.f : 0.f));
      vel = Velocity{normalize(vel) * ms.speed};
    });
  ecs.system<Position, const Velocity>()
    .each([&](flecs::entity e, Position &pos, const Velocity &vel)
    {      
      //Simple collision detection
      Position deltaPosition = vel * ecs.delta_time();

      ecs.query<const DungeonData>().each([&](const DungeonData& dd)
      {
          Position testPos = pos + deltaPosition;

        if (dd.tiles[(size_t)(pos.y / tile_size) * dd.width + (size_t)(testPos.x / tile_size)] == dungeon::wall)
        {
          deltaPosition.x = 0;
        }

        if (dd.tiles[(size_t)(testPos.y / tile_size) * dd.width + (size_t)(pos.x / tile_size)] == dungeon::wall)
        {
          deltaPosition.y = 0;
        }
      });

      pos += deltaPosition;
    });
  
  ecs.system<const Position, const Hitpoints, const Team, UtilityAction>()
    .each([&](flecs::entity e, const Position& p, const Hitpoints &hp, const Team& t, UtilityAction &a)
    {
      if (t.team == 0)
        return;

      int nearMonsters = 0;

      ecs.query<const Position, const Team>()
        .each([&](flecs::entity ne, const Position& np, const Team& t)
        {
          if (t.team == 0 || length(p - np) > 3 * tile_size)
            return;

          ++nearMonsters;
        });

      a.action = selectUtilityAction({ getApproachUtility(), getFleeUtility(hp.hitpoints, nearMonsters) });
    });

  // flowmap apply
  ecs.system<SteerDir, const MoveSpeed, const Velocity, const Position, const Team, const UtilityAction>()
   .each([&](SteerDir& sd, const MoveSpeed& ms, const Velocity& vel, const Position& p, const Team &t, const UtilityAction &a)
   {
     if (t.team == 0)
       return;

     ecs.query<const DungeonData>().each([&](const DungeonData& dd)
     {
       if (a.action == ACTION_APPROACH && !approachFlowMap.empty())
         sd += SteerDir{ normalize(approachFlowMap[size_t(p.y / tile_size) * dd.width + size_t(p.x / tile_size)]) * ms.speed - vel };
 
       if (a.action == ACTION_FLEE && !fleeFlowMap.empty())
         sd += SteerDir{ normalize(fleeFlowMap[size_t(p.y / tile_size) * dd.width + size_t(p.x / tile_size)]) * ms.speed - vel };
     });
    
   });

  //Check if player on exit tile, then generate new dungeon
  ecs.system<const Position, const IsPlayer>().each([&](const Position& pos, const IsPlayer)
  {
    ecs.query<const DungeonData>().each([&](const DungeonData& dd)
    {
      if (dd.tiles[(size_t)(pos.y / tile_size) * dd.width + (size_t)(pos.x / tile_size)] == dungeon::exit)
      {
        needToRebuildLevel = true;
        difficulty += 1;
      }
    });
  });

  ecs.system<const Position, const Color>()
    .with<TextureSource>(flecs::Wildcard)
    .with<BackgroundTile>()
    .each([&](flecs::entity e, const Position &pos, const Color color)
    {
      const auto textureSrc = e.target<TextureSource>();
      DrawTextureQuad(*textureSrc.get<Texture2D>(),
          Vector2{1, 1}, Vector2{0, 0},
          Rectangle{float(pos.x), float(pos.y), tile_size, tile_size}, color);
    });
  ecs.system<const Position, const Color>()
    .with<TextureSource>(flecs::Wildcard)
    .without<BackgroundTile>()
    .each([&](flecs::entity e, const Position &pos, const Color color)
    {
      const auto textureSrc = e.target<TextureSource>();
      DrawTextureQuad(*textureSrc.get<Texture2D>(),
          Vector2{1, 1}, Vector2{0, 0},
          Rectangle{float(pos.x - anchor.x), float(pos.y - anchor.y), tile_size, tile_size}, color);
    });

  ecs.system<const Position, const ExitTile>()
    .each([&](const Position& pos, const ExitTile)
    {
      const Rectangle rect = { pos.x, pos.y, tile_size, tile_size };
      DrawRectangleRec(rect, Color{ 0, 255, 255, 255 });
    });

  ecs.system<const Position, const MonsterSpawner>()
    .each([&](const Position& pos, const MonsterSpawner)
      {
        const Rectangle rect = { pos.x, pos.y, tile_size, tile_size };
        DrawRectangleRec(rect, Color{ 255, 0, 255, 255 });
      });

  ecs.system<Texture2D>()
    .each([&](Texture2D &tex)
    {
      SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    });

  //Update maps if player steps on another tile
  ecs.system<const Position, const IsPlayer>()
    .each([&](const Position& pos, const IsPlayer)
    {
      static Position lastPos = pos;

      if (size_t(lastPos.x / tile_size) != size_t(pos.x / tile_size) || size_t(lastPos.y / tile_size) != size_t(pos.y / tile_size))
      {
        update_maps(ecs);
      }

      //for (int i = 0; i < approachFlowMap.size(); ++i)
      //{
      //  DrawText(TextFormat("x: %d, y: %d", int(approachFlowMap[i].x), int(approachFlowMap[i].y)), i % 100 * tile_size + anchor.x, i / 100 * tile_size, 14, WHITE);
      //}

      lastPos = pos;
    });

  ecs.system<const Position, MonsterSpawner>()
    .each([&](const Position &pos, MonsterSpawner &ms)
    {
      ms.timeToSpawn -= ecs.delta_time();
      while (ms.timeToSpawn < 0.f)
      {
        steer::Type st = steer::Type(GetRandomValue(0, steer::Type::Num - 1));
        const Color colors[steer::Type::Num] = { WHITE, RED, BLUE, GREEN };
        const float distances[steer::Type::Num] = { 800.f, 800.f, 300.f, 300.f };
        const float dist = distances[st];
        constexpr int angRandMax = 1 << 16;
        const float angle = float(GetRandomValue(0, angRandMax)) / float(angRandMax) * PI * 2.f;
        Color col = colors[st];
        steer::create_steer_beh(create_monster(ecs, pos + anchor, col, "minotaur_tex"), st); 
        ms.timeToSpawn += ms.timeBetweenSpawns;
      }
    });

  //Show entity hp
  ecs.system<const Position, const Hitpoints>()
    .each([&](const Position& pos, const Hitpoints& hp)
    {
      DrawText(TextFormat("hp: %d", int(hp.hitpoints)), pos.x - anchor.x, pos.y - 12 - anchor.y, 18, WHITE);
    });

  ecs.system<const Position, const MeleeDamage, const IsPlayer>()
    .each([&](const Position &pos, const MeleeDamage &md, const IsPlayer)
    {
      //Hit every monster in weapon length radius
      auto monsterQuery = ecs.query<const Position, Hitpoints, const Team>();
      monsterQuery.each([&](flecs::entity e, const Position& m_pos, Hitpoints& m_hp, const Team &m_t)
      {
        if (m_t.team == 0)
          return;

        if (length(pos - m_pos) < md.weaponLength * tile_size)
        {
          m_hp.hitpoints -= md.damage * ecs.delta_time();

          //remove entity if killed
          if (m_hp.hitpoints <= 0.f)
            e.destruct();
        }
      });
    });

  ecs.system<const Position, const MeleeDamage, const Team>()
    .each([&](const Position& pos, const MeleeDamage& md, const Team &t)
    {
      if (t.team == 0)
        return;

      //Hit player in weapon length radius
      auto playerQuery = ecs.query<const Position, Hitpoints, const IsPlayer>();
      playerQuery.each([&](flecs::entity e, const Position& p_pos, Hitpoints& p_hp, const IsPlayer)
      {
        if (length(pos - p_pos) < md.weaponLength * tile_size)
        {
          p_hp.hitpoints -= md.damage * ecs.delta_time();
        }
      });
    });

  ecs.system<const DungeonPortals, const DungeonData>()
    .each([&](const DungeonPortals &dp, const DungeonData &dd)
    {
      size_t w = dd.width;
      size_t ts = dp.tileSplit;
      for (size_t y = 0; y < dd.height / ts; ++y)
        DrawLineEx(Vector2{0.f, y * ts * tile_size},
                   Vector2{dd.width * tile_size, y * ts * tile_size}, 1.f, GetColor(0xff000080));
      for (size_t x = 0; x < dd.width / ts; ++x)
        DrawLineEx(Vector2{x * ts * tile_size, 0.f},
                   Vector2{x * ts * tile_size, dd.height * tile_size}, 1.f, GetColor(0xff000080));
      auto cameraQuery = ecs.query<const Camera2D>();
      cameraQuery.each([&](Camera2D cam)
      {
        Vector2 mousePosition = GetScreenToWorld2D(GetMousePosition(), cam);
        size_t wd = w / ts;
        for (size_t y = 0; y < dd.height / ts; ++y)
        {
          if (mousePosition.y < y * ts * tile_size || mousePosition.y > (y + 1) * ts * tile_size)
            continue;
          for (size_t x = 0; x < dd.width / ts; ++x)
          {
            if (mousePosition.x < x * ts * tile_size || mousePosition.x > (x + 1) * ts * tile_size)
              continue;
            for (size_t idx : dp.tilePortalsIndices[y * wd + x])
            {
              const PathPortal &portal = dp.portals[idx];
              Rectangle rect{portal.startX * tile_size, portal.startY * tile_size,
                             (portal.endX - portal.startX + 1) * tile_size,
                             (portal.endY - portal.startY + 1) * tile_size};
              DrawRectangleLinesEx(rect, 5, BLACK);
            }
          }
        }
        for (const PathPortal &portal : dp.portals)
        {
          Rectangle rect{portal.startX * tile_size, portal.startY * tile_size,
                         (portal.endX - portal.startX + 1) * tile_size,
                         (portal.endY - portal.startY + 1) * tile_size};
          Vector2 fromCenter{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
          DrawRectangleLinesEx(rect, 1, WHITE);
          if (mousePosition.x < rect.x || mousePosition.x > rect.x + rect.width ||
              mousePosition.y < rect.y || mousePosition.y > rect.y + rect.height)
            continue;
          DrawRectangleLinesEx(rect, 4, WHITE);
          for (const PortalConnection &conn : portal.conns)
          {
            const PathPortal &endPortal = dp.portals[conn.connIdx];
            Vector2 toCenter{(endPortal.startX + endPortal.endX + 1) * tile_size * 0.5f,
                             (endPortal.startY + endPortal.endY + 1) * tile_size * 0.5f};
            DrawLineEx(fromCenter, toCenter, 1.f, WHITE);
            DrawText(TextFormat("%d", int(conn.score)),
                     (fromCenter.x + toCenter.x) * 0.5f,
                     (fromCenter.y + toCenter.y) * 0.5f,
                     16, WHITE);
          }
        }
      });
    });
  steer::register_systems(ecs);
}

void gen_exit_and_spawners(flecs::world& ecs, size_t nSpawners)
{
  ecs.query<DungeonData>().each([&](DungeonData& dd)
  {
    Position playerPos;

    ecs.query<const Position, const IsPlayer>().each([&](const Position& pos, const IsPlayer)
    {
      playerPos = pos;
    });

    //Generate dijkstra map with 0 at player position
    std::vector<float> map;
    dmaps::gen_player_approach_map(ecs, map);
    size_t farthestIndex = find_max_valid_index(map);
    size_t x = farthestIndex % dd.width;
    size_t y = farthestIndex / dd.width;
   
    //Place exit
    dd.tiles[y * dd.width + x] = dungeon::exit;

    //Place 0 at exit position
    map[y * dd.width + x] = 0.f;

    //Add exit entity
    ecs.entity()
      .set<Position>({ x * tile_size, y * tile_size })
      .add<ExitTile>();

    //Place spawners
    for (size_t i = 0; i < nSpawners; ++i)
    {
      dmaps::process_dmap(map, dd);

      farthestIndex = find_max_valid_index(map);
      x = farthestIndex % dd.width;
      y = farthestIndex / dd.width;

      dd.tiles[y * dd.width + x] = dungeon::spawner;
      map[y * dd.width + x] = 0.f;

      //Create spawner entity
      Position spawnerPos { x * tile_size, y * tile_size };
      create_spawner(ecs, spawnerPos);
    }
  });
}

void init_shoot_em_up(flecs::world &ecs, bool& needToRebuildLevel, size_t& difficulty)
{
  register_roguelike_systems(ecs, needToRebuildLevel, difficulty);

  ecs.entity("swordsman_tex")
    .set(Texture2D{LoadTexture("assets/swordsman.png")});
  ecs.entity("minotaur_tex")
    .set(Texture2D{LoadTexture("assets/minotaur.png")});

  const Position walkableTile = dungeon::find_walkable_tile(ecs);
  create_player(ecs, walkableTile * tile_size + anchor, "swordsman_tex");
}

void init_dungeon(flecs::world &ecs, char *tiles, size_t w, size_t h)
{
  flecs::entity wallTex = ecs.entity("wall_tex")
    .set(Texture2D{LoadTexture("assets/wall.png")});
  flecs::entity floorTex = ecs.entity("floor_tex")
    .set(Texture2D{LoadTexture("assets/floor.png")});

  std::vector<char> dungeonData;
  dungeonData.resize(w * h);
  for (size_t y = 0; y < h; ++y)
    for (size_t x = 0; x < w; ++x)
      dungeonData[y * w + x] = tiles[y * w + x];
  ecs.entity("dungeon")
    .set(DungeonData{dungeonData, w, h});

  for (size_t y = 0; y < h; ++y)
    for (size_t x = 0; x < w; ++x)
    {
      char tile = tiles[y * w + x];
      flecs::entity tileEntity = ecs.entity()
        .add<BackgroundTile>()
        .set(Position{float(x) * tile_size, float(y) * tile_size})
        .set(Color{255, 255, 255, 255});
      if (tile == dungeon::wall)
        tileEntity.add<TextureSource>(wallTex);
      else if (tile == dungeon::floor)
        tileEntity.add<TextureSource>(floorTex);
    }
  prebuild_map(ecs);
}

void process_game(flecs::world &ecs)
{
}

