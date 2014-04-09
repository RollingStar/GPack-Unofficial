#define _FILE_OFFSET_BITS 64
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
//#include "dirent.h"

#if defined(_WIN32)
#define WINDOWS
#include <Windows.h>
#include <ShellAPI.h>
#else
#define LINUX
#include <unistd.h>
#endif

//#define POLAR_SSL

#ifdef POLAR_SSL
#include <polarssl/md5.h>
#include <polarssl/sha1.h>
#endif

#define PATH_SIZE_LIMIT 10000
#define NAME_SIZE_LIMIT (256)
#define EXT_CMD_LIMIT 100000
#define HSA_SIZE_LIMIT 1000000
#define ROM_SIZE (72+NAME_SIZE_LIMIT+1+4)

#define MAX_ELEMENTS_FROM_GRP 100000

int rev=9;

char* err_fread="ERROR: fread seemed to fail";
char* err_fopen="ERROR: fopen seemed to fail";
char* err_fwrite="ERROR: fwrite seemed to fail";
char* err_def_write="ERROR: Failed to write to def file";
char* err_mem_alloc="ERROR: Failed to allocate memory";
char* err_cat_write="ERROR: Failed to open cat file for writing";
char* err_ini_incomplete="ERROR: options.ini does not define everything it should";

void help();
int str_ends_with(const char * str, const char * suffix);

void abort_err(char* desc);
void abort_err_str(char* desc, char*var);
void abort_err_int(char* desc, int var);

int read_dat_get(char* dat_path, char ret_cue, char*ret, unsigned int count);
int read_dat_count(char* dat_path, char ret_cue, unsigned int* num_elements);
unsigned int read_grp_basic(char* grp, char* out, int max_elements, int grp_index);

int compare_by_hash(const void * a, const void * b);
int compare_by_name(const void * a, const void * b);
int compare_by_hash_verbose(const void * a, const void * b);
int compare_by_name_verbose(const void * a, const void * b);

void list_files_with_ext2(char* dir_n, char* ext, char recurse, char* list, int* curr_loc);
int count_files_with_ext(char* dir_n, char* ext, char recurse);

void* malloc_chk(size_t size)
{
  void* ptr=malloc(size);
  if(ptr==NULL)
    abort_err(err_mem_alloc);
  return ptr;
}
#define malloc(x) malloc_chk(x)

void help()
{
  puts("");
  puts("Dat program to check GPack merged files against a standard dat");
  puts("");
  puts("Usage: GPackDatter [ops] dat dir_containing_merged_files");
  puts("");
  puts("[ops]");
  puts("Quick mode: --mode=quick (default)");
  puts("            Use roms from .grp without checking integrity of the merged files");
  /*puts("Full/test mode : --mode=full (slow)");
  puts("            Use roms from .grp if merged files pass integrity checks");*/
  puts("Print fully matching roms: --print-fullmatch=yes/no (default yes)");
  puts("Print unmatched roms: --print-unmatched=yes/no (default yes)");
  puts("Print name matched roms: --print-namematch=yes/no (default yes)");
  puts("Print hash matched roms: --print-hashmatch=yes/no (default yes)");
  puts("Print summary: --print-summary=yes/no (default yes)");
  puts("Recursively search dir containing grp files: --recurse=yes/no (default yes)");
  puts("");
  puts("Redirect console output to file with > for log");
  puts("");
}

