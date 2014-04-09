/***********************************************************************/
/*                                                                     */
/*            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE              */
/*                    Version 2, December 2004                         */
/*                                                                     */
/* Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>                    */
/*                                                                     */
/* Everyone is permitted to copy and distribute verbatim or modified   */
/* copies of this license document, and changing it is allowed as long */
/* as the name is changed.                                             */
/*                                                                     */
/*            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE              */
/*   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION   */
/*                                                                     */
/*  0. You just DO WHAT THE FUCK YOU WANT TO.                          */
/*                                                                     */
/***********************************************************************/

/*some of this is a little rough. Ignore ARCH_WINDOWS and ARCH_MAC, only linux
  supported for now*/

#define _FILE_OFFSET_BITS 64

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "dirent.h"
#include <assert.h>

#if defined(_WIN32)
/*windows*/
#define ARCH_WINDOWS
#include <Windows.h>
#include <ShellAPI.h>


#elif defined(__APPLE__)
/*mac*/
#define ARCH_MAC
#include <unistd.h>


#else
/*linux*/
#define ARCH_LINUX
#include <unistd.h>
#endif

#ifndef ARCH_LINUX
#error Only linux has been implemented
#endif

#define SYSTEM_MODES
#define POLAR_SSL

#ifdef POLAR_SSL
#include <polarssl/md5.h>
#include <polarssl/sha1.h>
#endif

#define PATH_SIZE_LIMIT 3000
#define NAME_SIZE_LIMIT 256
#define EXT_CMD_LIMIT 100000
#define HSA_SIZE_LIMIT 1000000

int rev=9;

char* err_fread="ERROR: fread seemed to fail";
char* err_fopen="ERROR: fopen seemed to fail";
char* err_fwrite="ERROR: fwrite seemed to fail";
char* err_def_write="ERROR: Failed to write to def file";
char* err_mem_alloc="ERROR: Failed to allocate memory";

char file_exists(const char * filename);
int go_up_level_in_path(char* path);
int check_for_libs(char* exe_root);

/*Exit out of program, printing an error message*/
void abort_err(char* desc);

/*execute an external program, with optional parameters. Wait for completion*/
int execute( char* program, char** params);

int if_exists_delete(char*root, char* file);
int if_exists_delete2(char* root);

/*check if string represents a directory path*/
int is_dir(char* directory);

/*count num files in dir_n with extension ext*/
int count_files_with_ext(char*dir_n, char*ext);
char* list_files_with_ext2(char*dir_n, char*ext);
char* list_and_count_files_with_ext(char* dir_n, char* ext, int* count_files);

int dec(char* in_hsa, char* exe_root, char* unpack_list, char unpack_type, char test, char* srep_decompress_executable, char* srep_decompress_commandline, char* compressor_decompress_executable, char* compressor_decompress_commandline, char* compressor_extension, char delete_merged_files);

char* system_sense(char* in_dir);

int str_ends_with(const char * str, const char * suffix);

/*ORDERING FUNCTIONS*/
/*convert sha1 hash (20) to hex string (40)*/
int sha1_byte_to_hex(char* bytes, char*hex_str);
int num_images_to_order(char*flags, int limit);
int is_hash_unique(char*stopper, char*curr_hashes, int limit, char*hash);

void help();

/*read grp file into a fixed length format*/
unsigned int read_grp(char* grp, char* out, int max_elements);

int clean_files(char* root, char* no_ext, char enc);

int uncat(FILE* in, int sector_size, FILE* out, char* md5_out, char* sha1_out, unsigned int num_sectors, char test_bool, int curr_img, int tot_img, int last_sec_used_size);
int unsplit(FILE* def, FILE* sec_2048, FILE* sec_2324, FILE* sec_2336, FILE* sec_2352, FILE* out, char* md5_out, char* sha1_out, unsigned int num_2048, unsigned int num_2324, unsigned int num_2336, unsigned int num_2352, char test_bool, int curr_img, int tot_img);

/*split functions*/
int chk_q(char*sector, char sector_type, FILE* def);
int chk_p(char*sector, char sector_type, FILE* def);
int chk_edc(char*sector, char sector_type, FILE* def);
int chk_zerofill(char*sector, char sector_type, FILE* def);
int chk_subheader(char*sector, char sector_type, FILE* def);
int chk_mode(char*sector, char sector_type, FILE* def);
int chk_msf(char*sector, char sector_type, FILE* def, int lba);

/*unsplit functions*/
int gen_q(char*sector, char sector_type);
int gen_p(char*sector, char sector_type);
int gen_edc(char*sector, char sector_type);
int gen_zerofill(char*sector, char sector_type);
int gen_subheader(char*sector, char sector_type);
int gen_mode(char*sector, char sector_type);
int gen_msf(char*sector, char sector_type, int lba);
int gen_cd_sync(char*sector, char sector_type);

int lba_to_msf(int lba, char* out);
int msf_to_lba(char* msf);

int bucket_skip_n_sectors(FILE* bucket, unsigned int sector_size, unsigned int sector_count);
int def_skip_n_sectors(FILE* def, unsigned int n);

void* malloc_chk(size_t size)
{
  void* ptr=malloc(size);
  if(ptr==NULL)
    abort_err(err_mem_alloc);
  return ptr;
}
#define malloc(x) malloc_chk(x)

int puts_flush(const char * str)
{
  int ret=puts(str);
  fflush(stdout);
  return ret;
}
#define puts(x) puts_flush(x)

size_t fread_chk(void * ptr, size_t size, size_t count, FILE * stream, char* msg)
{
  size_t ret;
  if(stream==NULL)
    return 0;
  if( (ret=fread(ptr, size, count, stream))!=count )
  {
    if(msg!=NULL)
      abort_err(msg);
    else
      abort_err("ERROR: Could not read data from stream\n");
  }
  return ret;
}

void sprintf_ret_chk(int ret, int* add);

int dec_simple(char* grp, char* exe_root, char* unpack_list, char unpack_type, char test, char delete_merged_files, char scrubbed);


void help()
{
  puts("");
  puts("Merge program for related images");
  puts("");
  puts("Usage:");
  puts("ungpack [ops] some_name.grp");
  puts("");
  puts("[ops]");
  puts("Unpack by index: -ui=#,...,# --unpack-index=#,...,#");
  puts("    Comma delimited list containing index of image in grp to unpack.");
  puts("    For example, -u=0,3,4 specifies that images on the 1st, 4th and 5th");
  puts("    lines of the .grp should be unpacked");
  puts("");
  puts("Unpack by tags: -ut=#,...,# --unpack-tags=#,...,#");
  puts("    Comma delimited list containing tags to unpack.");
  puts("    For example, -ut=!,pal specifies that any images with a tag of \"!\"");
  puts("    or \"pal\" should be unpacked");
  puts("    Wrap the entire -ut option in quotes if any tags have spaces");
  puts("");
  puts("Unpack by partial filename matches: -um=#,...,# --unpack-matching=#,...,#");
  puts("    Comma delimited list containing partial matches to filename to unpack.");
  puts("    For example, -u=USA specifies that any images containing \"USA\"");
  puts("    in the filename should be unpacked");
  puts("    Wrap the entire -um option in quotes if any filename matches have spaces");
  puts("");
  puts("Delete merged files: -d --delete");
  puts("    Delete merged files if program executes successfully.");
  puts("    NOTE: use at your own risk. Program may execute successfully yet not unpack");
  puts("          every image. Incompatible with test or partial unpack options");
  puts("");
  puts("Test mode: -t --test");
  puts("    Simulate decode, same as normal operation except the images are not created");
  puts("");
  puts("Ignore padding: --pad-no");
  puts("    For system decodes, ignore padding even if present (output is scrubbed)");
  puts("");
  puts("Pause on completion: --pause");
  puts("    Wait for user interaction at the end of execution");
  puts("");
  puts("Notes:");
  puts(" <1> A WARNING indicates that a non-critical routine failed, execution");
  puts("     continues as it may be recoverable. Does not affect resulting images");
  puts(" <2> An ERROR indicates that a critical routine failed, execution stops.");
  puts("     Any resulting images if created should be checked");
  puts(" <3> Temporary files may need to be manually removed in the event of a");
  puts("     WARNING or an ERROR");
  puts(" <4> Ensure you have enough hdd space before merging or unmerging");
  puts(" <5> Always make a backup of data before use, to avoid potential data loss");
  puts("");
}

#define UNPACK_NO_OPTION 0
#define UNPACK_INDEX 1
#define UNPACK_TAGS 2
#define UNPACK_MATCHING 3

#define EXIT_SUCCESS 0/*has to be 0*/
#define EXIT_FAIL 1/*has to be positive*/

int main(int argc, char**argv)
{
  char pause_on_completion=EXIT_FAIL;

  char unpack_type=UNPACK_NO_OPTION;
  char delete_merged_files=EXIT_FAIL;

  time_t t_start, t_end;
  double time_taken;

  int i;

  char* exe_root=NULL;

  char* unpack_list=NULL;

  char test=EXIT_FAIL;

  char c=0;

  char scrubbed=EXIT_FAIL;

  char* pad_chk_path;

  /*initial code to find exe_root*/
#ifdef ARCH_MAC
  int ret;
  pid_t pid;
  pid = getpid();
#endif

  assert(sizeof(off_t)>=8);

  /*increase open file limit ceiling*/
#ifdef ARCH_WINDOWS
  if( _setmaxstdio(2048)!=2048 )
    fprintf(stderr, "WARNING: Could not increase maximum simultaneous open files to 2048\n");
#endif

  time(&t_start);

  fprintf(stderr, "\nungpack r%d\n\n", rev);

  if(argc==1 || argc==0)
  {
    help();
    abort_err("ERROR: Not enough args");
  }

  if( memcmp(argv[1], "-h", 2)==0 || memcmp(argv[1], "--h", 3)==0 )
  {
    help();
    return EXIT_SUCCESS;
  }


  /*get exe_root*/
  exe_root=malloc(PATH_SIZE_LIMIT);
  memset(exe_root, 0, PATH_SIZE_LIMIT);
#ifdef ARCH_WINDOWS
  if(file_exists("unGPack.exe")==EXIT_SUCCESS)
  {
    /*try relative*/
    fprintf(stderr, "exe_root defined relative\n");
    exe_root[0]='.';
  }
  else if( GetModuleFileName(NULL, exe_root, PATH_SIZE_LIMIT)!=0)//os specific method
  {
    fprintf(stderr, "exe_root defined absolute\n");
    if(strrchr(exe_root, '\\')!=NULL)
      (strrchr(exe_root, '\\'))[0]=0;
  }
#endif
#ifdef ARCH_MAC
  /*try relative*/
  if(file_exists("unGPack")==EXIT_SUCCESS)
  {
    fprintf(stderr, "exe_root defined relative\n");
    exe_root[0]='.';
  }
  else if( proc_pidpath(pid, exe_root, PATH_SIZE_LIMIT)>0 )//os specific method
  {
    fprintf(stderr, "exe_root defined absolute\n");
    if(go_up_level_in_path(exe_root)!=EXIT_SUCCESS)
      fprintf(stderr, "WARNING: Could not go up a level from proc_pidpath, exe_root may not be correct\n");
  }
#endif
#ifdef ARCH_LINUX
  sprintf(exe_root, "/usr/bin");/*linux does not have an exe dir, it's all on PATH. This is just in case*/
  if(0==0)
  {}
/*
  //try relative
  if(file_exists("unGPack")==EXIT_SUCCESS)
  {
    puts("exe_root defined relative");
    exe_root[0]='.';
  }
  else if( readlink("/proc/self/exe", exe_root, PATH_SIZE_LIMIT-1)!=-1 )//os specific method
  {
    puts("exe_root defined absolute");
    if(go_up_level_in_path(exe_root)!=EXIT_SUCCESS)
      puts("WARNING: Could not go up a level from /proc/self/exe, exe_root may not be correct");
  }
*/
#endif
  else
  {
    fprintf(stderr, "WARNING: Could not find executable path with OS specific method, fall back to argv[0]\n");
   
    strcpy(exe_root, argv[0]);
    if(go_up_level_in_path(exe_root)!=EXIT_SUCCESS)
      fprintf(stderr, "WARNING: Could not go up a level from argv[0], exe_root may not be correct\n");
  }
//...
  /*global options*/
  i=1;
  while( i<(argc) )
  {
    if( memcmp(argv[i], "-", 1)==0 )
    {
      if( memcmp(argv[i], "--pause", 8)==0 )
        pause_on_completion=EXIT_SUCCESS;
    }
    ++i;
  }

  if(str_ends_with(argv[argc-1], ".grp")==EXIT_SUCCESS )
  {
    if(argc>2)
    {
      i=1;
      while( i<(argc) )
      {
        if( memcmp(argv[i], "-", 1)==0 )
        {
          if(0==1)
          {
          }
          else if( memcmp(argv[i], "-ui", 3)==0 || memcmp(argv[i], "--unpack-index", 14)==0 )
          {
            if(unpack_type!=UNPACK_NO_OPTION)
            {
              fprintf(stderr, "WARNING: Multiple -u# options defined, only one should be used at a time\n");
              fprintf(stderr, "         Only the first such option encountered will be used: %s\n", unpack_list);
            }
            else
            {
              if(delete_merged_files==EXIT_SUCCESS)
              {
                fprintf(stderr, "WARNING: Partial unpack mode selected, cannot activate delete merged files option\n");
                delete_merged_files=EXIT_FAIL;
              }
              unpack_type=UNPACK_INDEX;        
              unpack_list=argv[i];
            }
          }
          else if( memcmp(argv[i], "-ut", 3)==0 || memcmp(argv[i], "--unpack-tags", 13)==0 )
          {
            if(unpack_type!=UNPACK_NO_OPTION)
            {
              fprintf(stderr, "WARNING: Multiple -u# options defined, only one should be used at a time\n");
              fprintf(stderr, "         Only the first such option encountered will be used: %s\n", unpack_list);
            }
            else
            {
              if(delete_merged_files==EXIT_SUCCESS)
              {
                fprintf(stderr, "WARNING: Partial unpack mode selected, cannot activate delete merged files option\n");
                delete_merged_files=EXIT_FAIL;
              }
              unpack_type=UNPACK_TAGS;        
              unpack_list=argv[i];
            }
          }
          else if( memcmp(argv[i], "-um", 3)==0 || memcmp(argv[i], "--unpack-matching", 17)==0 )
          {
            if(unpack_type!=UNPACK_NO_OPTION)
            {
              fprintf(stderr, "WARNING: Multiple -u# options defined, only one should be used at a time\n");
              fprintf(stderr, "         Only the first such option encountered will be used: %s\n", unpack_list);
            }
            else
            {
              if(delete_merged_files==EXIT_SUCCESS)
              {
                fprintf(stderr, "WARNING: Partial unpack mode selected, cannot activate delete merged files option\n");
                delete_merged_files=EXIT_FAIL;
              }
              unpack_type=UNPACK_MATCHING;        
              unpack_list=argv[i];
            }
          }
          else if( memcmp(argv[i], "--pad-no", 8)==0 )
          {
            scrubbed=EXIT_SUCCESS;
          }
          else if( memcmp(argv[i], "-t", 2)==0 || memcmp(argv[i], "--t", 3)==0 )
          {
            if(delete_merged_files==EXIT_SUCCESS)
            {
              fprintf(stderr, "WARNING: Test mode selected, cannot activate delete merged files option\n");
              delete_merged_files=EXIT_FAIL;
            }
            test=EXIT_SUCCESS;
          }
          else if( memcmp(argv[i], "-d", 2)==0 || memcmp(argv[i], "--delete", 8)==0 )
          {
            if(test==EXIT_SUCCESS)
              fprintf(stderr, "WARNING: Test mode selected, cannot activate delete merged files option\n");
            else if(unpack_type!=UNPACK_NO_OPTION)
              fprintf(stderr, "WARNING: Partial unpack mode selected, cannot activate delete merged files option\n");
            else
              delete_merged_files=EXIT_SUCCESS;
          }
        }
        ++i;
      }
    }

    /*check for presence of pad file*/
    pad_chk_path=malloc(4096);
    strcpy(pad_chk_path, argv[argc-1]);
    sprintf((pad_chk_path + strlen(pad_chk_path)) - 3, "pad");
    if(file_exists(pad_chk_path)!=EXIT_SUCCESS)
      scrubbed=EXIT_SUCCESS;
    free(pad_chk_path);

    if( dec_simple(argv[argc-1], exe_root, unpack_list, unpack_type, test, delete_merged_files, scrubbed) !=EXIT_SUCCESS )
      return EXIT_FAIL;
  }
  else
  {
    abort_err("ERROR: Input is not a grp file");
  }

  time(&t_end);
  time_taken = difftime(t_end, t_start);
  fprintf(stderr, "\nExecution completed in %.1lf seconds\n", time_taken);

  free(exe_root);

  if(pause_on_completion==EXIT_SUCCESS)
  {
    fprintf(stderr, "Press ENTER to exit program\n");
    do
    {
      c=getchar();
      putchar (c);
    }
    while (c != '\n');
  }

  return EXIT_SUCCESS;
}

