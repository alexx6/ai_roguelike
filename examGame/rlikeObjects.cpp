#include "rlikeObjects.h"
#include "ecsTypes.h"

flecs::entity create_monster(flecs::world &ecs, Position pos, Color col, const char *texture_src)
{
  flecs::entity textureSrc = ecs.entity(texture_src);
  return ecs.entity()
    .set(Position{pos.x, pos.y})
    .set(Velocity{0.f, 0.f})
    .set(MoveSpeed{57.6f})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .set(Color{col})
    .add<TextureSource>(textureSrc)
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{10.f, 2.f});
}

flecs::entity create_spawner(flecs::world& ecs, Position &pos)
{
  return ecs.entity()
    .set(Position{ pos.x, pos.y })
    .set(MonsterSpawner{ 3.0f, 3.0f });
}

void create_player(flecs::world &ecs, Position pos, const char *texture_src)
{
  flecs::entity textureSrc = ecs.entity(texture_src);
  ecs.entity("player")
    .set(Position{pos.x, pos.y})
    .set(Velocity{0.f, 0.f})
    .set(MoveSpeed{64.f})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .add<IsPlayer>()
    .set(Team{0})
    .set(PlayerInput{})
    .set(NumActions{2, 0})
    .set(Color{255, 255, 255, 255})
    .add<TextureSource>(textureSrc)
    .set(MeleeDamage{15.f, 3.f});
}

