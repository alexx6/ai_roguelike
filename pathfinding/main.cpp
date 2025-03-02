#include "raylib.h"
#include <functional>
#include <vector>
#include <limits>
#include <float.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include "math.h"
#include "dungeonGen.h"
#include "dungeonUtils.h"

const int NUMBER_OF_GENERATIONS = 500;
const int MAX_AWA_STAR_ITERATIONS = 10;

template<typename T>
static size_t coord_to_idx(T x, T y, size_t w)
{
  return size_t(y) * w + size_t(x);
}

static void draw_nav_grid(const char *input, size_t width, size_t height)
{
  for (size_t y = 0; y < height; ++y)
    for (size_t x = 0; x < width; ++x)
    {
      char symb = input[coord_to_idx(x, y, width)];
      Color color = GetColor(symb == ' ' ? 0xeeeeeeff : symb == 'o' ? 0x7777ffff : 0x222222ff);
      const Rectangle rect = {float(x), float(y), 1.f, 1.f};
      DrawRectangleRec(rect, color);
      //DrawPixel(int(x), int(y), color);
    }
}

static void draw_path(std::vector<Position> path)
{
  for (const Position &p : path)
  {
    const Rectangle rect = {float(p.x), float(p.y), 1.f, 1.f};
    DrawRectangleRec(rect, GetColor(0x44000088));
  }
}

static std::vector<Position> reconstruct_path(std::vector<Position> prev, Position to, size_t width)
{
  Position curPos = to;
  std::vector<Position> res = {curPos};
  while (prev[coord_to_idx(curPos.x, curPos.y, width)] != Position{-1, -1})
  {
    curPos = prev[coord_to_idx(curPos.x, curPos.y, width)];
    res.insert(res.begin(), curPos);
  }
  return res;
}

float heuristic(Position lhs, Position rhs)
{
  return sqrtf(square(float(lhs.x - rhs.x)) + square(float(lhs.y - rhs.y)));
};

static float ida_star_search(const char *input, size_t width, size_t height, std::vector<Position> &path, const float g, const float bound, Position to)
{
  const Position &p = path.back();
  const float f = g + heuristic(p, to);
  if (f > bound)
    return f;
  if (p == to)
    return -f;
  float min = FLT_MAX;
  auto checkNeighbour = [&](Position p) -> float
  {
    // out of bounds
    if (p.x < 0 || p.y < 0 || p.x >= int(width) || p.y >= int(height))
      return 0.f;
    size_t idx = coord_to_idx(p.x, p.y, width);
    // not empty
    if (input[idx] == '#')
      return 0.f;
    if (std::find(path.begin(), path.end(), p) != path.end())
      return 0.f;
    path.push_back(p);
    float weight = input[idx] == 'o' ? 10.f : 1.f;
    float gScore = g + 1.f * weight; // we're exactly 1 unit away
    const float t = ida_star_search(input, width, height, path, gScore, bound, to);
    if (t < 0.f)
      return t;
    if (t < min)
      min = t;
    path.pop_back();
    return t;
  };
  float lv = checkNeighbour({p.x + 1, p.y + 0});
  if (lv < 0.f) return lv;
  float rv = checkNeighbour({p.x - 1, p.y + 0});
  if (rv < 0.f) return rv;
  float tv = checkNeighbour({p.x + 0, p.y + 1});
  if (tv < 0.f) return tv;
  float bv = checkNeighbour({p.x + 0, p.y - 1});
  if (bv < 0.f) return bv;
  return min;
}

static std::vector<Position> find_ida_star_path(const char *input, size_t width, size_t height, Position from, Position to)
{
  float bound = heuristic(from, to);
  std::vector<Position> path = {from};
  while (true)
  {
    const float t = ida_star_search(input, width, height, path, 0.f, bound, to);
    if (t < 0.f)
      return path;
    if (t == FLT_MAX)
      return {};
    bound = t;
    printf("new bound %0.1f\n", bound);
  }
  return {};
}