void reset_params(char** params, int* params_loc)
{
  int i;
  i=0;
  while(i<10000)
  {
    params[i][0]=0;
    ++i;
  }
  params_loc[0]=0;
}
void add_param(char** params, char* param, int* params_loc)
{
  strcpy(params[params_loc[0]], param);
  ++params_loc[0];
}
void print_params(char** params)
{
  int i=0;
  while(params[i][0]!=0)
  {
    fprintf(stderr, " %s", params[i]);
    ++i;
  }
}

int dec_simple(char* grp, char* exe_root, char* unpack_list, char unpack_type, char test, char delete_merged_files, char scrubbed)
{
  
  char* burn=NULL;
  char* grp_read_in=NULL;
  FILE* burn_f=NULL;
  long int burn_li;
  unsigned int grp_count;
  char* unpack_mask=NULL;

  unsigned int i;
  unsigned int j;

  unsigned char compressed;
  char* gen_name=NULL;
  char* curr_path=NULL;
  char* root=NULL;
  char* curr_str;

  char* prog=NULL;
  char** params=NULL;
  int params_loc;

  char* buckets=NULL;
  char* sreps=NULL;

  char enc_type=0;/*1==normal, 2==system*/

  //int loc_in_string;

  FILE** bucket_readers=NULL;
  FILE* out=NULL;

  char buckets_needed[6];
  char buckets_present[6];

  int curr_img;
  int tot_img;

  char md5_out[33];
  char sha1_out[41];

  char**tags=NULL;
  int num_tags=0;
  char* curr_tag_loc;
  char* unpack_list_loc;

  long int chk_size;

  //check grp exists
  if(file_exists(grp)!=EXIT_SUCCESS)
  {
    fprintf(stderr, "Failed to find grp file: %s\n", grp);
    return EXIT_FAIL;
  }

  //read grp file
  fprintf(stderr, "\n<<< Read grp file >>>\n");
  burn_f=fopen(grp, "rb");
  if(burn_f==NULL)
    abort_err(err_fopen);
  fseek(burn_f, 0, SEEK_END);
  burn_li=ftell(burn_f);
  rewind(burn_f);
  burn=malloc(burn_li+1);
  burn[burn_li]=0;
  fread_chk(burn, 1, burn_li, burn_f, "ERROR: Failed fread failed reading grp file");
  fclose(burn_f);
  grp_read_in=malloc(10000*1423);/*arbitrary 10000 file limit*/
  memset(grp_read_in, 0, 10000*1423);
  grp_count=read_grp(burn, grp_read_in, 10000);
  if(grp_count==0)
    abort_err("ERROR: Failed to parse grp file");
  free(burn);

  /*initialise unpack mask*/
  unpack_mask=malloc(grp_count);

  /*handle unpack options*/
  if(unpack_type!=UNPACK_NO_OPTION)
  {
    if(unpack_list==NULL)
    {
      fprintf(stderr, "ERROR: tags supposedly defined, yet not present. Should not happen");
      return EXIT_FAIL;
    }

    tags=malloc(1000*sizeof(char*));

    num_tags=1;
    tags[0]=strchr(unpack_list, '=')+1;
    unpack_list_loc=unpack_list;
    while( (curr_tag_loc=strchr(unpack_list_loc, ','))!=NULL )
    {
      curr_tag_loc[0]=0;
      tags[num_tags]=curr_tag_loc+1;
      unpack_list_loc=curr_tag_loc+1;
      ++num_tags;
      if(num_tags==1000)
      {
        fprintf(stderr, "ERROR: Hit tag count ceiling");
        return EXIT_FAIL;
      }
    }
  }

  i=0;
  while(i<num_tags)
  {
    fprintf(stderr, "Unpack value used: %s\n", tags[i]);
    ++i;
  }

  if(unpack_type==UNPACK_INDEX)
  {
    fprintf(stderr, "Unpack type: Unpack by index\n");
    memset(unpack_mask, EXIT_FAIL, grp_count);
    i=0;
    while(i<num_tags)
    {
      if(atoi(tags[i])>=0 && atoi(tags[i])<grp_count)
      {
        unpack_mask[atoi(tags[i])]=EXIT_SUCCESS;
      }
      ++i;
    }

    free(tags);
  }
  else if(unpack_type==UNPACK_TAGS)
  {
    fprintf(stderr, "Unpack type: Unpack by tags\n");
    memset(unpack_mask, EXIT_FAIL, grp_count);
    i=0;
    while(i<num_tags)
    {
      j=0;
      while(j<grp_count)
      {
        if( strstr(&grp_read_in[(1423*j)+1024+64+74], tags[i])!=NULL )
          unpack_mask[j]=EXIT_SUCCESS;
        ++j;
      }
      ++i;
    }
    free(tags);
  }
  else if(unpack_type==UNPACK_MATCHING)
  {
    fprintf(stderr, "Unpack type: Unpack by string matches in filename\n");
    memset(unpack_mask, EXIT_FAIL, grp_count);
    i=0;
    while(i<num_tags)
    {
      j=0;
      while(j<grp_count)
      {
        if( strstr(&grp_read_in[(1423*j)], tags[i])!=NULL )
          unpack_mask[j]=EXIT_SUCCESS;
        ++j;
      }
      ++i;
    }
    free(tags);
  }
  else if(unpack_type==UNPACK_NO_OPTION)
  {
    memset(unpack_mask, EXIT_SUCCESS, grp_count);
  }
  else
  {
    fprintf(stderr, "ERROR: unpack_type not recognised, should not happen");
    return EXIT_FAIL;
  }

  /*count images to unpack*/
  curr_img=0;
  tot_img=0;
  i=0;
  while(i<grp_count)
  {
    if(unpack_mask[i]==EXIT_SUCCESS)
      ++tot_img;
    ++i;
  }
  if(tot_img==0)
    abort_err("ERROR: No images to unpack\n");
  else
    fprintf(stderr, "Unpacking %u of %u files\n", tot_img, grp_count);

  /*get gen_name*/
  gen_name=malloc(4096);
  burn=malloc(4096);
  sprintf(burn, "%s", grp);
  burn[strlen(burn)-4]=0;
  if(strrchr(burn, '\\')!=NULL)
    strrchr(burn, '\\')[0]='/';
  if(strrchr(burn, '/')==NULL)
    sprintf(gen_name, "%s", burn);
  else
    sprintf(gen_name, "%s", strrchr(burn, '/')+1);
  free(burn);

  //get root
  root=malloc(4096);
  sprintf(root, "%s", grp);
  if(strrchr(root, '\\')!=NULL)
    strrchr(root, '\\')[0]='/';
  if(strrchr(root, '/')==NULL)
    strcpy(root, ".");
  else
    strrchr(root, '/')[0]=0;

  //see how files are organised
  curr_path=malloc(4096);
  sprintf(curr_path, "%s/%s.7z", root, gen_name);
  compressed = file_exists(curr_path);
  if(compressed!=EXIT_SUCCESS)
  {
    sprintf(curr_path, "%s/%s.2048.srep", root, gen_name);
    if( file_exists(curr_path)!=EXIT_SUCCESS)
    {
      /*haven't found normal encode*/
      sprintf(curr_path, "%s/%s.system.srep", root, gen_name);
      if( file_exists(curr_path)!=EXIT_SUCCESS)
      {
        fprintf(stderr, "ERROR: Could not find compressed or uncompressed data\n");
        return EXIT_FAIL;
      }
      else
      {
        enc_type=2;
        fprintf(stderr, "System data is stored uncompressed\n");
      }
    }
    else
    {
      enc_type=1;
      fprintf(stderr, "Normal data is stored uncompressed\n");
    }
  }
  else
    fprintf(stderr, "Data is stored compressed\n");

  prog=malloc(32768);
  params=malloc(10000*sizeof(char*));

  curr_str=malloc(1024);

  i=0;
  while(i<10000)
  {
    params[i]=malloc(1024);
    ++i;
  }

  //de-7z
  if(compressed==EXIT_SUCCESS)
  {
    sprintf(curr_path, "%s/%s.7z", root, gen_name);
#ifdef ARCH_LINUX
    strcpy(prog, "7z");

    reset_params(params, &params_loc);
    add_param(params, "7z", &params_loc);
    add_param(params, "e", &params_loc);
    sprintf(curr_str, "-o'%s'", root);
    add_param(params, curr_str, &params_loc);
    sprintf(curr_str, "'%s'", curr_path);
    add_param(params, curr_str, &params_loc);
#endif
    //print_params(params);
    free(params[params_loc]);
    params[params_loc]=NULL;
    fprintf(stderr, "\n<<< Decompress 7z >>>\n");
    if( execute(params[0], params)!=EXIT_SUCCESS )
    {
      fprintf(stderr, "ERROR: Failed to decompress 7z (is p7zip installed?)\n");
      return EXIT_FAIL;
    }
    params[params_loc]=malloc(1024);
  }
  //de-tak
  if(compressed==EXIT_SUCCESS)
  {
    sprintf(curr_path, "%s/%s.tak", root, gen_name);
    if(file_exists(curr_path)==EXIT_SUCCESS)
    {
#ifdef ARCH_LINUX
      strcpy(prog, "ffmpeg");

      reset_params(params, &params_loc);
      add_param(params, "ffmpeg", &params_loc);
      add_param(params, "-i", &params_loc);
      sprintf(curr_str, "'%s'", curr_path);
      add_param(params, curr_str, &params_loc);
      add_param(params, "-f", &params_loc);
      add_param(params, "s16le", &params_loc);
      add_param(params, "-acodec", &params_loc);
      add_param(params, "pcm_s16le", &params_loc);
      sprintf(curr_str, "'%s/2352.srep'", root);
      add_param(params, curr_str, &params_loc);
#endif
      //print_params(params);
      free(params[params_loc]);
      params[params_loc]=NULL;
      fprintf(stderr, "\n<<< Decompress tak >>>\n");
      if( execute(params[0], params)!=EXIT_SUCCESS )
      {
        fprintf(stderr, "ERROR: Failed to decompress tak (is ffmpeg >= v1.2 installed?)\n");
        return EXIT_FAIL;
      }
      params[params_loc]=malloc(1024);
    }
  }
  //de-srep
  buckets=malloc(16*7);
  memset(buckets, 0, 16*7);
  strcpy(&buckets[0*16], "2048");
  strcpy(&buckets[1*16], "2324");
  strcpy(&buckets[2*16], "2336");
  strcpy(&buckets[3*16], "2352");
  strcpy(&buckets[4*16], "96");
  strcpy(&buckets[5*16], "def");
  strcpy(&buckets[6*16], "2048");
  sreps=malloc(16*7);
  memcpy(sreps, buckets, 16*7);
  strcpy(&sreps[6*16], "system");
#ifdef ARCH_LINUX
  strcpy(prog, "srep");
#endif


  if(compressed==EXIT_SUCCESS)
  {
    /*detect enc_type of previously compressed data*/
    sprintf(curr_path, "%s/system.srep", root);
    enc_type= file_exists(curr_path)==EXIT_SUCCESS?2:1;
  }

  i=0;
  while(i<7)
  {
    if(compressed==EXIT_SUCCESS)
      sprintf(curr_path, "%s/%s.srep", root, &sreps[i*16]);
    else
      sprintf(curr_path, "%s/%s.%s.srep", root, gen_name, &sreps[i*16]);
    if(file_exists(curr_path)==EXIT_SUCCESS)
    {
#ifdef ARCH_LINUX
      reset_params(params, &params_loc);
      add_param(params, "srep", &params_loc);
      add_param(params, "-d", &params_loc);
      if(compressed==EXIT_SUCCESS)
        add_param(params, "-delete", &params_loc);
      add_param(params, "-mem700mb", &params_loc);
      sprintf(curr_str, "-temp='%s/srep-data.tmp'", root);
      add_param(params, curr_str, &params_loc);

      sprintf(curr_str, "'%s'", curr_path);
      add_param(params, curr_str, &params_loc);

      sprintf(curr_str, "'%s/%s.cat'", root, &buckets[i*16]);
      add_param(params, curr_str, &params_loc);
#endif
      //print_params(params);
      free(params[params_loc]);
      params[params_loc]=NULL;
      fprintf(stderr, "\n<<< Decompress %s.srep >>>\n", &sreps[i*16]);
      if( execute(params[0], params)!=EXIT_SUCCESS )
      {
        fprintf(stderr, "ERROR: Failed to decompress %s.srep (is srep installed?)\n", &sreps[i*16]);
        return EXIT_FAIL;
      }
      params[params_loc]=malloc(1024);
    }
    ++i;
  }

  i=0;
  while(i<6)
  {
    sprintf(curr_path, "%s/%s.cat", root, &sreps[i*16]);
    buckets_present[i] = file_exists(curr_path);
    ++i;
  }

  /*use grp_read_in and what the user wants to fill buckets_needed*/
  memset(buckets_needed, EXIT_FAIL, 6);
  i=0;
  while(i<grp_count)
  {
    if(unpack_mask[i]==EXIT_SUCCESS)
    {
      if( str_ends_with(&grp_read_in[1423*i], ".iso") || str_ends_with(&grp_read_in[1423*i], ".ISO") )
        buckets_needed[0]=EXIT_SUCCESS;
      else if( str_ends_with(&grp_read_in[1423*i], ".sub") || str_ends_with(&grp_read_in[1423*i], ".SUB") )
        buckets_needed[4]=EXIT_SUCCESS;
      else if( str_ends_with(&grp_read_in[1423*i], ".bin") || str_ends_with(&grp_read_in[1423*i], ".BIN") || str_ends_with(&grp_read_in[1423*i], ".img") || str_ends_with(&grp_read_in[1423*i], ".IMG") || str_ends_with(&grp_read_in[1423*i], ".raw") || str_ends_with(&grp_read_in[1423*i], ".RAW") )
      {
        buckets_needed[5]=EXIT_SUCCESS;
        if(atoi(&grp_read_in[(1423*i)+1024])!=0)
          buckets_needed[0]=EXIT_SUCCESS;
        if(atoi(&grp_read_in[(1423*i)+1024+16])!=0)
          buckets_needed[1]=EXIT_SUCCESS;
        if(atoi(&grp_read_in[(1423*i)+1024+32])!=0)
          buckets_needed[2]=EXIT_SUCCESS;
        if(atoi(&grp_read_in[(1423*i)+1024+48])!=0)
          buckets_needed[3]=EXIT_SUCCESS;
      }
      else if( str_ends_with(&grp_read_in[1423*i], ".ccd") || str_ends_with(&grp_read_in[1423*i], ".CCD") )
        buckets_needed[0]=EXIT_SUCCESS;
    }
    ++i;
  }

  /*check buckets are present*/
  i=0;
  while(i<6)
  {
    if(buckets_needed[i]==EXIT_SUCCESS && buckets_present[i]!=EXIT_SUCCESS)
      abort_err("ERROR: A cat file that should be present is not. Selected images cannot be restored.");
    ++i;
  }

  if(enc_type==1)
  {
    /*normal mode decode*/
    fprintf(stderr, "\n<<< Decode normal encode >>>\n");

    /*initialise bucket readers*/
    bucket_readers=malloc(sizeof(FILE*)*6);
    memset(bucket_readers, 0, sizeof(FILE*)*6);
    i=0;
    while(i<6)
    {
      if(buckets_present[i]==EXIT_SUCCESS)
      {
        sprintf(curr_path, "%s/%s.cat", root, &buckets[i*16]);
        if( (bucket_readers[i]=fopen(curr_path, "rb"))==NULL )
        {
          fprintf(stderr, "ERROR: Failed to open bucket: '%s'\n", curr_path);
          return EXIT_FAIL;
        }
      }
      ++i;
    }

  /*2048, 2324, 2336, 2352, 96, def*/
  /*int uncat(FILE* in, int sector_size, FILE* out, char* md5_out, char* sha1_out, unsigned int num_sectors, char test_bool, int curr_img, int tot_img, int last_sec_used_size)*/
    memset(md5_out, 0, 33);
    memset(sha1_out, 0, 41);
    /*decode every image*/
    i=0;
    while(i<grp_count)
    {
      if(unpack_mask[i]==EXIT_SUCCESS)
      {
        /*decode*/
        fprintf(stderr, "'%s': ", &grp_read_in[1423*i]);
        sprintf(curr_path, "%s/%s", root, &grp_read_in[1423*i]);
        out=fopen(curr_path, "wb");
        if( str_ends_with(&grp_read_in[1423*i], ".iso")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".ISO")==EXIT_SUCCESS )
        {
          uncat(bucket_readers[0], 2048, out, md5_out, sha1_out, (unsigned int) atoi(&grp_read_in[(1423*i)+1024]), test, curr_img, tot_img, 2048);
        }
        else if( str_ends_with(&grp_read_in[1423*i], ".sub")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".SUB")==EXIT_SUCCESS )
        {
          uncat(bucket_readers[4], 96, out, md5_out, sha1_out, (unsigned int) atoi(&grp_read_in[(1423*i)+1024]), test, curr_img, tot_img, 96);
        }
        else if( str_ends_with(&grp_read_in[1423*i], ".bin")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".BIN")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".img")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".IMG")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".raw")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".RAW")==EXIT_SUCCESS )
        {
//int unsplit(FILE* def, FILE* sec_2048, FILE* sec_2324, FILE* sec_2336, FILE* sec_2352, FILE* out, char* md5_out, char* sha1_out, unsigned int num_2048, unsigned int num_2324, unsigned int num_2336, unsigned int num_2352, char test_bool, int curr_img, int tot_img)
          unsplit(bucket_readers[5], bucket_readers[0], bucket_readers[1], bucket_readers[2], bucket_readers[3], out, md5_out, sha1_out, (unsigned int) atoi(&grp_read_in[(1423*i)+1024]), (unsigned int) atoi(&grp_read_in[(1423*i)+1024+16]), (unsigned int) atoi(&grp_read_in[(1423*i)+1024+32]), (unsigned int) atoi(&grp_read_in[(1423*i)+1024+48]), test, curr_img, tot_img);
        }
        else if( str_ends_with(&grp_read_in[1423*i], ".ccd")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".CCD")==EXIT_SUCCESS )
        {
          uncat(bucket_readers[0], 2048, out, md5_out, sha1_out, (unsigned int) atoi(&grp_read_in[(1423*i)+1024]), test, curr_img, tot_img, atoi(&grp_read_in[(1423*i)+1024+64+74+256]));
        }
        ++curr_img;
        fclose(out);
        fprintf(stderr, "%s\n", memcmp(&grp_read_in[(1423*i)+1024+64], md5_out, 32)==0?"Match":"Mismatch");
        if( memcmp(&grp_read_in[(1423*i)+1024+64], md5_out, 32)!=0 )
        {
          fprintf(stderr, "Hash: %s\n", md5_out);
          abort_err("ERROR: Hash mismatch");
        }
      }
      else
      {
        printf("'%s': Skipped\n", &grp_read_in[1423*i]);
        /*skip image*/
        if( str_ends_with(&grp_read_in[1423*i], ".iso")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".ISO")==EXIT_SUCCESS )
        {
// bucket_skip_n_sectors(FILE* bucket, unsigned int sector_size, unsigned int sector_count)
          if( bucket_skip_n_sectors( bucket_readers[0], 2048, (unsigned int) atoi(&grp_read_in[(1423*i)+1024]) ) !=EXIT_SUCCESS )
            abort_err("ERROR: Failed to skip sectors in 2048 bucket [0]");
        }
        else if( str_ends_with(&grp_read_in[1423*i], ".sub")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".SUB")==EXIT_SUCCESS )
        {
          if( bucket_skip_n_sectors( bucket_readers[4], 96, (unsigned int) atoi(&grp_read_in[(1423*i)+1024]) ) !=EXIT_SUCCESS )
            abort_err("ERROR: Failed to skip sectors in 96 bucket");
        }
        else if( str_ends_with(&grp_read_in[1423*i], ".bin")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".BIN")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".img")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".IMG")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".raw")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".RAW")==EXIT_SUCCESS )
        {
          def_skip_n_sectors(bucket_readers[5], (unsigned int) ( atoi(&grp_read_in[(1423*i)+1024]) + atoi(&grp_read_in[(1423*i)+1024+16]) + atoi(&grp_read_in[(1423*i)+1024+32]) + atoi(&grp_read_in[(1423*i)+1024+48]) ) );
          if( bucket_skip_n_sectors( bucket_readers[0], 2048, (unsigned int) atoi(&grp_read_in[(1423*i)+1024]) ) !=EXIT_SUCCESS )
            abort_err("ERROR: Failed to skip sectors in 2048 bucket [1]");
          if( bucket_skip_n_sectors( bucket_readers[1], 2324, (unsigned int) atoi(&grp_read_in[(1423*i)+1024+16]) ) !=EXIT_SUCCESS )
            abort_err("ERROR: Failed to skip sectors in 2324 bucket");
          if( bucket_skip_n_sectors( bucket_readers[2], 2336, (unsigned int) atoi(&grp_read_in[(1423*i)+1024+32]) ) !=EXIT_SUCCESS )
            abort_err("ERROR: Failed to skip sectors in 2336 bucket");
          if( bucket_skip_n_sectors( bucket_readers[3], 2352, (unsigned int) atoi(&grp_read_in[(1423*i)+1024+48]) ) !=EXIT_SUCCESS )
            abort_err("ERROR: Failed to skip sectors in 2352 bucket");

        }
        else if( str_ends_with(&grp_read_in[1423*i], ".ccd")==EXIT_SUCCESS || str_ends_with(&grp_read_in[1423*i], ".CCD")==EXIT_SUCCESS )
        {
          if( bucket_skip_n_sectors( bucket_readers[0], 2048, (unsigned int) atoi(&grp_read_in[(1423*i)+1024]) ) !=EXIT_SUCCESS )
            abort_err("ERROR: Failed to skip sectors in 2048 bucket [2]");
        }
      }
      ++i;
    }
    /*close buckets*/
    i=0;
    while(i<6)
    {
      if(buckets_present[i]==EXIT_SUCCESS)
      {
        fclose(bucket_readers[i]);
      }
      ++i;
    }
  }
  else
  {
    /*system mode decode*/
#ifdef ARCH_LINUX
    strcpy(prog, "gparse");

    reset_params(params, &params_loc);
    add_param(params, "gparse", &params_loc);

    //loc_in_string=0;

    if(test==EXIT_SUCCESS)
    {
      add_param(params, "--test", &params_loc);
    }

    add_param(params, "d", &params_loc);

    if(scrubbed==EXIT_SUCCESS)
      add_param(params, "NULL", &params_loc);
    else
    {
      sprintf(curr_str, "'%s/%s.pad'", root, gen_name);
      add_param(params, curr_str, &params_loc);
    }

    sprintf(curr_str, "'%s/2048.cat'", root);
    add_param(params, curr_str, &params_loc);

    sprintf(curr_str, "'%s/chk'", root);
    add_param(params, curr_str, &params_loc);

    //if(scrubbed==EXIT_SUCCESS)
      //ret=sprintf(&params[loc_in_string], "d NULL '%s/2048.cat' '%s/chk'", root, root);
    //else
      //ret=sprintf(&params[loc_in_string], "d '%s/%s.pad' '%s/2048.cat' '%s/chk'", root, gen_name, root, root);

    //sprintf_ret_chk(ret, &loc_in_string);
#endif

    //do unpacking
    i=0;
    while(i<grp_count)
    {
#ifdef ARCH_LINUX
      sprintf(curr_str, "'%s/%s'", root, &grp_read_in[1423*i]);
      add_param(params, curr_str, &params_loc);
      //if(unpack_mask[i]==EXIT_SUCCESS)
        //ret=sprintf(&params[loc_in_string], " '%s/%s'", root, &grp_read_in[1423*i]);
      //else
        //ret=sprintf(&params[loc_in_string], " NULL");
      //sprintf_ret_chk(ret, &loc_in_string);
#endif
      ++i;
    }
    //print_params(params);
    free(params[params_loc]);
    params[params_loc]=NULL;
    fprintf(stderr, "\n<<< Decode system encode with gparse >>>\n");
    if( execute(params[0], params)!=EXIT_SUCCESS )
    {
      fprintf(stderr, "ERROR: Failed to decode with gparse (is gparse installed?)\n");
      return EXIT_FAIL;
    }
    params[params_loc]=malloc(1024);

    //read chk file
    sprintf(curr_str, "%s/chk", root);
    burn_f=fopen(curr_str, "rb");
    if(burn_f==NULL)
      fprintf(stderr, "WARNING: Failed to open gparse chk file to verify success. Check output images\n");
    fseek(burn_f, 0, SEEK_END);
    chk_size=ftell(burn_f);
    rewind(burn_f);
    burn=malloc(chk_size+1);
    memset(burn, 0, chk_size+1);
    if( fread(burn, 1, chk_size, burn_f)!=chk_size )
      fprintf(stderr, "WARNING: Failed to read gparse chk file to verify success. Check output images\n");
    fclose(burn_f);
    if_exists_delete2(curr_str);
    if( strchr(burn, 's')==NULL || strchr(burn, 'f')==NULL || strchr(burn, 'g')==NULL || strchr(burn, 'b')!=NULL || strchr(burn, 'e')!=NULL)
      abort_err("ERROR: gparse chk file indicates error, check output images\n");
    free(burn);

  }

  free(prog);
  free(params);
  
  //delete whatever if necessary
  /*delete buckets*/
  i=0;
  while(i<6)
  {
    sprintf(curr_path, "%s/%s.cat", root, &buckets[i*16]);
    if_exists_delete2(curr_path);
    ++i;
  }
  /*delete srep if compressed*/
  if(compressed==EXIT_SUCCESS);
  {
    i=0;
    while(i<7)
    {
      sprintf(curr_path, "%s/%s.srep", root, &sreps[i*16]);
      if_exists_delete2(curr_path);
      ++i;
    }
  }
  /*delete merged files if the user wants to*/
  if(delete_merged_files==EXIT_SUCCESS)
  {
    if(compressed==EXIT_SUCCESS)
    {
      sprintf(curr_path, "%s/%s.7z", root, gen_name);
      if_exists_delete2(curr_path);
      sprintf(curr_path, "%s/%s.tak", root, gen_name);
      if_exists_delete2(curr_path);
    }
    else
    {
      i=0;
      while(i<7)
      {
        sprintf(curr_path, "%s/%s.%s.7z", root, gen_name, &sreps[i*16]);
        if_exists_delete2(curr_path);
        ++i;
      }
    }
    sprintf(curr_path, "%s/%s.grp", root, gen_name);
    if_exists_delete2(curr_path);
  }

  //cleanup etc



  return EXIT_SUCCESS;
}

