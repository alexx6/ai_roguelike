#include "roguelike.h"
#include "ecsTypes.h"
#include "raylib.h"
#include "stateMachine.h"
#include "aiLibrary.h"

template<typename T>
T sqr(T a) { return a * a; }

template<typename T, typename U>
static float dist_sq(const T& lhs, const U& rhs) { return float(sqr(lhs.x - rhs.x) + sqr(lhs.y - rhs.y)); }

template<typename T, typename U>
static float dist(const T& lhs, const U& rhs) { return sqrtf(dist_sq(lhs, rhs)); }

static void add_patrol_attack_flee_sm(flecs::entity entity)
{
  entity.get([](StateMachine &sm)
  {
    int patrol = sm.addState(create_patrol_state(3.f));
    int moveToEnemy = sm.addState(create_move_to_enemy_state());
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());

    sm.addTransition(create_enemy_available_transition(3.f), patrol, moveToEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), moveToEnemy, patrol);

    sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(5.f)),
                     moveToEnemy, fleeFromEnemy);
    sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(3.f)),
                     patrol, fleeFromEnemy);

    sm.addTransition(create_negate_transition(create_enemy_available_transition(7.f)), fleeFromEnemy, patrol);
  });
}

static void add_patrol_flee_sm(flecs::entity entity)
{
  entity.get([](StateMachine &sm)
  {
    int patrol = sm.addState(create_patrol_state(3.f));
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());

    sm.addTransition(create_enemy_available_transition(3.f), patrol, fleeFromEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), fleeFromEnemy, patrol);
  });
}

static void add_attack_sm(flecs::entity entity)
{
  entity.get([](StateMachine &sm)
  {
    sm.addState(create_move_to_enemy_state());
  });
}

static void add_archer_sm(flecs::entity entity)
{
    entity.get([](StateMachine& sm)
    {
        //int patrol = sm.addState(create_patrol_state(3.f));
        int nopState = sm.addState(create_nop_state());
        int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());
        int moveToEnemy = sm.addState(create_move_to_enemy_state());
        int shootEnemy = sm.addState(create_shoot_enemy_state());

        //nopState transitions
        sm.addTransition(create_enemy_available_transition(5.f), nopState, shootEnemy);

        //shootEnemy transitions
        sm.addTransition(create_enemy_available_transition(3.f), shootEnemy, fleeFromEnemy);
        sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), shootEnemy, nopState);

        //fleeFromEnemy transitions
        sm.addTransition(create_negate_transition(create_enemy_available_transition(7.f)), fleeFromEnemy, nopState);
    });
}

static void add_healer_sm(flecs::entity entity)
{
    entity.get([](StateMachine& sm)
    {
        int moveToPlayer = sm.addState(create_move_to_player_state());
        int moveToPlayerAndHeal = sm.addState(create_move_to_player_and_heal_state());
        int moveToEnemy = sm.addState(create_move_to_enemy_state());

        //moveToPlayer transitions
        sm.addTransition(create_and_transition(create_negate_transition(create_player_hitpoints_less_than_transition(40.f)), create_enemy_available_transition(5.f)), moveToPlayer, moveToEnemy);
        sm.addTransition(create_player_hitpoints_less_than_transition(40.f), moveToPlayer, moveToPlayerAndHeal);

        //moveToEnemy transitions
        sm.addTransition(create_and_transition(create_negate_transition(create_player_hitpoints_less_than_transition(40.f)), create_negate_transition(create_enemy_available_transition(5.f))), moveToEnemy, moveToPlayer);
        sm.addTransition(create_player_hitpoints_less_than_transition(40.f), moveToEnemy, moveToPlayerAndHeal);

        //moveToPlayerAndHeal transitions 
        sm.addTransition(create_and_transition(create_negate_transition(create_player_hitpoints_less_than_transition(40.f)), create_negate_transition(create_enemy_available_transition(5.f))), moveToPlayerAndHeal, moveToPlayer);
        sm.addTransition(create_and_transition(create_negate_transition(create_player_hitpoints_less_than_transition(40.f)), create_enemy_available_transition(5.f)), moveToPlayerAndHeal, moveToEnemy);
    });
}

static flecs::entity create_monster(flecs::world &ecs, int x, int y, Color color)
{
  return ecs.entity()
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(PatrolPos{x, y})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .set(Color{color})
    .set(StateMachine{})
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{20.f});
}

static flecs::entity create_archer(flecs::world& ecs, int x, int y, Color color)
{
    return ecs.entity()
        .set(Position{ x, y })
        .set(MovePos{ x, y })
        .set(PatrolPos{ x, y })
        .set(Hitpoints{ 100.f })
        .set(Action{ EA_NOP })
        .set(Color{ color })
        .set(StateMachine{})
        .set(Team{ 1 })
        .set(NumActions{ 1, 0 })
        .set(RangedDamage{ 20.f });
}

static void create_player(flecs::world &ecs, int x, int y)
{
  ecs.entity("player")
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(Hitpoints{10.f})
    .set(GetColor(0xeeeeeeff))
    .set(Action{EA_NOP})
    .add<IsPlayer>()
    .set(Team{0})
    .set(PlayerInput{})
    .set(NumActions{2, 0})
    .set(MeleeDamage{50.f});
}