static std::vector<Position> find_path_a_star(const char *input, size_t width, size_t height, Position from, Position to, float weight)
{
  if (from.x < 0 || from.y < 0 || from.x >= int(width) || from.y >= int(height))
    return std::vector<Position>();
  size_t inpSize = width * height;

  std::vector<float> g(inpSize, std::numeric_limits<float>::max());
  std::vector<float> f(inpSize, std::numeric_limits<float>::max());
  std::vector<Position> prev(inpSize, {-1,-1});

  auto getG = [&](Position p) -> float { return g[coord_to_idx(p.x, p.y, width)]; };
  auto getF = [&](Position p) -> float { return f[coord_to_idx(p.x, p.y, width)]; };

  g[coord_to_idx(from.x, from.y, width)] = 0;
  f[coord_to_idx(from.x, from.y, width)] = weight * heuristic(from, to);

  std::vector<Position> openList = {from};
  std::vector<Position> closedList;

  while (!openList.empty())
  {
    size_t bestIdx = 0;
    float bestScore = getF(openList[0]);
    for (size_t i = 1; i < openList.size(); ++i)
    {
      float score = getF(openList[i]);
      if (score < bestScore)
      {
        bestIdx = i;
        bestScore = score;
      }
    }
    if (openList[bestIdx] == to)
      return reconstruct_path(prev, to, width);
    Position curPos = openList[bestIdx];
    openList.erase(openList.begin() + bestIdx);
    if (std::find(closedList.begin(), closedList.end(), curPos) != closedList.end())
      continue;
    size_t idx = coord_to_idx(curPos.x, curPos.y, width);
    const Rectangle rect = {float(curPos.x), float(curPos.y), 1.f, 1.f};
    DrawRectangleRec(rect, Color{uint8_t(g[idx]), uint8_t(g[idx]), 0, 100});
    closedList.emplace_back(curPos);
    auto checkNeighbour = [&](Position p)
    {
      // out of bounds
      if (p.x < 0 || p.y < 0 || p.x >= int(width) || p.y >= int(height))
        return;
      size_t idx = coord_to_idx(p.x, p.y, width);
      // not empty
      if (input[idx] == '#')
        return;
      float edgeWeight = input[idx] == 'o' ? 10.f : 1.f;
      float gScore = getG(curPos) + 1.f * edgeWeight; // we're exactly 1 unit away
      if (gScore < getG(p))
      {
        prev[idx] = curPos;
        g[idx] = gScore;
        f[idx] = gScore + weight * heuristic(p, to);
      }
      bool found = std::find(openList.begin(), openList.end(), p) != openList.end();
      if (!found)
        openList.emplace_back(p);
    };
    checkNeighbour({curPos.x + 1, curPos.y + 0});
    checkNeighbour({curPos.x - 1, curPos.y + 0});
    checkNeighbour({curPos.x + 0, curPos.y + 1});
    checkNeighbour({curPos.x + 0, curPos.y - 1});
  }
  // empty path
  return std::vector<Position>();
}