void sprintf_ret_chk(int ret, int* add)
{
  if(ret<0)
    abort_err("ERROR: sprintf failed, returned negative\n");
  add[0]+=ret;
}

int str_ends_with(const char * str, const char * suffix)
{
  size_t str_len;
  size_t suffix_len;

  if( str == NULL || suffix == NULL )
    return EXIT_FAIL;

  str_len = strlen(str);
  suffix_len = strlen(suffix);

  if(suffix_len > str_len)
    return EXIT_FAIL;

  return strncmp( str + str_len - suffix_len, suffix, suffix_len ) == 0 ? EXIT_SUCCESS:EXIT_FAIL;
}

void abort_err(char* desc)
{
  fprintf(stderr, "%s\n", desc);
  exit(EXIT_FAIL);
}

int is_dir(char* directory)
{
  DIR *dir;
  int ret;

  dir = opendir (directory);
  ret = dir!=NULL ? EXIT_SUCCESS : EXIT_FAIL;
  closedir (dir);
  return ret;
}

int clean_files(char* root, char* no_ext, char enc)
{
  char* build=NULL;

  build=malloc(4096);

  if(enc==EXIT_SUCCESS)
  {
    /*encoding*/
    sprintf(build, "%s\\%s.7z", root, no_ext);
    if_exists_delete2(build);
    sprintf(build, "%s\\%s.tak", root, no_ext);
    if_exists_delete2(build);
    sprintf(build, "%s\\%s.pad", root, no_ext);
    if_exists_delete2(build);
  }
  else
  {
    /*decoding*/
  }

  if_exists_delete(root, "2048.cat");
  if_exists_delete(root, "2324.cat");
  if_exists_delete(root, "2336.cat");
  if_exists_delete(root, "2352.cat");
  if_exists_delete(root, "def.cat");
  if_exists_delete(root, "96.cat");
  if_exists_delete(root, "96.srep");
  if_exists_delete(root, "def.srep");
  if_exists_delete(root, "2048.srep");
  if_exists_delete(root, "2324.srep");
  if_exists_delete(root, "2336.srep");
  if_exists_delete(root, "2352.srep");
  if_exists_delete(root, "2352.wav");
  if_exists_delete(root, "temp.bat");
  if_exists_delete(root, "srep-data.tmp");
  if_exists_delete(root, "srep-virtual-memory.tmp");

  free(build);

  return EXIT_SUCCESS;
}

