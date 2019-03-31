/**
 * @file gamelogic.cpp
 * @author Benjamin Mastripolito, Haydn Jones
 * @brief Functions for interfacing with chromosomes
 * @date 2018-12-06
 */

#include <gamelogic.hpp>
#include <levelgen.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

int tile_at(struct Game *game, int x, int y);
bool tile_solid(struct Game *game, int x, int y);
void game_set_tile(struct Game *game, int x, int y, unsigned char val);
int physics_sim(struct Game *game, struct Body* body, bool jump);
float dist(float x1, float y1, float x2, float y2);

/**
 * @brief Setup a game, full reset
 * 
 * @param game The game to reset
 * @param seed The seed to generate the new level from
 */
void game_setup(struct Game *game, unsigned int seed)
{
	levelgen_clear_level(game);
	levelgen_gen_map(game, seed);
}

/**
 * @brief Initializes / resets player structure
 * 
 * @param player The player to initialize
 */
void player_setup(struct Player *player) {
	player->body.px = SPAWN_X * TILE_SIZE;
	player->body.py = SPAWN_Y * TILE_SIZE;
	player->body.vx = 0;
	player->body.vy = 0;
	player->body.tile_x = player->body.px / TILE_SIZE;
	player->body.tile_y = player->body.py / TILE_SIZE;
	player->body.immune = false;
	player->body.canjump = false;
	player->body.isjump = false;
	player->body.standing = false;
	player->score = 0;
	player->fitness = 0;
	player->time = 0;
	player->buttonpresses = 0;
}

/**
 * @brief Update a single frame of the game, simulating physics
 * 
 * @param game The game structure we are operating on
 * @param player The player playing the level
 * @param input Button controls for the player
 * @return int PLAYER_DEAD or PLAYER_COMPLETE
 */
int game_update(struct Game *game, struct Player *player, uint8_t input[BUTTON_COUNT])
{
	int return_value = 0;

	// Estimate of time
	player->time += 1.0f / (float)UPDATES_PS;
	
	// Time limit
	if (player->time >= MAX_TIME - 1.0f / (float)UPDATES_PS) {
		player->death_type = PLAYER_TIMEOUT;
		return PLAYER_TIMEOUT;
	}
	
	// Left and right button press
	player->body.vx += (V_X - player->body.vx) * input[BUTTON_RIGHT];
	player->body.vx += (-V_X - player->body.vx) * input[BUTTON_LEFT];

	// Button presses
	player->buttonpresses += input[BUTTON_JUMP] + input[BUTTON_LEFT] + input[BUTTON_RIGHT];

	// Physics sim for player
	return_value = physics_sim(game, &player->body, input[BUTTON_JUMP]);
	if (return_value == PLAYER_DEAD) {
		player->death_type = PLAYER_DEAD;
		return PLAYER_DEAD;
	}

	// Lower bound
	if (player->body.py > LEVEL_PIXEL_HEIGHT) {
		player->death_type = PLAYER_DEAD;
		return PLAYER_DEAD;
	}

	// Enemies
	for (int i = 0; i < game->n_enemies; i++) {
		struct Enemy *enemy;
		bool empty_below;
		int ret;

		enemy = &game->enemies[i];
		empty_below = true;
		if (!enemy->dead) {
			//Enemy physics simulation
			enemy->body.vx = enemy->direction;
			ret = physics_sim(game, &(enemy->body), false);
			if (ret == PLAYER_DEAD) {
				enemy->dead = true;
			}

			//Check if empty below
			for (int y = enemy->body.tile_y; y < LEVEL_HEIGHT; y++) {
				if (tile_solid(game, enemy->body.tile_x, y))
					empty_below = false;
			}

			//Determine if we need to change direction
			if (empty_below) {
				enemy->direction = -enemy->direction;
			} else if (ret == COL_RIGHT && enemy->direction > 0) {
				enemy->direction = -enemy->direction;
			} else if (ret == COL_LEFT && enemy->direction < 0) {
				enemy->direction = -enemy->direction;
			}

			//Kill player
			if (dist(player->body.px, player->body.py, enemy->body.px, enemy->body.py) < 32) {
				player->death_type = PLAYER_DEAD;
				return PLAYER_DEAD;
			}
		}
	}
	
	// Fitness
	float fitness;
	fitness = 100 + player->score + player->body.px;
	fitness -= player->time * FIT_TIME_WEIGHT;
	fitness -= player->buttonpresses * FIT_BUTTONS_WEIGHT;
	// Only increase fitness, never decrease.
	player->fitness = fitness > player->fitness ? fitness : player->fitness;

	// Player completed level
	if (player->body.px + PLAYER_RIGHT >= (LEVEL_WIDTH - 4) * TILE_SIZE) {
		player->death_type = PLAYER_COMPLETE;
		return PLAYER_DEAD;
	}

	return return_value;
}

