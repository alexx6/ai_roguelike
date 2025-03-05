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
    .each([&](Position &pos, const Velocity &vel)
    {      
      //Simple collision detection
      Position deltaPosition = vel * ecs.delta_time();

      ecs.query<const DungeonData>().each([&](const DungeonData& dd)
      {
        Position anchor = { 30.f, 30.0f };
        Position testPos = pos + deltaPosition + anchor;
        Position aPos = pos + anchor;

        if (dd.tiles[(size_t)(aPos.y / tile_size) * dd.width + (size_t)(testPos.x / tile_size)] == dungeon::wall)
        {
          deltaPosition.x = 0;
        }

        if (dd.tiles[(size_t)(testPos.y / tile_size) * dd.width + (size_t)(aPos.x / tile_size)] == dungeon::wall)
        {
          deltaPosition.y = 0;
        }
      });

      pos += deltaPosition;
    });

  //Check if player on exit tile, then generate new dungeon
  ecs.system<const Position, const IsPlayer>().each([&](const Position& pos, const IsPlayer)
  {
    ecs.query<const DungeonData>().each([&](const DungeonData& dd)
    {
      Position anchor = { 30.f, 30.0f };
      Position aPos = pos + anchor;

      if (dd.tiles[(size_t)(aPos.y / tile_size) * dd.width + (size_t)(aPos.x / tile_size)] == dungeon::exit)
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
          Rectangle{float(pos.x), float(pos.y), tile_size, tile_size}, color);
    });

  ecs.system<const Position, const ExitTile>()
    .each([&](const Position& pos, const ExitTile)
    {
      const Rectangle rect = { pos.x, pos.y, tile_size, tile_size };
      DrawRectangleRec(rect, Color{ 255, 0, 255, 255 });
    });

  ecs.system<Texture2D>()
    .each([&](Texture2D &tex)
    {
      SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    });

  ecs.system<MonsterSpawner>()
    .each([&](MonsterSpawner &ms)
    {
      auto playerPosQuery = ecs.query<const Position, const IsPlayer>();
      playerPosQuery.each([&](const Position &pp, const IsPlayer &)
      {
        ms.timeToSpawn -= ecs.delta_time();
        while (ms.timeToSpawn < 0.f)
        {
          steer::Type st = steer::Type(GetRandomValue(0, steer::Type::Num - 1));
          const Color colors[steer::Type::Num] = {WHITE, RED, BLUE, GREEN};
          const float distances[steer::Type::Num] = {800.f, 800.f, 300.f, 300.f};
          const float dist = distances[st];
          constexpr int angRandMax = 1 << 16;
          const float angle = float(GetRandomValue(0, angRandMax)) / float(angRandMax) * PI * 2.f;
          Color col = colors[st];
          steer::create_steer_beh(create_monster(ecs,
              {pp.x + cosf(angle) * dist, pp.y + sinf(angle) * dist}, col, "minotaur_tex"), st);
          ms.timeToSpawn += ms.timeBetweenSpawns;
        }
      });
    });

  //Show entity hp
  ecs.system<const Position, const Hitpoints>()
    .each([&](const Position& pos, const Hitpoints& hp)
    {
      DrawText(TextFormat("hp: %d", int(hp.hitpoints)), pos.x, pos.y - 12, 18, WHITE);
    });

  //Calculate timed melee hits from players with attack timer
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

          //remove entity if
          if (m_hp.hitpoints <= 0.f)
            e.destruct();
        }
      });
    });

  //Calculate timed melee hits from monsters with attack timer
  ecs.system<const Position, const MeleeDamage, const Team>()
    .each([&](const Position& pos, const MeleeDamage& md, const Team &t)
    {
      if (t.team == 0)
        return;

      //Hit player in weapon length radius if ready to attack
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

    std::vector<float> approachMap;
    dmaps::gen_player_approach_map(ecs, approachMap);
    
    size_t farthestIndex = find_max_valid_index(approachMap);

    size_t x = farthestIndex % dd.width;
    size_t y = farthestIndex / dd.width;

    dd.tiles[y * dd.width + x] = dungeon::exit;

    ecs.entity()
      .set<Position>({ x * tile_size, y * tile_size })
      .add<ExitTile>();
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
  create_player(ecs, walkableTile * tile_size, "swordsman_tex");
  //create_spawner(ecs);
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