static flecs::entity create_healer(flecs::world& ecs, int x, int y)
{
    return ecs.entity("healer")
        .set(Position{ x, y })
        .set(MovePos{ x, y })
        .set(Hitpoints{ 100.f })
        .set(GetColor(0xffeeeeff))
        .set(Action{ EA_NOP })
        .set(StateMachine{})
        .set(Team{ 0 })
        .set(MeleeDamage{ 50.f })
        .set(HealingMagic{ 10.f, 10, 0 });
}

static void create_heal(flecs::world &ecs, int x, int y, float amount)
{
  ecs.entity()
    .set(Position{x, y})
    .set(HealAmount{amount})
    .set(GetColor(0x44ff44ff));
}

static void create_powerup(flecs::world &ecs, int x, int y, float amount)
{
  ecs.entity()
    .set(Position{x, y})
    .set(PowerupAmount{amount})
    .set(Color{255, 255, 0, 255});
}

static void register_roguelike_systems(flecs::world &ecs)
{
  ecs.system<PlayerInput, Action, const IsPlayer>()
    .each([&](PlayerInput &inp, Action &a, const IsPlayer)
    {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);
      if (left && !inp.left)
        a.action = EA_MOVE_LEFT;
      if (right && !inp.right)
        a.action = EA_MOVE_RIGHT;
      if (up && !inp.up)
        a.action = EA_MOVE_UP;
      if (down && !inp.down)
        a.action = EA_MOVE_DOWN;
      inp.left = left;
      inp.right = right;
      inp.up = up;
      inp.down = down;
    });
  ecs.system<const Position, const Color>()
    .without<TextureSource>(flecs::Wildcard)
    .each([&](const Position &pos, const Color color)
    {
      const Rectangle rect = {float(pos.x), float(pos.y), 1, 1};
      DrawRectangleRec(rect, color);
    });
  ecs.system<const Position, const Color>()
    .with<TextureSource>(flecs::Wildcard)
    .each([&](flecs::entity e, const Position &pos, const Color color)
    {
      const auto textureSrc = e.target<TextureSource>();
      DrawTextureQuad(*textureSrc.get<Texture2D>(),
          Vector2{1, 1}, Vector2{0, 0},
          Rectangle{float(pos.x), float(pos.y), 1, 1}, color);
    });
}


void init_roguelike(flecs::world &ecs)
{
  register_roguelike_systems(ecs);

  //add_patrol_attack_flee_sm(create_monster(ecs, 5, 5, GetColor(0xee00eeff)));
  //add_patrol_attack_flee_sm(create_monster(ecs, 10, -5, GetColor(0xee00eeff)));
  //add_patrol_flee_sm(create_monster(ecs, -5, -5, GetColor(0x111111ff)));
  //add_attack_sm(create_monster(ecs, -5, 5, GetColor(0x880000ff)));

  add_archer_sm(create_archer(ecs, -5, 5, GetColor(0x00FF00FF)));
  add_healer_sm(create_healer(ecs, 0, 2));

  create_player(ecs, 0, 0);

  //create_powerup(ecs, 7, 7, 10.f);
  //create_powerup(ecs, 10, -6, 10.f);
  //create_powerup(ecs, 10, -4, 10.f);

  //create_heal(ecs, -5, -5, 50.f);
  //create_heal(ecs, -5, 5, 50.f);
}

static bool is_player_acted(flecs::world &ecs)
{
  static auto processPlayer = ecs.query<const IsPlayer, const Action>();
  bool playerActed = false;
  processPlayer.each([&](const IsPlayer, const Action &a)
  {
    playerActed = a.action != EA_NOP;
  });
  return playerActed;
}

static bool upd_player_actions_count(flecs::world &ecs)
{
  static auto updPlayerActions = ecs.query<const IsPlayer, NumActions>();
  bool actionsReached = false;
  updPlayerActions.each([&](const IsPlayer, NumActions &na)
  {
    na.curActions = (na.curActions + 1) % na.numActions;
    actionsReached |= na.curActions == 0;
  });
  return actionsReached;
}

static Position move_pos(Position pos, int action)
{
  if (action == EA_MOVE_LEFT)
    pos.x--;
  else if (action == EA_MOVE_RIGHT)
    pos.x++;
  else if (action == EA_MOVE_UP)
    pos.y--;
  else if (action == EA_MOVE_DOWN)
    pos.y++;
  return pos;
}

