#include <SDL2/SDL.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "crate.h"
#include "crate_ok.h"
#include "mario_down.h"
#include "mario_left.h"
#include "mario_right.h"
#include "mario_up.h"
#include "objective.h"
#include "wall.h"

#define pg_assert(condition)                                                   \
  do {                                                                         \
    if (!(condition))                                                          \
      __builtin_trap();                                                        \
  } while (0)

typedef enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT } Direction;
typedef enum {
  ENTITY_NONE = 0,
  ENTITY_WALL = 1 << 0,
  ENTITY_OBJECTIVE = 1 << 1,
  ENTITY_CRATE = 1 << 2,
  ENTITY_CRATE_OK = (ENTITY_OBJECTIVE | ENTITY_CRATE),
  ENTITY_MARIO = 1 << 4,
  ENTITY_MAX,
} Entity;

static bool entity_is_at_least(Entity a, Entity b) { return (a & b) == b; }
static bool entity_is_exactly(Entity a, Entity b) { return a == b; }
static Entity entity_minus_other(Entity a, Entity b) { return a & ~b; }

#define MAP_WIDTH 12
#define MAP_HEIGHT 12
#define MAP_SIZE ((MAP_WIDTH) * (MAP_WIDTH))

static const uint8_t map[MAP_WIDTH][MAP_HEIGHT] = {
    {ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL},
    {ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_WALL,
     ENTITY_WALL, ENTITY_WALL, ENTITY_WALL, ENTITY_NONE, ENTITY_MARIO,
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

uint8_t get_next_cell_i(Direction dir, uint8_t cell) {
  switch (dir) {
  case DIR_UP:
    return cell - 12;
  case DIR_RIGHT:
    return cell + 1;
  case DIR_DOWN:
    return cell + 12;
  case DIR_LEFT:
    return cell - 1;
  }
}

void load_map(uint8_t *map, uint8_t *crates_count, uint8_t *objectives_count,
              uint8_t *mario_cell_i, uint8_t *crates_ok_count) {
  pg_assert(map != NULL);
  pg_assert(crates_count != NULL);
  pg_assert(objectives_count != NULL);
  pg_assert(mario_cell_i != NULL);
  pg_assert(crates_ok_count != NULL);
  *crates_ok_count = 0;

  for (uint8_t i = 0; i < MAP_SIZE; i++) {
    const uint8_t cell = map[i];
    pg_assert(cell == ENTITY_NONE || cell == ENTITY_MARIO ||
              cell == ENTITY_WALL || cell == ENTITY_OBJECTIVE ||
              cell == ENTITY_CRATE);

    *crates_count += cell == ENTITY_CRATE;
    *objectives_count += cell == ENTITY_OBJECTIVE;
    if (cell == ENTITY_MARIO)
      *mario_cell_i = i;
  }
}

void go(Direction dir, uint8_t *mario_cell_i, uint8_t *map,
        uint8_t *creates_ok_count) {
  pg_assert(mario_cell_i != NULL);
  pg_assert(map != NULL);
  pg_assert(creates_ok_count != NULL);

  uint8_t next_cell_i = get_next_cell_i(dir, *mario_cell_i);
  uint8_t *next_cell = &map[next_cell_i];
  // MW
  if (entity_is_exactly(*next_cell, ENTITY_WALL))
    return;

  // MN, MO => Free pathing.
  if (entity_is_exactly(*next_cell, ENTITY_NONE) ||
      entity_is_exactly(*next_cell, ENTITY_OBJECTIVE)) {
    map[*mario_cell_i] &= ~ENTITY_MARIO;
    *next_cell |= ENTITY_MARIO;
    *mario_cell_i = next_cell_i;
    return;
  }

  // MC*, MCo* at this point

  uint8_t *next_next_cell = &map[get_next_cell_i(dir, next_cell_i)];

  // MCW, MCC, MCCo, MCoW, MCoC, MCoCo => No pathing.
  if (entity_is_exactly(*next_next_cell, ENTITY_WALL) ||
      entity_is_at_least(*next_next_cell, ENTITY_CRATE))
    return;

  // MCN, MCoN => Advance the crate.
  if (entity_is_exactly(*next_next_cell, ENTITY_NONE)) {
    map[*mario_cell_i] &= ~ENTITY_MARIO;
    *next_cell &= ~ENTITY_CRATE;
    *next_cell |= ENTITY_MARIO;
    *next_next_cell |= ENTITY_CRATE;
    *mario_cell_i = next_cell_i;
    return;
  }

  // MCO, MCoO => Advance the crate and count a point.
  if (entity_is_at_least(*next_next_cell, ENTITY_OBJECTIVE)) {
    map[*mario_cell_i] &= ~ENTITY_MARIO;
    *next_cell &= ~ENTITY_CRATE;
    *next_cell |= ENTITY_MARIO;
    *next_next_cell |= ENTITY_CRATE;
    *mario_cell_i = next_cell_i;
    return;
  }
}

int main() {
  uint8_t crates_count = 0, objectives_count = 0, mario_cell_i = 0,
          crates_ok_count = 0;
  uint8_t game_map[MAP_SIZE] = {0};
  __builtin_memcpy(game_map, map, MAP_SIZE);
  load_map(game_map, &crates_count, &objectives_count, &mario_cell_i,
           &crates_ok_count);

  const uint32_t CELL_SIZE = 34;
  const uint16_t SCREEN_WIDTH = MAP_WIDTH * CELL_SIZE;
  const uint16_t SCREEN_HEIGHT = MAP_HEIGHT * CELL_SIZE;

  SDL_Window *window =
      SDL_CreateWindow("Sokoban", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
  if (!window)
    exit(1);

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    fprintf(stderr, "Error creating renderer\n");
    exit(1);
  }
  SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
  SDL_Texture *mario[4];
  SDL_Surface *surface = NULL;
  surface =
      SDL_CreateRGBSurfaceFrom(mario_up_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  mario[DIR_UP] = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);

  surface =
      SDL_CreateRGBSurfaceFrom(mario_right_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  mario[DIR_RIGHT] = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);

  surface =
      SDL_CreateRGBSurfaceFrom(mario_down_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  mario[DIR_DOWN] = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);

  surface =
      SDL_CreateRGBSurfaceFrom(mario_left_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  mario[DIR_LEFT] = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  SDL_Texture *current = mario[DIR_UP];

  // We only have 6 textures but to make map human-readable and the parsing code
  // simplistic, we use ascii characters as enum values for entities.
  SDL_Texture *textures[ENTITY_MAX] = {0};

  SDL_Surface *crate_surface =
      SDL_CreateRGBSurfaceFrom(crate_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  textures[ENTITY_CRATE] =
      SDL_CreateTextureFromSurface(renderer, crate_surface);
  SDL_FreeSurface(crate_surface);

  SDL_Surface *crate_ok_surface =
      SDL_CreateRGBSurfaceFrom(crate_ok_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);

  textures[ENTITY_CRATE_OK] =
      SDL_CreateTextureFromSurface(renderer, crate_ok_surface);
  SDL_FreeSurface(crate_ok_surface);

  SDL_Surface *wall_surface =
      SDL_CreateRGBSurfaceFrom(wall_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  textures[ENTITY_WALL] = SDL_CreateTextureFromSurface(renderer, wall_surface);
  SDL_FreeSurface(wall_surface);

  SDL_Surface *objective_surface = surface =
      SDL_CreateRGBSurfaceFrom(objective_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  textures[ENTITY_OBJECTIVE] =
      SDL_CreateTextureFromSurface(renderer, objective_surface);
  SDL_FreeSurface(objective_surface);

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
        load_map(game_map, &crates_count, &objectives_count, &mario_cell_i,
                 &crates_ok_count);
        break;

      case SDLK_UP:
        current = mario[DIR_UP];
        go(DIR_UP, &mario_cell_i, game_map, &crates_ok_count);
        break;

      case SDLK_RIGHT:
        current = mario[DIR_RIGHT];
        go(DIR_RIGHT, &mario_cell_i, game_map, &crates_ok_count);
        break;

      case SDLK_DOWN:
        current = mario[DIR_DOWN];
        go(DIR_DOWN, &mario_cell_i, game_map, &crates_ok_count);
        break;

      case SDLK_LEFT:
        current = mario[DIR_LEFT];
        go(DIR_LEFT, &mario_cell_i, game_map, &crates_ok_count);
        break;
      }
    }
    SDL_RenderClear(renderer);

    for (uint8_t i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
      const uint8_t cell = game_map[i];
      if (entity_is_exactly(cell,ENTITY_NONE) || entity_is_at_least(cell, ENTITY_MARIO))
        continue;

      SDL_Rect rect = {.w = CELL_SIZE,
                       .h = CELL_SIZE,
                       .x = CELL_SIZE * (i % MAP_HEIGHT),
                       .y = CELL_SIZE * (i / MAP_WIDTH)};
      if (entity_is_exactly(cell, ENTITY_WALL)) {
        SDL_RenderCopy(renderer, textures[ENTITY_WALL], NULL, &rect);
      } else if (entity_is_exactly(cell, ENTITY_CRATE_OK)) {
        SDL_RenderCopy(renderer, textures[ENTITY_CRATE_OK], NULL, &rect);
      } else if (entity_is_exactly(cell, ENTITY_OBJECTIVE)) {
        SDL_RenderCopy(renderer, textures[ENTITY_OBJECTIVE], NULL, &rect);
      } else if (entity_is_exactly(cell, ENTITY_CRATE)) {
        SDL_RenderCopy(renderer, textures[ENTITY_CRATE], NULL, &rect);
      }
    }
    SDL_Rect mario_rect = {.w = CELL_SIZE,
                           .h = CELL_SIZE,
                           .x = CELL_SIZE * (mario_cell_i % MAP_HEIGHT),
                           .y = CELL_SIZE * (mario_cell_i / MAP_WIDTH)};
    SDL_RenderCopy(renderer, current, NULL, &mario_rect);
    SDL_RenderPresent(renderer);

    if (crates_ok_count == objectives_count) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "You won!", "Yeah!",
                               window);
      exit(0);
    }
  }
}