static std::vector<Position> find_path_awa_star(const char* input, size_t width, size_t height, Position from, Position to, float weight)
{
    if (from.x < 0 || from.y < 0 || from.x >= int(width) || from.y >= int(height))
        return std::vector<Position>();
    size_t inpSize = width * height;

    std::vector<float> g(inpSize, std::numeric_limits<float>::max());
    std::vector<float> f(inpSize, std::numeric_limits<float>::max());
    std::vector<float> fw(inpSize, std::numeric_limits<float>::max());
    std::vector<Position> prev(inpSize, { -1,-1 });

    auto getG = [&](Position p) -> float { return g[coord_to_idx(p.x, p.y, width)]; };
    auto getF = [&](Position p) -> float { return f[coord_to_idx(p.x, p.y, width)]; };
    auto getFW = [&](Position p) -> float { return fw[coord_to_idx(p.x, p.y, width)]; };

    g[coord_to_idx(from.x, from.y, width)] = 0;
    f[coord_to_idx(from.x, from.y, width)] = heuristic(from, to);
    fw[coord_to_idx(from.x, from.y, width)] = weight * heuristic(from, to);

    std::vector<Position> openList = { from };
    std::vector<Position> closedList;

    int iterations = 0;
    
    bool isIncumbentSet = false;
    Position incumbent = { 0, 0 };

    while (!openList.empty() && iterations < MAX_AWA_STAR_ITERATIONS)
    {
        size_t bestIdx = 0;
        float bestScore = getFW(openList[0]);
        for (size_t i = 1; i < openList.size(); ++i)
        {
            float score = getFW(openList[i]);
            if (score < bestScore)
            {
                bestIdx = i;
                bestScore = score;
            }
        }

        //if (openList[bestIdx] == to)
        //    return reconstruct_path(prev, to, width);

        Position curPos = openList[bestIdx];
        openList.erase(openList.begin() + bestIdx);

        //if (std::find(closedList.begin(), closedList.end(), curPos) != closedList.end())
        //    continue;

        size_t idx = coord_to_idx(curPos.x, curPos.y, width);
        const Rectangle rect = { float(curPos.x), float(curPos.y), 1.f, 1.f };
        DrawRectangleRec(rect, Color{ uint8_t(g[idx]), uint8_t(g[idx]), 0, 100 });

        if (!isIncumbentSet || getF(curPos) < getF(incumbent)) {
            if(std::find(closedList.begin(), closedList.end(), curPos) == closedList.end())
                closedList.emplace_back(curPos);

            auto checkNeighbour = [&](Position p)
            {
                if (p.x < 0 || p.y < 0 || p.x >= int(width) || p.y >= int(height))
                    return;

                size_t idx = coord_to_idx(p.x, p.y, width);

                if (input[idx] == '#')
                    return;

                float edgeWeight = input[idx] == 'o' ? 10.f : 1.f;
                float gScore = getG(curPos) + 1.f * edgeWeight;

                if (isIncumbentSet && gScore + heuristic(p, to) >= getF(incumbent)) 
                {
                    return;
                }

                auto pItOpen = std::find(openList.begin(), openList.end(), p);
                auto pItClosed = std::find(closedList.begin(), closedList.end(), p);

                if (p == to) 
                {
                    g[idx] = gScore;
                    f[idx] = g[idx];
                    prev[idx] = curPos;
                    ++iterations;
                    incumbent = p;
                    isIncumbentSet = true;
                }
                else if ((pItOpen != openList.end() || pItClosed != closedList.end()) && getG(p) > gScore)
                {
                    g[idx] = gScore;
                    f[idx] = g[idx] + heuristic(p, to);
                    fw[idx] = g[idx] + weight * heuristic(p, to);
                    prev[idx] = curPos;

                    if (pItClosed != closedList.end())
                    {
                        if (pItOpen == openList.end())
                            openList.emplace_back(p);

                        closedList.erase(pItClosed);
                    }
                }
                else if (pItOpen == openList.end() && pItClosed == closedList.end())
                {
                    g[idx] = gScore;
                    f[idx] = g[idx] + heuristic(p, to);
                    fw[idx] = g[idx] + weight * heuristic(p, to);
                    prev[idx] = curPos;
                    openList.emplace_back(p);
                }
            };

            checkNeighbour({ curPos.x + 1, curPos.y + 0 });
            checkNeighbour({ curPos.x - 1, curPos.y + 0 });
            checkNeighbour({ curPos.x + 0, curPos.y + 1 });
            checkNeighbour({ curPos.x + 0, curPos.y - 1 });
        }
    }
    
    
    // empty path
    return reconstruct_path(prev, to, width);
}

void draw_nav_data(const char *input, size_t width, size_t height, Position from, Position to, float weight)
{
  draw_nav_grid(input, width, height);
  std::vector<Position> path = find_path_awa_star(input, width, height, from, to, weight);
  //std::vector<Position> path = find_path_a_star(input, width, height, from, to, weight);
  //std::vector<Position> path = find_ida_star_path(input, width, height, from, to);
  draw_path(path);
}

int getPathCost(const char* input, size_t width, std::vector<Position>& path)
{
    int result = 0;

    for (int i = 1; i < path.size(); ++i) {
        result += input[coord_to_idx(path[i].x, path[i].y, width)] == 'o' ? 10 : 1;
    }

    return result;
}


float getOptimalSolutionsPercentage(float w)
{
    int totalOptimalSolutions = 0;

    constexpr size_t dungWidth = 100;
    constexpr size_t dungHeight = 100;
    char* navGrid = new char[dungWidth * dungHeight];
    Position from;
    Position to;

    for (int i = 0; i < NUMBER_OF_GENERATIONS; ++i)
    {
        gen_drunk_dungeon(navGrid, dungWidth, dungHeight, 24, 100);
        spill_drunk_water(navGrid, dungWidth, dungHeight, 8, 10);
        from = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);
        to = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);

        std::vector<Position> pathWeighed = find_path_a_star(navGrid, dungWidth, dungHeight, from, to, w);
        std::vector<Position> path = find_path_a_star(navGrid, dungWidth, dungHeight, from, to, 1.0f);

        if (getPathCost(navGrid, dungWidth, path) == getPathCost(navGrid, dungWidth, pathWeighed)) {
            totalOptimalSolutions += 1;
        }
    }

    return totalOptimalSolutions / (float)NUMBER_OF_GENERATIONS;
}

