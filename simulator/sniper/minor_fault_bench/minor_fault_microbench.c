

// Write a microbenchmark that triggers minor page faults to test the MimicOS memory allocator.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>
#include <time.h>   


#define PAGE_SIZE 4096
#define NUM_PAGES 10000  // Number of pages to allocate and access
#define ACCESS_PATTERN "sequential" // Change to "random" for random access pattern
#define NUM_ACCESSES 20000 // Number of page accesses to perform

void access_memory(char *mem, int num_pages, const char *pattern, int num_accesses) {
    if (strcmp(pattern, "sequential") == 0) {
        for (int i = 0; i < num_accesses; i++) {
            int page_index = i % num_pages;
            mem[page_index * PAGE_SIZE] = 1; // Access the first byte of the page
        }
    } else if (strcmp(pattern, "random") == 0) {
        for (int i = 0; i < num_accesses; i++) {
            int page_index = rand() % num_pages;
            mem[page_index * PAGE_SIZE] = 1; // Access the first byte of the page
        }
    } else {
        fprintf(stderr, "Unknown access pattern: %s\n", pattern);
        exit(EXIT_FAILURE);
    }
}

int main() {
    // Allocate memory with mmap
    char *mem = (char *)mmap(NULL, NUM_PAGES * PAGE_SIZE, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    access_memory(mem, NUM_PAGES, ACCESS_PATTERN, NUM_ACCESSES);

    printf("Memory accesses completed.\n");

    // Clean up
    if (munmap(mem, NUM_PAGES * PAGE_SIZE) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    return 0;
}