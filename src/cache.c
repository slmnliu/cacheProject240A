//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"
#include <stdio.h>

//
// TODO:Student Information
//
const char *studentName = "Solomon Liu, Avinash Kondareddy";
const char *studentID   = "A12920758, A13074121";
const char *email       = "smliu@ucsd.edu, akondare@ucsd.edu ";

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
  uint8_t lru;
  uint8_t valid;
} CacheBlock;

typedef struct CacheSet {
  uint8_t numValid;

  // Array of cache blocks
  CacheBlock * blocks;
} CacheSet;

typedef struct Cache {
  CacheSet * sets;
} Cache;

Cache * ICache;
Cache * DCache;
Cache * L2Cache;

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

Cache * createCache(uint32_t numSets, uint32_t assoc) {
  Cache * newCache = (Cache *) calloc(1, sizeof(Cache));

  // Create array of sets
  newCache->sets = (CacheSet*) calloc(numSets, sizeof(CacheSet));

  // Initialize sets
  for (int i = 0; i < numSets; i++) {
    newCache->sets[i].numValid = 0;

    // Create an array of blocks within the set of size assoc
    newCache->sets[i].blocks = (CacheBlock *) calloc(assoc, sizeof(CacheBlock));

    // mark all the blocks in the set as invalid
    for (int j = 0; j < assoc; j++) {
      newCache->sets[i].blocks[j].valid = 0;
      newCache->sets[i].blocks[j].lru = j;
    }
  }

  return newCache;
}

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

// Returns the set location in the cache of the address
uint32_t getSetBits(uint32_t addr, uint32_t numBlockBits, uint32_t numSets) {
  uint32_t numSetBits = 0;
  while (numSets >> numSetBits != 1)
    numSetBits += 1;
  
  uint32_t setBitMask = ~(-1 << numSetBits);

  return (addr >> numBlockBits) & setBitMask;
}