#define EXIT_SUCCESS 0/*has to be 0*/
#define EXIT_FAIL 1/*has to be positive*/
int main(int argc, char**argv)
{
  /*Dat variables*/
  char* dat_read_in=NULL;
  unsigned int dat_read_in_count=0;
  char** dat_by_name=NULL;
  char** dat_by_hash=NULL;
  unsigned int i=0;
  int j;
  FILE*out=NULL;
  char* grp_read_in=NULL;
  unsigned int grp_read_in_count=0;
  char* grp_file_list=NULL;
  unsigned int grp_file_count=0;
  int grp_file_list_loc;

  char** grp_data=NULL;//array holding contents of all grp files
  char** curr_rom;

  char* file_path=NULL;
  FILE* grp_file_reader=NULL;

  unsigned int count_unmatched=0;
  unsigned int count_fullymatched=0;
  unsigned int count_namematch=0;
  unsigned int count_hashmatch=0;
  unsigned int count_ignore=0;
  unsigned int count_in_use=0;

  char print_fullymatched=EXIT_SUCCESS;
  char print_unmatched=EXIT_SUCCESS;
  char print_hashmatch=EXIT_SUCCESS;
  char print_namematch=EXIT_SUCCESS;
  char print_summary=EXIT_SUCCESS;
  char ignore_cue=EXIT_SUCCESS;

  char include_grp=EXIT_SUCCESS;
  char recurse=EXIT_SUCCESS;

#define EXEC_MODE_QUICK 0
#define EXEC_MODE_FULLTEST 1

  char execution_mode=EXEC_MODE_QUICK;

  char* key;
  char** key_ptr;

  printf("\nGPackDatter r%d\n\n", rev);

  if(argc<3)
  {
    help();
    abort_err("ERROR: Too few args");
  }

  j=1;
  while(j<argc-2)
  {
    if(strcmp(argv[j], "--mode=quick")==0)
      execution_mode=EXEC_MODE_QUICK;
    else if(strcmp(argv[j], "--mode=full")==0)
      execution_mode=EXEC_MODE_FULLTEST;
    else if(strcmp(argv[j], "--print-fullmatch=yes")==0)
      print_fullymatched=EXIT_SUCCESS;
    else if(strcmp(argv[j], "--print-fullmatch=no")==0)
      print_fullymatched=EXIT_FAIL;
    else if(strcmp(argv[j], "--print-unmatched=yes")==0)
      print_unmatched=EXIT_SUCCESS;
    else if(strcmp(argv[j], "--print-unmatched=no")==0)
      print_unmatched=EXIT_FAIL;
    else if(strcmp(argv[j], "--print-hashmatch=yes")==0)
      print_hashmatch=EXIT_SUCCESS;
    else if(strcmp(argv[j], "--print-hashmatch=no")==0)
      print_hashmatch=EXIT_FAIL;
    else if(strcmp(argv[j], "--print-namematch=yes")==0)
      print_namematch=EXIT_SUCCESS;
    else if(strcmp(argv[j], "--print-namematch=no")==0)
      print_namematch=EXIT_FAIL;
    else if(strcmp(argv[j], "--print-summary=yes")==0)
      print_summary=EXIT_SUCCESS;
    else if(strcmp(argv[j], "--print-summary=no")==0)
      print_summary=EXIT_FAIL;
    else if(strcmp(argv[j], "--ignore-cue=yes")==0)
      ignore_cue=EXIT_SUCCESS;
    else if(strcmp(argv[j], "--ignore-cue=no")==0)
      ignore_cue=EXIT_FAIL;
    else if(strcmp(argv[j], "--recurse=yes")==0)
      recurse=EXIT_SUCCESS;
    else if(strcmp(argv[j], "--recurse=no")==0)
      recurse=EXIT_FAIL;
    else
      printf("WARNING: Unrecognised option: %s\n", argv[j]);
    ++i;
  }

  /*Read dat*/
  {
    read_dat_count(argv[argc-2], EXIT_SUCCESS, &dat_read_in_count);
    dat_read_in=malloc(dat_read_in_count*ROM_SIZE);
    memset(dat_read_in, 0, dat_read_in_count*ROM_SIZE );
    read_dat_get(argv[argc-2], EXIT_SUCCESS, dat_read_in, dat_read_in_count);
  
    dat_by_name=malloc(sizeof(char*)*dat_read_in_count);
    dat_by_hash=malloc(sizeof(char*)*dat_read_in_count);

    i=0;
    while(i<dat_read_in_count)
    {
      dat_by_name[i]=&dat_read_in[i*ROM_SIZE];
      ++i;
    }
    i=0;
    while(i<dat_read_in_count)
    {
      dat_by_hash[i]=&dat_read_in[i*ROM_SIZE];
      ++i;
    }

    qsort(dat_by_name, dat_read_in_count, sizeof(char*), compare_by_name);
    qsort(dat_by_hash, dat_read_in_count, sizeof(char*), compare_by_hash);

    if(print_summary==EXIT_SUCCESS)
      printf("%6u roms found in dat\n", dat_read_in_count);
  }

  /*read directory containing .grp files*/
  {
    file_path=malloc(4096);
    //argv[argc-1]
    grp_read_in=malloc(MAX_ELEMENTS_FROM_GRP*ROM_SIZE);
    grp_file_count= count_files_with_ext(argv[argc-1], ".grp", recurse);
    grp_file_list=malloc(PATH_SIZE_LIMIT*grp_file_count);
    memset(grp_file_list, 0, PATH_SIZE_LIMIT*grp_file_count);
    grp_file_list_loc=0;
    list_files_with_ext2(argv[argc-1], ".grp", recurse, grp_file_list, &grp_file_list_loc);
    grp_data=malloc(sizeof(char*)*grp_file_count);
    i=0;
    while(i<grp_file_count)
    {
      grp_data[i]=malloc(100000);
      include_grp=EXIT_SUCCESS;
      if(execution_mode==EXEC_MODE_FULLTEST)
      {
        /*Use GPack in test mode, set include_grp accordingly TODO*/
        include_grp=EXIT_SUCCESS;
      }
      if(include_grp==EXIT_SUCCESS)
      {
        sprintf( file_path, "%s", &grp_file_list[PATH_SIZE_LIMIT*i] );
        grp_file_reader=fopen(file_path, "rb");

        if(grp_file_reader==NULL)
          abort_err_str("ERROR: Failed to open .grp file for reading: %s\n", file_path);
        memset(grp_data[i], 0, 100000);
        fread(grp_data[i], 1, 100000, grp_file_reader);
        if(!feof(grp_file_reader))
          abort_err("ERROR: .grp file is too large (arbitrary 100KB limit)");
        fclose(grp_file_reader);
      
        grp_read_in_count+= read_grp_basic(grp_data[i], &grp_read_in[grp_read_in_count*ROM_SIZE], MAX_ELEMENTS_FROM_GRP-grp_read_in_count, i);
      }
      ++i;
    }
    if(print_summary==EXIT_SUCCESS)
      printf("%6u roms found in .grp files\n\n", grp_read_in_count);
  }
#define ROM_STATUS_UNMATCHED 0
#define ROM_STATUS_FULLYMATCHED 1
#define ROM_STATUS_NAMEMATCH 2
#define ROM_STATUS_HASHMATCH 3
#define ROM_STATUS_IGNORE 4
  /*match */
  {
    i=0;
    while(i<grp_read_in_count)
    {
//printf("%u\n", i);
//puts(&grp_read_in[i*ROM_SIZE]);
      key=&grp_read_in[i*ROM_SIZE];
      key_ptr=&key;
      curr_rom = bsearch( key_ptr, dat_by_name, dat_read_in_count, sizeof(char*), compare_by_name );
//puts("After bsearch");
      if(curr_rom==NULL)
      {
        //puts("//name not found, look for hash");
        curr_rom = bsearch( key_ptr, dat_by_hash, dat_read_in_count, sizeof(char*), compare_by_hash );
        if(curr_rom==NULL)
        {
          //puts("//name=no, hash=no");
        }
        else
        {
          //puts("//name=no, hash=yes");
          if(curr_rom[0][328]==ROM_STATUS_UNMATCHED)/*guard against overwriting a full match with a partial*/
            curr_rom[0][328]=ROM_STATUS_HASHMATCH;
        }
      }
      else
      {
        //puts("//name found, check hash");
        if( compare_by_hash(curr_rom, key_ptr)==0 )
        {
          //puts("//name=yes, hash=yes");
          curr_rom[0][328]=ROM_STATUS_FULLYMATCHED;
        }
        else
        {
          //puts("//name=yes, hash=no");
          curr_rom[0][328]=ROM_STATUS_NAMEMATCH;
        }
      }
      ++i;
    }
  }

  /*ignore cues if set*/
  if( ignore_cue==EXIT_SUCCESS )
  {
    i=0;
    while(i<dat_read_in_count)
    {
      if( str_ends_with( dat_by_hash[i], ".cue")==EXIT_SUCCESS )
      {
        dat_by_hash[i][328]=ROM_STATUS_IGNORE;
      }
      ++i;
    }
  }

  /*count stati*/
  {
    i=0;
    while(i<dat_read_in_count)
    {
      switch(dat_read_in[(i*ROM_SIZE)+328])
      {
        case ROM_STATUS_IGNORE:
          ++count_ignore;
          break;
        case ROM_STATUS_UNMATCHED:
          ++count_unmatched;
          break;
        case ROM_STATUS_FULLYMATCHED:
          ++count_fullymatched;
          break;
        case ROM_STATUS_NAMEMATCH:
          ++count_namematch;
          break;
        case ROM_STATUS_HASHMATCH:
          ++count_hashmatch;
          break;
      }
      ++i;
    }
  }
  count_in_use=count_unmatched+count_fullymatched+count_namematch+count_hashmatch;

  /*print summary*/
  if(print_summary==EXIT_SUCCESS)
  {
    if(count_ignore!=0)
      printf("\nComparison Summary:\n%6u roms ignored\n", count_ignore);
    if(count_unmatched!=0)
      printf("%6u roms unmatched\n", count_unmatched);
    if(count_fullymatched!=0)
      printf("%6u roms fully matched\n", count_fullymatched);
    if(count_namematch!=0)
      printf("%6u roms name matched (hash non-match, possible binary change required)\n", count_namematch);
    if(count_hashmatch!=0)
      printf("%6u roms hash matched (name non-match, possible name change required)\n\n", count_hashmatch);
    puts("");
  }

  /*grp altering stuff*/
  {
  }

  if(print_unmatched==EXIT_SUCCESS && count_unmatched!=0)
  {
    puts("\n\nUnmatched roms:");
    i=0;
    while(i<dat_read_in_count)
    {
      if(dat_by_name[i][328]==ROM_STATUS_UNMATCHED)
        puts(dat_by_name[i]);
      ++i;
    }
  }

  if(print_fullymatched==EXIT_SUCCESS && count_fullymatched!=0)
  {
    puts("\n\nFully matched roms:");
    i=0;
    while(i<dat_read_in_count)
    {
      if(dat_by_name[i][328]==ROM_STATUS_FULLYMATCHED)
        puts(dat_by_name[i]);
      ++i;
    }
  }

  if(print_namematch==EXIT_SUCCESS && count_namematch!=0)
  {
    puts("\n\nName matched roms:");
    i=0;
    while(i<dat_read_in_count)
    {
      if(dat_by_name[i][328]==ROM_STATUS_NAMEMATCH)
        puts(dat_by_name[i]);
      ++i;
    }
  }

  if(print_hashmatch==EXIT_SUCCESS && count_hashmatch!=0)
  {
    puts("\n\nHash matched roms:");
    i=0;
    while(i<dat_read_in_count)
    {
      if(dat_by_name[i][328]==ROM_STATUS_HASHMATCH)
        puts(dat_by_name[i]);
      ++i;
    }
  }

  /*clean up*/
  {
    free(dat_read_in);
    i=0;
    while(i<grp_file_count)
    {
      free(grp_data[i]);
      ++i;
    }
    free(grp_data);
  }


  return EXIT_SUCCESS;
}

