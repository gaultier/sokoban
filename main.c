#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

// Sprites
#include "character_down.h"
#include "character_left.h"
#include "character_right.h"
#include "character_up.h"
#include "crate.h"
#include "crate_ok.h"
#include "objective.h"
#include "wall.h"

#define pg_unused(x) ((void)(x))

typedef enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT } Direction;
typedef enum __attribute__((packed)) {
  // Numbers do not matter, as long as `ENTITY_NONE` is 0.
  ENTITY_NONE = 0,
  ENTITY_WALL = 1 << 0,
  ENTITY_OBJECTIVE = 1 << 1,
  ENTITY_CRATE = 1 << 2,
  ENTITY_CRATE_OK = (ENTITY_OBJECTIVE | ENTITY_CRATE), // Pseudo entity.
  ENTITY_CHARACTER = 1 << 4,
  ENTITY_MAX, // Not an entity.
} Entity;

static bool bitset_contains(Entity bitset, Entity b) {
  return (bitset & b) == b;
}
static bool bitset_is_exactly(Entity bitset, Entity b) { return bitset == b; }
static void bitset_remove(Entity *bitset, Entity b) { *bitset &= ~b; }
static void bitset_add(Entity *bitset, Entity b) { *bitset |= b; }

#define MAP_WIDTH 12
#define MAP_HEIGHT 12
#define MAP_SIZE ((MAP_WIDTH) * (MAP_WIDTH))

static const uint32_t CELL_SIZE = 34;
static const uint16_t SCREEN_WIDTH = MAP_WIDTH * CELL_SIZE;
static const uint16_t SCREEN_HEIGHT = MAP_HEIGHT * CELL_SIZE;

static const Entity map[MAP_WIDTH][MAP_HEIGHT] = {
    {ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_NONE, ENTITY_CHARACTER,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_NONE, ENTITY_NONE, ENTITY_CRATE, ENTITY_NONE,
     ENTITY_CRATE, ENTITY_NONE, ENTITY_CRATE, ENTITY_NONE, ENTITY_CRATE,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_NONE, ENTITY_WALL, ENTITY_NONE, ENTITY_OBJECTIVE,
     ENTITY_NONE, ENTITY_NONE, ENTITY_OBJECTIVE, ENTITY_NONE, ENTITY_NONE,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE,
     ENTITY_NONE, ENTITY_NONE, ENTITY_WALL, ENTITY_OBJECTIVE, ENTITY_NONE,
     ENTITY_NONE, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_NONE, ENTITY_CRATE, ENTITY_WALL, ENTITY_WALL,
     ENTITY_NONE, ENTITY_NONE, ENTITY_OBJECTIVE, ENTITY_NONE, ENTITY_NONE,
     ENTITY_NONE, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE,
     ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_WALL, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE,
     ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_WALL, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE,
     ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_OBJECTIVE, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_WALL, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE,
     ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_WALL, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE,
     ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL}};

static uint8_t get_next_cell_i(Direction dir, uint8_t cell_i) {
  switch (dir) {
  case DIR_UP:
    return cell_i - MAP_WIDTH;
  case DIR_RIGHT:
    return cell_i + 1;
  case DIR_DOWN:
    return cell_i + MAP_WIDTH;
  case DIR_LEFT:
    return cell_i - 1;
  }
  __builtin_unreachable();
}

static void load_map(Entity *map, uint8_t *crates_count,
                     uint8_t *objectives_count, uint8_t *character_cell_i) {
  SDL_assert(map != 0);
  SDL_assert(crates_count != 0);
  SDL_assert(objectives_count != 0);
  SDL_assert(character_cell_i != 0);

  for (uint8_t i = 0; i < MAP_SIZE; i++) {
    const Entity cell = map[i];
    SDL_assert(cell == ENTITY_NONE || cell == ENTITY_CHARACTER ||
               cell == ENTITY_WALL || cell == ENTITY_OBJECTIVE ||
               cell == ENTITY_CRATE);

    *crates_count += cell == ENTITY_CRATE;
    *objectives_count += cell == ENTITY_OBJECTIVE;
    if (cell == ENTITY_CHARACTER)
      *character_cell_i = i;
  }
}

