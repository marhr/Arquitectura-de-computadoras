/*
 * cache.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "cache.h"
#include "main.h"

#define MASK 0xFFFFFFFF

/* cache configuration parameters */
static int cache_split = 0;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE;
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;

/* cache model data structures */
static Pcache icache;
static Pcache dcache;
static cache c1;
static cache c2;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;

/************************************************************/
void set_cache_param(param, value)
  int param;
  int value;
{

  switch (param) {
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE; /* En bytes */
    break;
  case CACHE_PARAM_USIZE:
    cache_split = FALSE;
    cache_usize = value;
    break;
  case CACHE_PARAM_ISIZE:
    cache_split = TRUE;
    cache_isize = value;
    break;
  case CACHE_PARAM_DSIZE:
    cache_split = TRUE;
    cache_dsize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  case CACHE_PARAM_WRITEBACK:
    cache_writeback = TRUE;
    break;
  case CACHE_PARAM_WRITETHROUGH:
    cache_writeback = FALSE;
    break;
  case CACHE_PARAM_WRITEALLOC:
    cache_writealloc = TRUE;
    break;
  case CACHE_PARAM_NOWRITEALLOC:
    cache_writealloc = FALSE;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }

}
/************************************************************/

void init_pcache(pcache, type)
  Pcache pcache;
  int type;
{
    int pcachesize = (type==UCACHE) ? cache_usize : ( (type==ICACHE) ? cache_isize : cache_dsize );

    pcache->size = pcachesize;
    pcache->associativity = cache_assoc;
    pcache->n_sets = pcache->size / cache_block_size / pcache->associativity;
    pcache->LRU_head = (Pcache_line*)malloc(sizeof(Pcache_line)*pcache->n_sets);
    pcache->LRU_tail = (Pcache_line*)malloc(sizeof(Pcache_line)*pcache->n_sets);
    pcache->contents = 0;
    pcache->tag_mask_offset = LOG2(pcache->size/pcache->associativity);
    pcache->tag_mask = MASK << pcache->tag_mask_offset;
    pcache->index_mask_offset = pcache->tag_mask_offset - LOG2(pcache->n_sets);
    pcache->index_mask =1;
    for (size_t i = 0; i <  LOG2(cache_block_size) - 1; i++)
      pcache->index_mask = (pcache->index_mask << 1) | 1;
    pcache->index_mask = ~(pcache->tag_mask | pcache->index_mask);
    for(int i=0; i < pcache->n_sets; i++){
      pcache->LRU_head[i]=NULL;
      pcache->LRU_tail[i]=pcache->LRU_head[i];
    }
}

/************************************************************/
void init_cache()
{

  /* initialize the cache, and cache statistics data structures */

  // data structures
  cache_stat_inst.accesses = 0;
  cache_stat_inst.misses = 0;
  cache_stat_inst.replacements = 0;
  cache_stat_inst.demand_fetches = 0;
  cache_stat_inst.copies_back = 0;

  cache_stat_data.accesses = 0;
  cache_stat_data.misses = 0;
  cache_stat_data.replacements = 0;
  cache_stat_data.demand_fetches = 0;
  cache_stat_data.copies_back = 0;

  // cache
  icache = &c1;
  dcache = &c2;

  if(cache_split){
      init_pcache(icache, ICACHE);
      init_pcache(dcache, DCACHE);
  }
  else{
      init_pcache(dcache, UCACHE);
  }
}
/************************************************************/

void search_item(pcache, ind, tag)
  Pcache pcache;
  unsigned ind, tag;
{
  Pcache_line current = pcache->LRU_head[ind];

  int found = FALSE;
  for(pcache->contents = 0; pcache->contents < cache_assoc; pcache->contents++){
    if (current == NULL){
        break;
    }
    else{
      if (current->tag == tag) {
        found = TRUE;
        break;
      }
      current =  current->LRU_next;
    }
  }
  if (!found && pcache->contents < cache_assoc){
    pcache->contents = -1;
  }
}