int read_dat_count(char* dat_path, char ret_cue, unsigned int* num_elements)
{
  FILE* dat=NULL;
  char* buffer=NULL;
  char* buffer_2=NULL;
  char* curr_loc=NULL;
  char name[NAME_SIZE_LIMIT];
  char sha1[41];
  char md5[33];
  char*curr_loc_read=NULL;
  unsigned int count=0;
  unsigned int count_non_cue=0;
  sha1[40]=0;
  md5[32]=0;
  buffer=malloc(4096);
  buffer_2=malloc(4096);


  if( (dat=fopen(dat_path, "rb"))==NULL)
    abort_err("ERROR: Failed to open dat for reading");

  /*count roms*/
  while(fgets(buffer, 4096, dat)!=NULL)
  {
    if( (curr_loc=strstr(buffer, "<rom name="))!=NULL)
    {
      /*rom*/
      while(strstr(buffer, "\"/>")==NULL)
      {
        if(fgets(buffer_2, 4096, dat)==NULL)
          break;
        strcat(buffer, buffer_2);
      }
      curr_loc_read=name;
      curr_loc+=11;
      while(curr_loc[0]!='\"')
      {
        curr_loc_read[0]=curr_loc[0];
        ++curr_loc_read;++curr_loc;
      }
      curr_loc_read[0]=0;

      if( (curr_loc=strstr(buffer, "md5=\""))==NULL)
        continue;
      curr_loc+=7;
      memcpy(md5, curr_loc, 32);

      if( (curr_loc=strstr(buffer, "sha1=\""))==NULL)
        continue;
      curr_loc+=8;
      memcpy(sha1, curr_loc, 40);
      ++count;
      if( str_ends_with(name, ".cue")!=EXIT_SUCCESS )
        ++count_non_cue;
    }
  }

  if(ret_cue==EXIT_SUCCESS)
  {
    num_elements[0]=count;      
  }
  else
  {
    num_elements[0]=count_non_cue;
  }

  fclose(dat);
  free(buffer);
  free(buffer_2);

  return EXIT_SUCCESS;
}