/***/

/*order functions*/
int sha1_byte_to_hex(char* bytes, char*hex_str)
{
  char hex[]={'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  int i=0;
  while(i<20)
  {
    hex_str[(i*2)  ]=hex[ (bytes[i]>>4) & 0x0F ];
    hex_str[(i*2)+1]=hex[ (bytes[i]   ) & 0x0F ];
    ++i;
  }
  return EXIT_SUCCESS;
}


/*end of order functions*/

/*
int execute_old(char* program, char* params)
{
#ifdef ARCH_WINDOWS
  //TODO
#else
  //posix? for now assume yes.
  int status;
  int pid=fork();
  if(pid<0)
  {
    abort_err("ERROR: fork() failed, failed to execute external program");
  }
  else if(pid==0)
  {
    //child
    //execute program TODO
  }
  else
  {
    //parent
    waitpid(pid, &status, 0);//wait for external process. handled badly, fix TODO
  }
  
#endif
}
*/

int execute(char* program, char** params)
{
#ifdef ARCH_WINDOWS
  //TODO
#else
  /*posix? for now assume yes.*/
//#define EXEC_EXECVP
#define EXEC_SYSTEM

#ifdef EXEC_SYSTEM
  char* big_command=NULL;
  int big_command_loc=0;
  int i=1;
  big_command=malloc(10000*1024);
  big_command_loc+=sprintf(&big_command[big_command_loc], "%s", params[0]);
  while(params[i]!=NULL)
  {
    big_command_loc+=sprintf(&big_command[big_command_loc], " %s", params[i]);
    ++i;
  }
  if(system(big_command)!=0)
  {
    fprintf(stderr, "WARNING: Exit code non-zero\n");
    return EXIT_FAIL;
  }
  return EXIT_SUCCESS;
#endif

#ifdef EXEC_EXECVP
  //fix TODO
  int status;
  int pid=fork();
  /*int count;
  int i;
  char** params_use;*/
  if(pid<0)
  {
    abort_err("ERROR: fork() failed, failed to execute external program");
  }
  else if(pid==0)
  {
    //child
    /*count=0;
    while(params[count][0]!=0)
      ++count;
    ++count;
    params_use=malloc(count*sizeof(char*));
    i=0;
    while(i<count)
    {
      params_use[i]=params[i];
      ++i;
    }*/
    execvp(program, params);
    abort_err("ERROR: execvp() failed, failed to execute external program");
  }
  else
  {
    //parent
    waitpid(pid, &status, 0);//wait for external process. maybe handled badly, fix TODO

    pid_t tpid;

    /*do
    {
      tpid = wait(&status);
      if(tpid != pid)
        ;//process_terminated(tpid);
    } while(tpid != pid);*/

    if(status!=0)
    {
      fprintf(stderr, "WARNING: Exit code non-zero\n");
      return EXIT_FAIL;
    }
    else
      return EXIT_SUCCESS;
  }
#endif

#endif
}

int if_exists_delete(char* root, char* file)
{
  char synth[PATH_SIZE_LIMIT];
  FILE* f=NULL;

  memset(synth, 0, PATH_SIZE_LIMIT);

  sprintf(synth, "%s/%s", root, file);

  f=fopen(synth, "rb");
  if(f!=NULL)
  {
    fclose(f);
    if( remove(synth)==0 )
    {
      return EXIT_SUCCESS;
    }
    else
    {
      fprintf(stderr, "WARNING: File seems to exist, but could not be deleted: %s\n", synth);
    return EXIT_FAIL;
    }
  }
  else
  {
    return EXIT_SUCCESS;
  }
}

int if_exists_delete2(char* root)
{
  FILE* f=fopen(root, "rb");
  if(f!=NULL)
  {
    fclose(f);
    if( remove(root)==0 )
    {
      return EXIT_SUCCESS;
    }
    else
    {
      fprintf(stderr, "WARNING: File seems to exist, but could not be deleted: %s\n", root);
      return EXIT_FAIL;
    }
  }
  else
  {
    return EXIT_SUCCESS;
  }
}

/*  out format:
  1024: name
    16: 2048 count
    16: 2324 count
    16: 2336 count
    16: 2352 count
    33: md5 hex
    41: sha1 hex
   256: tags
     5: special
*/

unsigned int read_grp(char* grp, char* out, int max_elements)
{
  unsigned int loc=0;
  unsigned int curr_marker=0;
  unsigned int out_index=0;

  memset(out, 0, max_elements*1423);

  while(grp[loc]!=0)/*while not at end of file*/
  {
    if(out_index==max_elements)
      return 0;

    /*name*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    strncpy( &out[(out_index*1423)], &grp[curr_marker], loc-curr_marker );
    ++loc;

    /*2048 count*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    strncpy( &out[(out_index*1423)+1024], &grp[curr_marker], loc-curr_marker );
    ++loc;

    /*2324 count*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    strncpy( &out[(out_index*1423)+1024+16], &grp[curr_marker], loc-curr_marker );
    ++loc;

    /*2336 count*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    strncpy( &out[(out_index*1423)+1024+32], &grp[curr_marker], loc-curr_marker );
    ++loc;

    /*2352 count*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    strncpy( &out[(out_index*1423)+1024+48], &grp[curr_marker], loc-curr_marker );
    ++loc;

    /*md5*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    strncpy( &out[(out_index*1423)+1024+64], &grp[curr_marker], loc-curr_marker );
    ++loc;

    /*sha1*/
    curr_marker=loc;
    while(grp[loc]!=':' && grp[loc]!=0x0D && grp[loc]!=0x0A)
      ++loc;
    strncpy( &out[(out_index*1423)+1024+64+33], &grp[curr_marker], loc-curr_marker );
    switch(grp[loc])
    {
      case ':':
        ++loc;

        /*tags*/
        curr_marker=loc;
        while(grp[loc]!=':' && grp[loc]!=0x0D && grp[loc]!=0x0A)
          ++loc;
        strncpy( &out[(out_index*1423)+1024+64+74], &grp[curr_marker], loc-curr_marker );
        switch(grp[loc])
        {
          case ':':
            ++loc;

            /*special*/
            curr_marker=loc;
            while(grp[loc]!=0x0D && grp[loc]!=0x0A)
              ++loc;
            strncpy( &out[(out_index*1423)+1024+64+74+256], &grp[curr_marker], loc-curr_marker );
            if(grp[loc]==0x0A)
              ++loc;
            else if(grp[loc]==0x0D)
              loc+=2;
            break;

          case 0x0D:
            loc+=2;
            break;

          case 0x0A:
            ++loc;
            break;
        }
        break;

      case 0x0D:
        loc+=2;
        break;

      case 0x0A:
        ++loc;
        break;
    }
    ++out_index;
  }

  return out_index;
}

char file_exists(const char * filename)
{
  FILE* file=NULL;
  if( (file=fopen(filename, "rb"))!=NULL )
  {
    fclose(file);
    return EXIT_SUCCESS;
  }
  return EXIT_FAIL;
}

int go_up_level_in_path(char* path)
{
  if( (strrchr(path, '\\')!=NULL && strrchr(path, '/')==NULL) )
    (strrchr(path, '\\'))[0]=0;
  else if( (strrchr(path, '\\')==NULL && strrchr(path, '/')!=NULL) )
    (strrchr(path, '/'))[0]=0;
  else if( (strrchr(path, '\\')!=NULL && strrchr(path, '/')!=NULL) )
  {
    if( strrchr(path, '\\')<strrchr(path, '/') )
      (strrchr(path, '/'))[0]=0;
    else
      (strrchr(path, '\\'))[0]=0;
  }
  else
  {
    return EXIT_FAIL;
  }
  return EXIT_SUCCESS;
}

int check_for_libs(char* exe_root)
{
#ifdef ARCH_WINDOWS
  char* path=NULL;
  path=malloc(PATH_SIZE_LIMIT);
  //TODO
#endif
#ifdef ARCH_LINUX
  //TODO, or not
  if(file_exists("/usr/bin/7z")!=EXIT_SUCCESS)
    return EXIT_FAIL;
  if(file_exists("/usr/bin/gparse")!=EXIT_SUCCESS)
    return EXIT_FAIL;
  if(file_exists("/usr/bin/ffmpeg")!=EXIT_SUCCESS)
    return EXIT_FAIL;
#endif
  return EXIT_SUCCESS;
}

/*split funcs******************************************************************/
#define AUDIO 0
#define MODE0 1
#define MODE1 2
#define MODE2 3
#define MODE2FORM1 4
#define MODE2FORM2 5

unsigned char cd_sync[]={0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
unsigned char subheader[]={0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00};
unsigned char zerofill[]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*Constants for reed-solomon generation*/
unsigned char ecc_f_lut[] = {
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16,
    0x18, 0x1A, 0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E,
    0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E, 0x40, 0x42, 0x44, 0x46,
    0x48, 0x4A, 0x4C, 0x4E, 0x50, 0x52, 0x54, 0x56, 0x58, 0x5A, 0x5C, 0x5E,
    0x60, 0x62, 0x64, 0x66, 0x68, 0x6A, 0x6C, 0x6E, 0x70, 0x72, 0x74, 0x76,
    0x78, 0x7A, 0x7C, 0x7E, 0x80, 0x82, 0x84, 0x86, 0x88, 0x8A, 0x8C, 0x8E,
    0x90, 0x92, 0x94, 0x96, 0x98, 0x9A, 0x9C, 0x9E, 0xA0, 0xA2, 0xA4, 0xA6,
    0xA8, 0xAA, 0xAC, 0xAE, 0xB0, 0xB2, 0xB4, 0xB6, 0xB8, 0xBA, 0xBC, 0xBE,
    0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE, 0xD0, 0xD2, 0xD4, 0xD6,
    0xD8, 0xDA, 0xDC, 0xDE, 0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEC, 0xEE,
    0xF0, 0xF2, 0xF4, 0xF6, 0xF8, 0xFA, 0xFC, 0xFE, 0x1D, 0x1F, 0x19, 0x1B,
    0x15, 0x17, 0x11, 0x13, 0x0D, 0x0F, 0x09, 0x0B, 0x05, 0x07, 0x01, 0x03,
    0x3D, 0x3F, 0x39, 0x3B, 0x35, 0x37, 0x31, 0x33, 0x2D, 0x2F, 0x29, 0x2B,
    0x25, 0x27, 0x21, 0x23, 0x5D, 0x5F, 0x59, 0x5B, 0x55, 0x57, 0x51, 0x53,
    0x4D, 0x4F, 0x49, 0x4B, 0x45, 0x47, 0x41, 0x43, 0x7D, 0x7F, 0x79, 0x7B,
    0x75, 0x77, 0x71, 0x73, 0x6D, 0x6F, 0x69, 0x6B, 0x65, 0x67, 0x61, 0x63,
    0x9D, 0x9F, 0x99, 0x9B, 0x95, 0x97, 0x91, 0x93, 0x8D, 0x8F, 0x89, 0x8B,
    0x85, 0x87, 0x81, 0x83, 0xBD, 0xBF, 0xB9, 0xBB, 0xB5, 0xB7, 0xB1, 0xB3,
    0xAD, 0xAF, 0xA9, 0xAB, 0xA5, 0xA7, 0xA1, 0xA3, 0xDD, 0xDF, 0xD9, 0xDB,
    0xD5, 0xD7, 0xD1, 0xD3, 0xCD, 0xCF, 0xC9, 0xCB, 0xC5, 0xC7, 0xC1, 0xC3,
    0xFD, 0xFF, 0xF9, 0xFB, 0xF5, 0xF7, 0xF1, 0xF3, 0xED, 0xEF, 0xE9, 0xEB,
    0xE5, 0xE7, 0xE1, 0xE3
    };

/*Constants for reed-solomon generation*/
unsigned char ecc_b_lut[] = {
    0x00, 0xF4, 0xF5, 0x01, 0xF7, 0x03, 0x02, 0xF6, 0xF3, 0x07, 0x06, 0xF2,
    0x04, 0xF0, 0xF1, 0x05, 0xFB, 0x0F, 0x0E, 0xFA, 0x0C, 0xF8, 0xF9, 0x0D,
    0x08, 0xFC, 0xFD, 0x09, 0xFF, 0x0B, 0x0A, 0xFE, 0xEB, 0x1F, 0x1E, 0xEA,
    0x1C, 0xE8, 0xE9, 0x1D, 0x18, 0xEC, 0xED, 0x19, 0xEF, 0x1B, 0x1A, 0xEE,
    0x10, 0xE4, 0xE5, 0x11, 0xE7, 0x13, 0x12, 0xE6, 0xE3, 0x17, 0x16, 0xE2,
    0x14, 0xE0, 0xE1, 0x15, 0xCB, 0x3F, 0x3E, 0xCA, 0x3C, 0xC8, 0xC9, 0x3D,
    0x38, 0xCC, 0xCD, 0x39, 0xCF, 0x3B, 0x3A, 0xCE, 0x30, 0xC4, 0xC5, 0x31,
    0xC7, 0x33, 0x32, 0xC6, 0xC3, 0x37, 0x36, 0xC2, 0x34, 0xC0, 0xC1, 0x35,
    0x20, 0xD4, 0xD5, 0x21, 0xD7, 0x23, 0x22, 0xD6, 0xD3, 0x27, 0x26, 0xD2,
    0x24, 0xD0, 0xD1, 0x25, 0xDB, 0x2F, 0x2E, 0xDA, 0x2C, 0xD8, 0xD9, 0x2D,
    0x28, 0xDC, 0xDD, 0x29, 0xDF, 0x2B, 0x2A, 0xDE, 0x8B, 0x7F, 0x7E, 0x8A,
    0x7C, 0x88, 0x89, 0x7D, 0x78, 0x8C, 0x8D, 0x79, 0x8F, 0x7B, 0x7A, 0x8E,
    0x70, 0x84, 0x85, 0x71, 0x87, 0x73, 0x72, 0x86, 0x83, 0x77, 0x76, 0x82,
    0x74, 0x80, 0x81, 0x75, 0x60, 0x94, 0x95, 0x61, 0x97, 0x63, 0x62, 0x96,
    0x93, 0x67, 0x66, 0x92, 0x64, 0x90, 0x91, 0x65, 0x9B, 0x6F, 0x6E, 0x9A,
    0x6C, 0x98, 0x99, 0x6D, 0x68, 0x9C, 0x9D, 0x69, 0x9F, 0x6B, 0x6A, 0x9E,
    0x40, 0xB4, 0xB5, 0x41, 0xB7, 0x43, 0x42, 0xB6, 0xB3, 0x47, 0x46, 0xB2,
    0x44, 0xB0, 0xB1, 0x45, 0xBB, 0x4F, 0x4E, 0xBA, 0x4C, 0xB8, 0xB9, 0x4D,
    0x48, 0xBC, 0xBD, 0x49, 0xBF, 0x4B, 0x4A, 0xBE, 0xAB, 0x5F, 0x5E, 0xAA,
    0x5C, 0xA8, 0xA9, 0x5D, 0x58, 0xAC, 0xAD, 0x59, 0xAF, 0x5B, 0x5A, 0xAE,
    0x50, 0xA4, 0xA5, 0x51, 0xA7, 0x53, 0x52, 0xA6, 0xA3, 0x57, 0x56, 0xA2,
    0x54, 0xA0, 0xA1, 0x55
    };

/*constants for edc generation*/
unsigned int edc_lut[] = {
    0x00000000, 0x90910101, 0x91210201, 0x01B00300, 0x92410401, 0x02D00500,
    0x03600600, 0x93F10701, 0x94810801, 0x04100900, 0x05A00A00, 0x95310B01,
    0x06C00C00, 0x96510D01, 0x97E10E01, 0x07700F00, 0x99011001, 0x09901100,
    0x08201200, 0x98B11301, 0x0B401400, 0x9BD11501, 0x9A611601, 0x0AF01700,
    0x0D801800, 0x9D111901, 0x9CA11A01, 0x0C301B00, 0x9FC11C01, 0x0F501D00,
    0x0EE01E00, 0x9E711F01, 0x82012001, 0x12902100, 0x13202200, 0x83B12301,
    0x10402400, 0x80D12501, 0x81612601, 0x11F02700, 0x16802800, 0x86112901,
    0x87A12A01, 0x17302B00, 0x84C12C01, 0x14502D00, 0x15E02E00, 0x85712F01,
    0x1B003000, 0x8B913101, 0x8A213201, 0x1AB03300, 0x89413401, 0x19D03500,
    0x18603600, 0x88F13701, 0x8F813801, 0x1F103900, 0x1EA03A00, 0x8E313B01,
    0x1DC03C00, 0x8D513D01, 0x8CE13E01, 0x1C703F00, 0xB4014001, 0x24904100,
    0x25204200, 0xB5B14301, 0x26404400, 0xB6D14501, 0xB7614601, 0x27F04700,
    0x20804800, 0xB0114901, 0xB1A14A01, 0x21304B00, 0xB2C14C01, 0x22504D00,
    0x23E04E00, 0xB3714F01, 0x2D005000, 0xBD915101, 0xBC215201, 0x2CB05300,
    0xBF415401, 0x2FD05500, 0x2E605600, 0xBEF15701, 0xB9815801, 0x29105900,
    0x28A05A00, 0xB8315B01, 0x2BC05C00, 0xBB515D01, 0xBAE15E01, 0x2A705F00,
    0x36006000, 0xA6916101, 0xA7216201, 0x37B06300, 0xA4416401, 0x34D06500,
    0x35606600, 0xA5F16701, 0xA2816801, 0x32106900, 0x33A06A00, 0xA3316B01,
    0x30C06C00, 0xA0516D01, 0xA1E16E01, 0x31706F00, 0xAF017001, 0x3F907100,
    0x3E207200, 0xAEB17301, 0x3D407400, 0xADD17501, 0xAC617601, 0x3CF07700,
    0x3B807800, 0xAB117901, 0xAAA17A01, 0x3A307B00, 0xA9C17C01, 0x39507D00,
    0x38E07E00, 0xA8717F01, 0xD8018001, 0x48908100, 0x49208200, 0xD9B18301,
    0x4A408400, 0xDAD18501, 0xDB618601, 0x4BF08700, 0x4C808800, 0xDC118901,
    0xDDA18A01, 0x4D308B00, 0xDEC18C01, 0x4E508D00, 0x4FE08E00, 0xDF718F01,
    0x41009000, 0xD1919101, 0xD0219201, 0x40B09300, 0xD3419401, 0x43D09500,
    0x42609600, 0xD2F19701, 0xD5819801, 0x45109900, 0x44A09A00, 0xD4319B01,
    0x47C09C00, 0xD7519D01, 0xD6E19E01, 0x46709F00, 0x5A00A000, 0xCA91A101,
    0xCB21A201, 0x5BB0A300, 0xC841A401, 0x58D0A500, 0x5960A600, 0xC9F1A701,
    0xCE81A801, 0x5E10A900, 0x5FA0AA00, 0xCF31AB01, 0x5CC0AC00, 0xCC51AD01,
    0xCDE1AE01, 0x5D70AF00, 0xC301B001, 0x5390B100, 0x5220B200, 0xC2B1B301,
    0x5140B400, 0xC1D1B501, 0xC061B601, 0x50F0B700, 0x5780B800, 0xC711B901,
    0xC6A1BA01, 0x5630BB00, 0xC5C1BC01, 0x5550BD00, 0x54E0BE00, 0xC471BF01,
    0x6C00C000, 0xFC91C101, 0xFD21C201, 0x6DB0C300, 0xFE41C401, 0x6ED0C500,
    0x6F60C600, 0xFFF1C701, 0xF881C801, 0x6810C900, 0x69A0CA00, 0xF931CB01,
    0x6AC0CC00, 0xFA51CD01, 0xFBE1CE01, 0x6B70CF00, 0xF501D001, 0x6590D100,
    0x6420D200, 0xF4B1D301, 0x6740D400, 0xF7D1D501, 0xF661D601, 0x66F0D700,
    0x6180D800, 0xF111D901, 0xF0A1DA01, 0x6030DB00, 0xF3C1DC01, 0x6350DD00,
    0x62E0DE00, 0xF271DF01, 0xEE01E001, 0x7E90E100, 0x7F20E200, 0xEFB1E301,
    0x7C40E400, 0xECD1E501, 0xED61E601, 0x7DF0E700, 0x7A80E800, 0xEA11E901,
    0xEBA1EA01, 0x7B30EB00, 0xE8C1EC01, 0x7850ED00, 0x79E0EE00, 0xE971EF01,
    0x7700F000, 0xE791F101, 0xE621F201, 0x76B0F300, 0xE541F401, 0x75D0F500,
    0x7460F600, 0xE4F1F701, 0xE381F801, 0x7310F900, 0x72A0FA00, 0xE231FB01,
    0x71C0FC00, 0xE151FD01, 0xE0E1FE01, 0x7070FF00
    };


int chk_q(char*sector, char sector_type, FILE* def)
{
  char define[]={0x05};
  char copy[2352];
  memcpy(copy, sector, 2352);
  gen_q(copy, sector_type);

  if(memcmp(&copy[2248], &sector[2248], 104)!=0)
  {
    if(fwrite(define, 1, 1, def)!=1)
      abort_err(err_def_write);
    if(fwrite(&sector[2248], 1, 104, def)!=104)
      abort_err(err_def_write);
  }

  return EXIT_SUCCESS;
}

int chk_p(char*sector, char sector_type, FILE* def)
{
  char define[]={0x04};
  char copy[2352];
  memcpy(copy, sector, 2352);
  gen_p(copy, sector_type);

  if(memcmp(&copy[2076], &sector[2076], 172)!=0)
  {
    if(fwrite(define, 1, 1, def)!=1)
      abort_err(err_def_write);
    if(fwrite(&sector[2076], 1, 172, def)!=172)
      abort_err(err_def_write);
  }

  return EXIT_SUCCESS;
}

int chk_edc(char*sector, char sector_type, FILE* def)
{
  char define[]={0x03};
  char copy[2352];
  int edcloc;

  memcpy(copy, sector, 2352);
  gen_edc(copy, sector_type);

  edcloc=0;
  switch(sector_type)
  {
    case 2:
      edcloc=2064;
    break;
    case 4:
      edcloc=2072;
    break;
    case 5:
      edcloc=2348;
    break;
  }

  if(memcmp(&copy[edcloc], &sector[edcloc], 4)!=0)
  {
    if(fwrite(define, 1, 1, def)!=1)
      abort_err(err_def_write);
    if(fwrite(&sector[edcloc], 1, 4, def)!=4)
      abort_err(err_def_write);
  }

  return EXIT_SUCCESS;
}

int chk_zerofill(char*sector, char sector_type, FILE* def)
{
  char define[]={0x06};
  char copy[2352];
  memcpy(copy, sector, 2352);
  gen_zerofill(copy, sector_type);
  if(memcmp(&copy[2068], &sector[2068], 8)!=0)
  {
    if(fwrite(define, 1, 1, def)!=1)
      abort_err(err_def_write);
    if(fwrite(&sector[2068], 1, 8, def)!=8)
      abort_err(err_def_write);
  }

  return EXIT_SUCCESS;
}

int chk_subheader(char*sector, char sector_type, FILE* def)
{
  char define[]={0x02};
  char copy[2352];
  memcpy(copy, sector, 2352);
  gen_subheader(copy, sector_type);
  if(memcmp(&copy[16], &sector[16], 8)!=0)
  {
    if(fwrite(define, 1, 1, def)!=1)
      abort_err(err_def_write);
    if(fwrite(&sector[16], 1, 4, def)!=4)
      abort_err(err_def_write);    
  }
  return EXIT_SUCCESS;
}

int chk_mode(char*sector, char sector_type, FILE* def)
{
  char define[]={0x01};
  char copy[2352];
  memcpy(copy, sector, 2352);
  gen_mode(copy, sector_type);
  if(copy[15]!=sector[15])
  {
    if(fwrite(define, 1, 1, def)!=1)
      abort_err(err_def_write);
    if(fwrite(&sector[15], 1, 1, def)!=1)
      abort_err(err_def_write);
  }
  return EXIT_SUCCESS;
}

int chk_msf(char*sector, char sector_type, FILE* def, int lba)
{
  char define[]={0x00};
  char copy[2352];
  memcpy(copy, sector, 2352);
  gen_msf(copy, sector_type, lba);
  if(memcmp(&copy[12], &sector[12], 3)!=0)
  {
    if(fwrite(define, 1, 1, def)!=1)
      abort_err(err_def_write);
    if(fwrite(&sector[12], 1, 3, def)!=3)
      abort_err(err_def_write);
  }
  return EXIT_SUCCESS;
}

int lba_to_msf(int lba, char* out)
{
  int* tmp;
  int test;
  
  tmp=malloc(sizeof(int)*3);

  test=lba;
  tmp[0]=test/4500;
  test-=(tmp[0]*4500);
  tmp[1]=test/75;
  test-=(tmp[1]*75);
  tmp[2]=test;

  out[2] = ((((tmp[2]/10)<<4) + (tmp[2]%10)) & 0xFF);
  out[1] = ((((tmp[1]/10)<<4) + (tmp[1]%10)) & 0xFF);
  out[0] = ((((tmp[0]/10)<<4) + (tmp[0]%10)) & 0xFF);
  free(tmp);
  return EXIT_SUCCESS;
}

int msf_to_lba(char* msf)
{
  /*msf in bcd format*/
  int out=0;
  out+=((msf[0]&0x0f) + 10*((msf[0]&0xf0)>>4))*4500;
  out+=((msf[1]&0x0f) + 10*((msf[1]&0xf0)>>4))*75;
  out+=((msf[2]&0x0f) + 10*((msf[2]&0xf0)>>4));

  /*relax minute sector validation, someone could go out of spec*/
  if(((msf[1]&0xf0)>>4)>=6)
  {
    /*second is invalid*/
    return 1048576;
  }
  else if(((msf[2]&0xf0)>>4)>=8)
  {
    /*sub-second is invalid*/
    return 1048576;
  }
  else if(((msf[2]&0xf0)>>4)==7)
  {
    /*sub-second may be invalid*/
    if((msf[2]&0x0f)>=5)
    {
      /*sub-second is invalid*/
      return 1048576;
    }
  }
  return out;
}

int gen_cd_sync(char*sector, char sector_type)
{
    switch(sector_type)
    {
      case AUDIO:
        return EXIT_FAIL;
      break;

      case MODE0:
      break;

      case MODE1:
      break;

      case MODE2:
      break;

      case MODE2FORM1:
      break;

      case MODE2FORM2:
      break;

      default:
        return EXIT_FAIL;
    }
  memcpy(sector, cd_sync, 12);
  return EXIT_SUCCESS;
}

int gen_msf(char*sector, char sector_type, int lba)
{
  char msf[3];
  switch(sector_type)
  {
    case AUDIO:
      return EXIT_FAIL;
    break;

    case MODE0:
    break;

    case MODE1:
    break;

    case MODE2:
    break;

    case MODE2FORM1:
    break;

    case MODE2FORM2:
    break;

    default:
      return EXIT_FAIL;
  }
  lba_to_msf(lba, msf);
  memcpy(&sector[12], msf, 3);
  return EXIT_SUCCESS;
}

int gen_mode(char*sector, char sector_type)
{
    switch(sector_type)
    {
      case AUDIO:
        return EXIT_FAIL;
      break;

      case MODE0:
        sector[15]=0x00;
      break;

      case MODE1:
        sector[15]=0x01;
      break;

      case MODE2:
        sector[15]=0x02;
      break;

      case MODE2FORM1:
        sector[15]=0x02;
      break;

      case MODE2FORM2:
        sector[15]=0x02;
      break;

      default:
        return EXIT_FAIL;
    }
  return EXIT_SUCCESS;
}

int gen_subheader(char*sector, char sector_type)
{
    switch(sector_type)
    {
      case AUDIO:
        return EXIT_FAIL;
      break;

      case MODE0:
        return EXIT_FAIL;
      break;

      case MODE1:
        return EXIT_FAIL;
      break;

      case MODE2:
        return EXIT_FAIL;
      break;

      case MODE2FORM1:
      break;

      case MODE2FORM2:
      break;

      default:
        return EXIT_FAIL;
    }
  memcpy(&sector[16], subheader, 8);
  return EXIT_SUCCESS;
}

int gen_zerofill(char*sector, char sector_type)
{
  switch(sector_type)
  {
    case AUDIO:
      return EXIT_FAIL;
    break;

    case MODE0:
      return EXIT_FAIL;
    break;

    case MODE1:
    break;

    case MODE2:
      return EXIT_FAIL;
    break;

    case MODE2FORM1:
      return EXIT_FAIL;
    break;

    case MODE2FORM2:
      return EXIT_FAIL;
    break;

    default:
      return EXIT_FAIL;
  }
  memcpy(&sector[2068], zerofill, 8);
  return EXIT_SUCCESS;
}

int gen_edc(char*sector, char sector_type)
{
  int start, length, edcloc;
  unsigned int edc = 0;
  int i;
  char comp[4];

  switch(sector_type)
  {
    case AUDIO:
      return EXIT_FAIL;
    break;

    case MODE0:
      return EXIT_FAIL;
    break;

    case MODE1:
      start=0;
      length=2064;
      edcloc=2064;
    break;

    case MODE2:
      return EXIT_FAIL;
    break;

    case MODE2FORM1:
      start=16;
      length=2056;
      edcloc=2072;
    break;

    case MODE2FORM2:
      start=16;
      length=2332;
      edcloc=2348;
    break;

    default:
      return EXIT_FAIL;
  }


  i=0;  
  while(i<length)
  {
    edc= ((edc>>8)^edc_lut[(edc^(sector[start+i]))&0xFF]);
    ++i;
  }
                
  comp[0] = ((edc) & 0xFF);
  comp[1] = ((edc >>  8) & 0xFF);
  comp[2] = ((edc >> 16) & 0xFF);
  comp[3] = ((edc >> 24) & 0xFF);

  memcpy(&sector[edcloc], comp, 4);

  return EXIT_SUCCESS;
}

int gen_p(char*sector, char sector_type)
{
  char swap[4];

  unsigned int major_count=86;
  unsigned int minor_count=24;
  unsigned int major_mult=2;
  unsigned int minor_inc=86;
  int start=12;
  unsigned int size = major_count * minor_count;
  unsigned int major = 0, minor = 0;
  
  char tmp[172];

  unsigned int index;
  unsigned char ecc_a;
  unsigned char ecc_b;

  unsigned char temp;

  switch(sector_type)
  {
    case AUDIO:
      return EXIT_FAIL;
    break;

    case MODE0:
      return EXIT_FAIL;
    break;

    case MODE1:
      memcpy(swap, &sector[12], 4);
    break;

    case MODE2:
      return EXIT_FAIL;
    break;

    case MODE2FORM1:
      memcpy(swap, &sector[12], 4);
      memset(&sector[12], 0, 4);
    break;

    case MODE2FORM2:
      return EXIT_FAIL;
    break;

    default:
      return EXIT_FAIL;
  }


  
  major=0;
  while(major<major_count)
  {
    index = (major >> 1) * major_mult + (major & 1);
    ecc_a = 0;
    ecc_b = 0;
    minor=0;
    while(minor < minor_count)
    {
      temp = (0xFF & sector[ start+index ]);
      index += minor_inc;
      if (index >= size) index -= size;
      ecc_a ^= temp;
      ecc_b ^= temp;
      ecc_a =ecc_f_lut[ecc_a];

      ++minor;
    }
    ecc_a = (ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b]);
    tmp[ major ] = ecc_a;
    tmp[ major+major_count ] = (ecc_a ^ ecc_b);
    ++major;
  }
  
  memcpy(&sector[2076], tmp, 172);
  memcpy(&sector[12], swap, 4);

  return EXIT_SUCCESS;
}

int gen_q(char*sector, char sector_type)
{
  char swap[4];

  unsigned int major_count=52;
  unsigned int minor_count=43;
  unsigned int major_mult=86;
  unsigned int minor_inc=88;
  int start=12;
  unsigned int size = major_count * minor_count;
  unsigned int major = 0, minor = 0;
  
  char tmp[104];

  unsigned int index;
  unsigned char ecc_a;
  unsigned char ecc_b;

  unsigned char temp;


  switch(sector_type)
  {
    case AUDIO:
      return EXIT_FAIL;
    break;

    case MODE0:
      return EXIT_FAIL;
    break;

    case MODE1:
      memcpy(swap, &sector[12], 4);
    break;

    case MODE2:
      return EXIT_FAIL;
    break;

    case MODE2FORM1:
      memcpy(swap, &sector[12], 4);
      memset(&sector[12], 0, 4);
    break;

    case MODE2FORM2:
      return EXIT_FAIL;
    break;

    default:
      return EXIT_FAIL;
  }


  
  major=0;
  while(major<major_count)
  {
    index = (major >> 1) * major_mult + (major & 1);
    ecc_a = 0;
    ecc_b = 0;
    minor=0;
    while(minor < minor_count)
    {
      temp = (0xFF & sector[ start+index ]);
      index += minor_inc;
      if (index >= size) index -= size;
      ecc_a ^= temp;
      ecc_b ^= temp;
      ecc_a =ecc_f_lut[ecc_a];

      ++minor;
    }
    ecc_a = (ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b]);
    tmp[ major ] = ecc_a;
    tmp[ major+major_count ] = (ecc_a ^ ecc_b);
    ++major;
  }
  
  memcpy(&sector[2248], tmp, 104);
  memcpy(&sector[12], swap, 4);

  return EXIT_SUCCESS;
}