void insert_on_compulsory_miss(pcache, ind, tag, dirty)
  Pcache pcache;
  unsigned ind, tag, dirty;
{
  Pcache_line nl = malloc(sizeof(cache_line));
  nl->tag = tag;
  nl->dirty = dirty;
  insert(&pcache->LRU_head[ind], &pcache->LRU_tail[ind], nl);
}

void insert_on_conflict_miss(pcache, ind, tag, dirty)
  Pcache pcache;
  unsigned ind, tag, dirty;
{
  Pcache_line nl = malloc(sizeof(cache_line));
  nl->tag = tag;
  nl->dirty = dirty;
  delete(&pcache->LRU_head[ind], &pcache->LRU_tail[ind], pcache->LRU_tail[ind]);
  insert(&pcache->LRU_head[ind], &pcache->LRU_tail[ind], nl);
}

void insert_on_hit(pcache, ind, tag, dirty)
  Pcache pcache;
  unsigned ind, tag, dirty;
{
  Pcache_line current = pcache->LRU_head[ind];
  while(current != NULL)
  {
    if (current->tag == tag) {
      current->dirty = current->dirty + dirty;
      delete(&pcache->LRU_head[ind], &pcache->LRU_tail[ind], current);
      insert(&pcache->LRU_head[ind], &pcache->LRU_tail[ind], current);
      break;
    }
    else {
      current =  current->LRU_next;
    }
  }
}

void pa_wa_wb(access_type, tag, ind)
  unsigned access_type, tag, ind;
{
    switch (access_type) {
        case TRACE_DATA_LOAD:
            cache_stat_data.accesses++;
            search_item(dcache, ind, tag);
            if(dcache->contents < 0) // compulsory miss
            {
                cache_stat_data.misses++;
                insert_on_compulsory_miss(dcache, ind, tag, 0);
                cache_stat_data.demand_fetches += words_per_block;
            }
            else if(dcache->contents < cache_assoc){ // hit
                insert_on_hit(dcache, ind, tag, 0);
            }
            else // conflict miss
            {
                cache_stat_data.misses++;
                cache_stat_data.replacements++;
                cache_stat_data.demand_fetches += words_per_block;
                if (dcache->LRU_tail[ind]->dirty) {
                    cache_stat_data.copies_back += words_per_block;
                }
                insert_on_conflict_miss(dcache, ind, tag, 0);
            }
        break;
        case TRACE_DATA_STORE:
            cache_stat_data.accesses++;
            search_item(dcache, ind, tag);
            if(dcache->contents < 0) // compulsory miss
            {
                cache_stat_data.misses++;
                insert_on_compulsory_miss(dcache, ind, tag, 1);
                cache_stat_data.demand_fetches += words_per_block;
            }
            else if(dcache->contents < cache_assoc){ // hit
                insert_on_hit(dcache, ind, tag, 1);
            }
            else // conflict miss
            {
                if (dcache->LRU_tail[ind]->dirty) {
                    cache_stat_data.copies_back += words_per_block;
                }
                cache_stat_data.misses++;
                cache_stat_data.replacements++;
                cache_stat_data.demand_fetches += words_per_block;
                insert_on_conflict_miss(dcache, ind, tag, 1);
            }
        break;
        case TRACE_INST_LOAD:
            cache_stat_inst.accesses++;
            Pcache pcache = cache_split ? &c1 : &c2;

            search_item(pcache, ind, tag);
            if(pcache->contents < 0) // compulsory miss
            {
                cache_stat_inst.misses++;
                insert_on_compulsory_miss(pcache, ind, tag, 0);
                cache_stat_inst.demand_fetches+=words_per_block;
            }
            else if(pcache->contents < cache_assoc){ // hit
                insert_on_hit(pcache, ind, tag, 0);
            }
            else // conflict miss
            {
                if (pcache->LRU_tail[ind]->dirty) {
                    cache_stat_data.copies_back+=words_per_block;
                }
                cache_stat_inst.misses++;
                cache_stat_inst.replacements++;
                cache_stat_inst.demand_fetches+=words_per_block;
                insert_on_conflict_miss(pcache, ind, tag, 0);
            }
        break;
        default:
            printf("skipping access, unknown type(%d)\n", access_type);
    }
}

