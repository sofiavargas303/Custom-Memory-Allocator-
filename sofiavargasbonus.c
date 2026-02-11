#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned char BYTE;

struct chunkinfo {
    int size; // size of the complete chunk including the chunkinfo
    int inuse; // is it free? 0 or occupied? 1
    struct chunkinfo *next;
    struct chunkinfo *prev;
    struct chunkinfo *free_next; // pointer for the free list
};

typedef struct chunkinfo chunkhead;

void *startofheap = NULL;
void *last_chunk = NULL;
chunkhead *free_list = NULL;
const int PAGESIZE = 4096;

//This function is given by teacher
void analyze() {
    printf("\n--------------------------------------------------------------\n");
    if (!startofheap) {
        printf("no heap\n");
        return;
    }
    chunkhead* ch = (chunkhead*)startofheap;
    for (int no = 0; ch; ch = (chunkhead*)ch->next, no++) {
        printf("%d | current addr: %p |", no, (void*)ch);
        printf("size: %d | ", ch->size);
        printf("inuse: %d | ", ch->inuse);
        printf("next: %p | ", (void*)ch->next);
        printf("prev: %p", (void*)ch->prev);
        printf(" \n");
    }
    printf("program break on address: %p\n", sbrk(0));
}

BYTE* mymalloc(int size) {
    if (size <= 0) return NULL;

    chunkhead* best_fit = NULL;
    chunkhead* best_fit_prev = NULL;
    chunkhead* prev = NULL;

    // Find the best fit chunk in the free list
    for (chunkhead* chead = free_list; chead; prev = chead, chead = chead->free_next) {
        if (chead->size >= size + sizeof(chunkhead)) {
            if (!best_fit || chead->size < best_fit->size) {
                best_fit = chead;
                best_fit_prev = prev;
            }
        }
    }

    if (best_fit) {
        // Remove the best fit chunk from the free list
        if (best_fit_prev) {
            best_fit_prev->free_next = best_fit->free_next;
        } else {
            free_list = best_fit->free_next;
        }
        best_fit->free_next = NULL;

        // If the chunk is so big that we have at least enough space for another chunkhead, then split it
        if (best_fit->size >= size + 2 * sizeof(chunkhead)) {
            BYTE* best_fit_addr = (BYTE*) best_fit;
            chunkhead* new_chunk = (chunkhead*)(best_fit_addr + sizeof(chunkhead) + size);
            new_chunk->size = best_fit->size - size - sizeof(chunkhead);
            new_chunk->inuse = 0;
            new_chunk->next = best_fit->next;
            new_chunk->prev = best_fit;
            new_chunk->free_next = NULL;
            if (best_fit->next) {
                best_fit->next->prev = new_chunk;
            } else {
                last_chunk = new_chunk;
            }
            best_fit->next = new_chunk;
            best_fit->size = size + sizeof(chunkhead);

            // Add the split new chunk to the free list
            new_chunk->free_next = free_list;
            free_list = new_chunk;
        }
        best_fit->inuse = 1;
        return (BYTE*)(best_fit + 1);
    }

    // No chunk big enough, create a new one
    int total_size = size + sizeof(chunkhead);
    if (total_size < PAGESIZE) {
        total_size = PAGESIZE;
    }

    // Increment the heap and use its address to create a chunk struct
    chunkhead* new_chunk = (chunkhead*)sbrk(total_size);
    if (new_chunk == (void*)-1) {
        return NULL;
    }

    // Initialize the properties of the new chunk
    new_chunk->size = total_size;
    new_chunk->inuse = 1;
    new_chunk->next = NULL;
    new_chunk->prev = (struct chunkinfo*)last_chunk;
    new_chunk->free_next = NULL;

    // Update the last chunk
    if (last_chunk) {
        ((struct chunkinfo*)last_chunk)->next = new_chunk;
    } else {
        startofheap = new_chunk;
    }
    last_chunk = new_chunk;

    // Return the address pointer to the memory after the chunk
    return (BYTE*)(new_chunk + 1);
}

void myfree(BYTE* addr) {
    if (!addr) return;

    chunkhead* chead = (chunkhead*)(addr - sizeof(chunkhead));
    chead->inuse = 0;

    // Merge with next chunk if it's free
    if (chead->next && !chead->next->inuse) {
        chunkhead* next_chunk = chead->next;

        // Remove the next chunk from the free list
        if (free_list == next_chunk) {
            free_list = next_chunk->free_next;
        } else {
            chunkhead *prev = free_list;
            while (prev && prev->free_next != next_chunk) {
                prev = prev->free_next;
            }
            if (prev) {
                prev->free_next = next_chunk->free_next;
            }
        }
        next_chunk->free_next = NULL;

        chead->size += next_chunk->size;
        chead->next = next_chunk->next;
        if (chead->next) {
            chead->next->prev = chead;
        } else {
            last_chunk = chead;
        }
    }

    // Merge with previous chunk if it's free
    if (chead->prev && !chead->prev->inuse) {
        chunkhead* prev_chunk = chead->prev;

        // Remove the previous chunk from the free list
        if (free_list == prev_chunk) {
            free_list = prev_chunk->free_next;
        } else {
            chunkhead *prev = free_list;
            while (prev && prev->free_next != prev_chunk) {
                prev = prev->free_next;
            }
            if (prev) {
                prev->free_next = prev_chunk->free_next;
            }
        }
        prev_chunk->free_next = NULL;

        prev_chunk->size += chead->size;
        prev_chunk->next = chead->next;
        if (chead->next) {
            chead->next->prev = prev_chunk;
        } else {
            last_chunk = prev_chunk;
        }
        chead = prev_chunk;
    }

    // Add the chunk to the free list
    chead->free_next = free_list;
    free_list = chead;

    // Move the program break back if this is the last chunk
    if (!chead->next) {
        if (chead->prev) {
            chead->prev->next = NULL;
        } else {
            startofheap = NULL;
            free_list = NULL;
            last_chunk = NULL;
        }
        sbrk(-chead->size);
    }
}

int main(void) {
    int possibe_sizes[] = { 1000, 6000, 20000, 30000 };
    srand(time(0));
    BYTE* a[100];
    clock_t ca, cb;
    ca = clock();
    for (int i = 0; i < 100; i++) {
        a[i] = mymalloc(possibe_sizes[rand() % 4]);
    }
    int count = 0;
    for (int i = 0; i < 99; i++) {
        if (rand() % 2 == 0) {
            myfree(a[i]);
            a[i] = 0;
            count++;
        }
        if (count >= 50)
            break;
    }
    for (int i = 0; i < 100; i++) {
        if (a[i] == 0) {
            a[i] = mymalloc(possibe_sizes[rand() % 4]);
        }
    }
    for (int i = 0; i < 100; i++) {
        myfree(a[i]);
    }
    cb = clock();
    analyze();
    printf("duration: %f\n", (double)(cb - ca) / CLOCKS_PER_SEC);
    return 0;
}
