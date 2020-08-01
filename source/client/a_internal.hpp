#pragma once

#include "ai.hpp"

#include <stdint.h>
#include <common/containers.hpp>

void a_neat_module_init();

struct gene_t {
    uint32_t x;
    // y variable just used for display
    float y;

    uint32_t connection_count;
    uint32_t *connection_list;

    float current_value;
};

typedef uint32_t gene_id_t;

struct gene_connection_t {
    uint32_t from, to;
    uint32_t innovation_number;
    uint32_t enabled;
    float weight;
};

using connection_finder_t = hash_table_t<uint32_t, 10000, 40, 40>;

struct gene_connection_tracker_t {
    connection_finder_t *finder;

    uint32_t connection_count, max_connection_count;
    gene_connection_t *connections;

    void init(
        uint32_t max_connection_count);

    void free_tracker();

    // This function JUST adds a connection
    uint32_t add_connection(
        gene_id_t from,
        gene_id_t to);

    // This is if the connection index is known
    uint32_t add_connection(
        gene_id_t from,
        gene_id_t to,
        uint32_t connection_index);

    // Returns the index of the removed connection
    uint32_t remove_connection(
        gene_id_t from,
        gene_id_t to);

    // If we know that it exists
    gene_connection_t *get(
        uint32_t index);

    // If we don't know that it exists
    gene_connection_t *fetch_gene_connection(
        gene_id_t from,
        gene_id_t to);

    void sort_by_innovation();
};

// Structure which holds information on ALL the genes / gene connections
// that exist at the moment
struct neat_t {
    uint32_t input_count, output_count;

    uint32_t gene_count, max_gene_count;
    gene_t *genes;

    gene_connection_tracker_t connections;
};

struct genome_t {
    // Holds the index into the neat's genes array
    uint32_t gene_count, max_gene_count;
    gene_id_t *genes;

    gene_connection_tracker_t connections;
};

void a_print_genome(
    neat_t *neat,
    genome_t *genome);

// Has to match the number of inputs/outputs specified when creating the NEAT
void a_run_genome(
    neat_t *neat,
    genome_t *genome,
    float *inputs,
    float *outputs);

// Need the connections to be sorted by innovation number
float a_genome_distance(
    genome_t *a,
    genome_t *b);

struct neat_entity_t {
    uint32_t id;

    float score;
    genome_t genome;

    struct species_t *species;

    bool dont_kill_me;
};

struct species_t {
    uint32_t id;

    // That are of the same species
    neat_entity_t **entities;
    neat_entity_t **survivors;
    uint32_t entity_count, survivor_count;


    float average_score;
};

struct neat_universe_t {
    neat_t neat;

    uint32_t entity_count;
    neat_entity_t *entities;

    uint32_t species_count;
    species_t *species;

    uint32_t to_free_count;
    genome_t *to_free;
};

void a_universe_init(
    neat_universe_t *universe,
    uint32_t entity_count,
    uint32_t input_count,
    uint32_t output_count);

// After having calculated the scores, do some shit and evolve
void a_end_evaluation_and_evolve(
    neat_universe_t *universe);