void pa_wa_wt(access_type, tag, ind)
  unsigned access_type, tag, ind;
{
    switch (access_type) {
        case TRACE_DATA_LOAD:
          search_item(dcache, ind, tag);
          if(dcache->contents < 0) // compulsory miss
          {
              cache_stat_data.misses++;
              insert_on_compulsory_miss(dcache, ind, tag, 0);
              cache_stat_data.demand_fetches += words_per_block;
          }
          else if(dcache->contents < cache_assoc){ // hit
              insert_on_hit(dcache, ind, tag, 0);
          }
          else // conflict miss
          {
              cache_stat_data.misses++; 
              cache_stat_data.replacements++;
              cache_stat_data.demand_fetches += words_per_block;
              insert_on_conflict_miss(dcache, ind, tag, 0);
          }
        break;
        case TRACE_DATA_STORE:
          cache_stat_data.accesses++;
          search_item(dcache, ind, tag);
          if(dcache->contents < 0) // compulsory miss
          {
              cache_stat_data.misses++;
              insert_on_compulsory_miss(dcache, ind, tag, 0);
              cache_stat_data.demand_fetches += words_per_block;
          }
          else if(dcache->contents < cache_assoc){ // hit
            insert_on_hit(dcache, ind, tag, 0);
          }
          else //conflict miss
          {
              cache_stat_data.misses++; 
              cache_stat_data.replacements++;
              cache_stat_data.demand_fetches += words_per_block;
                insert_on_conflict_miss(dcache, ind, tag, 0);
          }
          cache_stat_data.copies_back += 1;
        break;
        case TRACE_INST_LOAD:
          cache_stat_inst.accesses++;
          Pcache pcache = cache_split ? &c1 : &c2;

          search_item(pcache, ind, tag);
          if(pcache->contents < 0) // compulsory miss
          {
              cache_stat_inst.misses++;
              insert_on_compulsory_miss(pcache, ind, tag, 0);
              cache_stat_inst.demand_fetches+=words_per_block;
          }
          else if(pcache->contents < cache_assoc){ // hit
              insert_on_hit(pcache, ind, tag, 0);
          }
          else // conflict miss
          {
              cache_stat_inst.misses++;
              cache_stat_inst.replacements++;
              cache_stat_inst.demand_fetches+=words_per_block;
              insert_on_conflict_miss(pcache, ind, tag, 0);
          }
        break;
        default:
            printf("skipping access, unknown type(%d)\n", access_type);
    }

}