int read_dat_get(char* dat_path, char ret_cue, char*ret, unsigned int count)
{
  FILE* dat=NULL;
  char* buffer=NULL;
  char* buffer_2=NULL;
  char* curr_loc=NULL;
  char name[NAME_SIZE_LIMIT];
  char sha1[41];
  char md5[33];
  char*curr_loc_read=NULL;
  unsigned int curr_loc_ret=0;
  unsigned int counter=0;
  unsigned int i;

  sha1[40]=0;
  md5[32]=0;

  buffer=malloc(4096);
  buffer_2=malloc(4096);

  if( (dat=fopen(dat_path, "rb"))==NULL)
    abort_err("ERROR: Failed to open dat for reading");
  /*build output*/
  while(fgets(buffer, 4096, dat)!=NULL)
  {
    if( (curr_loc=strstr(buffer, "<rom name="))!=NULL)
    {
      /*rom*/
      while(strstr(buffer, "\"/>")==NULL)
      {
        if(fgets(buffer_2, 4096, dat)==NULL)
          break;
        strcat(buffer, buffer_2);
      }
      memset(name, 0, NAME_SIZE_LIMIT);
      curr_loc_read=name;
      curr_loc+=11;
      while(curr_loc[0]!='\"')
      {
        curr_loc_read[0]=curr_loc[0];
        ++curr_loc_read;++curr_loc;
      }
      if(name[NAME_SIZE_LIMIT-1]!=0)
        abort_err_int("ERROR: Buffer overflow detected parsing dat. Compile with higher name size ceiling (>%u)\n", NAME_SIZE_LIMIT);
      if( (curr_loc=strstr(buffer, "md5=\""))==NULL)
        continue;
      curr_loc+=5;
      memcpy(md5, curr_loc, 32);

      if( (curr_loc=strstr(buffer, "sha1=\""))==NULL)
        continue;
      curr_loc+=6;
      memcpy(sha1, curr_loc, 40);

      /*skip cue if we don't want them*/
      if( ret_cue!=EXIT_SUCCESS && str_ends_with(name, ".cue")==EXIT_SUCCESS )
        continue;

      memcpy( &ret[counter], name, NAME_SIZE_LIMIT);
      if(counter%ROM_SIZE!=0)
        puts("The counter is fucked");
      counter+=NAME_SIZE_LIMIT;
      i=0;
      while(i<32)
      {
        ret[counter]=toupper(md5[i]);
        ++counter;
        ++i;
      }
      i=0;
      while(i<40)
      {
        ret[counter]=toupper(sha1[i]);
        ++counter;
        ++i;
      }
      counter+=5;

      ++curr_loc_ret;
    }
  }
  fclose(dat);
  free(buffer);
  free(buffer_2);

  return EXIT_SUCCESS;
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
  puts(desc);
  exit(EXIT_FAIL);
}

