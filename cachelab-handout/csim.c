#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/**
 * @brief Global counters for cache performance metrics
 */
int miss_cnt = 0, eviction_cnt = 0, hit_cnt = 0;
char trace_file[1000];
int verbose = 0;

/**
 * @brief Cache line structure
 * Contains validity bit, tag, and time stamp for LRU replacement
 */
typedef struct cache_line {
    int valid;      // Validity bit
    int tag;        // Tag bits
    int time;       // Time stamp for LRU (most recently used = 0)
} Cache_line;

/**
 * @brief Cache structure
 * Contains cache parameters and pointer to cache lines
 */
typedef struct cache_ {
    int B;          // Block size (2^b)
    int E;          // Associativity (lines per set)
    int S;          // Number of sets (2^s)
    Cache_line **line;  // 2D array of cache lines
} Cache;

Cache *cache = NULL;

/**
 * @brief Initialize cache with given parameters
 * @param s: number of set index bits
 * @param E: associativity (lines per set)
 * @param b: number of block offset bits
 */
void init_cache(int s, int E, int b) {
    int S = 1 << s;  // Number of sets = 2^s
    int B = 1 << b;  // Block size = 2^b
    
    cache = (Cache*)malloc(sizeof(Cache));
    cache->S = S;
    cache->E = E;
    cache->B = B;
    
    // Allocate memory for sets
    cache->line = (Cache_line**)malloc(sizeof(Cache_line*) * S);
    
    // Initialize each set
    for (int i = 0; i < S; ++i) {
        cache->line[i] = (Cache_line*)malloc(sizeof(Cache_line) * E);
        for (int j = 0; j < E; ++j) {
            cache->line[i][j].valid = 0;
            cache->line[i][j].tag = -1;
            cache->line[i][j].time = 0;
        }
    }
}

/**
 * @brief Update time stamps and tag for a cache line
 * @param line_idx: index of the line in the set
 * @param set_idx: index of the set
 * @param tag: tag value to set
 */
void update_time_tag(int line_idx, int set_idx, int tag) {
    cache->line[set_idx][line_idx].valid = 1;
    cache->line[set_idx][line_idx].tag = tag;
    
    // Increment time for all valid lines in the set
    for (int i = 0; i < cache->E; ++i) {
        if (cache->line[set_idx][i].valid == 1) {
            cache->line[set_idx][i].time++;
        }
    }
    
    // Set current line as most recently used (time = 0)
    cache->line[set_idx][line_idx].time = 0;
}

/**
 * @brief Find the line with maximum time (LRU) in a set
 * @param set_idx: index of the set
 * @return: index of the LRU line
 */
int find_lru_line(int set_idx) {
    int max_line = 0, max_time = 0;
    for (int i = 0; i < cache->E; ++i) {
        if (cache->line[set_idx][i].time > max_time) {
            max_time = cache->line[set_idx][i].time;
            max_line = i;
        }
    }
    return max_line;
}

/**
 * @brief Find line with given tag in a set
 * @param tag: tag to search for
 * @param set_idx: index of the set
 * @return: index of the line if found, -1 otherwise
 */
int find_line(int tag, int set_idx) {
    for (int i = 0; i < cache->E; ++i) {
        if (cache->line[set_idx][i].valid && cache->line[set_idx][i].tag == tag) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Check if there's an empty line in a set
 * @param set_idx: index of the set
 * @return: index of empty line if found, -1 if set is full
 */
int find_empty_line(int set_idx) {
    for (int i = 0; i < cache->E; ++i) {
        if (cache->line[set_idx][i].valid == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Simulate cache access
 * @param set_idx: set index
 * @param tag: tag value
 */
void access_cache(int set_idx, int tag) {
    int line_idx = find_line(tag, set_idx);
    
    if (line_idx == -1) {  // Cache miss
        miss_cnt++;
        if (verbose == 1) {
            printf("miss ");
        }
        
        int empty_line = find_empty_line(set_idx);
        if (empty_line == -1) {  // Need eviction
            eviction_cnt++;
            if (verbose == 1) {
                printf("eviction ");
            }
            empty_line = find_lru_line(set_idx);
        }
        update_time_tag(empty_line, set_idx, tag);
    } else {  // Cache hit
        hit_cnt++;
        if (verbose == 1) {
            printf("hit ");
        }
        update_time_tag(line_idx, set_idx, tag);
    }
}

/**
 * @brief Parse trace file and simulate cache operations
 * @param s: number of set index bits
 * @param E: associativity
 * @param b: number of block offset bits
 */
void simulate_cache(int s, int E, int b) {
    FILE *file = fopen(trace_file, "r");
    if (file == NULL) {
        printf("Error: Cannot open trace file %s\n", trace_file);
        exit(-1);
    }
    
    char operation;
    unsigned address;
    int size;
    
    while (fscanf(file, " %c %x,%d", &operation, &address, &size) > 0) {
        // Extract tag and set index from address
        int tag = address >> (s + b);
        int set_idx = (address >> b) & (((unsigned)(-1)) >> (8 * sizeof(unsigned) - s));
        
        if (verbose == 1) {
            printf("%c %x,%d ", operation, address, size);
        }
        
        switch (operation) {
            case 'M':  // Modify: load + store
                access_cache(set_idx, tag);
                access_cache(set_idx, tag);
                break;
            case 'L':  // Load
                access_cache(set_idx, tag);
                break;
            case 'S':  // Store
                access_cache(set_idx, tag);
                break;
        }
        
        if (verbose == 1) {
            printf("\n");
        }
    }
    
    fclose(file);
}

/**
 * @brief Print usage information
 */
void print_help() {
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

/**
 * @brief Free allocated cache memory
 */
void free_cache() {
    int s = cache->S;
    for (int i = 0; i < s; ++i) {
        free(cache->line[i]);
    }
    free(cache->line);
    free(cache);
}

/**
 * @brief Main function - parse arguments and run cache simulation
 */
int main(int argc, char *argv[]) {
    char opt;
    int s = -1, E = -1, b = -1;
    int trace_file_set = 0;
    
    // Parse command line arguments
    while (-1 != (opt = getopt(argc, argv, "hvs:E:b:t:"))) {
        switch (opt) {
            case 'h':
                print_help();
                exit(0);
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                strcpy(trace_file, optarg);
                trace_file_set = 1;
                break;
            default:
                print_help();
                exit(-1);
        }
    }
    
    // Check if all required arguments are provided
    if (s == -1 || E == -1 || b == -1 || !trace_file_set) {
        printf("./csim: Missing required command line argument\n");
        print_help();
        exit(-1);
    }
    
    // Initialize cache and run simulation
    init_cache(s, E, b);
    simulate_cache(s, E, b);
    free_cache();
    
    // Print summary results
    printSummary(hit_cnt, miss_cnt, eviction_cnt);
    
    return 0;
}
