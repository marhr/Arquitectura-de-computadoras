/*
 * cache.h
 */

#define TRUE 1
#define FALSE 0
#define DIR_SIZE 32

#define UCACHE -1
#define ICACHE 0
#define DCACHE 1

/* default cache parameters--can be changed */
#define WORD_SIZE 4 /* En bytes */ // 4B = 32 bits
#define WORD_SIZE_OFFSET 2 /* En bits */ // 2 bits, line = 14 bits, tag = 16
#define DEFAULT_CACHE_SIZE (8 * 1024) /* En bytes */ // 8 KB
#define DEFAULT_CACHE_BLOCK_SIZE 16 /* En bytes */ //
#define DEFAULT_CACHE_ASSOC 1 /* */
#define DEFAULT_CACHE_WRITEBACK TRUE
#define DEFAULT_CACHE_WRITEALLOC TRUE

/* constants for settting cache parameters */
#define CACHE_PARAM_BLOCK_SIZE 0
#define CACHE_PARAM_USIZE 1
#define CACHE_PARAM_ISIZE 2
#define CACHE_PARAM_DSIZE 3
#define CACHE_PARAM_ASSOC 4
#define CACHE_PARAM_WRITEBACK 5
#define CACHE_PARAM_WRITETHROUGH 6
#define CACHE_PARAM_WRITEALLOC 7
#define CACHE_PARAM_NOWRITEALLOC 8


/* structure definitions */
typedef struct cache_line_ {
  unsigned tag;
  int dirty;

  struct cache_line_ *LRU_next;
  struct cache_line_ *LRU_prev;
} cache_line, *Pcache_line;

typedef struct cache_ {
  int size;			/* cache size */
  int associativity;		/* cache associativity */
  int n_sets;			/* number of cache sets */
  unsigned index_mask;		/* mask to find cache index */
  int index_mask_offset;	/* number of zero bits in mask */
  unsigned tag_mask;  /* mask to find cache tag */
  int tag_mask_offset;	/* number of zero bits in mask */
  Pcache_line *LRU_head;	/* head of LRU list for each set */
  Pcache_line *LRU_tail;	/* tail of LRU list for each set */
  int *set_contents;		/* number of valid entries in set */
  int contents;			/* number of valid entries in cache */
} cache, *Pcache;

typedef struct cache_stat_ {
  int accesses;			/* number of memory references */
  int misses;			/* number of cache misses */
  int replacements;		/* number of misses that cause replacments */
  int demand_fetches;		/* number of fetches */
  int copies_back;		/* number of write backs */
} cache_stat, *Pcache_stat;


/* function prototypes */
void set_cache_param();
void init_cache();
void perform_access();
void pa_wa_wb();
void pa_wa_wt();
void pa_wna_wb();
void pa_wna_wt();
void search_item();
void flush();
void delete();
void insert();
void dump_settings();
void print_stats();
void custom_print();
void init_pcache();
void insert_on_compulsory_miss();
void insert_on_conflict_miss();
void insert_on_hit();


/* macros */
#define LOG2(x) ((int) rint((log((double) (x))) / (log(2.0))))
