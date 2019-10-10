from genetic_algo import GeneticAlgorithm
from training import evaluate_generation, breed_generation, setup_run, get_seeds
from game.core.Vector2 import Vector2
from models.FFNN import FFNN, breed
from tqdm import trange

def main():
    run_seed        = 1
    num_agents      = 100
    num_generations = 10

    log_dir = "./runs/test/"

    agent_class   = FFNN
    agent_breeder = breed

    agent_args = {
        'view_size': Vector2(11, 11),
        'layer_config': [
            ('linear', 32),  ('act', 'sigmoid'),
            ('linear', 32),  ('act', 'sigmoid'),
            ('linear', 32),  ('act', 'sigmoid'),
            ('linear', 32),  ('act', 'sigmoid'),
        ],
    }

    game_args = {
        'num_chunks': 10,
        'seed': 1569986158,
    }

    master_rng, agents, gen_algo, logger = setup_run(run_seed, agent_class, agent_args, num_agents, log_dir)

    for i in trange(num_generations):
        fitnesses, death_types = evaluate_generation(agents, game_args)

        logger.log_generation(agents, fitnesses, death_types, game_args)

        survivors = gen_algo.select_survivors(fitnesses)
        breeding_pairs = gen_algo.select_breeding_pairs(survivors)

        agents = breed_generation(agents, agent_breeder, breeding_pairs, master_rng)

        # Change level every 10 generations
        if i % 10 == 0:
            game_args['seed'] = get_seeds(master_rng)

if __name__ == "__main__":
    main()