static void process_actions(flecs::world &ecs)
{
  static auto processActionsMelee = ecs.query<Action, Position, MovePos, const MeleeDamage, const Team>();
  static auto processActionsRanged = ecs.query<Action, Position, MovePos, const RangedDamage, const Team>();
  static auto processActionsHealer = ecs.query<Action, Position, MovePos, const MeleeDamage, const Team, HealingMagic>();
  static auto checkAttacks = ecs.query<const MovePos, Hitpoints, const Team>();
  // Process all actions
  ecs.defer([&]
  {
    processActionsMelee.each([&](flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &dmg, const Team &team)
    {
      Position nextPos = move_pos(pos, a.action);
      bool blocked = false;
      checkAttacks.each([&](flecs::entity enemy, const MovePos &epos, Hitpoints &hp, const Team &enemy_team)
      {
        if (entity != enemy && epos == nextPos)
        {
          blocked = true;
          if (team.team != enemy_team.team)
            hp.hitpoints -= dmg.damage;
        }
      });
      if (blocked)
        a.action = EA_NOP;
      else
        mpos = nextPos;
    });

    processActionsRanged.each([&](flecs::entity entity, Action& a, Position& pos, MovePos& mpos, const RangedDamage& dmg, const Team& team)
    {
        Position nextPos = move_pos(pos, a.action);
        bool blocked = false;
        checkAttacks.each([&](flecs::entity enemy, const MovePos& epos, Hitpoints& hp, const Team& enemy_team)
        {
            if (entity != enemy)
            {
                if (epos == nextPos) {
                    blocked = true;
                }

                if (team.team != enemy_team.team && a.action == EA_RANGED_ATTACK && sqrtf(dist_sq(mpos, epos)) < 5)
                    hp.hitpoints -= dmg.damage;
            }
        });

        if (blocked)
            a.action = EA_NOP;
        else
            mpos = nextPos;
    });

    processActionsHealer.each([&](flecs::entity entity, Action& a, Position& pos, MovePos& mpos, const MeleeDamage& dmg, const Team& team, HealingMagic& healingMagic)
    {
        Position nextPos = move_pos(pos, a.action);
        bool blocked = false;
        checkAttacks.each([&](flecs::entity enemy, const MovePos& epos, Hitpoints& hp, const Team& enemy_team)
        {
            if (entity != enemy && epos == nextPos)
            {
                blocked = true;
                if (team.team != enemy_team.team) {
                    hp.hitpoints -= dmg.damage;
                }
            }
        });
        if (blocked)
            a.action = EA_NOP;
        else
            mpos = nextPos;

        healingMagic.healReloadCurrent = healingMagic.healReloadCurrent == 0 ? 0 : healingMagic.healReloadCurrent - 1;

        if (healingMagic.magicActive && healingMagic.healReloadCurrent == 0)
        {
            static auto playerQuery = ecs.query<const IsPlayer, const MovePos&, Hitpoints>();
            playerQuery.each([&](const IsPlayer&, const MovePos& ppos, Hitpoints& hp)
            {
                if (std::abs(ppos.x - nextPos.x) + std::abs(ppos.y - nextPos.y) <= 1)
                {
                    hp.hitpoints += healingMagic.amount;
                    healingMagic.healReloadCurrent = healingMagic.healReloadLength;
                }
            });
        }
    });
    // now move
    processActionsMelee.each([&](flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &, const Team&)
    {
      pos = mpos;
      a.action = EA_NOP;
    });    
    
    processActionsRanged.each([&](flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const RangedDamage &, const Team&)
    {
      pos = mpos;
      a.action = EA_NOP;
    });
  });

  static auto deleteAllDead = ecs.query<const Hitpoints>();
  ecs.defer([&]
  {
    deleteAllDead.each([&](flecs::entity entity, const Hitpoints &hp)
    {
      if (hp.hitpoints <= 0.f)
        entity.destruct();
    });
  });

  static auto playerPickup = ecs.query<const IsPlayer, const Position, Hitpoints, MeleeDamage>();
  static auto healPickup = ecs.query<const Position, const HealAmount>();
  static auto powerupPickup = ecs.query<const Position, const PowerupAmount>();
  ecs.defer([&]
  {
    playerPickup.each([&](const IsPlayer&, const Position &pos, Hitpoints &hp, MeleeDamage &dmg)
    {
      healPickup.each([&](flecs::entity entity, const Position &ppos, const HealAmount &amt)
      {
        if (pos == ppos)
        {
          hp.hitpoints += amt.amount;
          entity.destruct();
        }
      });
      powerupPickup.each([&](flecs::entity entity, const Position &ppos, const PowerupAmount &amt)
      {
        if (pos == ppos)
        {
          dmg.damage += amt.amount;
          entity.destruct();
        }
      });
    });
  });
}

void process_turn(flecs::world &ecs)
{
  static auto stateMachineAct = ecs.query<StateMachine>();
  if (is_player_acted(ecs))
  {
    if (upd_player_actions_count(ecs))
    {
      // Plan action for NPCs
      ecs.defer([&]
      {
        stateMachineAct.each([&](flecs::entity e, StateMachine &sm)
        {
          sm.act(0.f, ecs, e);
        });
      });
    }
    process_actions(ecs);
  }
}

void print_stats(flecs::world &ecs)
{
  static auto playerStatsQuery = ecs.query<const IsPlayer, const Hitpoints, const MeleeDamage>();
  playerStatsQuery.each([&](const IsPlayer &, const Hitpoints &hp, const MeleeDamage &dmg)
  {
    DrawText(TextFormat("hp: %d", int(hp.hitpoints)), 20, 20, 20, WHITE);
    DrawText(TextFormat("power: %d", int(dmg.damage)), 20, 40, 20, WHITE);
  });
}