void abort_err_str(char* desc, char*var)
{
  printf(desc, var);
  exit(EXIT_FAIL);
}

void abort_err_int(char* desc, int var)
{
  printf(desc, var);
  exit(EXIT_FAIL);
}

int compare_by_hash_verbose(const void * a, const void * b)
{
  const char **ia = (const char **)a;
  const char **ib = (const char **)b;
  printf("\nby_hash:\n%s\n%s\n", NAME_SIZE_LIMIT+*ia, NAME_SIZE_LIMIT+*ib);
  return memcmp(NAME_SIZE_LIMIT+*ia, NAME_SIZE_LIMIT+*ib, 72);
}

int compare_by_name_verbose(const void * a, const void * b)
{
  char **ia = (char **)a;
  char **ib = (char **)b;
  printf("\nby_name:\n%s\n%s\n", *ia, *ib);
  return memcmp(*ia, *ib, NAME_SIZE_LIMIT);
}

int compare_by_hash(const void * a, const void * b)
{
  const char **ia = (const char **)a;
  const char **ib = (const char **)b;
  return memcmp(NAME_SIZE_LIMIT+*ia, NAME_SIZE_LIMIT+*ib, 72);
}

int compare_by_name(const void * a, const void * b)
{
  char **ia = (char **)a;
  char **ib = (char **)b;
  return memcmp(*ia, *ib, NAME_SIZE_LIMIT);
}