void pa_wna_wb(access_type, tag, ind)
  unsigned access_type, tag, ind;
{
      switch (access_type) {
          case TRACE_DATA_LOAD:
              search_item(dcache, ind, tag);
              if(dcache->contents < 0) // compulsory miss
              {
                  cache_stat_data.misses++;
                  insert_on_compulsory_miss(dcache, ind, tag, 0);
                  cache_stat_data.demand_fetches += words_per_block;
              }
              else if(dcache->contents < cache_assoc){ // hit
                  insert_on_hit(dcache, ind, tag, 0);
              }
              else // conflict miss
              {
                  if (dcache->LRU_tail[ind]->dirty) {
                      cache_stat_data.copies_back += words_per_block;
                  }
                  cache_stat_data.misses++;
                  cache_stat_data.replacements++;
                  cache_stat_data.demand_fetches += words_per_block;
                  insert_on_conflict_miss(dcache, ind, tag, 0);
              }
          break;
          case TRACE_DATA_STORE:
              cache_stat_data.accesses++;
              search_item(dcache, ind, tag);
              if(dcache->contents < 0) // compulsory miss
              {
                  cache_stat_data.misses++;
                  cache_stat_data.copies_back++;
              }
              else if(dcache->contents < cache_assoc){ // hit
                  insert_on_hit(dcache, ind, tag, 1);
              }
              else // conflict miss
              {
                  cache_stat_data.misses++;
                  cache_stat_data.copies_back++;
              }
          break;
          case TRACE_INST_LOAD:
              cache_stat_inst.accesses++;
              Pcache pcache = cache_split ? &c1 : &c2;

              search_item(pcache, ind, tag);
              if(pcache->contents < 0) // compulsory miss
              {
                  cache_stat_inst.misses++;
                  insert_on_compulsory_miss(pcache, ind, tag, 0);
                  cache_stat_inst.demand_fetches+=words_per_block;
              }
              else if(pcache->contents < cache_assoc){ // hit
                  insert_on_hit(pcache, ind, tag, 0);
              }
              else // conflict miss
              {
                  if (pcache->LRU_tail[ind]->dirty) {
                      cache_stat_data.copies_back+=words_per_block;
                  }
                  cache_stat_inst.misses++;
                  cache_stat_inst.replacements++;
                  cache_stat_inst.demand_fetches+=words_per_block;
                  insert_on_conflict_miss(pcache, ind, tag, 0);
              }
          break;
          default:
              printf("skipping access, unknown type(%d)\n", access_type);
      }

}

//este no vienen ejemplos
void pa_wna_wt(access_type, tag, ind)
  unsigned access_type, tag, ind;
{
      switch (access_type) {
          case TRACE_DATA_LOAD:
              search_item(dcache, ind, tag);
              if(dcache->contents < 0) // compulsory miss
              {
                  cache_stat_data.misses++;
                  insert_on_compulsory_miss(dcache, ind, tag, 0);
                  cache_stat_data.demand_fetches += words_per_block;
              }
              else if(dcache->contents < cache_assoc){ // hit
                  insert_on_hit(dcache, ind, tag, 0);
              }
              else // conflict miss
              {
                  if (dcache->LRU_tail[ind]->dirty) {
                      cache_stat_data.copies_back += words_per_block;
                  }
                  cache_stat_data.misses++;
                  cache_stat_data.replacements++;
                  cache_stat_data.demand_fetches += words_per_block;
                  insert_on_conflict_miss(dcache, ind, tag, 0);
              }
          break;
          case TRACE_DATA_STORE:
              cache_stat_data.accesses++;
              search_item(dcache, ind, tag);
              if(dcache->contents < 0) // compulsory miss
              {
                  cache_stat_data.misses++;
                  cache_stat_data.copies_back++;
              }
              else if(dcache->contents < cache_assoc){ // hit
                  insert_on_hit(dcache, ind, tag, 1);
              }
              else // conflict miss
              {
                  cache_stat_data.misses++;
                  cache_stat_data.copies_back++;
              }
          break;
          case TRACE_INST_LOAD:
              cache_stat_inst.accesses++;
          Pcache pcache = cache_split ? &c1 : &c2;

          search_item(pcache, ind, tag);
          if(pcache->contents < 0) // compulsory miss
          {
              cache_stat_inst.misses++;
              insert_on_compulsory_miss(pcache, ind, tag, 0);
              cache_stat_inst.demand_fetches+=words_per_block;
          }
          else if(pcache->contents < cache_assoc){ // hit
              insert_on_hit(pcache, ind, tag, 0);
          }
          else // conflict miss
          {
              cache_stat_inst.misses++;
              cache_stat_inst.replacements++;
              cache_stat_inst.demand_fetches+=words_per_block;
              insert_on_conflict_miss(pcache, ind, tag, 0);
          }
          break;
          default:
              printf("skipping access, unknown type(%d)\n", access_type);
      }
}