void updateBlocksLRUHit(CacheBlock * blocks, uint32_t numBlocks, uint32_t blockHitLRU) {
  for (int i = 0; i < numBlocks; i++) {
    
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
  for (int i = 0; i < numBlocks; i++) {
    blocks[i].lru++;
    if (blocks[i].lru == numBlocks) {
      blocks[i].lru = 0;
      blocks[i].address = addr;
    }
  }
}

// Increases the lru of all the blocks by 1 then inserts the new address at the LRU block
void updateBlocksLRUInclusive(CacheBlock * blocks, uint32_t numBlocks, uint32_t addr, uint32_t setBits) {
  CacheBlock * iblocks = ICache->sets[setBits].blocks; 
  CacheBlock * dblocks = DCache->sets[setBits].blocks; 
  uint32_t temp = 0;

  for (int i = 0; i < numBlocks; i++) {
    blocks[i].lru++;
    if (blocks[i].lru == numBlocks) {
      temp++;
      
      // check if block to evict is present in l1 cache
      for (int j = 0; j < numBlocks; j++) {
        if (iblocks[j].address == blocks[i].address) {
          iblocks[j].valid = 0;
          ICache->sets[setBits].numValid--;
        }
        if (dblocks[j].address == blocks[i].address) {
          dblocks[j].valid = 0;
          DCache->sets[setBits].numValid--;
        }
      }

      blocks[i].lru = 0;
      blocks[i].address = addr;
    }
  }
  if (temp > 1) {
    printf("Error in updateBlocksLRU");
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
  uint32_t addrSetBits = getSetBits(addr, numBlockBits, icacheSets);
  uint32_t zeroedBlockAddr = addr & blockMask;
  CacheBlock * blocks = ICache->sets[addrSetBits].blocks;

  if (icacheSets == 0) {
    return l2cache_access(zeroedBlockAddr);
  }

  icacheRefs++;
  // check if addr exists in cache
  for (int i = 0; i < icacheAssoc; i++) {

    CacheBlock blockToCheck = blocks[i];
    if (blockToCheck.valid == 1 && blockToCheck.address == zeroedBlockAddr) {
            // update LRU of blocks on hit
            updateBlocksLRUHit(blocks, icacheAssoc, blockToCheck.lru);
            
            return icacheHitTime;
          }
  }

  // icache missed, check l2 cache
  icacheMisses++;

  uint32_t l2Latency = l2cache_access(zeroedBlockAddr);

  // bring the value into the l1 cache
  if (ICache->sets[addrSetBits].numValid == icacheAssoc) {
    updateBlocksLRUMiss(blocks, icacheAssoc, zeroedBlockAddr);
  }
  else {
    ICache->sets[addrSetBits].numValid++;

    // replace the first invalid block with the new block and mark it valid
    for (int i = 0; i < icacheAssoc; i++) {
      CacheBlock * checkedBlock = blocks + i;

      if (checkedBlock->valid == 0) {
        checkedBlock->valid = 1;
        checkedBlock->address = zeroedBlockAddr;

        for (int j = 0; j < icacheAssoc; j++) {
          blocks[j].lru++;
        }
        checkedBlock->lru = 0;

        break;
      }
    }
  }

  icachePenalties += l2Latency;
  
  return icacheHitTime + l2Latency;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  uint32_t addrSetBits = getSetBits(addr, numBlockBits, dcacheSets);
  uint32_t zeroedBlockAddr = addr & blockMask;
  CacheBlock * blocks = DCache->sets[addrSetBits].blocks;

  if (dcacheSets == 0) {
    return l2cache_access(zeroedBlockAddr);
  }

  dcacheRefs++;
  // check if addr exists in cache
  for (int i = 0; i < dcacheAssoc; i++) {

    CacheBlock blockToCheck = blocks[i];
    if (blockToCheck.valid == 1 && blockToCheck.address == zeroedBlockAddr) {
            // update LRU of blocks on hit
            updateBlocksLRUHit(blocks, dcacheAssoc, blockToCheck.lru);
            
            return dcacheHitTime;
          }
  }

  // dcache missed, check l2 cache
  dcacheMisses++;

  uint32_t l2Latency = l2cache_access(zeroedBlockAddr);

  // bring the value into the l1 cache
  if (DCache->sets[addrSetBits].numValid == dcacheAssoc) {
    updateBlocksLRUMiss(blocks, dcacheAssoc, zeroedBlockAddr);
  }
  else {
    DCache->sets[addrSetBits].numValid++;
    // replace the first invalid block with the new block and mark it valid
    for (int i = 0; i < dcacheAssoc; i++) {
      CacheBlock * checkedBlock = blocks + i;

      if (checkedBlock->valid == 0) {
        checkedBlock->valid = 1;
        checkedBlock->address = zeroedBlockAddr;

        for (int j = 0; j < dcacheAssoc; j++) {
          blocks[j].lru++;
        }
        checkedBlock->lru = 0;

        break;
      }
    }
  }
  dcachePenalties += l2Latency;
  
  return dcacheHitTime + l2Latency;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  uint32_t addrSetBits = getSetBits(addr, numBlockBits, l2cacheSets);
  uint32_t zeroedBlockAddr = addr & blockMask;
  CacheBlock * blocks = L2Cache->sets[addrSetBits].blocks;

  if (l2cacheSets == 0) {
    return memspeed;
  }

  l2cacheRefs++;
  // check if addr exists in cache
  for (int i = 0; i < l2cacheAssoc; i++) {

    CacheBlock blockToCheck = blocks[i];
    if (blockToCheck.valid == 1 && blockToCheck.address == zeroedBlockAddr) {
            // update LRU of blocks on hit
            updateBlocksLRUHit(blocks, l2cacheAssoc, blockToCheck.lru);
            
            return l2cacheHitTime;
          }
  }

  // l2cache missed, check l2 cache
  l2cacheMisses++;

  // bring the value into the l2 cache
  if (L2Cache->sets[addrSetBits].numValid == l2cacheAssoc) {
    if (inclusive) 
      updateBlocksLRUInclusive(blocks, l2cacheAssoc, zeroedBlockAddr, addrSetBits);
    else
      updateBlocksLRUMiss(blocks, l2cacheAssoc, zeroedBlockAddr);
  }
  else {
    L2Cache->sets[addrSetBits].numValid++;
    // replace the first invalid block with the new block and mark it valid
    for (int i = 0; i < l2cacheAssoc; i++) {
      CacheBlock * checkedBlock = blocks + i;

      if (checkedBlock->valid == 0) {
        checkedBlock->valid = 1;
        checkedBlock->address = zeroedBlockAddr;

        for (int j = 0; j < l2cacheAssoc; j++) {
          blocks[j].lru++;
        }
        checkedBlock->lru = 0;

        break;
      }
    }
  }

  l2cachePenalties += memspeed;
  
  return l2cacheHitTime + memspeed;
}