void uint_to_char_arr_BE(unsigned int num, unsigned char* arr)
{
  unsigned int work=num;
  arr[3]=work%256;
  work/=256;
  arr[2]=work%256;
  work/=256;
  arr[1]=work%256;
  work/=256;
  arr[0]=work%256;
}

/*  out format:
  256: name
    32: md5 hex
    40: sha1 hex
*/
unsigned int read_grp_basic(char* grp, char* out, int max_elements, int grp_index)
{
  unsigned int loc=0;
  unsigned int curr_marker=0;
  unsigned int out_index=0;


  while(grp[loc]!=0)/*while not at end of file*/
  {
    if(out_index==max_elements)
      return 0;
    /*name*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    strncpy( &out[(out_index*ROM_SIZE)], &grp[curr_marker], loc-curr_marker );
    ++loc;

    /*2048 count*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    ++loc;

    /*2324 count*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    ++loc;

    /*2336 count*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    ++loc;

    /*2352 count*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    ++loc;

    /*md5*/
    curr_marker=loc;
    while(grp[loc]!=':')
      ++loc;
    strncpy( &out[(out_index*ROM_SIZE)+256], &grp[curr_marker], loc-curr_marker );
    ++loc;

    /*sha1*/
    curr_marker=loc;
    while(grp[loc]!=':' && grp[loc]!=0x0D && grp[loc]!=0x0A)
      ++loc;
    strncpy( &out[(out_index*ROM_SIZE)+256+32], &grp[curr_marker], loc-curr_marker );
    switch(grp[loc])
    {
      case ':':
        ++loc;

        /*tags*/
        curr_marker=loc;
        while(grp[loc]!=':' && grp[loc]!=0x0D && grp[loc]!=0x0A)
          ++loc;
        switch(grp[loc])
        {
          case ':':
            ++loc;

            /*special*/
            curr_marker=loc;
            while(grp[loc]!=0x0D && grp[loc]!=0x0A)
              ++loc;
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
    uint_to_char_arr_BE((unsigned int)grp_index, &out[(out_index*ROM_SIZE)+329]);
    ++out_index;
  }

  return out_index;
}



//NAME MD5 SHA1 STATUS
// 256  32   40      1

/*we know how many files match as we've just counted them*/
void list_files_with_ext2(char* dir_n, char* ext, char recurse, char* list, int* curr_loc)
{

#ifdef WINDOWS
  WIN32_FIND_DATA fdFile;
  HANDLE hFind = NULL;

  char* sPath=NULL;
  char* new_dir=NULL;
  new_dir=malloc(5000);

  sPath=malloc(5000);

  /*Specify a file mask, *.ext */
  sprintf(sPath, "%s\\*.*", dir_n);

  if((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE)
  {
    free(sPath);
    free(new_dir);
    abort_err_str("ERROR: Path not found: [%s]\n", dir_n);
  }

  do
  {
    /*ignore backtrack*/
    if(strcmp(fdFile.cFileName, ".") != 0 && strcmp(fdFile.cFileName, "..") != 0)
    {
      /*recurse into directories if recursing*/
      if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
        if(recurse==EXIT_SUCCESS)
        {
          sprintf(new_dir, "%s\\%s", dir_n, fdFile.cFileName);
          list_files_with_ext2(new_dir, ext, recurse, list, curr_loc);
        }
      }
      else if( str_ends_with(fdFile.cFileName, ext)==EXIT_SUCCESS )
      {
        sprintf(&list[curr_loc[0]*PATH_SIZE_LIMIT], "%s\\%s", dir_n, fdFile.cFileName);
        ++curr_loc[0];
      }
    }
  }
  while(FindNextFile(hFind, &fdFile)); /*Find the next file.*/

  FindClose(hFind);
#endif

  free(sPath);
  free(new_dir);
}

int count_files_with_ext(char* dir_n, char* ext, char recurse)
{
  int count_files=0;

#ifdef WINDOWS
  WIN32_FIND_DATA fdFile;
  HANDLE hFind = NULL;

  char* sPath=NULL;

  char* new_dir=NULL;
  new_dir=malloc(5000);

  sPath=malloc(5000);

  /*Specify a file mask. *.ext */
  sprintf(sPath, "%s\\*.*", dir_n);

  if((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE)
  {
    free(sPath);
    free(new_dir);
    return 0;
  }

  do
  {
    /*ignore backtrack*/
    if(strcmp(fdFile.cFileName, ".") != 0 && strcmp(fdFile.cFileName, "..") != 0)
    {
      /*recurse into directories if recursing*/
      if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
        if(recurse==EXIT_SUCCESS)
        {
          sprintf(new_dir, "%s\\%s", dir_n, fdFile.cFileName);
          count_files+=count_files_with_ext(new_dir, ext, recurse);
        }
      }
      else if( str_ends_with(fdFile.cFileName, ext)==EXIT_SUCCESS )
      {
        ++count_files;
      }
    }
  }
  while(FindNextFile(hFind, &fdFile)); /*Find the next file.*/

  FindClose(hFind);

  free(sPath);
  free(new_dir);
#endif

  return count_files;
}

