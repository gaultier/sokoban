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
  NONE = ' ',
  WALL = '#',
  CRATE = '$',
  CRATE_OK = '*',
  OBJECTIVE = '.',
  MARIO = '@'
} Entity;

#define MAP_WIDTH 12
#define MAP_HEIGHT 12
#define MAP_SIZE ((MAP_WIDTH) * (MAP_WIDTH))

static uint8_t map[MAP_SIZE] = "############"
                               "#        @##"
                               "#  $ $ $ $##"
                               "# # .  .  ##"
                               "#      #.  #"
                               "# $##  .   #"
                               "#        ###"
                               "##      ####"
                               "##      .###"
                               "##      ####"
                               "##      ####"
                               "############";

uint8_t get_next_cell_i(Direction dir, uint8_t cell);
void go(Direction dir, uint8_t *cell, uint8_t *map);
uint8_t count(Entity entity, uint8_t *map);

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

uint8_t count(Entity entity, uint8_t *map) {
  uint8_t c = 0;

  for (uint8_t i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
    if (map[i] == entity)
      c++;
  };
  return c;
}

void load_map(uint8_t *crates_count, uint8_t *objectives_count,
              uint8_t *mario_cell) {
  pg_assert(map != NULL);
  pg_assert(crates_count != NULL);
  pg_assert(objectives_count != NULL);
  pg_assert(mario_cell != NULL);

  for (uint8_t i = 0; i < MAP_SIZE; i++) {
    const uint8_t cell = map[i];
    pg_assert(cell == NONE || cell == MARIO || cell == WALL ||
              cell == OBJECTIVE || cell == CRATE);

    *crates_count += cell == CRATE;
    *objectives_count += cell == OBJECTIVE;
    if (cell == MARIO)
      *mario_cell = i;
  }
}

void go(Direction dir, uint8_t *mario_cell, uint8_t *map) {
  uint8_t next_cell_i = get_next_cell_i(dir, *mario_cell);
  uint8_t *next_cell = &map[next_cell_i];
  // MW
  if (*next_cell == WALL)
    return;
  // MN, MO
  if (*next_cell == NONE || *next_cell == OBJECTIVE) {
    *mario_cell = next_cell_i;
    return;
  }

  // MC*, MCo* at this point

  uint8_t *next_next_cell = &map[get_next_cell_i(dir, next_cell_i)];

  // MCW, MCC, MCC0, MCoW, MCoC, MCoCo
  if (*next_next_cell == WALL || *next_next_cell == CRATE ||
      *next_next_cell == CRATE_OK)
    return;

  // MCN, MCoN
  if (*next_next_cell == NONE) {
    *next_next_cell = CRATE;
    *next_cell = *next_cell == CRATE_OK ? OBJECTIVE : NONE;
    *mario_cell = next_cell_i;
    return;
  }

  // MCO, MCoO
  if (*next_next_cell == OBJECTIVE) {
    *next_next_cell = CRATE_OK;
    *next_cell = *next_cell == CRATE_OK ? OBJECTIVE : NONE;
    *mario_cell = next_cell_i;
    return;
  }
}

int main() {
  uint8_t crates_count = 0, objectives_count = 0, mario_cell = 0;
  load_map(&crates_count, &objectives_count, &mario_cell);

  const uint32_t CELL_SIZE = 34;
  const uint16_t SCREEN_WIDTH = (MAP_WIDTH - 1) * CELL_SIZE;
  const uint16_t SCREEN_HEIGHT = (MAP_HEIGHT - 1) * CELL_SIZE;

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
  SDL_Texture *textures[255] = {NULL};

  SDL_Surface *crate_surface =
      SDL_CreateRGBSurfaceFrom(crate_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  textures[CRATE] = SDL_CreateTextureFromSurface(renderer, crate_surface);
  SDL_FreeSurface(crate_surface);

  SDL_Surface *crate_ok_surface =
      SDL_CreateRGBSurfaceFrom(crate_ok_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);

  textures[CRATE_OK] = SDL_CreateTextureFromSurface(renderer, crate_ok_surface);
  SDL_FreeSurface(crate_ok_surface);

  SDL_Surface *wall_surface =
      SDL_CreateRGBSurfaceFrom(wall_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  textures[WALL] = SDL_CreateTextureFromSurface(renderer, wall_surface);
  SDL_FreeSurface(wall_surface);

  SDL_Surface *objective_surface = surface =
      SDL_CreateRGBSurfaceFrom(objective_rgb, CELL_SIZE, CELL_SIZE, 24,
                               CELL_SIZE * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);
  textures[OBJECTIVE] =
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
        //      case SDLK_r:
        //        load_level(map_io, map, level, &mario_cell, &crates_count,
        //                   &objectives_count, map_backup, &mario_cell_backup);
        //        break;
        //      case SDLK_F9: {
        //        map_file = open(argv[1], O_RDONLY);
        //        if (!map_file)
        //          exit(1);
        //        read_res = read(map_file, &level_count, 1);
        //        if (read_res == 0)
        //          exit(1);
        //        read_res = read(map_file, map_io, 3 * 12 * 12);
        //        if (read_res == 0)
        //          exit(1);
        //        close(map_file);
        //        load_level(map_io, map, level, &mario_cell, &crates_count,
        //                   &objectives_count, map_backup, &mario_cell_backup);
        //
        //        break;
        //      }
      case SDLK_UP:
        current = mario[DIR_UP];
        go(DIR_UP, &mario_cell, map);
        break;
      case SDLK_RIGHT:
        current = mario[DIR_RIGHT];
        go(DIR_RIGHT, &mario_cell, map);
        break;

      case SDLK_DOWN:
        current = mario[DIR_DOWN];
        go(DIR_DOWN, &mario_cell, map);
        break;

      case SDLK_LEFT:
        current = mario[DIR_LEFT];
        go(DIR_LEFT, &mario_cell, map);
        break;
      }
    }
    SDL_RenderClear(renderer);

    for (uint8_t i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
      if (map[i] != NONE && map[i] != MARIO) {
        SDL_Rect rect = {.w = CELL_SIZE,
                         .h = CELL_SIZE,
                         .x = CELL_SIZE * (i % MAP_HEIGHT),
                         .y = CELL_SIZE * (i / MAP_WIDTH)};
        SDL_RenderCopy(renderer, textures[map[i]], NULL, &rect);
      }
    }
    SDL_Rect mario_rect = {.w = CELL_SIZE,
                           .h = CELL_SIZE,
                           .x = CELL_SIZE * (mario_cell % MAP_HEIGHT),
                           .y = CELL_SIZE * (mario_cell / MAP_WIDTH)};
    SDL_RenderCopy(renderer, current, NULL, &mario_rect);
    SDL_RenderPresent(renderer);

    uint8_t crates_ok_count = count(CRATE_OK, map);
    if (crates_ok_count == objectives_count) {
      SDL_MessageBoxData message_box = {.flags = SDL_MESSAGEBOX_INFORMATION,
                                        .window = window,
                                        .title = "You won!",
                                        .message = "Yeah!",
                                        .numbuttons = 0,
                                        .buttons = NULL,
                                        .colorScheme = NULL};
      int button_id = 0;
      SDL_ShowMessageBox(&message_box, &button_id);
      exit(0);
    }
  }
}
