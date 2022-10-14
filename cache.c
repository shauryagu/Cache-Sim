/*
 * EECS 370, University of Michigan
 * Project 4: LC-2K Cache Simulator
 * Instructions are found in the project spec.
 */

#include <stdio.h>
#include <math.h>
#define MAX_CACHE_SIZE 256
#define MAX_BLOCK_SIZE 256

extern int mem_access(int addr, int write_flag, int write_data);
extern int get_num_mem_accesses();

enum actionType
{
    cacheToProcessor,
    processorToCache,
    memoryToCache,
    cacheToMemory,
    cacheToNowhere
};

typedef struct blockStruct
{
    int data[MAX_BLOCK_SIZE];
    int valid;
    int dirty;
    int lruLabel;
    int set;
    int tag;
} blockStruct;

typedef struct cacheStruct
{
    blockStruct blocks[MAX_CACHE_SIZE];
    int blockSize;
    int blockBits;
    int numSets;
    int setBits;
    int blocksPerSet;
    int cacheSize;
    int cacheBlocks;
    int lruBits;
} cacheStruct;

/* Global Cache variable */
cacheStruct cache;

void printAction(int, int, enum actionType);
void printCache();

/*
 * Set up the cache with given command line parameters. This is 
 * called once in main(). You must implement this function.
 */
void cache_init(int blockSize, int numSets, int blocksPerSet){
    cache.blockSize = blockSize;                            //cache_size = cache_blocks*blocksPerSet
    cache.numSets = numSets;                                //cache_blocks = numSets*blocksPerSet
    cache.blocksPerSet = blocksPerSet;
    cache.cacheSize = blockSize*numSets*blocksPerSet;
    cache.cacheBlocks = numSets*blocksPerSet;
    cache.blockBits = log2(blockSize);
    cache.setBits = log2(numSets);
    cache.lruBits = log2(blocksPerSet);
    for(int i = 0; i < cache.cacheBlocks; ++i){
        cache.blocks[i].valid = 0;
        cache.blocks[i].dirty = -1;
        cache.blocks[i].lruLabel = 0;
    }
}

/*
 * Access the cache. This is the main part of the project,
 * and should call printAction as is appropriate.
 * It should only call mem_access when absolutely necessary.
 * addr is a 16-bit LC2K word address.
 * write_flag is 0 for reads (fetch/lw) and 1 for writes (sw).
 * write_data is a word, and is only valid if write_flag is 1.
 * The return of mem_access is undefined if write_flag is 1.
 * Thus the return of cache_access is undefined if write_flag is 1.
 */