void findOptimalWeight(float p, float startingWeight = 1.0f)
{
    float epsilon = 0.01f * 0.1f;
    float weight = startingWeight;
    float optimalPercentage;
    float delta = 0.1f;
    float sign = 1.0f;

    do
    {
        weight += delta * sign;

        optimalPercentage = getOptimalSolutionsPercentage(weight);

        if (optimalPercentage - p > 0)
        {
            if (sign < 0)
                delta /= 2.0f;

            sign = 1.0f;
        }
        else {
            if (sign > 0)
                delta /= 2.0f;

            sign = -1.0f;
        }

    } while (std::abs(optimalPercentage - p) > epsilon);

    printf("Found weight = %f, optimal in %f%% of %d generaions\n", weight, optimalPercentage, NUMBER_OF_GENERATIONS);
}

int main(int /*argc*/, const char ** /*argv*/)
{
  int width = 1920;
  int height = 1080;
  InitWindow(width, height, "w3 AI MIPT");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  constexpr size_t dungWidth = 100;
  constexpr size_t dungHeight = 100;
  char *navGrid = new char[dungWidth * dungHeight];
  gen_drunk_dungeon(navGrid, dungWidth, dungHeight, 24, 100);
  spill_drunk_water(navGrid, dungWidth, dungHeight, 8, 10);
  float weight = 2.f;

  Position from = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);
  Position to = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);

  Camera2D camera = { {0, 0}, {0, 0}, 0.f, 1.f };
  //camera.offset = Vector2{ width * 0.5f, height * 0.5f };
  camera.zoom = float(height) / float(dungHeight);

  //findOptimalWeight(0.99f); //Found weight = 1.225000, optimal in 0.990000% of 500 generaions
  //findOptimalWeight(0.95f, 1.4f); //Found weight = 1.506250, optimal in 0.950000% of 500 generaions
  //findOptimalWeight(0.90f, 1.5f); //Found weight = 1.753125, optimal in 0.900000% of 500 generaions
  //findOptimalWeight(0.75f, 2.5f); //Found weight = 2.540528, optimal in 0.750000% of 500 generaions

  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
  while (!WindowShouldClose())
  {
    // pick pos
    Vector2 mousePosition = GetScreenToWorld2D(GetMousePosition(), camera);
    Position p{int(mousePosition.x), int(mousePosition.y)};
    if (IsMouseButtonPressed(2) || IsKeyPressed(KEY_Q))
    {
      size_t idx = coord_to_idx(p.x, p.y, dungWidth);
      if (idx < dungWidth * dungHeight)
        navGrid[idx] = navGrid[idx] == ' ' ? '#' : navGrid[idx] == '#' ? 'o' : ' ';
    }
    else if (IsMouseButtonPressed(0))
    {
      Position &target = from;
      target = p;
    }
    else if (IsMouseButtonPressed(1))
    {
      Position &target = to;
      target = p;
    }
    if (IsKeyPressed(KEY_SPACE))
    {
      gen_drunk_dungeon(navGrid, dungWidth, dungHeight, 24, 100);
      spill_drunk_water(navGrid, dungWidth, dungHeight, 8, 10);
      from = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);
      to = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);
    }
    if (IsKeyPressed(KEY_UP))
    {
      weight += 0.1f;
      printf("new weight %f\n", weight);
    }
    if (IsKeyPressed(KEY_DOWN))
    {
      weight = std::max(1.f, weight - 0.1f);
      printf("new weight %f\n", weight);
    }
    BeginDrawing();
      ClearBackground(BLACK);
      BeginMode2D(camera);
        draw_nav_data(navGrid, dungWidth, dungHeight, from, to, weight);
      EndMode2D();
    EndDrawing();
  }
  CloseWindow();
  return 0;
}