/************************************************************/
void perform_access(addr, access_type)
  unsigned addr, access_type;
{

  /* handle an access to the cache */

  unsigned tag, ind;
  tag = dcache->tag_mask & addr;
  ind = (addr & dcache->index_mask) >> dcache->index_mask_offset;

  if (cache_writealloc && cache_writeback)
      pa_wa_wb(access_type, tag, ind);
  else if (cache_writealloc && !cache_writeback)
      pa_wa_wt(access_type, tag, ind);
  else if (!cache_writealloc && cache_writeback)
      pa_wna_wb(access_type, tag, ind);
  else if (!cache_writealloc && !cache_writeback)
      pa_wna_wt(access_type, tag, ind);
}
/************************************************************/

/************************************************************/
void flush()
{
  for(int i=0; i < dcache->n_sets; i++){
      if(dcache->LRU_head[i]!=NULL){
        Pcache_line current = dcache->LRU_head[i];
        while(current != NULL)
        {
          if(current->dirty)
              cache_stat_data.copies_back+=words_per_block;
          current =  current->LRU_next;
        }
      }
  }
}
/************************************************************/

/************************************************************/
void delete(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  item->LRU_next = *head;
  item->LRU_prev = (Pcache_line)NULL;

  if (item->LRU_next)
    item->LRU_next->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/

void custom_print()
{
  printf("%d, %s, %d, %d, %s, %s, %d, %d, %d, %d, %d, %d\n",
    cache_split ? cache_isize : cache_usize,
    cache_split ? "Split" : "Unified",
    cache_block_size, cache_assoc,
    cache_writeback ? "WB" : "WT",
    cache_writealloc ? "WA" : "WNA",
    cache_stat_inst.misses, cache_stat_inst.replacements,
    cache_stat_data.misses, cache_stat_data.replacements,
    cache_stat_inst.demand_fetches + cache_stat_data.demand_fetches,
    cache_stat_inst.copies_back + cache_stat_data.copies_back);
}

/************************************************************/
void dump_settings()
{
  printf("*** CACHE SETTINGS ***\n");
  if (cache_split) {
    printf("  Split I- D-cache\n");
    printf("  I-cache size: \t%d\n", cache_isize);
    printf("  D-cache size: \t%d\n", cache_dsize);
  } else {
    printf("  Unified I- D-cache\n");
    printf("  Size: \t%d\n", cache_usize);
  }
  printf("  Associativity: \t%d\n", cache_assoc);
  printf("  Block size: \t%d\n", cache_block_size);
  printf("  Write policy: \t%s\n",
	 cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
  printf("  Allocation policy: \t%s\n",
	 cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
}

/************************************************************/

/************************************************************/
void print_stats()
{
  printf("\n*** CACHE STATISTICS ***\n");

  printf(" INSTRUCTIONS\n");
  printf("  accesses:  %d\n", cache_stat_inst.accesses);
  printf("  misses:    %d\n", cache_stat_inst.misses);
  if (!cache_stat_inst.accesses)
    printf("  miss rate: 0 (0)\n");
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n",
	 (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses,
	 1.0 - (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
  printf("  replace:   %d\n", cache_stat_inst.replacements);

  printf(" DATA\n");
  printf("  accesses:  %d\n", cache_stat_data.accesses);
  printf("  misses:    %d\n", cache_stat_data.misses);
  if (!cache_stat_data.accesses)
    printf("  miss rate: 0 (0)\n");
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n",
	 (float)cache_stat_data.misses / (float)cache_stat_data.accesses,
	 1.0 - (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
  printf("  replace:   %d\n", cache_stat_data.replacements);

  printf(" TRAFFIC (in words)\n");
  printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches +
	 cache_stat_data.demand_fetches);
  printf("  copies back:   %d\n", cache_stat_inst.copies_back +
	 cache_stat_data.copies_back);
}
/************************************************************/
