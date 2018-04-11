#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <cstdint>

#include <chrono>
#include <random>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

using namespace std;


//---------------------------------------------------------------
// Configuration


#define PAGE_SIZE 4096


//---------------------------------------------------------------
// Helpers


#define die(M,C) { cerr << M << endl << errno << ": " << strerror (errno) << "." << endl; exit (C); }


volatile int blackhole;


typedef struct page_t {
    int data [PAGE_SIZE / sizeof (int)];
} page_t;


void drop_page_cache () {

    // Make sure dirty pages are flushed.
    sync ();

    // Ask for all caches to be dropped.
    ofstream drop_file ("/proc/sys/vm/drop_caches");
    drop_file << 3 << endl;
}


size_t get_free_pages () {

    // Read free page count from kernel.
    ifstream stat_file ("/proc/vmstat");
    string line_string;
    while (getline (stat_file, line_string)) {
        stringstream line_stream (line_string);
        string line_keyword;
        size_t line_value;
        line_stream >> line_keyword >> line_value;
        if (line_keyword.compare ("nr_free_pages") == 0) {
            return (line_value);
        }
    }

    // This should not happen.
    exit (ENOKEY);
}


void update_free_pages (size_t &old_free_pages, size_t owned_pages) {

    size_t new_free_pages = owned_pages + get_free_pages ();
    if (new_free_pages < old_free_pages) {
        cout << "- Adjusting free pages by " << old_free_pages - new_free_pages << " to " << new_free_pages << "." << endl;
        old_free_pages = new_free_pages;
    }
}


//---------------------------------------------------------------
// Mainline


int main (void) {

    // Drop page cache before doing anything else.
    // Page cache does not count as free pages.
    cout << "Syncing and dropping cache pages." << endl;
    drop_page_cache ();
    size_t free_pages = get_free_pages ();
    cout << "See " << free_pages << " free pages after drop and sync." << endl;

    // We will need an array for random page shuffle.
    // The array must cover all free pages.
    // Fill now to consume memory.
    size_t *walk_array = new size_t [free_pages];
    for (size_t index = 0 ; index < free_pages ; index ++) walk_array [index] = index;

    // Reflect memory consumption.
    update_free_pages (free_pages, 0);

    // Allocate available memory in single block.
    cout << "Allocating every free page." << endl;
    void *memory = mmap (NULL, free_pages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) die ("Failure in mmap.", ENOMEM);
    page_t *pages = (page_t *) memory;

    // Write touch the block in sequential order.
    // Adjust to reflect memory consumption.
    cout << "Touching all allocated pages." << endl;
    size_t index_check_limit = free_pages / 2;
    for (size_t index = 0 ; index < free_pages ; index ++) {
        if (index > index_check_limit) {
            update_free_pages (free_pages, index);
            index_check_limit = (index + free_pages) / 2;
        }
        pages [index].data [0] = blackhole;
    }

    // Prepare the random walk for whatever remaining memory there is.
    cout << "Preparing random walk through allocated pages." << endl;
    unsigned int seed = chrono::system_clock::now ().time_since_epoch ().count ();
    shuffle (walk_array, walk_array + free_pages, default_random_engine (seed));

    cout << "Releasing all allocated pages." << endl;
    for (size_t index = 0 ; index < free_pages ; index ++) {
        if (madvise (pages + walk_array [index], PAGE_SIZE, MADV_DONTNEED) != 0) die ("Failure in madvise.", EINVAL);
    }

    cout << "Done." << endl;
    return (0);
}
