# Frame Allocation Randomizer

Performance of certain application classes (in particular memory bound computation kernels)
may depend on the addresses of physical frames used to back the application memory structures.
Automated performance evaluation experiments may yield very regular frame allocation behavior,
which can lead to biased measurement results. The Frame Allocation Randomizer attempts to
disrupt the frame allocation behavior to prevent such bias.

## Usage

Simply execute the `frame-randomizer` binary with root privileges (needed to drop page cache content).

## Notes

The Frame Allocation Randomizer works by allocating all available physical memory in one chunk
and then releasing individual frames in random order. This temporarily puts the system in
low memory situation. To prevent errors, other system activities should be suspended.