int unsplit(FILE* def, FILE* sec_2048, FILE* sec_2324, FILE* sec_2336, FILE* sec_2352, FILE* out, char* md5_out, char* sha1_out, unsigned int num_2048, unsigned int num_2324, unsigned int num_2336, unsigned int num_2352, char test_bool, int curr_img, int tot_img)
{
#ifdef TIMERS
  time_t t_start, t_end;
  double time_taken;
#endif

  char buf_msf[3+1];
  char buf_mode[1+1];
  char buf_subheader[4+1];
  char buf_edc[4+1];
  char buf_zerofill[8+1];

  char* buf_p=NULL;
  char* buf_q=NULL;

  char* sec_buf=NULL;

  char* buf_2048=NULL;
  char* buf_2324=NULL;
  char* buf_2336=NULL;
  char* buf_2352=NULL;
  char* buf_out=NULL;

  unsigned int buf_2048_loc=0;
  unsigned int buf_2324_loc=0;
  unsigned int buf_2336_loc=0;
  unsigned int buf_2352_loc=0;

  unsigned int num_2048_left = num_2048;
  unsigned int num_2324_left = num_2324;
  unsigned int num_2336_left = num_2336;
  unsigned int num_2352_left = num_2352;

  unsigned int num_sectors;

  char def_buf[1];

  int i;
  unsigned int j=0;

  char handled[7];
  char sector_type;

  int current_sector=0;
  int parsing_sector;
  unsigned char md5_hash[16];
  unsigned char sha1_hash[20];
  char hex[]={'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

#ifdef CHANGE_TITLE
  char* title=NULL;
  int perc_complete=0;
  int one_perc;
#endif

#ifdef POLAR_SSL
  md5_context* ctx;
  sha1_context* s_ctx;
#endif

  num_sectors=num_2048+num_2324+num_2336+num_2352;

  buf_2048=malloc(2048*4096);
  buf_2324=malloc(2324*4096);
  buf_2336=malloc(2336*4096);
  buf_2352=malloc(2352*4096);
  buf_out=malloc(2352*4096);

  buf_p=malloc(173);
  buf_q=malloc(105);

#ifdef CHANGE_TITLE
  one_perc=num_sectors/100;
  if(one_perc==0)
    ++one_perc;

  title=malloc(2048);

  sprintf(title, "%u / %u : %u %%", curr_img, tot_img, perc_complete);
  SetConsoleTitle(title);
#endif

#ifdef TIMERS
  time(&t_start);
#endif

  memset(buf_msf, 0, 3+1);
  memset(buf_mode, 0, 1+1);
  memset(buf_subheader, 0, 4+1);
  memset(buf_edc, 0, 4+1);
  memset(buf_p, 0, 172+1);
  memset(buf_q, 0, 104+1);
  memset(buf_zerofill, 0, 8+1);

#ifdef POLAR_SSL
  ctx=malloc( sizeof(md5_context) );
  md5_starts(ctx);

  s_ctx=malloc( sizeof(sha1_context) );
  sha1_starts(s_ctx);
#endif

  /*initialise buffers*/
  if(num_2048_left!=0)
  {
    if( fread( buf_2048, 1, 2048*(num_2048_left>=4096?4096:num_2048_left), sec_2048)!=2048*(num_2048_left>=4096?4096:num_2048_left) )
      abort_err("ERROR: Failed to initialise 2048 buffer");
    num_2048_left-=(num_2048_left>=4096?4096:num_2048_left);
  }
  if(num_2324_left!=0)
  {
    if( fread( buf_2324, 1, 2324*(num_2324_left>=4096?4096:num_2324_left), sec_2324)!=2324*(num_2324_left>=4096?4096:num_2324_left) )
      abort_err("ERROR: Failed to initialise 2324 buffer");
    num_2324_left-=(num_2324_left>=4096?4096:num_2324_left);
  }
  if(num_2336_left!=0)
  {
    if( fread( buf_2336, 1, 2336*(num_2336_left>=4096?4096:num_2336_left), sec_2336)!=2336*(num_2336_left>=4096?4096:num_2336_left) )
      abort_err("ERROR: Failed to initialise 2336 buffer");
    num_2336_left-=(num_2336_left>=4096?4096:num_2336_left);
  }
  if(num_2352_left!=0)
  {
    if( fread( buf_2352, 1, 2352*(num_2352_left>=4096?4096:num_2352_left), sec_2352)!=2352*(num_2352_left>=4096?4096:num_2352_left) )
      abort_err("ERROR: Failed to initialise 2352 buffer");
    num_2352_left-=(num_2352_left>=4096?4096:num_2352_left);
  }

  while( j<num_sectors )/*while we haven't met the end*/
  {
    sec_buf=&buf_out[2352*(j%4096)];

#ifdef CHANGE_TITLE
    if( (j+1)%one_perc==0)
    {
      ++perc_complete;
      sprintf(title, "%u / %u : %u %%", curr_img, tot_img, perc_complete);
      SetConsoleTitle(title);
    }
#endif

    /*reset everything for every sector*/
    memset(handled, 0, 7);
    memset(sec_buf, 0, 2352);
    memset(buf_msf, 0, 3);
    memset(buf_mode, 0, 1);
    memset(buf_subheader, 0, 4);
    memset(buf_edc, 0, 4);
    memset(buf_p, 0, 172);
    memset(buf_q, 0, 104);
    memset(buf_zerofill, 0, 8);

    if(fread(&def_buf[0], 1, 1, def)!=1)
      abort_err("ERROR: Failed to read from .def file");

    /*puts("handled[] reset");*/
    parsing_sector=1;
    while(parsing_sector==1)
    {
      switch(def_buf[0])
      {
        case 0x00:/*msf*/
          if(fread(&buf_msf[0], 1, 3, def)!=3)
            abort_err(err_fread);
          if(fread(&def_buf[0], 1, 1, def)!=1)
            abort_err(err_fread);
          handled[0]=1;
        break;

        case 0x01:/*mode*/
          if(fread(&buf_mode[0], 1, 1, def)!=1)
            abort_err(err_fread);
          if(fread(&def_buf[0], 1, 1, def)!=1)
            abort_err(err_fread);
          handled[1]=1;
        break;

        case 0x02:/*subheader*/
          if(fread(&buf_subheader[0], 1, 4, def)!=4)
            abort_err(err_fread);
          if(fread(&def_buf[0], 1, 1, def)!=1)
            abort_err(err_fread);
          handled[2]=1;
        break;

        case 0x03:/*edc*/
          if(fread(&buf_edc[0], 1, 4, def)!=4)
            abort_err(err_fread);
          if(fread(&def_buf[0], 1, 1, def)!=1)
            abort_err(err_fread);
          handled[3]=1;
        break;

        case 0x04:/*p*/
          if(fread(&buf_p[0], 1, 172, def)!=172)
            abort_err(err_fread);
          if(fread(&def_buf[0], 1, 1, def)!=1)
            abort_err(err_fread);
          handled[4]=1;
        break;

        case 0x05:/*q*/
          if(fread(&buf_q[0], 1, 104, def)!=104)
            abort_err(err_fread);
          if(fread(&def_buf[0], 1, 1, def)!=1)
            abort_err(err_fread);
          handled[5]=1;
        break;

        case 0x06:/*zero fill*/
          if(fread(&buf_zerofill[0], 1, 8, def)!=8)
            abort_err(err_fread);
          if(fread(&def_buf[0], 1, 1, def)!=1)
            abort_err(err_fread);
          handled[6]=1;
        break;

        case 0x10:
          sector_type=AUDIO;
          if(sec_2352==NULL)
            abort_err("ERROR: Def file defines a 2352 byte data chunk, but no such data store exists");
          parsing_sector=0;
        break;

        case 0x11:
          sector_type=MODE0;
          if(sec_2336==NULL)
            abort_err("ERROR: Def file defines a 2336 byte data chunk, but no such data store exists");
          parsing_sector=0;
        break;

        case 0x12:
          sector_type=MODE1;
          if(sec_2048==NULL)
            abort_err("ERROR: Def file defines a 2048 byte data chunk, but no such data store exists");
          parsing_sector=0;
        break;

        case 0x13:
          sector_type=MODE2;
          if(sec_2336==NULL)
            abort_err("ERROR: Def file defines a 2336 byte data chunk, but no such data store exists");
          parsing_sector=0;
        break;

        case 0x14:
          sector_type=MODE2FORM1;
          if(sec_2048==NULL)
            abort_err("ERROR: Def file defines a 2048 byte data chunk, but no such data store exists");
          parsing_sector=0;
        break;

        case 0x15:
          sector_type=MODE2FORM2;
          if(sec_2324==NULL)
            abort_err("ERROR: Def file defines a 2324 byte data chunk, but no such data store exists");
          parsing_sector=0;
        break;

        default:
          abort_err("ERROR: Undefined symbol in def file");
      }
    }
    /*puts("def for sector parsed");*/
    /*parsed a sector, now gather it up and write it*/

    switch(sector_type)
    {
      case AUDIO:
        memcpy(sec_buf, &buf_2352[buf_2352_loc*2352], 2352);/*copy from buffer*/
        ++buf_2352_loc;
        if(buf_2352_loc==4096)
        {
          if(num_2352_left!=0)
          {
            buf_2352_loc=0;
            if( fread( buf_2352, 1, 2352*(num_2352_left>=4096?4096:num_2352_left), sec_2352)!=2352*(num_2352_left>=4096?4096:num_2352_left) )
              abort_err("ERROR: Failed to read audio sectors");
            num_2352_left-=(num_2352_left>=4096?4096:num_2352_left);
          }
        }
      break;

      case MODE0:/*cd_sync msf mode data*/
        if( gen_cd_sync(sec_buf, sector_type)!=EXIT_SUCCESS )
          abort_err("ERROR: Failed to generate cd_sync for mode 0 sector");
        if(handled[0]==1)
          memcpy(&sec_buf[12], buf_msf, 3);
        else
        {
          if( gen_msf(sec_buf, sector_type, current_sector+150)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate MSF for mode 0 sector");
        }
        if(handled[1]==1)
          memcpy(&sec_buf[15], buf_mode, 1);
        else
        {
          if( gen_mode(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate mode for mode 0 sector");
        }
        memcpy(&sec_buf[16], &buf_2336[buf_2336_loc*2336], 2336);
        ++buf_2336_loc;
        if(buf_2336_loc==4096)
        {
          if(num_2336_left!=0)
          {
            buf_2336_loc=0;
            if( fread( buf_2336, 1, 2336*(num_2336_left>=4096?4096:num_2336_left), sec_2336)!=2336*(num_2336_left>=4096?4096:num_2336_left) )
              abort_err("ERROR: Failed to read data for mode 0 sector");
            num_2336_left-=(num_2336_left>=4096?4096:num_2336_left);
          }
        }
      break;

      case MODE1:/*cd_sync msf mode data zerofill edc p q*/
        if( gen_cd_sync(sec_buf, sector_type)!=EXIT_SUCCESS )
          abort_err("ERROR: Failed to generate cd_sync for mode 1 sector");
        if(handled[0]==1)
          memcpy(&sec_buf[12], buf_msf, 3);
        else
        {
          if( gen_msf(sec_buf, sector_type, current_sector+150)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate MSF for mode 1 sector");
        }
        if(handled[1]==1)
          memcpy(&sec_buf[15], buf_mode, 1);
        else
        {
          if( gen_mode(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate mode for mode 1 sector");
        }
        memcpy(&sec_buf[16], &buf_2048[buf_2048_loc*2048], 2048);
        ++buf_2048_loc;
        if(buf_2048_loc==4096)
        {
          if(num_2048_left!=0)
          {
            buf_2048_loc=0;
            if( fread( buf_2048, 1, 2048*(num_2048_left>=4096?4096:num_2048_left), sec_2048)!=2048*(num_2048_left>=4096?4096:num_2048_left) )
              abort_err("ERROR: Failed to read data for mode 1 sector");
            num_2048_left-=(num_2048_left>=4096?4096:num_2048_left);
          }
        }
        if(handled[6]==1)
          memcpy(&sec_buf[2068], buf_zerofill, 8);
        else
        {
          if( gen_zerofill(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate zerofill for mode 1 sector");
        }
        if(handled[3]==1)
          memcpy(&sec_buf[2064], buf_edc, 4);
        else
        {
          if( gen_edc(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate edc for mode 1 sector");
        }
        if(handled[4]==1)
          memcpy(&sec_buf[2076], buf_p, 172);
        else
        {
          if( gen_p(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate edc for mode 1 sector");
        }
        if(handled[5]==1)
          memcpy(&sec_buf[2248], buf_q, 104);
        else
        {
          if( gen_q(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate edc for mode 1 sector");
        }
      break;

      case MODE2:/*cd_sync msf mode data*/
        if( gen_cd_sync(sec_buf, sector_type)!=EXIT_SUCCESS )
          abort_err("ERROR: Failed to generate cd_sync for mode 2 sector");
        if(handled[0]==1)
          memcpy(&sec_buf[12], buf_msf, 3);
        else
        {
          if( gen_msf(sec_buf, sector_type, current_sector+150)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate MSF for mode 2 sector");
        }
        if(handled[1]==1)
          memcpy(&sec_buf[15], buf_mode, 1);
        else
        {
          if( gen_mode(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate mode for mode 2 sector");
        }
        memcpy(&sec_buf[16], &buf_2336[buf_2336_loc*2336], 2336);
        ++buf_2336_loc;
        if(buf_2336_loc==4096)
        {
          if(num_2336_left!=0)
          {
            buf_2336_loc=0;
            if( fread( buf_2336, 1, 2336*(num_2336_left>=4096?4096:num_2336_left), sec_2336)!=2336*(num_2336_left>=4096?4096:num_2336_left) )
              abort_err("ERROR: Failed to read data for mode 2 sector");
            num_2336_left-=(num_2336_left>=4096?4096:num_2336_left);
          }
        }
      break;

      case MODE2FORM1:/*cd_sync msf mode subheader data edc p q*/
        if( gen_cd_sync(sec_buf, sector_type)!=EXIT_SUCCESS )
          abort_err("ERROR: Failed to generate cd_sync for mode 2 form 1 sector");
        if(handled[0]==1)
          memcpy(&sec_buf[12], buf_msf, 3);
        else
        {
          if( gen_msf(sec_buf, sector_type, current_sector+150)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate MSF for mode 2 form 1 sector");
        }
        if(handled[1]==1)
          memcpy(&sec_buf[15], buf_mode, 1);
        else
        {
          if( gen_mode(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate mode for mode 2 form 1 sector");
        }
        memcpy(&sec_buf[24], &buf_2048[buf_2048_loc*2048], 2048);
        ++buf_2048_loc;
        if(buf_2048_loc==4096)
        {
          if(num_2048_left!=0)
          {
            buf_2048_loc=0;
            if( fread( buf_2048, 1, 2048*(num_2048_left>=4096?4096:num_2048_left), sec_2048)!=2048*(num_2048_left>=4096?4096:num_2048_left) )
              abort_err("ERROR: Failed to read data for mode 2 form 1 sector");
            num_2048_left-=(num_2048_left>=4096?4096:num_2048_left);
          }
        }
        if(handled[2]==1)
        {
          memcpy(&sec_buf[16], buf_subheader, 4);
          memcpy(&sec_buf[20], buf_subheader, 4);
        }
        else
        {
          if( gen_subheader(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate subheader for mode 2 form 1 sector");
        }
        if(handled[3]==1)
          memcpy(&sec_buf[2072], buf_edc, 4);
        else
        {
          if( gen_edc(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate edc for mode 2 form 1 sector");
        }
        if(handled[4]==1)
          memcpy(&sec_buf[2076], buf_p, 172);
        else
        {
          if( gen_p(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate edc for mode 2 form 1 sector");
        }
        if(handled[5]==1)
          memcpy(&sec_buf[2248], buf_q, 104);
        else
        {
          if( gen_q(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate edc for mode 2 form 1 sector");
        }
      break;

      case MODE2FORM2:/*cd_sync msf mode data subheader edc*/
        if( gen_cd_sync(sec_buf, sector_type)!=EXIT_SUCCESS )
          abort_err("ERROR: Failed to generate cd_sync for mode 2 form 2 sector");
        if(handled[0]==1)
          memcpy(&sec_buf[12], buf_msf, 3);
        else
        {
          if( gen_msf(sec_buf, sector_type, current_sector+150)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate MSF for mode 2 form 2 sector");
        }
        if(handled[1]==1)
          memcpy(&sec_buf[15], buf_mode, 1);
        else
        {
          if( gen_mode(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate mode for mode 2 form 2 sector");
        }
        memcpy(&sec_buf[24], &buf_2324[buf_2324_loc*2324], 2324);
        ++buf_2324_loc;
        if(buf_2324_loc==4096)
        {
          if(num_2324_left!=0)
          {
            buf_2324_loc=0;
            if( fread( buf_2324, 1, 2324*(num_2324_left>=4096?4096:num_2324_left), sec_2324)!=2324*(num_2324_left>=4096?4096:num_2324_left) )
              abort_err("ERROR: Failed to read data for mode 2 form 2 sector");
            num_2324_left-=(num_2324_left>=4096?4096:num_2324_left);
          }
        }
        if(handled[2]==1)
        {
          memcpy(&sec_buf[16], buf_subheader, 4);
          memcpy(&sec_buf[20], buf_subheader, 4);
        }
        else
        {
          if( gen_subheader(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate subheader for mode 2 form 2 sector");
        }
        if(handled[3]==1)
          memcpy(&sec_buf[2348], buf_edc, 4);
        else
        {
          if( gen_edc(sec_buf, sector_type)!=EXIT_SUCCESS )
            abort_err("ERROR: Failed to generate edc for mode 2 form 2 sector");
        }
      break;

      default:
        return EXIT_FAIL;
    }

    ++current_sector;
    ++j;

    if(j%4096==0 || j==num_sectors)
    {
      if(test_bool!=EXIT_SUCCESS)
      {
        if( fwrite(buf_out, 1, 2352*(j%4096==0?4096:j%4096), out)!=2352*(j%4096==0?4096:j%4096) )
          abort_err("ERROR: Could not write to image file");
      }

#ifdef POLAR_SSL
      md5_update(ctx, (unsigned char*) buf_out, 2352*(j%4096==0?4096:j%4096) );
      sha1_update(s_ctx, (unsigned char*) buf_out, 2352*(j%4096==0?4096:j%4096) );
#endif
    }

  }

#ifdef POLAR_SSL
  md5_finish(ctx, md5_hash);
  sha1_finish(s_ctx, sha1_hash);
#endif

  memset(md5_out, 0, 33);  
  i=0;
  while(i<16)
  {
    md5_out[(i*2)  ]=hex[ (md5_hash[i]>>4) & 0x0F ];
    md5_out[(i*2)+1]=hex[ (md5_hash[i]   ) & 0x0F ];
    ++i;
  }

  memset(sha1_out, 0, 41);
  i=0;
  while(i<20)
  {
    sha1_out[(i*2)  ]=hex[ (sha1_hash[i]>>4) & 0x0F ];
    sha1_out[(i*2)+1]=hex[ (sha1_hash[i]   ) & 0x0F ];
    ++i;
  }

#ifdef POLAR_SSL
  free(ctx);
  free(s_ctx);
#endif

#ifdef CHANGE_TITLE
  free(title);
#endif

  free(buf_p);
  free(buf_q);

  free(buf_2048);
  free(buf_2324);
  free(buf_2336);
  free(buf_2352);
  free(buf_out);

#ifdef TIMERS
  time(&t_end);
  time_taken = difftime(t_end, t_start);
  fprintf(stderr, "%.2lf seconds\n", time_taken);
#endif

  return EXIT_SUCCESS;
}

int def_skip_n_sectors(FILE* def, unsigned int n)
{
  unsigned int i=0;
  char* burn=NULL;
  int parsing_sector;
  char def_buf;

  burn=malloc(200);

  while(i<n)
  {
    if(fread(&def_buf, 1, 1, def)!=1)
      abort_err(err_fread);

    parsing_sector=1;
    while(parsing_sector==1)
    {
      switch(def_buf)
      {
        case 0x00:
          if(fread(&burn[0], 1, 3, def)!=3)
            abort_err(err_fread);
          if(fread(&def_buf, 1, 1, def)!=1)
            abort_err(err_fread);
        break;

        case 0x01:
          if(fread(&burn[0], 1, 1, def)!=1)
            abort_err(err_fread);
          if(fread(&def_buf, 1, 1, def)!=1)
            abort_err(err_fread);
        break;

        case 0x02:
          if(fread(&burn[0], 1, 4, def)!=4)
            abort_err(err_fread);
          if(fread(&def_buf, 1, 1, def)!=1)
            abort_err(err_fread);
        break;

        case 0x03:
          if(fread(&burn[0], 1, 4, def)!=4)
            abort_err(err_fread);
          if(fread(&def_buf, 1, 1, def)!=1)
            abort_err(err_fread);
        break;

        case 0x04:
          if(fread(&burn[0], 1, 172, def)!=172)
            abort_err(err_fread);
          if(fread(&def_buf, 1, 1, def)!=1)
            abort_err(err_fread);
        break;

        case 0x05:
          if(fread(&burn[0], 1, 104, def)!=104)
            abort_err(err_fread);
          if(fread(&def_buf, 1, 1, def)!=1)
            abort_err(err_fread);
        break;

        case 0x06:
          if(fread(&burn[0], 1, 8, def)!=8)
            abort_err(err_fread);
          if(fread(&def_buf, 1, 1, def)!=1)
            abort_err(err_fread);
        break;

        case 0x10:
          parsing_sector=0;
        break;

        case 0x11:
          parsing_sector=0;
        break;

        case 0x12:
          parsing_sector=0;
        break;

        case 0x13:
          parsing_sector=0;
        break;

        case 0x14:
          parsing_sector=0;
        break;

        case 0x15:
          parsing_sector=0;
        break;

        default:
          abort_err("ERROR: Undefined symbol in def file");
      }
    }
    ++i;
  }

  free(burn);
  return EXIT_SUCCESS;
}

int bucket_skip_n_sectors(FILE* bucket, unsigned int sector_size, unsigned int sector_count)
{
  off_t skip=sector_size;
  if(sector_count==0)
    return EXIT_SUCCESS;
  skip*=sector_count;
  if( fseek(bucket, skip, SEEK_CUR)==0 )
    return EXIT_SUCCESS;
  else
    return EXIT_FAIL;
}

int uncat(FILE* in, int sector_size, FILE* out, char* md5_out, char* sha1_out, unsigned int num_sectors, char test_bool, int curr_img, int tot_img, int last_sec_used_size)
{
#ifdef TIMERS
  time_t t_start, t_end;
  double time_taken;
#endif

  char* sec_buf=NULL;
  unsigned int i=0;

#ifdef POLAR_SSL
  md5_context* ctx;
  sha1_context* s_ctx;
  char md5_hash[16];
  char sha1_hash[20];
#endif

  char hex[]={'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

#ifdef CHANGE_TITLE
  int perc_complete=0;
  int one_perc=num_sectors/100;

  char* title=NULL;

  if(one_perc==0)
    ++one_perc;

  title=malloc(2048);

  sprintf(title, "%u / %u : %u %%", curr_img, tot_img, perc_complete);
  SetConsoleTitle(title);
#endif

  sec_buf=malloc(sector_size);

#ifdef TIMERS
  time(&t_start);
#endif

#ifdef POLAR_SSL
  ctx=malloc( sizeof(md5_context) );
  md5_starts(ctx);

  s_ctx=malloc( sizeof(sha1_context) );
  sha1_starts(s_ctx);
#endif

  while(i<(num_sectors-1))
  {

#ifdef CHANGE_TITLE
    if(one_perc!=0)
    {
      if( (i+1)%one_perc==0)
      {
        ++perc_complete;
        sprintf(title, "%u / %u : %u %%", curr_img, tot_img, perc_complete);
        SetConsoleTitle(title);
      }
    }
#endif

    if( fread(sec_buf, 1, sector_size, in)!=sector_size )
      abort_err("ERROR: Failed to read sector from cat file");
    if(test_bool!=EXIT_SUCCESS)
    {
      if( fwrite(sec_buf, 1, sector_size, out)!=sector_size )
        abort_err("ERROR: Failed to write sector to embedded file");
    }

#ifdef POLAR_SSL
    md5_update(ctx, (unsigned char*) sec_buf, sector_size);
    sha1_update(s_ctx, (unsigned char*) sec_buf, sector_size);
#endif
    ++i;
  }
#ifdef CHANGE_TITLE
  if(one_perc!=0)
  {
    if( (i+1)%one_perc==0)
    {
      ++perc_complete;
      sprintf(title, "%u / %u : %u %%", curr_img, tot_img, perc_complete);
      SetConsoleTitle(title);
    }
  }
#endif

  if( fread(sec_buf, 1, sector_size, in)!=sector_size )
    abort_err("ERROR: Failed to read sector from cat file [2]");
  if(test_bool!=EXIT_SUCCESS)
  {
    if( fwrite(sec_buf, 1, last_sec_used_size, out)!=last_sec_used_size )
      abort_err("ERROR: Failed to write sector to embedded file");
  }

#ifdef POLAR_SSL
  md5_update(ctx, (unsigned char*) sec_buf, last_sec_used_size);
  sha1_update(s_ctx, (unsigned char*) sec_buf, last_sec_used_size);
#endif

#ifdef POLAR_SSL
  md5_finish(ctx, (unsigned char*)md5_hash);
  sha1_finish(s_ctx, (unsigned char*)sha1_hash);
#endif

  memset(md5_out, 0, 33);  
  i=0;
  while(i<16)
  {
    md5_out[(i*2)  ]=hex[ (md5_hash[i]>>4) & 0x0F ];
    md5_out[(i*2)+1]=hex[ (md5_hash[i]   ) & 0x0F ];
    ++i;
  }

  memset(sha1_out, 0, 41);
  i=0;
  while(i<20)
  {
    sha1_out[(i*2)  ]=hex[ (sha1_hash[i]>>4) & 0x0F ];
    sha1_out[(i*2)+1]=hex[ (sha1_hash[i]   ) & 0x0F ];
    ++i;
  }

#ifdef POLAR_SSL
  free(ctx);
  free(s_ctx);
#endif

#ifdef TIMERS
  time(&t_end);
  time_taken = difftime(t_end, t_start);
  fprintf(stderr, "%.2lf seconds\n", time_taken);
#endif

  free(sec_buf);

#ifdef CHANGE_TITLE
  free(title);
#endif

  return EXIT_SUCCESS;
}