/**
 * @brief Runs physics simulation for a given body
 * 
 * @param game Game the body is in
 * @param body Body to apply physics to
 * @param jump Whether or not the body is attempting to jump
 * @return int If player died or collided with something on the left/right
 */
int physics_sim(struct Game *game, struct Body* body, bool jump)
{
	int return_value = 0;

	// Jumping
	if (jump && body->canjump) {
		body->isjump = true;
		body->canjump = false;
		if (!body->standing)
			body->vy = -V_JUMP;
	}
	if (!jump && body->isjump)
		body->isjump = false;
	if (body->isjump) {
		body->vy -= 1.5f;
		if (body->vy <= -V_JUMP) {
			body->isjump = false;
			body->vy = -V_JUMP;
		}
	}

	// Player physics
	int tile_x = (body->px + body->vx + 16) / TILE_SIZE;
	int tile_y = (body->py + body->vy + 16) / TILE_SIZE;
	int feet_y = (body->py + body->vy + 33) / TILE_SIZE;
	int top_y = (body->py + body->vy - 1) / TILE_SIZE;
	int right_x = (body->px + body->vx + PLAYER_RIGHT + 1) / TILE_SIZE;
	int left_x = (body->px + body->vx + PLAYER_LEFT - 1) / TILE_SIZE;

	body->tile_x = tile_x;
	body->tile_y = tile_y;

	body->vy += GRAVITY;
	body->vx /= INTERTA;

	// Right collision
	if (tile_solid(game, right_x, tile_y) || right_x >= LEVEL_WIDTH) {
		body->vx = 0;
		body->px = (right_x - 1) * TILE_SIZE + PLAYER_MARGIN - 2;
		return_value = COL_RIGHT;
	}

	// Left collision
	if (tile_solid(game, left_x, tile_y) || left_x < 0) {
		body->vx = 0;
		body->px = (left_x + 1) * TILE_SIZE - PLAYER_MARGIN + 2;
		return_value = COL_LEFT;
	}

	int tile_xr = (body->px + PLAYER_RIGHT) / TILE_SIZE;
	int tile_xl = (body->px + PLAYER_LEFT) / TILE_SIZE;

	// Collision on bottom
	body->standing = false;
	if (tile_solid(game, tile_xl, feet_y) > 0 || tile_solid(game, tile_xr, feet_y) > 0) {
		if (body->vy >= 0) {
			body->vy = 0;
			body->canjump = true;
			body->standing = true;
			if (!body->immune && (tile_at(game, tile_xl, feet_y) == SPIKES_TOP || tile_at(game, tile_xr, feet_y) == SPIKES_TOP)) {
				return PLAYER_DEAD;
			}
		}
		body->py = (feet_y - 1) * TILE_SIZE;
	}

	// Collision on top
	if (tile_solid(game, tile_xl, top_y) > 0 || tile_solid(game, tile_xr, top_y) > 0) {
		if (body->vy < 0) {
			body->vy = 0;
			body->isjump = false;
			if (!body->immune && (tile_at(game, tile_xl, top_y) == SPIKES_BOTTOM || tile_at(game, tile_xr, top_y) == SPIKES_BOTTOM)) {
				return PLAYER_DEAD;
			}
		}
		body->py = (top_y + 1) * TILE_SIZE;
	}

	// Apply body->velocity
	body->px = round(body->px + body->vx);
	body->py = round(body->py + body->vy);

	// Update tile position
	body->tile_x = (body->px + 16) / TILE_SIZE;
	body->tile_y = (body->py + 16) / TILE_SIZE;
	
	return return_value;
}