//             < ------ SET 0 ------->< ------ SET 1 ------>
//Blocks array [ 0 ] [ 1 ] [ 2 ] [ 3 ][ 4 ][ 5 ][ 6 ][ 7 ]      //this is what i'm doing
int cache_access(int addr, int write_flag, int write_data) {
    //printf("cache called\n");
    int tag = addr >> (cache.blockBits+cache.setBits);   // tag is everything that isnt block or set bits
    int bOff = addr & (cache.blockSize - 1);             // mask block offset
    int ind = -1;
    int insInd = -1;
    if(cache.setBits == 0){ind = 0;}                     // fully associative set --> starts at begining of blocks
    else{ind = (addr >> cache.blockBits) & (cache.numSets-1);}  // everything else, just gets what set it is in
    ind = ind*cache.blocksPerSet;                               // gets the index in blocks
    int hit = 0;
    int cLRU = 0;
    
    for(int i  = ind; i < ind + cache.blocksPerSet; ++i){              // checks to see if it's a hit
        if(cache.blocks[i].tag == tag && cache.blocks[i].valid == 1){   // hit and valid
            hit = 1;
            insInd = i;                      // index of the block in the set.
            cLRU = cache.blocks[i].lruLabel;                         // stores old lru
        }
    }
    if(hit){                                                         // is data a hit
        for(int i = ind; i < ind + cache.blocksPerSet; ++i){         // selective decrement
            if(cache.blocks[i].lruLabel > cLRU && cache.blocks[i].valid == 1){
                cache.blocks[i].lruLabel = cache.blocks[i].lruLabel - 1;
            }
        }
        cache.blocks[insInd].lruLabel = cache.blocksPerSet - 1;       // lru update
        if(write_flag == 0){                                         // do we have to update it, if not, then return data in cache
            printAction(addr, 1, cacheToProcessor);
            return cache.blocks[insInd].data[bOff];
        }
        else{                                                        // if so
            printAction(addr, 1, processorToCache);
            cache.blocks[insInd].data[bOff] = write_data;            // update data
            cache.blocks[insInd].dirty = 1;                          // set dirty bit
            return -1;
        }
    }
    
    else{                                                           // if it's a miss
        for(int i = ind; i < ind + cache.blocksPerSet; ++i){        // go through set that it belongs to, empty ind
            if(cache.blocks[i].valid == 0){
                insInd = i;
                break;
            }
        }
        if(insInd == -1){                                           // if the set/cache is full, find lowest lru
            for(int i = ind; i < ind + cache.blocksPerSet; ++i){
                if(cache.blocks[i].lruLabel == 0 && cache.blocks[i].valid == 1){
                    insInd = i;
                }
            }
        }
        
        for(int i = ind; i < ind + cache.blocksPerSet; ++i){        // decrement all lru
            cache.blocks[i].lruLabel = cache.blocks[i].lruLabel - 1;
        }
        int tempAddy = cache.blocks[insInd].tag << (cache.blockBits+cache.setBits);
        tempAddy = tempAddy + (cache.blocks[insInd].set << cache.blockBits);
        if(cache.blocks[insInd].dirty == 1){                    // if block we are writing to is dirty
            printAction(tempAddy, cache.blockSize, cacheToMemory);
            for(int i = 0; i < cache.blockSize; ++i){            // write back each element of data
                mem_access(tempAddy + i, 1, cache.blocks[insInd].data[i]);
            }
        }
        if(cache.blocks[insInd].dirty == 0 && cache.blocks[insInd].valid){printAction(tempAddy, cache.blockSize, cacheToNowhere);}
        printAction(addr - bOff, cache.blockSize, memoryToCache);
        for(int i = 0; i < cache.blockSize; ++i){               // replace data
            cache.blocks[insInd].data[i] = mem_access(addr - bOff + i, 0, 0);
        }                                                       // set all the stuff
        cache.blocks[insInd].lruLabel = cache.blocksPerSet-1;
        cache.blocks[insInd].set = (addr >> cache.blockBits) & (cache.numSets-1);
        cache.blocks[insInd].tag = tag;
        cache.blocks[insInd].valid = 1;
        cache.blocks[insInd].dirty = 0;
        if(write_flag == 1){
            printAction(addr, 1, processorToCache);
            cache.blocks[insInd].data[bOff] = write_data;
            cache.blocks[insInd].dirty = 1;
            return -1;
        }
        printAction(addr, 1, cacheToProcessor);
        return cache.blocks[insInd].data[bOff];
    }
}


/*
 * print end of run statistics like in the spec. This is not required,
 * but is very helpful in debugging.
 * This should be called once a halt is reached.
 * DO NOT delete this function, or else it won't compile.
 * DO NOT print $$$ in this function
 */
void printStats(){
    return;
}

/*
 * Log the specifics of each cache action.
 *
 * address is the starting word address of the range of data being transferred.
 * size is the size of the range of data being transferred.
 * type specifies the source and destination of the data being transferred.
 *  -    cacheToProcessor: reading data from the cache to the processor
 *  -    processorToCache: writing data from the processor to the cache
 *  -    memoryToCache: reading data from the memory to the cache
 *  -    cacheToMemory: evicting cache data and writing it to the memory
 *  -    cacheToNowhere: evicting cache data and throwing it away
 */
void printAction(int address, int size, enum actionType type)
{
    printf("$$$ transferring word [%d-%d] ", address, address + size - 1);

    if (type == cacheToProcessor) {
        printf("from the cache to the processor\n");
    }
    else if (type == processorToCache) {
        printf("from the processor to the cache\n");
    }
    else if (type == memoryToCache) {
        printf("from the memory to the cache\n");
    }
    else if (type == cacheToMemory) {
        printf("from the cache to the memory\n");
    }
    else if (type == cacheToNowhere) {
        printf("from the cache to nowhere\n");
    }
}

/*
 * Prints the cache based on the configurations of the struct
 * This is for debugging only and is not graded, so you may
 * modify it, but that is not recommended.
 */
void printCache()
{
    printf("\ncache:\n");
    for (int set = 0; set < cache.numSets; ++set) {
        printf("\tset %i:\n", set);
        for (int block = 0; block < cache.blocksPerSet; ++block) {
            printf("\t\t[ %i ]: {", block);
            for (int index = 0; index < cache.blockSize; ++index) {
                printf(" %i", cache.blocks[set * cache.blocksPerSet + block].data[index]);
            }
            printf(" }\n");
        }
    }
    printf("end cache\n");
}