static SDL_Texture *load_texture(SDL_Renderer *renderer, const uint8_t *data) {
  SDL_Surface *surface =
      SDL_CreateRGBSurfaceFrom((void *)data, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  SDL_assert(surface != 0);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_assert(texture != 0);
  SDL_FreeSurface(surface);

  return texture;
}

static void go(Direction dir, uint8_t *character_cell_i, Entity *map) {
  SDL_assert(character_cell_i != 0);
  SDL_assert(map != 0);

  uint8_t next_cell_i = get_next_cell_i(dir, *character_cell_i);
  Entity *const next_cell = &map[next_cell_i];
  // MW => No pathing.
  if (bitset_is_exactly(*next_cell, ENTITY_WALL))
    return;

  // MN, MO => Free pathing.
  if (bitset_is_exactly(*next_cell, ENTITY_NONE) ||
      bitset_is_exactly(*next_cell, ENTITY_OBJECTIVE)) {
    bitset_remove(&map[*character_cell_i], ENTITY_CHARACTER);
    bitset_add(next_cell, ENTITY_CHARACTER);

    *character_cell_i = next_cell_i;
    return;
  }

  // MC* from this point on.

  Entity *const next_next_cell = &map[get_next_cell_i(dir, next_cell_i)];

  // MCW, MCC => No pathing.
  if (bitset_is_exactly(*next_next_cell, ENTITY_WALL) ||
      bitset_contains(*next_next_cell, ENTITY_CRATE))
    return;

  // MCN, MCO => Advance the crate.
  if (bitset_is_exactly(*next_next_cell, ENTITY_NONE) ||
      bitset_contains(*next_next_cell, ENTITY_OBJECTIVE)) {
    bitset_remove(&map[*character_cell_i], ENTITY_CHARACTER);
    bitset_remove(next_cell, ENTITY_CRATE);
    bitset_add(next_cell, ENTITY_CHARACTER);
    bitset_add(next_next_cell, ENTITY_CRATE);

    *character_cell_i = next_cell_i;
  }
}

int main(int argc, char *argv[]) {
  pg_unused(argc);
  pg_unused(argv);


  uint8_t crates_count = 0, objectives_count = 0, character_cell_i = 0;
  Entity game_map[MAP_SIZE] = {0};
  __builtin_memcpy(game_map, map, MAP_SIZE);
  load_map(game_map, &crates_count, &objectives_count, &character_cell_i);

  SDL_Window *window =
      SDL_CreateWindow("Sokoban", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
  if (!window)
    exit(1);

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
  SDL_assert(renderer != 0);
  SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
  SDL_Texture *character[4] = {
      [DIR_UP] = load_texture(renderer, character_up_rgb),
      [DIR_DOWN] = load_texture(renderer, character_down_rgb),
      [DIR_LEFT] = load_texture(renderer, character_left_rgb),
      [DIR_RIGHT] = load_texture(renderer, character_right_rgb),
  };
  SDL_Texture *current = character[DIR_UP]; // Start up.

  SDL_Texture *textures[ENTITY_MAX] = {
      [ENTITY_CRATE] = load_texture(renderer, crate_rgb),
      [ENTITY_CRATE_OK] = load_texture(renderer, crate_ok_rgb),
      [ENTITY_OBJECTIVE] = load_texture(renderer, objective_rgb),
      [ENTITY_WALL] = load_texture(renderer, wall_rgb),
  };

  while (true) {
    SDL_Event e;
    SDL_WaitEvent(&e);
    if (e.type == SDL_QUIT) {
      exit(0);
    } else if (e.type == SDL_KEYDOWN) {
      switch (e.key.keysym.sym) {
      case SDLK_ESCAPE:
        exit(0);
        break;

      case SDLK_r:
        __builtin_memcpy(game_map, map, MAP_SIZE);
        load_map(game_map, &crates_count, &objectives_count, &character_cell_i);
        break;

      case SDLK_UP:
        current = character[DIR_UP];
        go(DIR_UP, &character_cell_i, game_map);
        break;

      case SDLK_RIGHT:
        current = character[DIR_RIGHT];
        go(DIR_RIGHT, &character_cell_i, game_map);
        break;

      case SDLK_DOWN:
        current = character[DIR_DOWN];
        go(DIR_DOWN, &character_cell_i, game_map);
        break;

      case SDLK_LEFT:
        current = character[DIR_LEFT];
        go(DIR_LEFT, &character_cell_i, game_map);
        break;
      }
    }
    SDL_RenderClear(renderer);

    uint8_t crates_ok_count = 0;
    for (uint8_t i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
      const Entity cell = game_map[i];

      if (bitset_is_exactly(cell, ENTITY_NONE)) // Nothing to render.
        continue;

      // Since we are iterating on each cell, count the number of crates ok.
      crates_ok_count += bitset_is_exactly(cell, ENTITY_CRATE_OK);

      const SDL_Rect rect = {.w = CELL_SIZE,
                             .h = CELL_SIZE,
                             .x = CELL_SIZE * (i % MAP_HEIGHT),
                             .y = CELL_SIZE * (i / MAP_WIDTH)};

      // Get the right texture. Maybe it could be made branchless with bit
      // operations, e.g. get the highest bit.
      // There is a bit of precedence here: in the case of multiple entities
      // occupying the same cell, we want to draw: character > crate_ok > crate
      // > objective.
      if (bitset_contains(cell, ENTITY_CHARACTER)) {
        SDL_RenderCopy(renderer, current, 0, &rect);
      } else if (bitset_is_exactly(cell, ENTITY_WALL)) {
        SDL_RenderCopy(renderer, textures[ENTITY_WALL], 0, &rect);
      } else if (bitset_is_exactly(cell, ENTITY_CRATE_OK)) {
        SDL_RenderCopy(renderer, textures[ENTITY_CRATE_OK], 0, &rect);
      } else if (bitset_is_exactly(cell, ENTITY_CRATE)) {
        SDL_RenderCopy(renderer, textures[ENTITY_CRATE], 0, &rect);
      } else if (bitset_is_exactly(cell, ENTITY_OBJECTIVE)) {
        SDL_RenderCopy(renderer, textures[ENTITY_OBJECTIVE], 0, &rect);
      }
    }
    SDL_RenderPresent(renderer);

    // The end?
    if (crates_ok_count == objectives_count) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "You won!", "Yeah!",
                               window);
      exit(0);
    }
  }

  return 0;
}
