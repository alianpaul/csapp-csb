/*
 *something wrong with the freeCache,
 *free() generate an error, 
 *need to learn malloc free more precisely*/

#include "cachelab.h"
#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>

const char *argp_program_version = "cache simulator";

const char *argp_program_bug_address = "<alianpaul512@gmail.com";

/*Program documentation*/
static char doc[] = "a small simple cache simulator work with traces";

/*A description of the arguments we acceptO*/
static char args_doc[] = "ARG1 ARG2 ARG3 ARG4";

/*Options */
static struct argp_option options[] = {
  {"set", 's', "SET", 0, "the set num"},
  {"line", 'E', "LINE", 0, "the line num"},
  {"block", 'b', "BLOCK", 0, "the block num"},
  {"trace", 't', "PATH", 0, "the path of the trace file"},
  {"view", 'v', 0, 0, "view the detail for every entry in the file"},
  {0}
};

/*the sturcture used by main to communicate with parse_opt*/
struct arguments
{
  int arg_num;
  int view;     /*the -v flag*/
  char *set_num,*line_num,*block_num,*file_name;
};

/*Parser*/
static error_t 
parse_opt(int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;
 
  switch(key)
    {
    case 'v':
      arguments->view = 1;
      break;
    case 's':
      arguments->set_num  = arg;
      break;
    case 'E':
      arguments->line_num = arg;
      break;
    case 'b':
      arguments->block_num = arg;
      break;
    case 't':
      arguments->file_name = arg;
      break;
    case ARGP_KEY_ARG:
      printf("sdfsf\n");
      printf("%d\n",state->arg_num);
  
      break;  
    case ARGP_KEY_END:
      if(state->arg_num < 4)
	//	argp_usage(state);
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
};

static struct argp argp = {options, parse_opt, args_doc, doc};

typedef struct
{
  long long int line_tag;
  unsigned int access_time;
  struct line *next_line;
}line;

typedef struct
{
  long long int set_tag;
  struct set *next_set;
  line *line_head;
}set;

typedef struct
{
  int s,E,b,v;
  unsigned int caching_time;
  set* set_head;
}cache;

long long int getSetTag(long long int addr, int s, int b)
{
  long long int mask = -1 << s;
  addr = addr >> b;
  return addr & ~mask;
};

long long int getLineTag(long long int addr, int s, int b)
{
  return addr>>(s+b);
};

set* setSelect(cache* Ln_cache,long long  int set_tag)
{
  set* pset = Ln_cache->set_head;
  if(pset == NULL)
    {
      pset = (set*)malloc(sizeof(set));
      pset->set_tag = set_tag;
      pset->next_set = NULL;
      pset->line_head = NULL;
      Ln_cache->set_head = pset;
    }
  else
    {
      while( pset->set_tag != set_tag && pset->next_set != NULL)
	pset =(set*) pset->next_set;
      if(pset->set_tag != set_tag && pset->next_set == NULL) //doesn't exist
	{
	  pset->next_set = (set*)malloc(sizeof(set));
	  pset =(set*)pset->next_set;
	  pset->set_tag = set_tag;
	  pset->next_set = NULL;
	  pset->line_head = NULL;
	}
    }
  return pset;
  
};

line* lineMatch(set* pselected_set, long long int line_tag,
		int* miss_count, int* hit_count, int* eviction_count,
		cache* Ln_cache, char instr)
{

  int line_num = Ln_cache->E;
  line* pline = pselected_set->line_head;
  /*hit cold conflict*/
  if(pline == NULL)
    {
      /*cold miss, all lines are invalid*/
      pline = (line*)malloc(sizeof(line));
      pline->line_tag = line_tag;
      pline->next_line = NULL;
      pline->access_time = Ln_cache->caching_time;
      pselected_set->line_head = pline;
      switch(instr)
	{
	case 'L':case 'S':
	  (*miss_count)++;
	  if(Ln_cache->v)
	    printf(" miss\n");
	  break;
	case 'M':
	  (*miss_count)++;
	  (*hit_count)++;
	  if(Ln_cache->v)
	    printf(" miss,hit\n");
	  break;
	}
    }
  else
    {
      int i;
      for(i = 0; pline->line_tag != line_tag && pline->next_line != NULL; i++)
	pline = pline->next_line;

      /**********************************************/
      if(pline->line_tag != line_tag && i == (Ln_cache->E - 1))
	{
	  /*conflict
	   *find the min access time line,evict this line 
	   */
	  pline = pselected_set->line_head;
	  line* pline_min = pline;
	  unsigned int min_access_time = pline->access_time;
	  pline = pline->next_line;
	  while(pline != NULL)
	    {
	      if(pline->access_time < min_access_time)
		{
		  pline_min = pline;
		  min_access_time = pline->access_time;
		}
	      pline = pline->next_line;

	    }
	  pline_min->line_tag = line_tag; //replace the line
	  pline_min->access_time = Ln_cache->caching_time;//refresh the access time
	  pline = pline_min;
	  
	  switch(instr)
	    {
	    case 'L':case 'S':
	      (*miss_count)++;
	      (*eviction_count)++;
	      if(Ln_cache->v)
		printf(" miss,eviction\n");
	      break;
	    case 'M':
	      (*miss_count)++;
	      (*eviction_count)++;
	      (*hit_count)++;
	      if(Ln_cache->v)
		printf(" miss,eviction,hit\n");
	      break;
	    }
	  
	}
      else if(pline->line_tag == line_tag)
	{
	  /*hit*/
	  pline->access_time = Ln_cache->caching_time;
	  switch(instr)
	    {
	    case 'L':case 'S':
	       (*hit_count)++;
	      if(Ln_cache->v)
		printf(" hit\n");
	      break;
	    case 'M':
	      (*hit_count)++;
	      (*hit_count)++;
	      if(Ln_cache->v)
		printf(" hit,hit\n");
	      break;
	    }
	
	}
      else 
	{
	  /*cole miss*/
	  pline->next_line = (line*)malloc(sizeof(line));
	  pline = pline->next_line;
	  pline->line_tag = line_tag;
	  pline->next_line = NULL;
	  pline->access_time = Ln_cache->caching_time;
	  
	  switch(instr)
	    {
	    case 'L':case 'S':
	      (*miss_count)++;
	      if(Ln_cache->v)
		printf(" miss\n");
	      break;
	    case 'M':
	      (*miss_count)++;
	      (*hit_count)++;
	      if(Ln_cache->v)
		printf(" miss,hit\n");
	      break;
	    }
	  
	}
    }
  //printf("%d\n",pline->line_tag);
  return pline;
  
};

void viewCache(cache* Ln_cache)
{
  set* pset = Ln_cache->set_head;
  while(pset != NULL)
    {
      /*scan the set*/
      printf("set:0x%x:%llx\n",pset,pset->set_tag);
      line* pline = pset->line_head;
      while(pline != NULL)
	{
	  printf("line:0x%x:%llx\n",pline, pline->line_tag);
	  pline = pline->next_line;
	}
      pset = pset->next_set;
    }
};

void freeCache(cache* Ln_cache)
{
  set* pnext_set = NULL;
  set* pset = Ln_cache->set_head;
  while( pset != NULL)
    {
      line* pnext_line = NULL;
      line* pline = pset->line_head;
      while( pline != NULL)
	{
	  pnext_line = pline->next_line;
	 
	  printf("0x%x line:%x\n",pline,pline->line_tag);
	  free(pline);
	  pline = pnext_line;
	}
      pnext_set = pset->next_set;
      
      printf("0x%x set:%x\n",pset, pset->set_tag);
      free(pset);
      pset = pnext_set;
    }
  free(Ln_cache);
};

int caching(cache* Ln_cache,
	    int* miss_count, int* hit_count, int* eviction_count,
	    long long int addr, char instr)
{
  long long int set_tag,line_tag;
  set* pselected_set = NULL;
  line* pselected_line = NULL;
  set_tag = getSetTag(addr, Ln_cache->s, Ln_cache->b);
  line_tag = getLineTag(addr, Ln_cache->s, Ln_cache->b);
  
  //printf("\nset_tag: %llx,line_tag: %llx\n", set_tag, line_tag);

  if(Ln_cache->set_head == NULL)
    {
      *miss_count = 0;
      *hit_count = 0;
      *eviction_count = 0;
    }
  /*set selection*/
  pselected_set = setSelect(Ln_cache,set_tag);
  //printf("0x%llx\n",pselected_set);
  //if(instr != 'L')
  //printf("fuck\n");
  /*line matching*/
  pselected_line = lineMatch(pselected_set, line_tag,
		miss_count, hit_count, eviction_count,
		Ln_cache, instr);
  Ln_cache->caching_time++;
  return 0;
};


int main(int argc, char *argv[])
{
  struct arguments arguments;
  int  fd,n,data_size;
  long long int addr;
  int miss_count,hit_count,eviction_count;
  char *file_path = NULL;
  rio_t rio;
  char buf[MAXLINE],instr;
  

  arguments.view = 0;
  arguments.set_num = "";
  arguments.line_num = "";
  arguments.block_num = "";
  arguments.file_name = "";

  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  file_path = arguments.file_name;

  cache L1_cache;
  L1_cache.s = atoi(arguments.set_num);
  L1_cache.E = atoi(arguments.line_num);
  L1_cache.b = atoi(arguments.block_num);
  L1_cache.v = arguments.view;
  L1_cache.set_head = NULL;
  L1_cache.caching_time = 0;

  fd = Open(file_path, O_RDONLY, 0);
  Rio_readinitb(&rio, fd);
  while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
 
      if(buf[0] == ' ')
	{
	  sscanf(buf, " %c %llx,%d",&instr,&addr,&data_size);
	  if(L1_cache.v = 1)
	    printf("%c %llx,%d",instr,addr,data_size);
	  
	  // printf("%c %lld = %llx %d\n",instr, addr, addr, data_size);
	  /*
	    when work with the long.trace,the address =  7fefe058
	    the addr we get is fefe058,lost the highest character
	    
	    solution: overflow,use %llx not %x;
	   */
	  caching(&L1_cache,&miss_count,&hit_count,&eviction_count,addr,instr);
	  
	}
    }
  // viewCache(&L1_cache);
  printf("hits:%d misses:%d evictions:%d\n", hit_count, miss_count, eviction_count);
  //freeCache(&L1_cache);
  return 0;
}