/**
 * @brief Returns the tile value at the given tile position
 * 
 * @param Game the game object to grab the tile value from
 * @param x X tile coordinate
 * @param y Y tile coordinate
 * @return int Tile value
 */
int tile_at(struct Game *game, int x, int y)
{
	if (x < 0 || x >= LEVEL_WIDTH || y < 0 || y >= LEVEL_HEIGHT)
		return 0;
	return game->tiles[y * LEVEL_WIDTH + x];
}

/**
 * @brief Returns whether or not a tile at (x, y) is solid
 * 
 * @param game Game tile is in
 * @param x Tile position x
 * @param y Tile position y
 * @return bool 
 */
bool tile_solid(struct Game *game, int x, int y)
{
	int tile = tile_at(game, x, y);
	switch(tile) {
		case EMPTY:
		case FLAG:
			return false;
		default:
			break;
	}
	return true;
}

/**
 * @brief Set tile to given type at (x, y)
 * 
 * @param Game the game object to operate on
 * @param x X tile coordinate
 * @param y Y tile coordinate
 * @param val The value to set the tile with
 */
void game_set_tile(struct Game *game, int x, int y, unsigned char val)
{
	if (x < 0 || x >= LEVEL_WIDTH || y < 0 || y >= LEVEL_HEIGHT) {
		return;
	}
	game->tiles[y * LEVEL_WIDTH + x] = val;
}

/**
 * @brief Basic distance function
 * 
 * @param x1 Tile position x1
 * @param y1 Tile position y1
 * @param x2 Tile position x2
 * @param y2 Tile position y2
 * @return float distance between the two tiles
 */
float dist(float x1, float y1, float x2, float y2)
{
	return sqrt(pow(x2 - x1, 2.0f) + pow(y2 - y1, 2.0f));
}

/**
 * @brief Exposes the tiles around the player to the neural network
 * 
 * @param game The game object to get tiles from
 * @param player The player object
 * @param tiles The array of tiles to reference
 * @param in_h Height of the chromsome input matrix
 * @param in_w Width of the chromsome input matrix
 */
void get_input_tiles(struct Game *game, struct Player *player, float *tiles, uint8_t in_h, uint8_t in_w)
{
	int tile_x1, tile_y1;
	int tile_x2, tile_y2;
	int x, y;
	float *tmp;
	uint8_t tile;

	tmp = tiles;

	//Calculate bounds for drawing tiles
	tile_x1 = player->body.tile_x - in_w / 2;
	tile_x2 = player->body.tile_x + in_w / 2;
	tile_y1 = player->body.tile_y - in_h / 2;
	tile_y2 = player->body.tile_y + in_h / 2;

	//Loop over tiles and draw them
	for (y = tile_y1; y < tile_y2; y++) {
		for (x = tile_x1; x < tile_x2; x++) {
			// Report walls on left and right side of level
			if (x < 0 || x >= LEVEL_WIDTH)
				tile = BRICKS;
			else if (y < 0 || y >= LEVEL_HEIGHT)
				tile = EMPTY;
			else
				tile = game->tiles[y * LEVEL_WIDTH + x];

			//Converting tile types to something the chromosome can understand
			switch(tile) {
				//Empty
				case EMPTY:
				case FLAG:
					*tmp = 0.0f;
					break;
				
				//Solid tiles
				case PIPE_BOTTOM:
				case PIPE_MIDDLE:
				case PIPE_TOP:
				case GRASS:
				case DIRT:
				case BRICKS:
					*tmp = (1.0f / 3.0f);
					break;
				
				//Hazards
				case SPIKES_TOP:
					*tmp = (2.0f / 3.0f);
					break;
				case SPIKES_BOTTOM:
					*tmp = 1.0f;
					break;
				default:
					break;
			}

			tmp++;
		}
	}
}