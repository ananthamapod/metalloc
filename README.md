# metalloc
Custom malloc function implementation for Rutgers Computer Science Class, Systems Programming 198:214

This version of malloc also integrates several features of valgrind, such as detecting leaks. The name is a play on metallic. Each of the malloc family of functions (malloc, realloc, free, etc) are implemented with names related to metals.

Our implementation of malloc (metalloc) begins with a large global char[20480]. This is because each char* is 1 byte big, so it means that we have a space of 20480 bytes to work with. We split this memory block into 2 parts, one 5120 byte chunk for small allocations (of size 512 bytes and lower), and the remainder for larger allocations (above 512 bytes). The memory block is split to prevent fragmentation.

Each allocation has a header object defined by the MemEntry struct, which contains a links to the previous and succeeding header, a 1 bit flag signalling if the space is free, and the size of the allocation.

When allocating for the first time with our malloc function (metalloc()), we initialize the large memory block with two root headers, one for small allocations and one for big allocations. On all metalloc() calls, we check the size of allocation desired, if it is small enough, we begin at the small root, otherwise we begin at the big root. We then traverse through succeeding headers until a free space space of suitable size is found. If a suitable free space is found, we prepare the succeeding header, modify the current header, and return an address to the space. If there's just enough space for the allocation and 1 or fewer headers, we simply modify the current header and return an address to the space. If no workable space is found, we print an error and return a NULL pointer.

When freeing a pointer with the free (freealloc()) function, we iterate through the linke lists of headers until we find the appropriate header. If no such header is found, we print an error statement and return. If the appropriate header is found, we see if it has a preceding, free header. If there is, we merge the current header and allocated space to the preceding header's space, and point the preceding header's successor to the current header's successor, and the succeeding header's predecessor to the current head's predecessor. We then check to see if there is a free succeeding header. If there is, we merge it with the current or preceding header's space, and adjust pointers accordingly. Then, we return.

When calling our calloc function (metallurgilloc()), we use the metalloc() function to allocate space, and use memset() to 0 out the bits. We then return a pointer to the space.

When calling our realloc function (alchemalloc()), we free the passed in pointer and allocate a new space of the desired size (total size desired, not just the difference). We then write the bytes of the old space (bytes have not been 0'd out, so data persists) to the new space, for the length of the old space.

Our leak detection function (leekHarvest()) simply iterates through both the small and big set of headers and sums up the total number of unfree memory in the big memory block. Then, it prints out how much memory remains unfree. This function is intended to run on exit of the program, using the atexit() function in the c standard library (stdlib.h).
