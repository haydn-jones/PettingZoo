import numpy as np
import game.core.defs as pz
from math import floor
from game.core.levelgen import LevelGenerator

# Player physics parameters
V_X     = 6
V_JUMP  = 8.5
INTERTA = 1.4
GRAVITY = 0.3

class Game():
    def __init__(self, num_chunks, seed, view_size=None):
        # Level data
        self.tiles = None
        self.num_chunks = num_chunks
        self.map_seed = seed
        self.width  = None
        self.height = None
        self.level_generator = LevelGenerator()

        self.padded_tiles = None
        self.solid_tiles = None

        # Player data
        self.player = Player()
        self.game_over      = False
        self.game_over_type = None

        self.view_r, self.view_c = view_size if view_size != None else (None, None)

        self.setup_game()

    def setup_game(self):
        self.tiles, spawn_point = self.level_generator.generate_level(self.num_chunks, self.map_seed)
        self.height, self.width = self.tiles.shape

        self.solid_tiles = np.isin(self.tiles, pz.SOLID_TILES)

        if self.view_r != None and self.view_c != None:
            self.padded_tiles = pad_tiles(self.tiles, self.view_r, self.view_c)

        self.player.tile = np.array([1, spawn_point])
        self.player.pos  = self.player.tile * pz.TILE_SIZE

    def update(self, keys):
        if self.game_over:
            print("STOP CALLING UPDATE! THE GAME IS OVER DUMMY")
            return

        # Estimate of time
        self.player.time += 1.0 / pz.UPDATES_PS

        # Time limit
        if self.player.time > pz.MAX_TIME:
            self.game_over_type = pz.PLAYER_TIMEOUT
            self.game_over = True
            return 

        # Left and right button press
        if keys[pz.RIGHT]:
            self.player.vel[0] = V_X
        if keys[pz.LEFT]:
            self.player.vel[0] = -V_X

        # Button presses
        #self.player.presses += sum(keys)

        # Physics sim for player
        ret = self.physicsSim(self.player, keys[pz.JUMP])
        if ret == pz.PLAYER_DEAD:
            self.game_over_type = pz.PLAYER_DEAD
            self.game_over = True

            return

        # Lower bound
        if self.player.pos[1] >= self.height * pz.TILE_SIZE:
            self.game_over_type = pz.PLAYER_DEAD
            self.game_over = True

            return

        # Player completed level
        if ret == pz.PLAYER_COMPLETE:
            # Reward for finishing
            self.player.fitness += 2000
            self.game_over_type = pz.PLAYER_COMPLETE
            self.game_over = True

            return

        # Fitness
        self.player.fitness += self.player.vel[0]

    def physicsSim(self, body, jump):
        # Jumping
        if jump and body.can_jump:
            body.can_jump = False
            body.is_jump  = True
            if not body.standing:
                body.vel[1] = -V_JUMP

        if not jump and body.is_jump:
            body.is_jump = False

        if body.is_jump:
            body.vel[1] -= 1.5
            if body.vel[1] <= -V_JUMP:
                body.is_jump = False
                body.vel[1] = -V_JUMP

        # Player physics
        body.vel[1] = body.vel[1] + GRAVITY
        body.vel[0] = body.vel[0] / INTERTA

        body.tile = np.floor((body.pos + body.vel) / pz.TILE_SIZE)
        feet_tile  = int((body.pos[1] + body.vel[1] + body.half[1] + 1) // pz.TILE_SIZE)
        head_tile  = int((body.pos[1] + body.vel[1] - body.half[1] - 1) // pz.TILE_SIZE)
        right_tile = int((body.pos[0] + body.vel[0] + body.half[0] + 1) // pz.TILE_SIZE)
        left_tile  = int((body.pos[0] + body.vel[0] - body.half[0] - 1) // pz.TILE_SIZE)

        tile_yu = int((body.pos[1] - body.half[1]) / pz.TILE_SIZE)
        tile_yd = int((body.pos[1] + body.half[1]) / pz.TILE_SIZE)

        # Right collision
        if self.tile_solid(tile_yd, right_tile) or self.tile_solid(tile_yu, right_tile):
            body.vel[0] = 0
            body.pos[0] = right_tile * pz.TILE_SIZE - body.half[0] - 1

        # Left collision
        if self.tile_solid(tile_yd, left_tile) or self.tile_solid(tile_yu, left_tile):
            body.vel[0] = 0
            body.pos[0] = (left_tile + 1) * pz.TILE_SIZE + body.half[0]

        tile_xr = int((body.pos[0] + body.half[0]) / pz.TILE_SIZE)
        tile_xl = int((body.pos[0] - body.half[0]) / pz.TILE_SIZE)

        # Collision on bottom
        body.standing = False
        if self.tile_solid(feet_tile, tile_xl) or self.tile_solid(feet_tile, tile_xr):
            body.vel[1] = 0
            body.can_jump = True
            body.standing = True

            if pz.SPIKE_TOP in [self.tiles[feet_tile, tile_xl], self.tiles[feet_tile, tile_xr]]:
                return pz.PLAYER_DEAD

            if pz.FINISH_TOP in [self.tiles[feet_tile, tile_xl]]:
                return pz.PLAYER_COMPLETE

            body.pos[1] = feet_tile * pz.TILE_SIZE - body.half[1]

        # Collision on top
        if self.tile_solid(head_tile, tile_xl) or self.tile_solid(head_tile, tile_xr):
            if body.vel[1] < 0:
                body.vel[1] = 0
                body.is_jump = False

                if pz.SPIKE_BOT in [self.tiles[head_tile, tile_xl], self.tiles[head_tile, tile_xr]]:
                    return pz.PLAYER_DEAD

            body.pos[1] = (head_tile + 1) * pz.TILE_SIZE + body.half[1]

        # Apply body.velocity
        body.pos = np.round(body.pos + body.vel)

        # Update tile position
        body.tile = np.floor(body.pos / pz.TILE_SIZE)

    def tile_solid(self, row, col):
        if col >= self.width or col < 0:
            return True
        if row >= self.height:
            return False

        return self.solid_tiles[int(row), int(col)]
    
    def get_player_view(self):
        """ Returns a numpy matrix of size (in_h, in_w) of tiles around the player\n
            Player is considered to be in the 'center'
        """

        lbound = self.player.tile[0]
        rbound = self.player.tile[0] + 2 * (self.view_r // 2) + 1
        ubound = self.player.tile[1]
        bbound = self.player.tile[1] + 2 * (self.view_c // 2) + 1

        view = self.padded_tiles[ubound:bbound, lbound:rbound]

        return view.copy()

class Body():
    def __init__(self):
        self.vel  = np.array([0.0, 0.0], dtype=np.float)
        self.tile = np.array([0, 0])
        self.pos  = np.array([0, 0])
        self.size = np.array([22, 23])
        self.half = self.size / 2

        self.can_jump = True
        self.is_jump  = False
        self.standing = True

class Player(Body):
    def __init__(self):
        super().__init__()

        self.time    = 0
        self.fitness = 0
        self.presses = 0

def pad_tiles(arr, view_r, view_c):
    tile_r, tile_c = arr.shape

    p_c = view_c // 2
    p_r = view_r // 2

    pad_shape = (tile_r + 2 * p_r, tile_c + 2 * p_c)
    pad = np.zeros(shape=pad_shape, dtype=np.uint8)

    pad[:, :p_c]  = pz.COBBLE # Left
    pad[:, -p_c:] = pz.COBBLE # Right
    pad[:p_c, :]  = pz.SPIKE_BOT # Top
    pad[-p_c:, :] = pz.SPIKE_TOP # Bottom

    pad[p_r: p_r + tile_r, p_c: p_c + tile_c] = arr
    
    return pad