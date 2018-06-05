//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"

//
// TODO:Student Information
//
const char *studentName = "Solomon Liu, Avinash Kondareddy";
const char *studentID   = "A12920758, ";
const char *email       = "smliu@ucsd.edu, ";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;     // Number of sets in the I$
uint32_t icacheAssoc;    // Associativity of the I$
uint32_t icacheHitTime;  // Hit Time of the I$

uint32_t dcacheSets;     // Number of sets in the D$
uint32_t dcacheAssoc;    // Associativity of the D$
uint32_t dcacheHitTime;  // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$
uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize;      // Block/Line size
uint32_t memspeed;       // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//
uint32_t numBlockBits;
uint32_t blockMask;

typedef struct CacheBlock {
  uint32_t address;
  uint32_t lru;
  uint32_t valid;
} CacheBlock;

typedef struct CacheSet {
  uint32_t numBlocks;
  uint32_t lru;

  // Array of cache blocks
  CacheBlock * blocks;
} CacheSet;

typedef struct Cache {
  uint32_t numSets;

  CacheSet * sets;
} Cache;

Cache * ICache;

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

// Initialize the Cache Hierarchy
//
void
init_cache()
{
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;
  
  numBlockBits = 0;

  if (blocksize == 0)
    printf("blocksize is zero, which is a problem @@@@@@@@");
  // emulate log base 2 of blocksize
  while (blocksize >> numBlockBits != 1)
    numBlockBits += 1;
  
  blockMask = (-1 << numBlockBits);

  //
  //TODO: Initialize Cache Simulator Data Structures
  //

  ICache = createCache(icacheSets, icacheAssoc);
  DCache = createCache(dcacheSets, dcacheAssoc);
  L2Cache = createCache(l2cacheSets, l2cacheAssoc);

}

Cache * createCache(uint32_t numSets, uint32_t assoc) {
  Cache * newCache = (Cache *) calloc(sizeof(Cache));
  newCache->numSets = numSets;

  // Create array of sets
  newCache->sets = (CacheSet*) calloc(size * sizeof(CacheSet));

  // Initialize sets
  for (int i = 0; i < size; i++) {
    hash->sets[i].numBlocks = assoc;
    hash->sets[i].lru = 0;

    // Create an array of blocks within the set of size assoc
    hash->sets[i].blocks = (CacheBlock *) calloc(assoc * sizeof(CacheBlock));

    // mark all the blocks in the set as invalid
    for (int j = 0; j < assoc; j++) {
      hash->sets[i].blocks[j].valid = 0;
    }
  }

  return newCache;
}

// Returns the set location in the cache of the address
uint32_t getSetBits(uint32_t addr, uint32_t numBlockBits, uint32_t numSets) {
  uint32_t numSetBits = 0;
  if (numSets == 0)
    printf("Num sets is zero, which is a problem @@@@@@@@");
  while (numSets >> numSetBits != 1)
    numSetBits += 1;
  
  uint32_t setBitMask = ~(-1 << numSetBits);

  return (addr >> numBlockBits) & setBitMask;
}

void updateBlocksLRUHit(CacheBlock * blocks, uint32_t numBlocks, uint32_t blockHitLRU) {
  for (int i = 0; i < numBlocks, i++) {
    
    // increase the lru of all blocks whose lru is less than the lru of the hit block
    if (blocks[i].lru < blockHitLRU) {
      blocks[i].lru++;
    }
    else if(blocks[i].lru == blockHitLRU) {
      blocks[i].lru = 0;
    }
  }
}

// Increases the lru of all the blocks by 1 then inserts the new address at the LRU block
void updateBlocksLRUMiss(CacheBlock * blocks, uint32_t numBlocks, uint32_t addr) {
  for (int i = 0; i < numBlocks, i++) {
    blocks[i].lru++;
    if (blocks[i].lru == numBlocks) {
      blocks[i].lru = 0;
      blocks[i].address = addr;
    }
  }
}

uint32_t allBlocksValid (CacheBlock * blocks, uint32_t numBlocks) {
  for (int i = 0; i < numBlocks; i++) {
    if (blocks[i].valid == 0) {
      return 0;
    }
  }

  return 1;
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  icacheRefs++;

  uint32_t addrSetBits = getSetBits(addr, numBlockBits, icacheSets);
  uint32_t zeroedBlockAddr = addr & blockMask;

  // check if addr exists in cache
  for (int i = 0; i < icacheAssoc; i++) {

    CacheBlock blockToCheck = ICache->sets[addrSetBits].blocks[i];
    if (blockToCheck.valid == 1 && blockToCheck.address == zeroedBlockAddr) {
            // update LRU of blocks on hit
            updateBlocksLRUHit(ICache->sets[addrSetBits].blocks, icacheAssoc, blockToCheck.lru);
            
            return icacheHitTime;
          }
  }

  // icache missed, check l2 cache
  uint32_t l2Latency = l2cache_access(zeroedBlockAddr);

  // bring the value into the l1 cache
  if (allBlocksValid(ICache->sets[addrSetBits].blocks)) {
    updateBlocksLRUMiss(ICache->sets[addrSetBits].blocks, icacheAssoc, zeroedBlockAddr);
  }
  else {
    // replace the first invalid block with the new block and mark it valid TODO
  }
  
  return icacheHitTime + l2latency;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  //
  //TODO: Implement D$
  //
  return memspeed;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  //
  //TODO: Implement L2$
  //
  return memspeed;
}
