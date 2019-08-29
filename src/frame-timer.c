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
#define LINE_SIZE 64
#define TIME_LIST 64


//---------------------------------------------------------------
// Helpers


#define die(M,C) { cerr << M << endl << errno << ": " << strerror (errno) << "." << endl; exit (C); }


volatile uint32_t blackhole;


typedef struct page_t {
    uint32_t dummy;
    uint8_t padding_one [LINE_SIZE - sizeof (uint32_t)];
    uint32_t times [TIME_LIST];
    uint8_t padding_two [PAGE_SIZE - LINE_SIZE - TIME_LIST * sizeof (uint32_t)];
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


uint32_t native_direct_write (uint32_t &field, uint32_t value) {

    uint32_t time;
    __asm__ __volatile__ (
        "mfence \n\t"
        "rdtscp \n\t"
        "mov %%eax,%%ebx \n\t"
        "movnti %[value],(%[address]) \n\t"
        "mfence \n\t"
        "rdtscp \n\t"
        "sub %%ebx,%%eax \n\t"
        : "=a" (time)
        : [value] "r" (value), [address] "r" (&field)
        : "memory", "cc", "rbx", "rcx", "rdx"
    );
    return (time);
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

    // Allocate available memory in single block.
    cout << "Allocating every free page." << endl;
    void *memory = mmap (NULL, free_pages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) die ("Failure in mmap.", ENOMEM);
    page_t *pages = reinterpret_cast <page_t *> (memory);

    // Write touch the block in sequential order.
    // Adjust to reflect memory consumption.
    cout << "Touching all allocated pages." << endl;
    size_t index_check_limit = free_pages / 2;
    for (size_t index = 0 ; index < free_pages ; index ++) {
        if (index > index_check_limit) {
            update_free_pages (free_pages, index);
            index_check_limit = (index + free_pages) / 2;
        }
        pages [index].dummy = blackhole;
    }

    // Fill the time list.
    cout << "Timing all allocated pages." << endl;
    for (size_t stamp = 0 ; stamp < TIME_LIST ; stamp ++) {
        for (size_t index = 0 ; index < free_pages ; index ++) {
            pages [index].times [stamp] = native_direct_write (pages [index].dummy, blackhole);
        }
    }

    // Print the times with the addresses.
    // Free memory as we go to avoid paging.
    cout << "Printing all collected times." << endl;
    for (size_t index = 0 ; index < free_pages ; index ++) {
        page_t &page = pages [index];
        cerr << hex << &page << dec;
        for (size_t stamp = 0 ; stamp < TIME_LIST ; stamp ++) {
            cerr << "," << page.times [stamp];
        }
        cerr << endl;
        if (madvise (&pages[index], PAGE_SIZE, MADV_DONTNEED) != 0) die ("Failure in madvise.", EINVAL);
    }

    cout << "Done." << endl;
    return (0);
}
