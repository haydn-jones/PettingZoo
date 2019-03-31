# Petting Zoo
Implementation of a genetic algorithm learning to play a platformer.

## Game
The game is a simple platformer.

## Algorithms
Evolution, Neural Networks, Stuff.

## Installation and Requirements
SFML must be installed to build, however, SFML is only required to watch a chromosome play.
Here are some sample build instructions:

```
mkdir build
cd build
cmake ..
make 
```

## Running
When built, the following executables will be created:
- pettingzoo: This is the platformer, just run it to play the game. To watch a chromosome play, run `./pettingzoo -f PATH_TO_CHROMOSOME`
- train: Run this to begin training the NN. Run `./trainCPU -h` for help
