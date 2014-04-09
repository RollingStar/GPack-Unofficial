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

/*
  Dependencies:
    PolarSSL (tested with 1.1.3)
    OpenMP (ps3 is multi-threaded)
*/

/*print status for debugging*/
//#define VERBOSE

#define _FILE_OFFSET_BITS 64
#define _CRT_SECURE_NO_WARNINGS

#ifdef FUTURE_STUFF
// ./unifdef-2.6/unifdef -o './GParse-small.c' -U FUTURE_STUFF './GParse.c'
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

/*if defined, use multi-threaded code. if not, use single-threaded code*/
#define USE_OPENMP

#ifdef USE_OPENMP
#include <omp.h>
#endif

//#define NDEBUG
#include <assert.h>

#define CHANGE_TITLE

#if defined(_WIN32)

#define WINDOWS
#include <Windows.h>
#include <ShellAPI.h>
#include <io.h>
#define my_fseek(a,b,c) _fseeki64(a,b,c)
#define my_ftell(a) _ftelli64(a)
#define my_int64 __int64

#else

#define my_fseek(a,b,c) fseeko(a,b,c)
#define my_ftell(a) ftello(a)
#define my_int64 off_t

void SetConsoleTitle(char* title)
{
  /*printf("%c]0;%s%c", '\033', title, '\007');*/ /*linux title seems to update too slowly and is buggy*/
}

#endif

#define POLAR_SSL

#ifdef POLAR_SSL
#include <polarssl/aes.h>
#include <polarssl/md5.h>
#include <polarssl/sha1.h>
#endif

#define NAME_SIZE_LIMIT 1024

/*ngc defs*/
#define NGC_SECTOR_COUNT 712880
#define NGC_BUFFER_SIZE (67*19*5*2048)

/*ps3 defs*/
#define THREAD_COUNT 12
#define MODE_ENCRYPT AES_ENCRYPT
#define MODE_DECRYPT AES_DECRYPT

/*wii defs*/
#define CLUSTER_SIZE (32768)
#define SUBGROUP_SIZE (32768*8)
#define GROUP_SIZE (32768*64)
#define DEC_CLUSTER_SIZE (31744)
#define DEC_SUBGROUP_SIZE (31744*8)
#define DEC_GROUP_SIZE (31744*64)

/*xbox defs*/
#define XBOXOLD_SECTOR_COUNT 3629408
#define XBOX_SECTOR_COUNT 3820880 /*extra video partition*/

/*global defs*/
#define BUFFER_SIZE_SEC (4096)
#define BUFFER_SIZE (BUFFER_SIZE_SEC*2048)

#define EXIT_FAIL 1
#define EXIT_SUCCESS 0

#define WII_DESC 0
#define NGC_DESC 1
#define XBOX_DESC 2
#define PS3_DESC 3

int rev=32;

#ifdef FUTURE_STUFF
#define XBOX360_DESC 4
unsigned system_descriptor[]={0x00, 0x01, 0x02, 0x03, 0x04};
#else
unsigned system_descriptor[]={0x00, 0x01, 0x02, 0x03};
#endif

unsigned char chk_tab[]={'g', 'b', 'e', 's', 'f'};
unsigned char* gparse_magic="GParse_ISOParser";

/*global variables for ps3*****************************************************/
aes_context** aes=NULL;
unsigned char* key=NULL;
unsigned char* iv=NULL;
char* burn=NULL;
unsigned char* sec0sec1=NULL;
unsigned int global_lba=0;

void abort_err(char* desc)
{
  fprintf(stderr, "%s\n", desc);
  exit(EXIT_FAIL);
}

void abort_err_str(char* desc, char*var)
{
  fprintf(stderr, desc, var);
  exit(EXIT_FAIL);
}

void abort_err_int(char* desc, int var)
{
  fprintf(stderr, desc, var);
  exit(EXIT_FAIL);
}

/*general funcs****************************************************************/
void help();
void print_key(unsigned char* key, unsigned int len);
int mark_used(char* used_group, my_int64 start, my_int64 length, my_int64 chunk_size, unsigned char marker);
int used_map_crunch(char* used_map, char* used_map_bit, unsigned int used_map_size);
int used_map_uncrunch(char* used_map, char* used_map_bit, unsigned int used_map_size);
int is_sec_filled(char* buffer)
{
  char* comp=NULL;
  int ret;
  comp=malloc(2048);
  memset(comp, buffer[0], 2048);
  ret=memcmp(buffer, comp, 2048);
  free(comp);
  return ret==0?EXIT_SUCCESS:EXIT_FAIL;
}
void write_element_to_grp(FILE* grp, char* filename, unsigned int sec_count, char* hash_md5, char* hash_raw);
void write_to_chk_file(FILE*chk, char* hash, char* hash_scrub, char* hash_raw);
int byte_to_hex(char* bytes, char* hex_str, unsigned int len);

unsigned int char_arr_BE_to_uint(unsigned char* arr)
{
  return arr[3] + 256*(arr[2] + 256*(arr[1] + 256*arr[0]));
}

unsigned int char_arr_LE_to_uint(unsigned char* arr)
{
  return arr[0] + 256*(arr[1] + 256*(arr[2] + 256*arr[3]));
}

unsigned int char_arr_LE_2_to_uint(unsigned char* arr)
{
  return arr[0] + 256*arr[1];
}

/*wii/gc stores some offsets in three bytes (string table for fst)*/
unsigned int char_arr_BE_trip_to_uint(unsigned char* arr)
{
  return arr[2] + 256*(arr[1] + 256*arr[0]);
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

/*ps3 funcs********************************************************************/
int hex_to_key(char* pot_key, unsigned char* key);
void process(unsigned char* data, int sector_count, int mode);
void reset_iv(unsigned char* iv, unsigned int j);
int sanatise_key(char* pot_key);
void swap_ptrs(unsigned char* base, unsigned char** r, unsigned char** w, unsigned char**p);

/*wii funcs********************************************************************/
int decrypt_block(unsigned char* enc, unsigned char* dec, unsigned char* key, char* iv, size_t len);
int decrypt_group(unsigned char* enc_group, unsigned char* dec_group, unsigned char* title_key);
int encrypt_block(unsigned char* enc, unsigned char* dec, unsigned char* key, char* iv, size_t len);
int encrypt_group(unsigned char* dec_group, unsigned char* enc_group, unsigned char* title_key);
int partition_sort(const void * a, const void * b);
int encrypt_block_quicker(unsigned char* enc, unsigned char* dec, aes_context* aes, char* iv, size_t len);//aes already initialised
int decrypt_block_quicker(unsigned char* enc, unsigned char* dec, aes_context* aes, char* iv, size_t len);//aes already initialised

/*xbox funcs*******************************************************************/
void read_dir_entry(char* buffer, int loc, char* dir_table, unsigned int* dir_table_write_loc, unsigned char* used_map, my_int64 xdvdfs_loc);

/*global controllers***********************************************************/
int encode(int argc, char*argv[], char test);
int decode(int argc, char*argv[], char test);

/*game controllers*************************************************************/
int encode_game_wii(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size);
int decode_game_wii(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename);
int encode_game_ngc(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size);
int decode_game_ngc(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename);
int encode_game_ps3(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size, char* key);
int decode_game_ps3(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename, FILE* key_f);
int encode_game_xbox(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size);
int decode_game_xbox(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename);
#ifdef FUTURE_STUFF
int encode_game_xbox360(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size);
int decode_game_xbox360(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename);
#endif
int decode_game_simple_map(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename, unsigned int sector_count);

/*managed wrappers*************************************************************/
int fclose_chk( FILE * stream )
{
  int ret;
  if(stream==NULL)
    return 0;
  ret=fclose(stream);
  if(ret!=0)
    fprintf(stderr, "WARNING: fclose failed\n");
  return ret;
}
#define fclose(a) fclose_chk(a)

void* malloc_chk(size_t size)
{
  void* ptr=malloc(size);
  if(ptr==NULL)
    abort_err("ERROR: Failed to allocate memory");
  return ptr;
}
#define malloc(a) malloc_chk(a)

size_t fread_chk(void * ptr, size_t size, size_t count, FILE * stream, char* msg)
{
  size_t ret;
  assert(ptr!=NULL);
  assert(size==1);
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

size_t fwrite_chk(const void * ptr, size_t size, size_t count, FILE * stream, char* msg)
{
  size_t ret;
  assert(ptr!=NULL);
  if(stream==NULL)
    return 0;
  if( (ret=fwrite(ptr, size, count, stream))!=count )
  {
    if(msg!=NULL)
      abort_err(msg);
    else
      abort_err("ERROR: Could not write data to stream\n");
  }
  return ret;
}

size_t gwrite(unsigned char* buffer, size_t size, size_t num, FILE* file, sha1_context* sha1, unsigned int* curr_sector)
{
  assert(size==1);
  assert(buffer!=NULL);
  assert(sha1!=NULL);
  assert(curr_sector!=NULL);
  assert(file!=NULL);
  fwrite_chk(buffer, size, num, file, "ERROR: Could not write everything to image file");
  sha1_update(sha1, buffer, num);
  curr_sector[0]+=(num/2048);
  return num;
}

int count_used(char* used_group, int len)
{
  int i=0;
  int count=0;
  assert(used_group!=NULL);
  while(i<len)
  {
    if(used_group[i]=='1')
      ++count;
    ++i;
  }
  return count;
}
void print_used(char* used_group, int len)
{
  int i=0;
  assert(used_group!=NULL);
  while(i<len)
  {
    if(i!=0 && i%64==0)
      fprintf(stderr, "\n");
    fprintf(stderr, "%c", used_group[i]);
    ++i;
  }
}

int main(int argc, char* argv[])
{
  time_t t_start, t_end;
  double time_taken;
  int i;
  int non_ops;
  char test=EXIT_FAIL;

  time(&t_start);
  fprintf(stderr, "\nGParse r%u\n\n", rev);

  assert(sizeof(unsigned int)>=4);
  assert(sizeof(my_int64)>=8);

  if(argc<3)
  {
    help();
    return EXIT_FAIL;
  }

#ifdef WINDOWS
  /*linux doesn't need this, text==binary*/
  if( _setmode ( _fileno ( stdout ), O_BINARY ) == -1 )
    abort_err("ERROR: Cannot set stdin to binary mode");
  if( _setmode ( _fileno ( stdin ), O_BINARY ) == -1 )
    abort_err("ERROR: Cannot set stdin to binary mode");
#endif

  /*find ops*/
  non_ops=1;
  while(memcmp(argv[non_ops], "-", 1)==0 && non_ops<(argc-1) )
    ++non_ops;

  /*parse ops*/
  i=1;
  while(i<non_ops)
  {
    if(memcmp(argv[non_ops], "-t", 2)==0 || memcmp(argv[non_ops], "--t", 3)==0)
      test=EXIT_SUCCESS;
    ++i;
  }

  if(memcmp(argv[non_ops], "e", 1)==0)
    encode((argc-non_ops)+1, &argv[non_ops-1], test);
  else if(memcmp(argv[non_ops], "d", 1)==0)
    decode((argc-non_ops)+1, &argv[non_ops-1], test);
  else
    abort_err("ERROR: Unsupported operation (mode must be 'e' or 'd')");

  fprintf(stderr, "\nAll seems ok\n");

  time(&t_end);
  time_taken = difftime(t_end, t_start);
  fprintf(stderr, "\nExecution completed in %.2lf seconds\n\n", time_taken);

  return EXIT_SUCCESS;
}

int encode(int argc, char*argv[], char test)
{
  /*test not implemented TODO*/
  FILE* game=NULL;
  FILE* pad=NULL;
  FILE* dec=NULL;
  FILE* grp=NULL;

  my_int64 mod_pad_loc;
  my_int64 pad_size=0;
  unsigned int mod_pad_val=0;
  unsigned char mod_pad_arr[4];

  unsigned char key[16];

  unsigned int i=6, j;

  unsigned char dec_type;

  char* names=NULL;
  char count_arr[4];

  char null_char=0;

#ifdef VERBOSE
  fprintf(stderr, "---encode\n");
#endif

  if(argc<7)
    abort_err("ERROR: Not enough args");

  /*prep for name inclusion*/
  names=malloc((argc-6)*NAME_SIZE_LIMIT);
  memset(names, 0, (argc-6)*NAME_SIZE_LIMIT);
  uint_to_char_arr_BE(argc-6, count_arr);

  if(memcmp(argv[2], "wii", 3)==0)
    dec_type=system_descriptor[WII_DESC];
  else if(memcmp(argv[2], "ngc", 3)==0)
    dec_type=system_descriptor[NGC_DESC];
  else if(memcmp(argv[2], "xbox", 4)==0)
    dec_type=system_descriptor[XBOX_DESC];
  else if(memcmp(argv[2], "ps3", 3)==0)
    dec_type=system_descriptor[PS3_DESC];
#ifdef FUTURE_STUFF
  else if(memcmp(argv[2], "360", 3)==0)
    dec_type=system_descriptor[XBOX360_DESC];
#endif
  else
    dec_type=255;//unsupported

  switch(dec_type)
  {
    case WII_DESC:
      fputs("Encode Mode: Wii\n\n", stderr);
      break;
    case NGC_DESC:
      fputs("Encode Mode: GameCube\n\n", stderr);
      break;
    case XBOX_DESC:
      fputs("Encode Mode: Xbox\n\n", stderr);
      break;
    case PS3_DESC:
      fputs("Encode Mode: PS3\n\n", stderr);
      break;
#ifdef FUTURE_STUFF
    case XBOX360_DESC:
      fputs("Encode Mode: Xbox 360\n\n", stderr);
      break;
#endif
    default:
      abort_err("ERROR: Unsupported operation [1]");
  }

  //pad
  if(memcmp(argv[3], "-", 1)==0)
    pad=stdout;
  else if(memcmp(argv[3], "NULL", 4)==0)
    pad=NULL;
  else
  {
    pad=fopen(argv[3], "wb");
    if(pad==NULL)
      abort_err("ERROR: Failed to open pad_file for writing");
  }

  //dec
  if(memcmp(argv[4], "-", 1)==0)
    abort_err("ERROR: stdout is not a valid dec_file path");
  else if(memcmp(argv[4], "NULL", 4)==0)
    abort_err("ERROR: NULL is not a valid dec_file path");
  else
  {
    dec=fopen(argv[4], "wb");
    if(dec==NULL)
      abort_err("ERROR: Failed to open dec_file for writing");
  }

  //write header
  {
    fwrite_chk(gparse_magic, 1, 16, dec, "ERROR: Could not write GParse magic id to dec_file header");
    mod_pad_loc=my_ftell(dec);
    uint_to_char_arr_BE(mod_pad_val, mod_pad_arr);
    fwrite_chk(mod_pad_arr, 1, 4, dec, "ERROR: Could not write pad size mod placeholder to dec file header");//placeholder
    fwrite_chk(count_arr, 1, 4, dec, "ERROR: Could not write image count to dec_file header");
    //storing names not implemented, so name table size is always same as image count
    fwrite_chk(count_arr, 1, 4, dec, "ERROR: Could not write name table size to dec_file header");
    j=0;
    while(j<(unsigned int)(argc-6))
    {
      //here is where to add name, if flag is set by user (--include-names or something)
      //not implemented yet
      fwrite_chk(&null_char, 1, 1, dec, "ERROR: Could not write system type to dec_file header");
      ++j;
    }

    fwrite_chk(&dec_type, 1, 1, dec, "ERROR: Could not write system type to dec_file header");
  }

  //grp
  if(memcmp(argv[5], "-", 1)==0)
    abort_err("ERROR: stdout is not a valid grp_file path");
  else if(memcmp(argv[5], "NULL", 4)==0)
    grp=NULL;
  else
  {
    grp=fopen(argv[5], "wb");
    if(grp==NULL)
      abort_err("ERROR: Failed to open grp_file for writing");
  }

  while(argv[i]!=NULL)//for every image
  {
    if(memcmp(argv[i], "-", 1)==0)
      abort_err("ERROR: '-' is not a valid game path when encoding");
    else if(memcmp(argv[i], "NULL", 4)==0)
      abort_err("ERROR: 'NULL' is not a valid game path when encoding");
    else
    {
      game=fopen(argv[i], "rb");
      if(game==NULL)
        abort_err_str("ERROR: Failed to open image file for reading: '%s'\n", argv[i]);
    }

    fprintf(stderr, "Parse '%s'\n", argv[i]);
    j=0;
    while(j<strlen(argv[i]))
    {
      if(argv[i][j]=='\\')
        argv[i][j]='/';
      ++j;
    }

    assert( (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i])!=NULL );

    switch(dec_type)
    {
      case WII_DESC:
        if(encode_game_wii(game, pad, dec, grp, i-5, argc-6, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]), &pad_size )!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;

      case NGC_DESC:
        if(encode_game_ngc(game, pad, dec, grp, i-5, argc-6, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]), &pad_size )!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;

      case XBOX_DESC:
        if(encode_game_xbox(game, pad, dec, grp, i-5, argc-6, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]), &pad_size )!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;

      case PS3_DESC:
        /*get key from commandline*/
        if( strlen(argv[i+1])==34 )/*remove 0x if present*/
          argv[i+1]=&argv[i+1][2];
        if( strlen(argv[i+1])!=32 )
          abort_err("ERROR: Key must be 32 hex characters in length");
        if( sanatise_key(argv[i+1])!=EXIT_SUCCESS )
          abort_err("ERROR: Supplied key contains invalid characters");
        if( hex_to_key(argv[i+1], key) !=EXIT_SUCCESS )
          abort_err("ERROR: hex string to char array key conversion failed");
        if(encode_game_ps3(game, pad, dec, grp, (i-5)/2, (argc-6)/2, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]), &pad_size, key )!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        ++i;
        break;

#ifdef FUTURE_STUFF
      case XBOX360_DESC:
        if(encode_game_xbox360(game, pad, dec, grp, i-5, argc-6, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]), &pad_size )!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;
#endif
    }


    fclose(game);
    ++i;
  }

  //pad_size=my_ftell(pad);
  mod_pad_val=(pad_size%4294967295U);
  my_fseek(dec, mod_pad_loc, SEEK_SET);
  uint_to_char_arr_BE(mod_pad_val, mod_pad_arr);
  fwrite_chk(mod_pad_arr, 1, 4, dec, "ERROR: Could not write pad size mod to dec file");

  if(pad!=NULL)
    fclose(pad);
  if(dec!=NULL)
    fclose(dec);
  if(grp!=NULL)
    fclose(grp);

  return EXIT_SUCCESS;
}

int decode(int argc, char*argv[], char test)
{
  FILE* game=NULL;
  FILE* pad=NULL;
  FILE* dec=NULL;
  FILE* chk=NULL;
  FILE* key=NULL;/*key file for ps3*/

  char* key_path=NULL;/*path to key file*/

  unsigned int i=5, j;

  unsigned char dec_type;

  unsigned char pad_present=1;

  unsigned char magic_chk[16];

  my_int64 pad_size;
  unsigned int mod_pad_val=0;
  unsigned int mod_pad_stored_val=0;
  unsigned char mod_pad_arr[4];

  char count_arr[4];
  unsigned int count;
  char* names=NULL;

  unsigned int name_table_size;
  char*name_table=NULL;

#ifdef VERBOSE
  fprintf(stderr, "---decode\n");
#endif

  if(argc<6)
    abort_err("ERROR: Not enough args");

  //pad
  if(memcmp(argv[2], "-", 1)==0)
    abort_err("ERROR: stdin is not a valid pad_file path when decoding");
  else if(memcmp(argv[2], "NULL", 4)==0)
    pad=NULL;
  else
  {
    pad=fopen(argv[2], "rb");
    if(pad==NULL)
      abort_err("ERROR: Failed to open pad_file for reading");
  }

  //dec
  if(memcmp(argv[3], "-", 1)==0)
    dec=stdin;
  else if(memcmp(argv[3], "NULL", 4)==0)
    abort_err("ERROR: 'NULL' is not a valid dec_file path when decoding");
  else
  {
    dec=fopen(argv[3], "rb");
    if(dec==NULL)
      abort_err("ERROR: Failed to open dec_file for reading");
  }

  //read header
  fread_chk(magic_chk, 1, 16, dec, "ERROR: Failed to read magic word from dec file");
  if(memcmp(magic_chk, gparse_magic, 16)!=0)
    abort_err("ERROR: Dec failed magic test, not a valid GParse stream");

  /*mod*/
  //get stored size
  fread_chk(mod_pad_arr, 1, 4, dec, "ERROR: Failed to read pad size mod from dec file");
  mod_pad_val=char_arr_BE_to_uint(mod_pad_arr);
  if(pad!=NULL)
  {
    //get actual size
    my_fseek(pad, 0, SEEK_END);
    pad_size=my_ftell(pad);
    rewind(pad);
    mod_pad_stored_val=(pad_size%4294967295U);

    if(mod_pad_stored_val==0 && mod_pad_val!=0)
    {
      fprintf(stderr, "WARNING: Mod size of pad file does not match expected, but stored value is 0 so may not have been set on encode. Attempting to use pad file\n");
    }
    else if(mod_pad_stored_val!=mod_pad_val)
    {
      fprintf(stderr, "WARNING: Mod size of pad file does not match expected. Ignoring pad file\n");
      fclose(pad);
      pad=NULL;
    }
    else
    {
      fprintf(stderr, "Pad file passed size mod test\n\n");
    }
  }
  /*image count*/
  fread_chk(count_arr, 1, 4, dec, "ERROR: Failed to read image count from dec file");
  count=char_arr_BE_to_uint(count_arr);
  /*image names*/
  names=malloc(count*NAME_SIZE_LIMIT);
  memset(names, 0, count*NAME_SIZE_LIMIT);
  fread_chk(count_arr, 1, 4, dec, "ERROR: Failed to read name table size from dec file");
  name_table_size=char_arr_BE_to_uint(count_arr);
  name_table=malloc(name_table_size);
  fread_chk(name_table, 1, name_table_size, dec, "ERROR: Failed to read name table from dec file");
  //here strings from variable length name_table should be moved into fixed length names
  //not implemented

  fread_chk(&dec_type, 1, 1, dec, "ERROR: Failed to read system type from dec file");
  switch(dec_type)
  {
    case WII_DESC:
      fputs("Decode Mode: Wii\n", stderr);
      break;
    case NGC_DESC:
      fputs("Decode Mode: GameCube\n", stderr);
      break;
    case XBOX_DESC:
      fputs("Decode Mode: Xbox\n", stderr);
      break;
    case PS3_DESC:
      fputs("Decode Mode: PS3\n", stderr);
      break;
#ifdef FUTURE_STUFF
    case XBOX360_DESC:
      fputs("Decode Mode: Xbox 360\n", stderr);
      break;
#endif
    default:
      abort_err("ERROR: Unsupported operation. dec_file is invalid, or was created with a newer version of this program");
  }

  if(dec_type!=PS3_DESC)
  {
    if(pad==NULL)
      fprintf(stderr, "Decode type: Scrubbed\n\n");
    else
      fprintf(stderr, "Decode type: Original\n\n");
  }

  //chk
  if(memcmp(argv[4], "-", 1)==0)
    abort_err("ERROR: stdout is not a valid chk_file path");
  else if(memcmp(argv[4], "NULL", 4)==0)
    chk=NULL;
  else
  {
    chk=fopen(argv[4], "wb");
    if(chk==NULL)
      abort_err("ERROR: Failed to open chk_file for writing");
  }

  if(chk!=NULL)
    fwrite_chk(chk_tab+3, 1, 1, chk, "ERROR: Could not write 's' to chk file");


  key_path=malloc(4096);

  while(argv[i]!=NULL)//for every image
  {
    game=NULL;
    if(test!=EXIT_SUCCESS)
    {
      key=NULL;
      if(memcmp(argv[i], "-", 1)==0)
        game=stdout;
      else if(memcmp(argv[i], "NULL", 4)==0)
        game=NULL;
      else
      {
        game=fopen(argv[i], "wb");
        if(game==NULL)
          abort_err_str("ERROR: Failed to open image file for writing: '%s'\n", argv[i]);

        /*if ps3, create key file*/
        if(dec_type==PS3_DESC)
        {
          if(str_ends_with(argv[i], ".iso")==EXIT_SUCCESS )
          {
            sprintf(key_path, "%s", argv[i]);
            sprintf(strrchr(key_path, '.')+1, "key");
          }
          else
            sprintf(key_path, "%s.key", argv[i]);
          key=fopen(key_path, "wb");
          if(key==NULL)
            abort_err_str("ERROR: Failed to open key file for writing: '%s'\n", key_path);
        }
      }
    }
    fprintf(stderr, "Parse '%s'\n", argv[i]);
    j=0;
    while(j<strlen(argv[i]))
    {
      if(argv[i][j]=='\\')
        argv[i][j]='/';
      ++j;
    }
    switch(dec_type)
    {
      case WII_DESC:
        if(decode_game_wii(game, pad, dec, chk, i-4, argc-5, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]))!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;

      case NGC_DESC:
        if(decode_game_ngc(game, pad, dec, chk, i-4, argc-5, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]))!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;

      case XBOX_DESC:
        if(decode_game_xbox(game, pad, dec, chk, i-4, argc-5, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]))!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;

      case PS3_DESC:
        if(decode_game_ps3(game, pad, dec, chk, i-4, argc-5, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]), key)!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;

#ifdef FUTURE_STUFF
      case XBOX360_DESC:
        if(decode_game_xbox360(game, pad, dec, chk, i-4, argc-5, (strrchr(argv[i], '/')!=NULL?strrchr(argv[i], '/')+1: argv[i]))!=EXIT_SUCCESS)
          abort_err_str("ERROR: Failed to parse image: '%s'\n", argv[i]);
        break;
#endif
    }
    fclose(game);
    if(key!=NULL)
      fclose(key);
    ++i;
  }

  free(key_path);

  if(chk!=NULL)
    fwrite_chk(chk_tab+4, 1, 1, chk, "ERROR: Could not write 'f' to chk file");

  if(pad!=NULL)
    fclose(pad);
  if(dec!=NULL)
    fclose(dec);
  if(chk!=NULL)
    fclose(chk);

  return EXIT_SUCCESS;
}

/*general funcs****************************************************************/
void write_element_to_grp(FILE* grp, char* filename, unsigned int sec_count, char* hash_md5, char* hash_raw)
{
  char* formatting=NULL;

  assert(grp!=NULL);
  assert(filename!=NULL);
  assert(hash_md5!=NULL);
  assert(hash_raw!=NULL);

  formatting=malloc(45);

  formatting[0]=':';
  formatting[1]=0x0D;
  formatting[2]=0x0A;
  formatting[3]='0';
  formatting[44]=0;

  fwrite_chk(filename, 1, strlen(filename), grp, "ERROR: Could not write to grp_file (filename)");
  fwrite_chk(formatting, 1, 1, grp, "ERROR: Could not write to grp_file (colon [1])");
  if(sprintf(formatting+4, "%u", sec_count)<0)
    abort_err("ERROR: sprintf failed");
  fwrite_chk(formatting+4, 1, strlen(formatting+4), grp, "ERROR: Could not write to grp_file (2048)");
  fwrite_chk(formatting, 1, 1, grp, "ERROR: Could not write to grp_file (colon [2])");
  fwrite_chk(formatting+3, 1, 1, grp, "ERROR: Could not write to grp_file (2324)");
  fwrite_chk(formatting, 1, 1, grp, "ERROR: Could not write to grp_file (colon [3])");
  fwrite_chk(formatting+3, 1, 1, grp, "ERROR: Could not write to grp_file (2336)");
  fwrite_chk(formatting, 1, 1, grp, "ERROR: Could not write to grp_file (colon [4])");
  fwrite_chk(formatting+3, 1, 1, grp, "ERROR: Could not write to grp_file (2352)");
  fwrite_chk(formatting, 1, 1, grp, "ERROR: Could not write to grp_file (colon [5])");
  byte_to_hex(hash_md5, formatting+4, 16);
  fwrite_chk(formatting+4, 1, 32, grp, "ERROR: Could not write to grp_file (md5)");
  fwrite_chk(formatting, 1, 1, grp, "ERROR: Could not write to grp_file (colon [6])");
  byte_to_hex(hash_raw, formatting+4, 20);
  fwrite_chk(formatting+4, 1, 40, grp, "ERROR: Could not write to grp_file (sha1)");
  fwrite_chk(formatting+1, 1, 2, grp, "ERROR: Could not write to grp_file (newline)");

  free(formatting);

  fflush(grp);
}

int byte_to_hex(char* bytes, char* hex_str, unsigned int len)
{
  char hex[]={'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  unsigned int i=0;
  while(i<len)
  {
    hex_str[(i*2)  ]=hex[ (bytes[i]>>4) & 0x0F ];
    hex_str[(i*2)+1]=hex[ (bytes[i]   ) & 0x0F ];
    ++i;
  }
  return EXIT_SUCCESS;
}

void print_key(unsigned char* key, unsigned int len)
{
  unsigned int i=0;
  while(i<len)
  {
    fprintf(stderr, "%02X ", key[i]);
    ++i;
  }
  fputs("\n", stderr);
}

void write_to_chk_file(FILE* chk, char* hash, char* hash_scrub, char* hash_raw)
{
  assert(hash!=NULL);
  assert(hash_scrub!=NULL);
  assert(hash_raw!=NULL);

  if(memcmp(hash, hash_raw, 20)==0)
  {
    if(chk!=NULL)
      fwrite_chk(chk_tab, 1, 1, chk, "ERROR: Could not write to chk file (good:original)");
    fputs("Resulting image matches original image hash\n\n", stderr);
  }
  else if(memcmp(hash, hash_scrub, 20)==0)
  {
    if(chk!=NULL)
      fwrite_chk(chk_tab, 1, 1, chk, "ERROR: Could not write to chk file (good:scrubbed)");
    fputs("Resulting image matches scrubbed image hash\n\n", stderr);
  }
  else
  {
    if(chk!=NULL)
    {
      fwrite_chk(chk_tab+1, 1, 1, chk, "ERROR: Could not write to chk file (bad)");
      fclose(chk);
    }
    fprintf(stderr, "ERROR: Resulting image hash does not match expected\n");
    fprintf(stderr, "Expected (raw) : ");print_key(hash_raw, 20);
    fprintf(stderr, "OR (scrubbed)  : ");print_key(hash_scrub, 20);
    fprintf(stderr, "GOT            : ");print_key(hash, 20);
    abort_err("");
  }
}

void help()
{
  fputs("Usage:\n", stderr);
  fputs("\n", stderr);
  fputs("Encode (all except ps3):\n", stderr);
  fputs("GParse e <system> pad_file dec_file grp_file game_1 game_2 game_3 ...\n", stderr);
  fputs("\n", stderr);
  fputs("Encode (ps3):\n", stderr);
  fputs("GParse e <system> pad_file dec_file grp_file game_1 key_1 game_2 key_2 ...\n", stderr);
  fputs("\n", stderr);
  fputs("Decode:\n", stderr);
  fputs("GParse d pad_file dec_file chk_file game_1 game_2 game_3 ...\n", stderr);
  fputs("\n", stderr);
  fputs("<system> : wii for wii, ngc for gamecube, xbox for xbox, ps3 for ps3 (all lowercase)\n", stderr);
  fputs("pad_file : The file the padding will be/is located in\n", stderr);
  fputs("dec_file : The file the decrypted game data will be/is located in\n", stderr);
  fputs("grp_file : Stores names, sizes, hashes\n", stderr);
  fputs("chk_file : Allows external source to check status of execution\n", stderr);
  fputs("game_#   : The next game to process\n", stderr);
  fputs("key_#    : The key used to decrypt/encrypt game_# (disc_key, not d1)\n", stderr);
  fputs("\n", stderr);
  fputs("Note: Even systems without padding (ps3) have to include the pad_file in the\n", stderr);
  fputs("      commandline. It keeps things consistent and simple. Specify it as NULL\n", stderr);
  fputs("\n", stderr);
  fputs(" '-' in place of the file path indicates read from stdin (or write to stdout)\n", stderr);
  fputs(" 'NULL' in place of the file path indicates ignore file\n", stderr);
  fputs("\n", stderr);
  fputs("On encode, pad_file can be stdout, NULL or a file path\n", stderr);
  fputs("On encode, dec_file must be a file path\n", stderr);
  fputs("On encode, grp_file can be NULL or a file path\n", stderr);
  fputs("On decode, pad_file can be NULL or a file path\n", stderr);
  fputs("On decode, dec_file can be stdin or a file path\n", stderr);
  fputs("On decode, chk_file can be NULL or a file path\n", stderr);
  fputs("\n", stderr);
}

int mark_used(char* used_group, my_int64 start, my_int64 length, my_int64 chunk_size, unsigned char marker)
{
  my_int64 i;
  my_int64 end=(start+length)-1;

  if(length<=0)
    return EXIT_SUCCESS;

  if(start<0)
    return EXIT_SUCCESS;

  i=(start/chunk_size);
  while(i<(end/chunk_size)+1)
  {
    used_group[i]=marker;
    ++i;
  }
  return EXIT_SUCCESS;
}
int used_map_crunch(char* used_map, char* used_map_bit, unsigned int used_map_size)
{
  unsigned int i=0;
  while(i<(used_map_size/8))
  {
    used_map_bit[i]=0;
    used_map_bit[i]+= used_map[(i*8)+0]!='0' ? 128 : 0;
    used_map_bit[i]+= used_map[(i*8)+1]!='0' ?  64 : 0;
    used_map_bit[i]+= used_map[(i*8)+2]!='0' ?  32 : 0;
    used_map_bit[i]+= used_map[(i*8)+3]!='0' ?  16 : 0;
    used_map_bit[i]+= used_map[(i*8)+4]!='0' ?   8 : 0;
    used_map_bit[i]+= used_map[(i*8)+5]!='0' ?   4 : 0;
    used_map_bit[i]+= used_map[(i*8)+6]!='0' ?   2 : 0;
    used_map_bit[i]+= used_map[(i*8)+7]!='0' ?   1 : 0;
    ++i;
  }
  return EXIT_SUCCESS;
}
int used_map_uncrunch(char* used_map, char* used_map_bit, unsigned int used_map_size)
{
  unsigned int i=0;
  while(i<(used_map_size/8))
  {
    used_map[(i*8)+0]=0x30+((used_map_bit[i]>>7)%2);
    used_map[(i*8)+1]=0x30+((used_map_bit[i]>>6)%2);
    used_map[(i*8)+2]=0x30+((used_map_bit[i]>>5)%2);
    used_map[(i*8)+3]=0x30+((used_map_bit[i]>>4)%2);
    used_map[(i*8)+4]=0x30+((used_map_bit[i]>>3)%2);
    used_map[(i*8)+5]=0x30+((used_map_bit[i]>>2)%2);
    used_map[(i*8)+6]=0x30+((used_map_bit[i]>>1)%2);
    used_map[(i*8)+7]=0x30+((used_map_bit[i]>>0)%2);
    ++i;
  }
  return EXIT_SUCCESS;
}

/*ps3 funcs********************************************************************/
void process(unsigned char* data, int sector_count, int mode)
{
  int k=0;
#ifdef USE_OPENMP
  #pragma omp parallel for num_threads(THREAD_COUNT)
  for(k=0;k<sector_count;++k)
  {
    int id=omp_get_thread_num();
    reset_iv(&iv[16*id], global_lba+k);
    if(aes_crypt_cbc(aes[id], mode, 2048, &iv[16*id], &data[2048*k], &data[2048*k])!=0)
      abort_err(mode==MODE_ENCRYPT?"ERROR: AES encrypt failed":"ERROR: AES decrypt failed");
  }
#else
  while(k<sector_count)
  {
    if(aes_crypt_cbc(aes[0], mode, 2048, iv, &data[2048*k], &data[2048*k])!=0)
      abort_err(mode==MODE_ENCRYPT?"ERROR: AES encrypt failed":"ERROR: AES decrypt failed");
    ++k;
  }
#endif
  global_lba+=sector_count;
}

int process_ps3_game(FILE* in_file, FILE* out_file, unsigned char* key, unsigned char mode, sha1_context* md_in, sha1_context* md_out, md5_context* md5, unsigned int* iso_size_in_sectors)
{
  /*io*/
  unsigned char* in=NULL;

  /*loop variables*/
  unsigned int i;

  /*region variables*/
  char first=EXIT_SUCCESS;
  char plain=EXIT_SUCCESS;
  unsigned int regions;
  unsigned int region_last_sector;

  unsigned char* read_ptr;
  unsigned char* write_ptr;
  unsigned char* process_ptr;

  unsigned int num_blocks;
  unsigned int num_full_blocks;
  unsigned int curr_block;
  unsigned int partial_block_size;

#ifdef CHANGE_TITLE
  char* title;
  unsigned int total_sectors;
  unsigned int last_printed=0;
#endif

#ifdef REGION_HASH
  sha1_context* reg_hash_bef=NULL;
  sha1_context* reg_hash_aft=NULL;

  unsigned char reg_arr_bef[20];
  unsigned char reg_arr_aft[20];

  reg_hash_bef=malloc(sizeof(sha1_context));
  reg_hash_aft=malloc(sizeof(sha1_context));
#endif

  global_lba=0;

#ifdef CHANGE_TITLE
  title=malloc(4096);
#endif

  sec0sec1=malloc(4096);
  fread_chk(sec0sec1, 1, 4096, in_file, "ERROR: Could not read game header");

  fprintf(stderr, "Key:");
  print_key(key, 16);

  in=malloc(3*BUFFER_SIZE);/*triple sized buffer*/
  /*initialise aes*/
#ifdef USE_OPENMP
  aes=malloc(sizeof(aes_context*)*THREAD_COUNT);
  i=0;
  while(i<THREAD_COUNT)
  {
    aes[i]=malloc(sizeof(aes_context));
    if(mode==MODE_ENCRYPT)
    {
      if( aes_setkey_enc( aes[i], key, 128 )!=0 )
        abort_err("ERROR: AES encryption key initialisation failed");
    }
    else
    {
      if( aes_setkey_dec( aes[i], key, 128 )!=0 )
        abort_err("ERROR: AES decryption key initialisation failed");
    }
    ++i;
  }
  iv=malloc(16*THREAD_COUNT);
#else
  aes=malloc(sizeof(aes_context*));
  aes[0]=malloc(sizeof(aes_context));
  if(mode==MODE_ENCRYPT)
  {
    if( aes_setkey_enc( aes[0], key, 128 )!=0 )
      abort_err("ERROR: AES encryption key initialisation failed");
  }
  else
  {
    if( aes_setkey_dec( aes[0], key, 128 )!=0 )
      abort_err("ERROR: AES decryption key initialisation failed");
  }
  iv=malloc(16);
#endif

  /*read encryption layer from first sector*/
  regions=(char_arr_BE_to_uint(sec0sec1)*2)-1;
  if(regions==0 || regions==(unsigned int)-1)
    abort_err("ERROR: Could not find any regions, PS3 image may be invalid");

  total_sectors=1+char_arr_BE_to_uint(sec0sec1+12+((regions-1)*4));
  if(iso_size_in_sectors!=NULL)
    iso_size_in_sectors[0]=total_sectors;

  /*do every region*/
  i=0;
  while(i<regions)
  {
#ifdef REGION_HASH
    sha1_starts(reg_hash_bef);
    sha1_starts(reg_hash_aft);
#endif
    region_last_sector=char_arr_BE_to_uint(sec0sec1+12+(i*4));
    region_last_sector-= (plain==EXIT_SUCCESS?0:1);
    fprintf(stderr, "%s sectors %8u to %8u\n", (plain==EXIT_SUCCESS?"   Copying":(mode==MODE_DECRYPT?"Decrypting":"Encrypting")), global_lba, region_last_sector);
    fflush(stdout);

    num_full_blocks =    (1+region_last_sector-global_lba)/BUFFER_SIZE_SEC;
    partial_block_size = (1+region_last_sector-global_lba)%BUFFER_SIZE_SEC;
    num_blocks=num_full_blocks+ (partial_block_size==0?0:1);

    if(plain==EXIT_SUCCESS)
    {
      /*multi-threaded plain region code*/
      read_ptr=&in[0];
      write_ptr=&in[BUFFER_SIZE];

      /*read first block of region*/
      if(first==EXIT_SUCCESS)
      {
        /*to avoid seeking*/
        memcpy(read_ptr, sec0sec1, 4096);
        fread_chk(read_ptr+4096, 1, (num_full_blocks==0?2048*(partial_block_size-2):BUFFER_SIZE-4096), in_file, "ERROR: Could not read first block");
      }
      else
        fread_chk(read_ptr, 1, (num_full_blocks==0?2048*partial_block_size:BUFFER_SIZE), in_file, "ERROR: Could not read block");

      first=EXIT_FAIL;
      swap_ptrs(in, &read_ptr, &write_ptr, NULL);

      curr_block=1;
      while(curr_block<num_blocks)
      {
#ifdef CHANGE_TITLE
        sprintf(title, "GParse r%d [%2u%%] [Region %2u/%2u]", rev, ((100*global_lba)/total_sectors), i+1, regions);
        SetConsoleTitle(title);
        if( ((100*global_lba)/total_sectors)!=0 && ((100*global_lba)/total_sectors)%10==0 && ((100*global_lba)/total_sectors)!=last_printed)
        {
          fprintf(stderr, "%s\n", title);
          last_printed+=10;
        }
#endif
        /*simultaneous read/write*/
#ifdef USE_OPENMP
        #pragma omp parallel num_threads(5)
        {
          /*thread 0 reads, thread 1 writes, thread 2,3,4 hashes*/
          switch(omp_get_thread_num())
          {
            case 0:
              fread_chk(read_ptr, 1, (curr_block==num_full_blocks?2048*partial_block_size:BUFFER_SIZE), in_file, "ERROR: Failed to read block");
              break;

            case 1:
              fwrite_chk(write_ptr, 1, BUFFER_SIZE, out_file, "ERROR: Failed to write block");
              break;

            case 2:
#ifdef POLAR_SSL
              if(md_in!=NULL)
                sha1_update(md_in, write_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
              sha1_update(reg_hash_bef, write_ptr, BUFFER_SIZE);
#endif
              break;

            case 3:
#ifdef POLAR_SSL
              if(md_out!=NULL)
                sha1_update(md_out, write_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
              sha1_update(reg_hash_aft, write_ptr, BUFFER_SIZE);
#endif
              break;

            case 4:
#ifdef POLAR_SSL
              if(md5!=NULL)
                md5_update(md5, write_ptr, BUFFER_SIZE);
#endif
              break;
          }
        }
#else
        /*same as openmp code, just serial*/
        fread_chk(read_ptr, 1, (curr_block==num_full_blocks?2048*partial_block_size:BUFFER_SIZE), in_file, "ERROR: Failed to read block");
        fwrite_chk(write_ptr, 1, BUFFER_SIZE, out_file, "ERROR: Failed to write block");

#ifdef POLAR_SSL
        if(md_in!=NULL)
          sha1_update(md_in, write_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
        sha1_update(reg_hash_bef, write_ptr, BUFFER_SIZE);
#endif

#ifdef POLAR_SSL
        if(md_out!=NULL)
          sha1_update(md_out, write_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
        sha1_update(reg_hash_aft, write_ptr, BUFFER_SIZE);
#endif

#ifdef POLAR_SSL
        if(md5!=NULL)
          md5_update(md5, write_ptr, BUFFER_SIZE);
#endif

#endif
        global_lba+=BUFFER_SIZE_SEC;
        swap_ptrs(in, &read_ptr, &write_ptr, NULL);
        ++curr_block;
      }
      /*write last block*/
      fwrite_chk(write_ptr, 1, (partial_block_size==0?BUFFER_SIZE:2048*partial_block_size), out_file, "ERROR: Could not write last block");
#ifdef POLAR_SSL
      if(md_in!=NULL)
        sha1_update(md_in, write_ptr, (partial_block_size==0?BUFFER_SIZE:2048*partial_block_size));
      if(md_out!=NULL)
        sha1_update(md_out, write_ptr, (partial_block_size==0?BUFFER_SIZE:2048*partial_block_size));
      if(md5!=NULL)
        md5_update(md5, write_ptr, (partial_block_size==0?BUFFER_SIZE:2048*partial_block_size));
#endif
#ifdef REGION_HASH
      sha1_update(reg_hash_bef, write_ptr, (partial_block_size==0?BUFFER_SIZE:2048*partial_block_size));
      sha1_update(reg_hash_aft, write_ptr, (partial_block_size==0?BUFFER_SIZE:2048*partial_block_size));
#endif
      global_lba+=(partial_block_size==0?BUFFER_SIZE_SEC:partial_block_size);
    }
    else
    {
      /*multi-threaded encrypted/decrypted region*/
      read_ptr=&in[0];
      write_ptr=&in[BUFFER_SIZE];
      process_ptr=&in[2*BUFFER_SIZE];

      if(num_blocks<3)
      {
        /*escape for small regions, do io serially*/
        curr_block=0;
        while(curr_block<num_blocks)
        {
#ifdef CHANGE_TITLE
          sprintf(title, "GParse r%d [%2u%%] [Region %2u/%2u]", rev, ((100*global_lba)/total_sectors), i+1, regions);
          SetConsoleTitle(title);
          if( ((100*global_lba)/total_sectors)!=0 && ((100*global_lba)/total_sectors)%10==0 && ((100*global_lba)/total_sectors)!=last_printed)
          {
            fprintf(stderr, "%s\n", title);
            last_printed+=10;
          }
#endif
          fread_chk(read_ptr, 1, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC), in_file, "ERROR: Could not read block of small region");
#ifdef POLAR_SSL
          if(md_in!=NULL)
            sha1_update(md_in, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif
#ifdef REGION_HASH
          sha1_update(reg_hash_bef, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif

#ifdef POLAR_SSL
          if(md5!=NULL)
            md5_update(md5, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif

          process(read_ptr, (curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC), mode);
          fwrite_chk(read_ptr, 1, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC), out_file, "ERROR: Could not write block of small region");
#ifdef POLAR_SSL
          if(md_out!=NULL)
            sha1_update(md_out, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif
#ifdef REGION_HASH
          sha1_update(reg_hash_aft, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif
          ++curr_block;
        }
      }
      else
      {
        /*initial reading/processing to fill the buffers*/
        fread_chk(read_ptr, 1, BUFFER_SIZE, in_file, "ERROR: Could not read initial block of region");
#ifdef POLAR_SSL
        if(md_in!=NULL)
          sha1_update(md_in, read_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
        sha1_update(reg_hash_bef, read_ptr, BUFFER_SIZE);
#endif

#ifdef POLAR_SSL
        if(md5!=NULL)
          md5_update(md5, read_ptr, BUFFER_SIZE);
#endif
        swap_ptrs(in, &read_ptr, &write_ptr, &process_ptr);
        fread_chk(read_ptr, 1, BUFFER_SIZE, in_file, "ERROR: Could not read second block of region");
#ifdef POLAR_SSL
        if(md_in!=NULL)
          sha1_update(md_in, read_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
        sha1_update(reg_hash_bef, read_ptr, BUFFER_SIZE);
#endif

#ifdef POLAR_SSL
        if(md5!=NULL)
          md5_update(md5, read_ptr, BUFFER_SIZE);
#endif
        process(process_ptr, BUFFER_SIZE_SEC, mode);
        swap_ptrs(in, &read_ptr, &write_ptr, &process_ptr);
        /*the meat of the work*/
        curr_block=2;
        while(curr_block<num_blocks)
        {
#ifdef CHANGE_TITLE
          sprintf(title, "GParse r%d [%2u%%] [Region %2u/%2u]", rev, ((100*global_lba)/total_sectors), i+1, regions);
          SetConsoleTitle(title);
          if( ((100*global_lba)/total_sectors)!=0 && ((100*global_lba)/total_sectors)%10==0 && ((100*global_lba)/total_sectors)!=last_printed)
          {
            fprintf(stderr, "%s\n", title);
            last_printed+=10;
          }
#endif
#ifdef USE_OPENMP
          /*hashing hacked in. quicker (?) version would use 5x buffer and 5 threads for simultaneous io + hashing*/
          #pragma omp parallel num_threads(3)
          {
            switch(omp_get_thread_num())
            {
              case 0:/*read*/
                fread_chk(read_ptr, 1, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC), in_file, "ERROR: Could not read block of region");
#ifdef POLAR_SSL
                if(md_in!=NULL)
                  sha1_update(md_in, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif
#ifdef REGION_HASH
                sha1_update(reg_hash_bef, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif

#ifdef POLAR_SSL
                if(md5!=NULL)
                  md5_update(md5, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif
                break;
              case 1:/*write*/
                fwrite_chk(write_ptr, 1, BUFFER_SIZE, out_file, "ERROR: Could not write block of region");
#ifdef POLAR_SSL
                if(md_out!=NULL)
                  sha1_update(md_out, write_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
                sha1_update(reg_hash_aft, write_ptr, BUFFER_SIZE);
#endif
                break;
              case 2:/*process*/
                process(process_ptr, BUFFER_SIZE_SEC, mode);
                break;             
            }
          }
#else
          /*same as openmp code, just serial*/
          fread_chk(read_ptr, 1, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC), in_file, "ERROR: Could not read block of region");
#ifdef POLAR_SSL
          if(md_in!=NULL)
            sha1_update(md_in, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif
#ifdef REGION_HASH
          sha1_update(reg_hash_bef, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif

#ifdef POLAR_SSL
          if(md5!=NULL)
            md5_update(md5, read_ptr, 2048*(curr_block==num_full_blocks?partial_block_size:BUFFER_SIZE_SEC));
#endif
          fwrite_chk(write_ptr, 1, BUFFER_SIZE, out_file, "ERROR: Could not write block of region");
#ifdef POLAR_SSL
          if(md_out!=NULL)
            sha1_update(md_out, write_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
          sha1_update(reg_hash_aft, write_ptr, BUFFER_SIZE);
#endif
          process(process_ptr, BUFFER_SIZE_SEC, mode);
#endif
          swap_ptrs(in, &read_ptr, &write_ptr, &process_ptr);
          ++curr_block;
        }
        /*last processing/writing to empty the buffers*/
        process(process_ptr, (partial_block_size==0?BUFFER_SIZE_SEC:partial_block_size), mode);
        fwrite_chk(write_ptr, 1, BUFFER_SIZE, out_file, "ERROR: Could not write second-from-last block of region");
#ifdef POLAR_SSL
        if(md_out!=NULL)
          sha1_update(md_out, write_ptr, BUFFER_SIZE);
#endif
#ifdef REGION_HASH
        sha1_update(reg_hash_aft, write_ptr, BUFFER_SIZE);
#endif
        swap_ptrs(in, &read_ptr, &write_ptr, &process_ptr);
        fwrite_chk(write_ptr, 1, 2048*(partial_block_size==0?BUFFER_SIZE_SEC:partial_block_size), out_file, "ERROR: Could not write last block of region");
#ifdef POLAR_SSL
        if(md_out!=NULL)
          sha1_update(md_out, write_ptr, 2048*(partial_block_size==0?BUFFER_SIZE_SEC:partial_block_size));
#endif
#ifdef REGION_HASH
        sha1_update(reg_hash_aft, write_ptr, 2048*(partial_block_size==0?BUFFER_SIZE_SEC:partial_block_size));
#endif
      }
    }
    plain = (plain+1)%2;
    ++i;

#ifdef REGION_HASH
    sha1_finish(reg_hash_bef, reg_arr_bef);
    sha1_finish(reg_hash_aft, reg_arr_aft);

    fprintf(stderr, "Region %u before sha1: ", i);print_key(reg_arr_bef, 20);
    fprintf(stderr, "Region %u  after sha1: ", i);print_key(reg_arr_aft, 20);
#endif

  }

  /*cleanup*/
  i=0;
  while(i<THREAD_COUNT)
  {
    free(aes[i]);
    ++i;
  }
  free(aes);
  free(iv);
  free(in);
  free(sec0sec1);

  return EXIT_SUCCESS;
}

void reset_iv(unsigned char* iv, unsigned int j)
{
  memset(iv, 0, 12);

  iv[12] = (j & 0xFF000000)>>24;
  iv[13] = (j & 0x00FF0000)>>16;
  iv[14] = (j & 0x0000FF00)>> 8;
  iv[15] = (j & 0x000000FF)>> 0;
}

int sanatise_key(char* pot_key)
{
  unsigned int i=1;
  /*weed out invalid characters*/
  while(i<48)
  {
    if(strchr(pot_key, i)!=NULL)
      return EXIT_FAIL;
    ++i;
  }
  i=58;
  while(i<65)
  {
    if(strchr(pot_key, i)!=NULL)
      return EXIT_FAIL;
    ++i;
  }
  i=71;
  while(i<97)
  {
    if(strchr(pot_key, i)!=NULL)
      return EXIT_FAIL;
    ++i;
  }
  i=103;
  while(i<127)
  {
    if(strchr(pot_key, i)!=NULL)
      return EXIT_FAIL;
    ++i;
  }
  if(strchr(pot_key, i)!=NULL)
    return EXIT_FAIL;

  /*to uppercase*/
  i=0;
  while(i<32)
  {
    switch(pot_key[i])
    {
      case 'a':
        pot_key[i]='A';
      break;
      case 'b':
        pot_key[i]='B';
      break;
      case 'c':
        pot_key[i]='C';
      break;
      case 'd':
        pot_key[i]='D';
      break;
      case 'e':
        pot_key[i]='E';
      break;
      case 'f':
        pot_key[i]='F';
      break;
    }
    ++i;
  }

  return EXIT_SUCCESS;
}

void swap_ptrs(unsigned char* base, unsigned char** r, unsigned char** w, unsigned char**p)
{
  if(p==NULL)
  {
    r[0]=base+((r[0]+BUFFER_SIZE)-base)%(2*BUFFER_SIZE);
    w[0]=base+((w[0]+BUFFER_SIZE)-base)%(2*BUFFER_SIZE);
  }
  else
  {
    r[0]=base+((r[0]+BUFFER_SIZE)-base)%(3*BUFFER_SIZE);
    w[0]=base+((w[0]+BUFFER_SIZE)-base)%(3*BUFFER_SIZE);
    p[0]=base+((p[0]+BUFFER_SIZE)-base)%(3*BUFFER_SIZE);
  }
}

int hex_to_key(char* pot_key, unsigned char* key)
{
  /*sscanf crashes wine, so hack*/
  unsigned int count=0;
  char* tmp_key=NULL;
  tmp_key=malloc(32);
  count=0;
  while(count<32)
  {
    tmp_key[count] = pot_key[count]>57?pot_key[count]-55:pot_key[count]-48;
    ++count;
  }
  count=0;
  while(count<16)
  {
    key[count]=16*tmp_key[(count*2)];
    key[count]+=tmp_key[(count*2)+1];
    ++count;
  }
  /*sscanf
  char* pos = pot_key;
  unsigned int count = 0;
  while(count < 16)
  {
    if(sscanf(pos, "%2hhx", &key[count])!=1)
      return EXIT_FAIL;
    pos += 2;
    ++count;
  }*/
  return EXIT_SUCCESS;
}

/*wii funcs********************************************************************/
int decrypt_block(unsigned char* enc, unsigned char* dec, unsigned char* key, char* iv, size_t len)
{
#ifdef POLAR_SSL
  char synth_iv[16];
  aes_context* aes=NULL;

  memcpy(synth_iv, iv, 16);

  aes=malloc( sizeof(aes_context) );

  if( aes_setkey_dec( aes, key, 128 )!=0 )
    abort_err("ERROR: AES decryption key initialisation failed");

  if(len%16!=0)
    abort_err("ERROR: Block to decrypt must have a length a multiple of 16");

  if(aes_crypt_cbc( aes, AES_DECRYPT, len, synth_iv, enc, dec)!=0)
    abort_err("ERROR: Failed to decrypt block");

  free(aes);

  return EXIT_SUCCESS;
#endif
  return EXIT_FAIL;
}

int decrypt_block_quicker(unsigned char* enc, unsigned char* dec, aes_context* aes, char* iv, size_t len)
{
#ifdef POLAR_SSL
  char synth_iv[16];

  memcpy(synth_iv, iv, 16);

  if(len%16!=0)
    abort_err("ERROR: Block to decrypt must have a length a multiple of 16");

  if(aes_crypt_cbc( aes, AES_DECRYPT, len, synth_iv, enc, dec)!=0)
    abort_err("ERROR: Failed to decrypt block");

  return EXIT_SUCCESS;
#else
  return EXIT_FAIL;
#endif
}

int encrypt_block(unsigned char* enc, unsigned char* dec, unsigned char* key, char* iv, size_t len)
{
#ifdef POLAR_SSL
  char synth_iv[16];

  aes_context* aes=NULL;
  aes=malloc( sizeof(aes_context) );

  if( aes_setkey_enc( aes, key, 128 )!=0 )
    abort_err("ERROR: AES encryption key initialisation failed");

  if(iv==NULL)
  {
    memset(synth_iv, 0, 16);
    if(aes_crypt_cbc( aes, AES_ENCRYPT, len, synth_iv, dec, enc)!=0)
      abort_err("ERROR: Failed to encrypt block");
  }
  else
  {
    memcpy(synth_iv, iv, 16);
    if(aes_crypt_cbc( aes, AES_ENCRYPT, len, synth_iv, dec, enc)!=0)
      abort_err("ERROR: Failed to encrypt block");
  }

  free(aes);

  return EXIT_SUCCESS;
#else
  return EXIT_FAIL;
#endif
}

int encrypt_block_quicker(unsigned char* enc, unsigned char* dec, aes_context* aes, char* iv, size_t len)
{
#ifdef POLAR_SSL
  char synth_iv[16];

  if(iv==NULL)
  {
    memset(synth_iv, 0, 16);
    if(aes_crypt_cbc( aes, AES_ENCRYPT, len, synth_iv, dec, enc)!=0)
      abort_err("ERROR: Failed to encrypt block");
  }
  else
  {
    memcpy(synth_iv, iv, 16);
    if(aes_crypt_cbc( aes, AES_ENCRYPT, len, synth_iv, dec, enc)!=0)
      abort_err("ERROR: Failed to encrypt block");
  }

  return EXIT_SUCCESS;
#else
  return EXIT_FAIL;
#endif
}

int hash_block(unsigned char*block, size_t len, unsigned char* hash)
{
  assert(block!=NULL);
  assert(hash!=NULL);
#ifdef POLAR_SSL
  sha1(block, len, hash);
  return EXIT_SUCCESS;
#else
  return EXIT_FAIL;
#endif
}

int decrypt_group(unsigned char* enc_group, unsigned char* dec_group, unsigned char* title_key)
{
  int i;
  int j;
  unsigned char* check=NULL;
  unsigned char ret;
  aes_context** aes=NULL;

  assert(enc_group!=NULL);
  assert(dec_group!=NULL);
  assert(title_key!=NULL);

#ifdef USE_OPENMP
  aes=malloc(sizeof(aes_context*)*THREAD_COUNT);
  i=0;
  while(i<THREAD_COUNT)
  {
    aes[i]=malloc(sizeof(aes_context));
    if( aes_setkey_dec( aes[i], title_key, 128 )!=0 )
      abort_err("ERROR: AES decryption key initialisation failed");
    ++i;
  }
  #pragma omp parallel for num_threads(THREAD_COUNT)
  for(i=0;i<64;++i)
  {
    int id=omp_get_thread_num();
    decrypt_block_quicker(enc_group+(i*CLUSTER_SIZE)+1024, dec_group+(i*DEC_CLUSTER_SIZE), aes[id],  enc_group+(i*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
  }
  i=0;
  while(i<THREAD_COUNT)
  {
    free(aes[i]);
    ++i;
  }
#else
  aes=malloc(sizeof(aes_context*));
  aes[0]=malloc(sizeof(aes_context));
  if( aes_setkey_dec( aes[0], title_key, 128 )!=0 )
    abort_err("ERROR: AES decryption key initialisation failed");
  i=0;
  while(i<64)
  {
    decrypt_block_quicker(enc_group+(i*CLUSTER_SIZE)+1024, dec_group+(i*DEC_CLUSTER_SIZE), aes[0],  enc_group+(i*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
    ++i;
  }
  free(aes[0]);
#endif
  free(aes);

  check=malloc(2097152);
  j=encrypt_group(dec_group, check, title_key);
  ret= ( j==EXIT_FAIL || memcmp(enc_group, check, 2097152)!=0 )?EXIT_FAIL:EXIT_SUCCESS;
  free(check);
  return ret;
}

int encrypt_group(unsigned char* dec_group, unsigned char* enc_group, unsigned char* title_key)
{
  unsigned int j;
  int i;
  aes_context** aes;
  assert(enc_group!=NULL);
  assert(dec_group!=NULL);
  assert(title_key!=NULL);

  memset(enc_group, 0, 2097152);

  /*for every cluster, calculate H0*/
  i=0;/*cluster*/
  while(i<64)
  {
    j=0;/*KiB*/
    while(j<31)
    {
      hash_block(dec_group+(i*DEC_CLUSTER_SIZE)+(j*1024), 1024, &enc_group[(i*CLUSTER_SIZE)+(j*20)]);
      ++j;
    }
    ++i;
  }

  /*for every subgroup, calculate H1*/
  i=0;/*subgroup*/
  while(i<8)
  {
    j=0;/*cluster*/
    while(j<8)
    {
      hash_block(enc_group+(i*SUBGROUP_SIZE)+(j*CLUSTER_SIZE), 620, enc_group+(i*SUBGROUP_SIZE)+(j*20)+640);
      ++j;
    }

    /*copy H1 to every cluster in subgroup*/
    j=1;/*subgroup*/
    while(j<8)
    {
      memcpy(enc_group+(i*SUBGROUP_SIZE)+(j*CLUSTER_SIZE)+640, enc_group+(i*SUBGROUP_SIZE)+640, 20*8);
      ++j;
    }
    ++i;
  }

  /*calculate H2 for the group*/
  i=0;/*subgroup*/
  while(i<8)
  {
    hash_block(enc_group+(i*SUBGROUP_SIZE)+640, 160, enc_group+832+(i*20));
    ++i;
  }

  /*copy H2 to every cluster*/
  i=1;/*cluster*/
  while(i<64)
  {
    memcpy(enc_group+(i*CLUSTER_SIZE)+832, enc_group+832, 160);
    ++i;
  }

  /*init aes*/
#ifdef USE_OPENMP
  aes=malloc(sizeof(aes_context*)*THREAD_COUNT);
  i=0;
  while(i<THREAD_COUNT)
  {
    aes[i]=malloc(sizeof(aes_context));
    if( aes_setkey_enc( aes[i], title_key, 128 )!=0 )
      abort_err("ERROR: AES encryption key initialisation failed");
    ++i;
  }
#else
  aes=malloc(sizeof(aes_context*));
  if( aes_setkey_enc( aes[0], title_key, 128 )!=0 )
    abort_err("ERROR: AES encryption key initialisation failed");
#endif

  /*encrypt hashes*/
#ifdef USE_OPENMP
  #pragma omp parallel for num_threads(THREAD_COUNT)
  for(i=0;i<64;++i)/*cluster*/
  {
    int id=omp_get_thread_num();
    encrypt_block_quicker(enc_group+(i*CLUSTER_SIZE), enc_group+(i*CLUSTER_SIZE), aes[id], NULL, 1024);
  }
#else
  i=0;/*cluster*/
  while(i<64)
  {
    encrypt_block_quicker(enc_group+(i*CLUSTER_SIZE), enc_group+(i*CLUSTER_SIZE), aes[0], NULL, 1024);
    ++i;
  }
#endif

  /*encrypt data*/
#ifdef USE_OPENMP
  #pragma omp parallel for num_threads(THREAD_COUNT)
  for(i=0;i<64;++i)/*cluster*/
  {
    int id=omp_get_thread_num();
    encrypt_block_quicker(enc_group+(i*CLUSTER_SIZE)+1024, dec_group+(i*DEC_CLUSTER_SIZE), aes[id], enc_group+(i*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
  }
#else
  i=0;/*cluster*/
  while(i<64)
  {
    encrypt_block_quicker(enc_group+(i*CLUSTER_SIZE)+1024, dec_group+(i*DEC_CLUSTER_SIZE), aes[0], enc_group+(i*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
    ++i;
  }
#endif

  /*free aes*/
#ifdef USE_OPENMP
  i=0;
  while(i<THREAD_COUNT)
  {
    free(aes[i]);
    ++i;
  }
#else
  free(aes[0]);
#endif
  free(aes);

  return EXIT_SUCCESS;
}

int partition_sort(const void * a, const void * b)
{
  if(char_arr_BE_to_uint(*((char**)(a)))<char_arr_BE_to_uint(*((char**)(b))))
  {
    //printf("partition_sort: %u is less than %u\n", char_arr_BE_to_uint(*((char**)(a))), char_arr_BE_to_uint(*((char**)(b))) );
    return -1;
  }
  else if(char_arr_BE_to_uint(*((char**)(a)))>char_arr_BE_to_uint(*((char**)(b))))
  {
    //printf("partition_sort: %u is greater than %u\n", char_arr_BE_to_uint(*((char**)(a))), char_arr_BE_to_uint(*((char**)(b))) );
    return 1;
  }
  else
  {
    //printf("partition_sort: %u equals %u\n", char_arr_BE_to_uint(*((char**)(a))), char_arr_BE_to_uint(*((char**)(b))) );
    return 0;
  }
}

/*xbox funcs*******************************************************************/
void read_dir_entry(char* buffer, int loc, char* dir_table, unsigned int* dir_table_write_loc, unsigned char* used_map, my_int64 xdvdfs_loc)
{
  my_int64 curr_seek_val;
  my_int64 curr_size_val;

  assert(buffer!=NULL);
  assert(dir_table!=NULL);
  assert(dir_table_write_loc!=NULL);
  assert(used_map!=NULL);


  curr_seek_val=char_arr_LE_to_uint(buffer+loc+4);
  curr_seek_val*=2048;
  curr_seek_val+=xdvdfs_loc;
  curr_size_val=char_arr_LE_to_uint(buffer+loc+8);

  mark_used(used_map, curr_seek_val, curr_size_val, 2048, '1');
  if( (buffer[loc+12]>>4)%2==1 && curr_seek_val!=0 && curr_size_val!=0 )
  {
    /*is a directory*/
    memcpy(dir_table+(8*dir_table_write_loc[0]), buffer+loc+4, 8);
    ++dir_table_write_loc[0];
  }

  if(char_arr_LE_2_to_uint(buffer+loc)!=0)
    read_dir_entry(buffer, (char_arr_LE_2_to_uint(buffer+loc)*4), dir_table, dir_table_write_loc, used_map, xdvdfs_loc);
  if(char_arr_LE_2_to_uint(buffer+loc+2)!=0)
    read_dir_entry(buffer, (char_arr_LE_2_to_uint(buffer+loc+2)*4), dir_table, dir_table_write_loc, used_map, xdvdfs_loc);
}

/*system controllers***********************************************************/
int encode_game_ngc(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size)
{
#ifdef CHANGE_TITLE
  unsigned int chunk=0;
  unsigned char* console_title_builder=NULL;
#endif

  unsigned char* used_map=NULL;
  unsigned char* used_map_bit=NULL;
  unsigned int used_map_loc=0;

  unsigned char* zeroes=NULL;
  unsigned char* buffer=NULL;
  unsigned char* pad_buffer=NULL;
  unsigned char* dec_buffer=NULL;
  unsigned int pad_loc=0;
  unsigned int dec_loc=0;

  unsigned int dol_offset;
  unsigned int dol_size=0;
  unsigned int fst_offset;
  unsigned int fst_size;
  unsigned int fst_entries;
  unsigned int user_position;
  unsigned int user_length;
  unsigned int apploader_size;
  unsigned int apploader_trailer_size;

  unsigned char* fst_data=NULL;
  unsigned char* dol_header=NULL;

  unsigned char magic[]={0xc2, 0x33, 0x9f, 0x3d};

  unsigned int i, j;

  my_int64 stream_start_marker;
  my_int64 stream_end_marker;

  //FILE* f_peek;

#ifdef POLAR_SSL
  sha1_context* md_raw;
  sha1_context* md_scrub;
  md5_context* md5=NULL;

  unsigned char hash_raw[20];
  unsigned char hash_scrub[20];
  unsigned char hash_md5[16];

  md_raw=malloc(sizeof(sha1_context));
  md_scrub=malloc(sizeof(sha1_context));
  if(grp!=NULL)
    md5=malloc(sizeof(md5_context));

  sha1_starts(md_raw);
  sha1_starts(md_scrub);
  if(grp!=NULL)
    md5_starts(md5);
#endif

#ifdef CHANGE_TITLE
  console_title_builder=malloc(8192);
#endif

#ifdef VERBOSE
  fprintf(stderr, "---encode_game_ngc\n");
#endif

  my_fseek(game, 0, SEEK_END);
  if(my_ftell(game)!=1459978240)
    abort_err("ERROR: Game size not equal to 1,459,978,240 bytes (the size of a NGC game)");
  rewind(game);

  used_map=malloc(NGC_SECTOR_COUNT);
  used_map_bit=malloc(NGC_SECTOR_COUNT/8);
  memset(used_map, '0', NGC_SECTOR_COUNT);

  zeroes=malloc(2048);
  memset(zeroes, 0, 2048);

  buffer=malloc(NGC_BUFFER_SIZE);
  if(pad!=NULL)
    pad_buffer=malloc(NGC_BUFFER_SIZE);
  dec_buffer=malloc(NGC_BUFFER_SIZE);

  fread_chk(buffer, 1, BUFFER_SIZE, game, "ERROR: Failed to read start of game");

  /*buffer contains 'disc header' and 'disc header information'*/
  if(memcmp(magic, buffer+28, 4)!=0)
    abort_err("ERROR: Failed magic word test");

  dol_offset=char_arr_BE_to_uint(buffer+0x420);
  fst_offset=char_arr_BE_to_uint(buffer+0x424);
  fst_size=char_arr_BE_to_uint(buffer+0x428);
  user_position=char_arr_BE_to_uint(buffer+0x430);
  user_length=char_arr_BE_to_uint(buffer+0x434);
  apploader_size=char_arr_BE_to_uint(buffer+0x2440+0x14);
  apploader_trailer_size=char_arr_BE_to_uint(buffer+0x2440+0x18);

#ifdef VERBOSE
  fprintf(stderr, "dol_offset: %u\n", dol_offset);
  fprintf(stderr, "fst_offset: %u\n", fst_offset);
  fprintf(stderr, "fst_size: %u\n", fst_size);
  fprintf(stderr, "user_position: %u\n", user_position);
  fprintf(stderr, "user_length: %u\n", user_length);
  fprintf(stderr, "apploader_size: %u\n", apploader_size);
  fprintf(stderr, "apploader_trailer_size: %u\n", apploader_trailer_size);
#endif

  fst_data=malloc(fst_size);
  my_fseek(game, fst_offset, SEEK_SET);
  fread_chk(fst_data, 1, fst_size, game, "ERROR: Failed to read fst from game");


  fst_entries=char_arr_BE_to_uint(fst_data+8);

  mark_used(used_map, fst_offset, fst_size, 2048, '1');
  i=0;
  while(i<fst_entries)
  {
    if(fst_data[i*12]==0)
      mark_used(used_map, char_arr_BE_to_uint(fst_data+(i*12)+4), char_arr_BE_to_uint(fst_data+(i*12)+8), 2048, '1');//file
    ++i;
  }

  mark_used(used_map, 0, 0x440, 2048, '1');//header
  mark_used(used_map, 0x440, 0x2000, 2048, '1');//header information
  mark_used(used_map, 0x2440, apploader_size+apploader_trailer_size, 2048, '1');//apploader

  dol_header=malloc(216);
  my_fseek(game, dol_offset, SEEK_SET);
  fread_chk(dol_header, 1, 216, game, "ERROR: Failed to read dol header");
  i=0;
  while(i<18)
  {
    if( char_arr_BE_to_uint(dol_header+(i*4))+char_arr_BE_to_uint(dol_header+(i*4)+0x90)>dol_size )
      dol_size = char_arr_BE_to_uint(dol_header+(i*4))+char_arr_BE_to_uint(dol_header+(i*4)+0x90);
    ++i;
  }
  free(dol_header);

  mark_used(used_map, dol_offset, dol_size, 2048, '1');//dol

#ifdef VERBOSE
  fprintf(stderr, "fst_entries: %u\n", fst_entries);
  fprintf(stderr, "dol_size: %u\n", dol_size);
#endif

  used_map_crunch(used_map, used_map_bit, NGC_SECTOR_COUNT);

  stream_start_marker=my_ftell(dec);
  fwrite_chk(used_map_bit, 1, (NGC_SECTOR_COUNT/8), dec, "ERROR: Could not write map placeholder");//placeholder

  rewind(game);
  i=0;
  while(i<112)
  {
    fread_chk(buffer, 1, NGC_BUFFER_SIZE, game, "ERROR: Failed to read data from game");
#ifdef POLAR_SSL
    sha1_update(md_raw, buffer, NGC_BUFFER_SIZE);
    if(grp!=NULL)
      md5_update(md5, buffer, NGC_BUFFER_SIZE);
#endif
    j=0;
    while(j<NGC_BUFFER_SIZE/2048)
    {
#ifdef CHANGE_TITLE
      if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %6u/712880]", game_num, game_tot, used_map_loc)<0)
        abort_err("ERROR: sprintf failed");
      if(j%100==0)
        SetConsoleTitle(console_title_builder);
      if((used_map_loc/118000)==chunk)
      {
        fprintf(stderr, "%s\n", console_title_builder);
        fflush(stderr);
        ++chunk;
      }
#endif
      if(used_map[used_map_loc]=='0' && is_sec_filled(buffer+(j*2048))!=EXIT_SUCCESS )
      {
        //padding
#ifdef POLAR_SSL
        sha1_update(md_scrub, zeroes, 2048);
#endif
        {
          memcpy(pad_buffer+pad_loc, buffer+(j*2048), 2048);
          pad_loc+=2048;
          if(pad_loc==NGC_BUFFER_SIZE)
          {
            //flush buffer
            if(pad!=NULL)
              fwrite_chk(pad_buffer, 1, NGC_BUFFER_SIZE, pad, "ERROR: Could not write to pad file (NGC)");
            pad_size[0]+=NGC_BUFFER_SIZE;
            pad_loc=0;
          }
        }
      }
      else
      {
        //game_data
        used_map[used_map_loc]='1';
#ifdef POLAR_SSL
        sha1_update(md_scrub, buffer+(j*2048), 2048);
#endif
        memcpy(dec_buffer+dec_loc, buffer+(j*2048), 2048);
        dec_loc+=2048;
        if(dec_loc==NGC_BUFFER_SIZE)
        {
          //flush buffer
          fwrite_chk(dec_buffer, 1, NGC_BUFFER_SIZE, dec, "ERROR: Could not write to dec file (NGC)");
          dec_loc=0;
        }
      }
      ++j;
      ++used_map_loc;
    }
    ++i;
  }


  {
    if(pad_loc>0)
    {
      if(pad!=NULL)
        fwrite_chk(pad_buffer, 1, pad_loc, pad, "ERROR: Could not write last to pad file (NGC)");
      pad_size[0]+=pad_loc;
    }
  }
  if(dec_loc>0)
    fwrite_chk(dec_buffer, 1, dec_loc, dec, "ERROR: Could not write last to dec file (NGC)");

#ifdef POLAR_SSL
  sha1_finish(md_raw, hash_raw);
  sha1_finish(md_scrub, hash_scrub);
  if(grp!=NULL)
    md5_finish(md5, hash_md5);
#endif
  fwrite_chk(hash_raw, 1, 20, dec, "ERROR: Could not write raw hash to dec file");
  fwrite_chk(hash_scrub, 1, 20, dec, "ERROR: Could not write scrubbed hash to dec file");

  /*go back to map and rewrite it, some sectors may have moved from padding to dec*/
  stream_end_marker=my_ftell(dec);
  my_fseek(dec, stream_start_marker, SEEK_SET);
  used_map_crunch(used_map, used_map_bit, NGC_SECTOR_COUNT);
  fwrite_chk(used_map_bit, 1, (NGC_SECTOR_COUNT/8), dec, "ERROR: Could not write final map to dec file");
  my_fseek(dec, stream_end_marker, SEEK_SET);

  if(grp!=NULL)
  {
    free(md5);
    fprintf(stderr, "Original MD5:");print_key(hash_md5, 16);
  }
  fprintf(stderr, "Original SHA1:");print_key(hash_raw, 20);
  fprintf(stderr, "Scrubbed SHA1:");print_key(hash_scrub, 20);
  free(md_raw);
  free(md_scrub);


  if(grp!=NULL)
    write_element_to_grp(grp, filename, NGC_SECTOR_COUNT, hash_md5, hash_raw);

  free(zeroes);
  free(buffer);
  if(pad!=NULL)
    free(pad_buffer);
  free(dec_buffer);
  free(fst_data);

  free(used_map);
  free(used_map_bit);

#ifdef CHANGE_TITLE
  free(console_title_builder);
#endif

  return EXIT_SUCCESS;
}

int decode_game_ngc(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename)
{
  return decode_game_simple_map(game, pad, dec, chk, game_num, game_tot, filename, NGC_SECTOR_COUNT);
}
int encode_game_ps3(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size, char* key)
{
  unsigned char reserved=0x00;

  unsigned int iso_size_in_sectors;

#ifdef POLAR_SSL
  sha1_context* md_orig=NULL;
  sha1_context* md_dec=NULL;
  md5_context* md5=NULL;

  unsigned char hash_orig[20];
  unsigned char hash_dec[20];
  unsigned char hash_md5[16];

  md_orig=malloc(sizeof(sha1_context));
  md_dec=malloc(sizeof(sha1_context));
  if(grp!=NULL)
    md5=malloc(sizeof(md5_context));

  sha1_starts(md_orig);
  sha1_starts(md_dec);
  if(grp!=NULL)
    md5_starts(md5);
#endif

  /*reserved byte*/
  fwrite_chk(&reserved, 1, 1, dec, "ERROR: Failed to write format byte");
  /*key*/
  fwrite_chk(key, 1, 16, dec, "ERROR: Failed to write key");
  /*code from ps3dec*/
  process_ps3_game(game, dec, key, MODE_DECRYPT, md_orig, md_dec, md5, &iso_size_in_sectors);

#ifdef POLAR_SSL
    sha1_finish(md_orig, hash_orig);
    sha1_finish(md_dec, hash_dec);
    if(grp!=NULL)
      md5_finish(md5, hash_md5);
#endif
    fwrite_chk(hash_orig, 1, 20, dec, "ERROR: Could not write original hash to dec file");
    fwrite_chk(hash_dec, 1, 20, dec, "ERROR: Could not write decrypted hash to dec file");

    if(grp!=NULL)
    {
      free(md5);
      fprintf(stderr, "Original  MD5:");print_key(hash_md5, 16);
    }
    fprintf(stderr, "Original  SHA1:");print_key(hash_orig, 20);
    fprintf(stderr, "Decrypted SHA1:");print_key(hash_dec, 20);
    free(md_orig);
    free(md_dec);

  if(grp!=NULL)
    write_element_to_grp(grp, filename, iso_size_in_sectors, hash_md5, hash_orig);

  return EXIT_SUCCESS;
}
int decode_game_ps3(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename, FILE* key_f)
{
  unsigned char reserved;
  unsigned char key[16];
  char key_ascii[32];

  unsigned char hash_chk_orig[20];
  unsigned char hash_chk_dec[20];

#ifdef POLAR_SSL
  sha1_context* md_orig=NULL;
  sha1_context* md_dec=NULL;

  unsigned char hash_orig[20];
  unsigned char hash_dec[20];

  md_orig=malloc(sizeof(sha1_context));
  md_dec=malloc(sizeof(sha1_context));

  sha1_starts(md_orig);
  sha1_starts(md_dec);
#endif

  fread_chk(&reserved, 1, 1, dec, "ERROR: Could not read reserved byte from dec_file");
  if(reserved!=0)
    abort_err("ERROR: Reserved byte non-zero. Either an invalid dec_file, or was created with\n       a newer version of this program");

  fread_chk(key, 1, 16, dec, "ERROR: Could not read key from dec_file");
  if(key_f!=NULL)
  {
    byte_to_hex(key, key_ascii, 16);
    fwrite_chk(key_ascii, 1, 32, key_f, "ERROR: Could not write to key file");
  }

  process_ps3_game(dec, game, key, MODE_ENCRYPT, md_dec, md_orig, NULL, NULL);
#ifdef POLAR_SSL
  sha1_finish(md_orig, hash_orig);
  sha1_finish(md_dec, hash_dec);
#endif

  fread_chk(hash_chk_orig,   1, 20, dec, "ERROR: Failed to read original hash from dec file");
  fread_chk(hash_chk_dec, 1, 20, dec, "ERROR: Failed to read decrypted hash from dec file");

  write_to_chk_file(chk, hash_orig, hash_chk_dec, hash_chk_orig);

  return EXIT_SUCCESS;
}
int encode_game_wii(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size)
{
#ifdef CHANGE_TITLE
  unsigned int chunk=0;
  unsigned char* console_title_builder=NULL;
#endif

  my_int64 loc_marker;
  my_int64 curr_seek_val;

  my_int64 iso_size;
  unsigned int iso_size_in_sectors;
  unsigned char image_type;

  unsigned char* buffer=NULL;
  unsigned char* buffer2=NULL;
  unsigned char* zeroes=NULL;

  char* used_group=NULL;

  unsigned char* partition_info=NULL;
  unsigned char* curr_pointer;

  unsigned int partition_count=0;

  unsigned char* partition_table_entries=NULL;
  unsigned char** partition_table_entries_sorted=NULL;

  my_int64 apploader_size;

  my_int64 loc_of_dol;
  my_int64 size_of_dol;
  unsigned int loc_of_fst;
  unsigned int size_of_fst;
  unsigned int loc_of_fst_in_buffer;
  unsigned int group_fst_starts_in;
  unsigned int clusters_fst_spans;

  unsigned int i, j, k, m;

  unsigned char curr_iv[16];
  unsigned char curr_key[16];

  unsigned int curr_num_clusters;
  unsigned int curr_clusters_read;
  unsigned int curr_num_groups;
  unsigned int buffer_used;
  unsigned int numfiles;

  unsigned char common_key_nor[]={0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};
  unsigned char common_key_kor[]={0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e, 0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e};
  unsigned char* common_key;

  unsigned int curr_sector;

  unsigned char group_byte;

  unsigned char to_arr[4];

#ifdef POLAR_SSL
  sha1_context* md_raw;
  sha1_context* md_scrub;
  md5_context* md5=NULL;

  unsigned char hash_raw[20];
  unsigned char hash_scrub[20];
  unsigned char hash_md5[16];

  md_raw=malloc(sizeof(sha1_context));
  md_scrub=malloc(sizeof(sha1_context));
  if(grp!=NULL)
    md5=malloc(sizeof(md5_context));

  sha1_starts(md_raw);
  sha1_starts(md_scrub);
  if(grp!=NULL)
    md5_starts(md5);
#endif

#ifdef CHANGE_TITLE
  console_title_builder=malloc(8192);
#endif

  my_fseek(game, 0, SEEK_END);
  iso_size=my_ftell(game);
  my_fseek(game, 0, SEEK_SET);
  iso_size_in_sectors=(unsigned int)(iso_size/2048);
  if(iso_size_in_sectors==2294912)
    image_type=1;//single layer
  else if(iso_size_in_sectors==4155840)
    image_type=2;//dual layer
  else
    abort_err("ERROR: Image has incorrect size for a wii disc (2294912 or 4155840\n        sectors only)");

  buffer=malloc(2048*4096);
  buffer2=malloc(2048*4096);
  zeroes=malloc(2048*4096);
  memset(zeroes, 0, 2048*4096);

  used_group=malloc(4608);


  fread_chk(buffer, 1, 2048*160, game, "ERROR: Failed to read image header");

  /*read header*/
  {
    if(char_arr_BE_to_uint(buffer+0x18)!=0x5D1C9EA3)
      abort_err("ERROR: Wii magic word not present in header. Input not a wii disc image");
  }

  /*read partition info*/
  {
    /*count partitions*/
    partition_info=buffer+(128*2048);

    partition_count+=char_arr_BE_to_uint(partition_info);
    partition_count+=char_arr_BE_to_uint(partition_info+8);
    partition_count+=char_arr_BE_to_uint(partition_info+16);
    partition_count+=char_arr_BE_to_uint(partition_info+24);

    partition_table_entries=malloc(8*partition_count);

    /*collate 4 partition tables to one*/
    curr_pointer=partition_table_entries;
    i=0;
    while(i<4)
    {
      curr_seek_val=char_arr_BE_to_uint(partition_info+4+(i*8));
      curr_seek_val*=4;
      my_fseek(game, curr_seek_val, SEEK_SET);

      fread_chk(curr_pointer, 1, 8*char_arr_BE_to_uint(partition_info+(i*8)), game, "ERROR: Failed to read partition tables");

      curr_pointer+=8*char_arr_BE_to_uint(partition_info+(i*8));
      ++i;
    }
    partition_info=NULL;
  }

  /*Sort partitions into order in iso*/
  partition_table_entries_sorted=malloc(sizeof(unsigned char*)*partition_count);
  i=0;
  while(i<partition_count)
  {
    partition_table_entries_sorted[i]=&partition_table_entries[i*8];
    ++i;
  }
  qsort(partition_table_entries_sorted, partition_count, sizeof(unsigned char*), partition_sort);

  /*write everything*/
  {
    fwrite_chk(&image_type, 1, 1, dec, "ERROR: Could not write layer count to dec file");
    uint_to_char_arr_BE(partition_count, to_arr);
    fwrite_chk(to_arr, 1, 4, dec, "ERROR: Could not write partition count to dec file");
    i=0;
    while(i<partition_count)
    {
      fwrite_chk(partition_table_entries_sorted[i], 1, 8, dec, "ERROR: Could not write global partition table to dec file");
      ++i;
    }

    my_fseek(game, 0, SEEK_SET);
    fputs("[Write header]\n", stderr);

    fread_chk(buffer, 1, 2048*160, game, "ERROR: Failed to read image header [2]");

    fwrite_chk(buffer, 1, 2048*160, dec, "ERROR: Could not write header to dec file");

#ifdef POLAR_SSL
    sha1_update(md_raw, buffer, 2048*160);
    sha1_update(md_scrub, buffer, 2048*160);
    if(grp!=NULL)
      md5_update(md5, buffer, 2048*160);
#endif
    curr_sector=160;

    i=0;
    while(i<partition_count)
    {
      /*padding*/
      if( curr_sector<((char_arr_BE_to_uint(partition_table_entries_sorted[i])/512)) )
        fprintf(stderr, "[Write inter-partition (%u sectors)]\n", ((char_arr_BE_to_uint(partition_table_entries_sorted[i])/512))-curr_sector );
      while( curr_sector<((char_arr_BE_to_uint(partition_table_entries_sorted[i])/512)) )
      {
#ifdef CHANGE_TITLE
        if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %7u/%u] [Inter-Partition %u/%u]", game_num, game_tot, curr_sector, iso_size_in_sectors, curr_sector-160, (char_arr_BE_to_uint(partition_table_entries_sorted[i])/512) )<0)
          abort_err("ERROR: sprintf failed");
        if(curr_sector%100==0)
          SetConsoleTitle(console_title_builder);
        if((curr_sector/229400)==chunk)
        {
          fprintf(stderr, "%s\n", console_title_builder);
          fflush(stderr);
          ++chunk;
        }
#endif
        buffer_used = ((char_arr_BE_to_uint(partition_table_entries_sorted[i])/512)-curr_sector)>=4096 ? 4096 : ((char_arr_BE_to_uint(partition_table_entries_sorted[i])/512)-curr_sector);
        fread_chk(buffer, 1, 2048*buffer_used, game, "ERROR: Failed to read inter-partition");
        fwrite_chk(buffer, 1, 2048*buffer_used, dec, "ERROR: Could not write inter-partition to dec file");
#ifdef POLAR_SSL
        sha1_update(md_raw, buffer, 2048*buffer_used);
        sha1_update(md_scrub, buffer, 2048*buffer_used);
        if(grp!=NULL)
          md5_update(md5, buffer, 2048*buffer_used);
#endif
        curr_sector+=buffer_used;
      }

      /*partition*/
      fputs("[Write partition]\n", stderr);
      fread_chk(buffer, 1, 2048*64, game, "ERROR: Failed to read partition header");
      fwrite_chk(buffer, 1, 2048*64, dec, "ERROR: Could not write partition header to dec file");

#ifdef POLAR_SSL
      sha1_update(md_raw, buffer, 2048*64);
      sha1_update(md_scrub, buffer, 2048*64);
      if(grp!=NULL)
        md5_update(md5, buffer, 2048*64);
#endif

      curr_sector+=64;
      /*read Ticket*/
      common_key= buffer[497]==1 ? common_key_kor : common_key_nor;

      memcpy(curr_iv, buffer+476, 8);

      memset(curr_iv+8, 0, 8);

      decrypt_block(buffer+447, curr_key, common_key, curr_iv, 16);
      curr_num_clusters=char_arr_BE_to_uint(buffer+700)/8192;

      {
        //start of partition data, build table showing what's data and what's padding

        loc_marker=my_ftell(game);

        /*fill buffers (temp, need to ftell to fseek later)*/
        curr_clusters_read = curr_num_clusters>=256?256:curr_num_clusters;
        fread_chk(buffer, 1, curr_clusters_read*CLUSTER_SIZE, game, "ERROR: Failed to read start of partition data");

        //decrypt buffer
        m=0;
        while(m<curr_clusters_read)
        {
          decrypt_block(buffer+(m*CLUSTER_SIZE)+1024, buffer2+(m*DEC_CLUSTER_SIZE), curr_key, buffer+(m*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
          ++m;
        }

        memset(used_group, '0', 4608);

        /*header*/
        mark_used(used_group, 0, 9280, DEC_GROUP_SIZE, '1');

        /*apploader*/
        apploader_size=char_arr_BE_to_uint(buffer2+9280+20)+char_arr_BE_to_uint(buffer2+9280+24);
        if(apploader_size%32!=0)/*round up to 32*/
        {
          apploader_size/=32;
          ++apploader_size;
          apploader_size*=32;
        }
        mark_used(used_group, 9280, apploader_size, DEC_GROUP_SIZE, '1');

        /*dol*/
        /*treats dol as contiguous, according to dolphin*/
        /*ignore BSS, according to dolphin*/
        /*BSS appears to be something to do with ram, or at least not something for us*/
        loc_of_dol=char_arr_BE_to_uint(buffer2+0x420);
        loc_of_dol*=4;
        size_of_dol=0;
        j=0;
        while(j<18)
        {
          if( char_arr_BE_to_uint(buffer2+loc_of_dol+(j*4))+char_arr_BE_to_uint(buffer2+loc_of_dol+(j*4)+0x90)>size_of_dol )
            size_of_dol = char_arr_BE_to_uint(buffer2+loc_of_dol+(j*4))+char_arr_BE_to_uint(buffer2+loc_of_dol+(j*4)+0x90);
          ++j;
        }
        mark_used(used_group, loc_of_dol, size_of_dol, DEC_GROUP_SIZE, '1');

        /*fst*/
        loc_of_fst=char_arr_BE_to_uint(buffer2+0x424);
        loc_of_fst*=4;
        group_fst_starts_in=loc_of_fst/DEC_GROUP_SIZE;
        loc_of_fst_in_buffer=loc_of_fst%DEC_GROUP_SIZE;
        size_of_fst=char_arr_BE_to_uint(buffer2+0x428);
        size_of_fst*=4;

        my_fseek(game, loc_marker+(GROUP_SIZE*group_fst_starts_in), SEEK_SET);
        clusters_fst_spans=(loc_of_fst_in_buffer+size_of_fst)/DEC_CLUSTER_SIZE;
        clusters_fst_spans+=  (loc_of_fst_in_buffer+size_of_fst)%DEC_CLUSTER_SIZE!=0 ? 1:0;

        if(clusters_fst_spans>256)
          abort_err("ERROR: FST is massive! Too big to fit into a measly 8MiB buffer");

        curr_clusters_read = clusters_fst_spans;
        fread_chk(buffer, 1, curr_clusters_read*CLUSTER_SIZE, game, "ERROR: Failed to read fst of partition");

        //decrypt buffer
        m=0;
        while(m<curr_clusters_read)
        {
          decrypt_block(buffer+(m*CLUSTER_SIZE)+1024, buffer2+(m*DEC_CLUSTER_SIZE), curr_key, buffer+(m*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
          ++m;
        }

        /*
        //old way, calculate manually
        size_of_fst=char_arr_BE_to_uint(buffer2+loc_of_fst+8);
        loc_of_str=loc_of_fst+(12*size_of_fst);
        size_of_str=0;
        k=0;
        while(k<size_of_fst-1)
        {
          if(buffer2[loc_of_str+size_of_str]==0)
            ++k;
          ++size_of_str;
        }
        mark_used(used_group, loc_of_str, size_of_str, DEC_GROUP_SIZE, '1');*/
        mark_used(used_group, loc_of_fst, size_of_fst, DEC_GROUP_SIZE, '1');

        numfiles=char_arr_BE_to_uint(buffer2+loc_of_fst_in_buffer+8);
        //printf("Number of files in fst: %u\n", numfiles);
        /*files*/
        k=0;
        while(k<numfiles)
        {
          if(buffer2[loc_of_fst_in_buffer+(k*12)]==0)/*not dir aka is file*/
          {
            mark_used( used_group, 4*char_arr_BE_to_uint(buffer2+loc_of_fst_in_buffer+(k*12)+4), char_arr_BE_to_uint(buffer2+loc_of_fst_in_buffer+(k*12)+8), DEC_GROUP_SIZE, '1' );
          }
          ++k;
        }

        curr_num_groups=curr_num_clusters/64;
        if(curr_num_clusters%64!=0)
          ++curr_num_groups;
      }


      my_fseek(game, loc_marker, SEEK_SET);
      j=0;
      while(j<(curr_num_groups-1))
      {
#ifdef CHANGE_TITLE
        if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %7u/%u] [Partition %u/%u] [Group %u/%u]", game_num, game_tot, curr_sector, iso_size_in_sectors, i+1, partition_count, j, curr_num_groups)<0)
          abort_err("ERROR: sprintf failed");
        SetConsoleTitle(console_title_builder);
        if((curr_sector/229400)==chunk)
        {
          fprintf(stderr, "%s\n", console_title_builder);
          fflush(stderr);
          ++chunk;
        }
#endif
        curr_clusters_read = 64;
        fread_chk(buffer, 1, GROUP_SIZE, game, "ERROR: Failed to read group from partition");
        /*hash*/
#ifdef POLAR_SSL
        sha1_update(md_raw, buffer, CLUSTER_SIZE*curr_clusters_read);
        if(grp!=NULL)
          md5_update(md5, buffer, CLUSTER_SIZE*curr_clusters_read);
        if(used_group[j]=='0')//padding
          sha1_update(md_scrub, zeroes, CLUSTER_SIZE*curr_clusters_read);
        else//data
          sha1_update(md_scrub, buffer, CLUSTER_SIZE*curr_clusters_read);
#endif

        group_byte = decrypt_group(buffer, buffer2, curr_key)==EXIT_SUCCESS ? 0 : 1;//verbatim==1

        if(used_group[j+1]=='0')
          group_byte+=2;//next group is in padding

        fwrite_chk(&group_byte, 1, 1, dec, "ERROR: Could not write group byte to dec file");
        if(used_group[j]=='0')//padding
        {
          {
            if((group_byte%2)==0)//write decrypted
            {
              if(pad!=NULL)
                fwrite_chk(buffer2, 1, DEC_CLUSTER_SIZE*curr_clusters_read, pad, "ERROR: Could not write decrypted group to pad file");
              pad_size[0]+=DEC_CLUSTER_SIZE*curr_clusters_read;
            }
            else//write encrypted, ie write hashes encrypted, data decrypted
            {
              k=0;
              while(k<curr_clusters_read)
              {
                memcpy( buffer+(k*CLUSTER_SIZE)+1024, buffer2+(k*DEC_CLUSTER_SIZE), DEC_CLUSTER_SIZE );
                ++k;
              }
              if(pad!=NULL)
                fwrite_chk(buffer, 1, CLUSTER_SIZE*curr_clusters_read, pad, "ERROR: Could not write decrypted group + hashes to pad file");
              pad_size[0]+=CLUSTER_SIZE*curr_clusters_read;
            }
          }
        }
        else//data
        {
          if((group_byte%2)==0)//write decrypted
            fwrite_chk(buffer2, 1, DEC_CLUSTER_SIZE*curr_clusters_read, dec, "ERROR: Could not write decrypted group to dec file");
          else//write encrypted, ie write hashes encrypted, data decrypted
          {
            k=0;
            while(k<curr_clusters_read)
            {
              memcpy( buffer+(k*CLUSTER_SIZE)+1024, buffer2+(k*DEC_CLUSTER_SIZE), DEC_CLUSTER_SIZE );
              ++k;
            }
            fwrite_chk(buffer, 1, CLUSTER_SIZE*curr_clusters_read, dec, "ERROR: Could not write decrypted group + hashes to dec file");
          }
        }
        curr_sector+=(16*curr_clusters_read);

        ++j;
      }

      {//last group
        group_byte=129;//last_group + verbatim
        group_byte+=((curr_num_clusters%64)*2);//num_clusters%64
        curr_clusters_read= (curr_num_clusters%64)==0?64:(curr_num_clusters%64);
        fread_chk(buffer, 1, curr_clusters_read*CLUSTER_SIZE, game, "ERROR: Failed to read last group from partition");

        /*hash*/
#ifdef POLAR_SSL
        sha1_update(md_raw, buffer, CLUSTER_SIZE*curr_clusters_read);
        if(grp!=NULL)
          md5_update(md5, buffer, CLUSTER_SIZE*curr_clusters_read);
        if(used_group[j]=='0')
          sha1_update(md_scrub, zeroes, CLUSTER_SIZE*curr_clusters_read);
        else
          sha1_update(md_scrub, buffer, CLUSTER_SIZE*curr_clusters_read);
#endif

        k=0;
        while(k<curr_clusters_read)
        {
          decrypt_block(buffer+(k*CLUSTER_SIZE)+1024, buffer2+(k*DEC_CLUSTER_SIZE), curr_key,  buffer+(k*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
          memcpy( buffer+(k*CLUSTER_SIZE)+1024, buffer2+(k*DEC_CLUSTER_SIZE), DEC_CLUSTER_SIZE );
          ++k;
        }
        /*as a hack, don't decrypt, just store verbatim*/
        /*avoids potential problems with regenerating hashes and encrypting*/
        /*with only a partial group*/
        fwrite_chk(&group_byte, 1, 1, dec, "ERROR: Could not write last group byte to dec file");
        if(used_group[j]=='0')
        {
          if(pad!=NULL)
            fwrite_chk(buffer, 1, CLUSTER_SIZE*curr_clusters_read, pad, "ERROR: Could not write last group to pad file");
          pad_size[0]+=CLUSTER_SIZE*curr_clusters_read;
        }
        else
          fwrite_chk(buffer, 1, CLUSTER_SIZE*curr_clusters_read, dec, "ERROR: Could not write last group to dec file");
        curr_sector+=(16*curr_clusters_read);
      }

      ++i;
    }
    if(curr_sector<iso_size_in_sectors)
      fputs("[Write trailing sectors]\n\n", stderr);
    while( curr_sector<iso_size_in_sectors )
    {
#ifdef CHANGE_TITLE
      if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %7u/%u] [Trailing Sectors]", game_num, game_tot, curr_sector, iso_size_in_sectors)<0)
        abort_err("ERROR: sprintf failed");
      if(curr_sector%100==0)
        SetConsoleTitle(console_title_builder);
      if((curr_sector/229400)==chunk)
      {
        fprintf(stderr, "%s\n", console_title_builder);
        fflush(stderr);
        ++chunk;
      }
#endif
      buffer_used = (iso_size_in_sectors-curr_sector)>=4096 ? 4096 : (iso_size_in_sectors-curr_sector);
      fread_chk(buffer, 1, 2048*buffer_used, game, "ERROR: Failed to read trailing sectors from image");
      fwrite_chk(buffer, 1, 2048*buffer_used, dec, "ERROR: Could not write trailing sectors to dec file");
#ifdef POLAR_SSL
      sha1_update(md_raw, buffer, 2048*buffer_used);
      sha1_update(md_scrub, buffer, 2048*buffer_used);
      if(grp!=NULL)
        md5_update(md5, buffer, 2048*buffer_used);
#endif
      curr_sector+=buffer_used;
    }

    if(pad!=NULL)
      fflush(pad);
    if(dec!=NULL)
      fflush(dec);

#ifdef POLAR_SSL
    sha1_finish(md_raw, hash_raw);
    sha1_finish(md_scrub, hash_scrub);
    if(grp!=NULL)
      md5_finish(md5, hash_md5);
#endif
    fwrite_chk(hash_raw, 1, 20, dec, "ERROR: Could not write raw hash to dec file");
    fwrite_chk(hash_scrub, 1, 20, dec, "ERROR: Could not write scrubbed hash to dec file");

    if(grp!=NULL)
    {
      free(md5);
      fprintf(stderr, "Original MD5:");print_key(hash_md5, 16);
    }
    fprintf(stderr, "Original SHA1:");print_key(hash_raw, 20);
    fprintf(stderr, "Scrubbed SHA1:");print_key(hash_scrub, 20);
    free(md_raw);
    free(md_scrub);
  }

  if(grp!=NULL)
    write_element_to_grp(grp, filename, iso_size_in_sectors, hash_md5, hash_raw);

  free(buffer);
  free(buffer2);
  free(zeroes);

  free(used_group);

  free(partition_table_entries);
  free(partition_table_entries_sorted);

#ifdef CHANGE_TITLE
  free(console_title_builder);
#endif


  fputs("\n", stderr);

  return EXIT_SUCCESS;
}
int decode_game_wii(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename)
{
  unsigned int chunk=0;

  unsigned int curr_num_groups;

  unsigned char* curr_iv=NULL;
  unsigned char* curr_hash=NULL;
  unsigned int curr_num_clusters;

  unsigned int curr_sector=0;

  char* curr_string=NULL;

  unsigned int i, j;
  int k;

  unsigned char common_key_nor[]={0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};
  unsigned char common_key_kor[]={0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e, 0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e};
  unsigned char* common_key;



  unsigned char* buffer=NULL;
  unsigned char* buffer2=NULL;
  unsigned char* zeroes=NULL;

  unsigned int buffer_used;

  my_int64* partition_mask_offset=NULL;
  my_int64* partition_mask_length=NULL;

  char* curr_key=NULL;
  char* console_title_builder=NULL;

  char* partition_table_entries=NULL;

  unsigned int partition_count=0;

  unsigned char group_byte;
  unsigned char prev_group_byte;

  unsigned char image_type;
  unsigned int iso_size_in_sectors;

#ifdef POLAR_SSL
  sha1_context* md;
  unsigned char hash[20];
  unsigned char hash_raw[20];
  unsigned char hash_scrub[20];

  md=malloc(sizeof(sha1_context));

  sha1_starts(md);
#endif

  buffer=malloc(BUFFER_SIZE);
  buffer2=malloc(BUFFER_SIZE);
  zeroes=malloc(BUFFER_SIZE);
  partition_mask_offset=malloc(BUFFER_SIZE);
  partition_mask_length=malloc(BUFFER_SIZE);
  curr_iv=malloc(16);
  curr_hash=malloc(16);
  curr_key=malloc(16);
#ifdef CHANGE_TITLE
  console_title_builder=malloc(4096);
#endif


  fread_chk(&image_type, 1, 1, dec, "ERROR: Failed to read layer count from dec file");
  if(image_type==1)
    iso_size_in_sectors=2294912;
  else if(image_type==2)
    iso_size_in_sectors=4155840;
  else
  {
    if(chk!=NULL)
    {
      fwrite_chk(chk_tab+2, 1, 1, chk, "ERROR: Could not write 'e' to chk file");//early exit
      fclose(chk);
    }
    abort_err("ERROR: Unexpected symbol in dec_file (image_type)");
  }

  /*read header*/
  {

  }

  fread_chk(buffer, 1, 4, dec, "ERROR: Failed to read partition count from dec file");
  partition_count=char_arr_BE_to_uint(buffer);
  partition_table_entries=malloc(8*partition_count);
  fread_chk(partition_table_entries, 1, 8*partition_count, dec, "ERROR: Failed to read global partition table from dec file");

  /*partitions should have been sorted by encoder*/



  /*write everything*/
  {
    fputs("[Write header]\n", stderr);
    curr_sector=0;
    fread_chk(buffer, 1, 2048*160, dec, "ERROR: Failed to read header from dec file");
    gwrite(buffer, 1, 2048*160, game, md, &curr_sector);
    i=0;
    while(i<partition_count)
    {
      /*padding*/
      if( curr_sector<((char_arr_BE_to_uint(partition_table_entries+(i*8))/512)) )
        fprintf(stderr, "[Write inter-partition (%u sectors)]\n", ((char_arr_BE_to_uint(partition_table_entries+(i*8))/512))-curr_sector );
      while( curr_sector<((char_arr_BE_to_uint(partition_table_entries+(i*8))/512)) )
      {
#ifdef CHANGE_TITLE
        if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %7u/%u] [Inter-Partition %u/%u]", game_num, game_tot, curr_sector, iso_size_in_sectors, curr_sector-160, (char_arr_BE_to_uint(partition_table_entries+(i*8))/512) )<0)
          abort_err("ERROR: sprintf failed");
        if(curr_sector%100==0)
          SetConsoleTitle(console_title_builder);
        if((curr_sector/229400)==chunk)
        {
          fprintf(stderr, "%s\n", console_title_builder);
          fflush(stderr);
          ++chunk;
        }
#endif
        buffer_used = ((char_arr_BE_to_uint(partition_table_entries+(i*8))/512)-curr_sector)>=4096 ? 4096 : ((char_arr_BE_to_uint(partition_table_entries+(i*8))/512)-curr_sector);
        fread_chk(buffer, 1, 2048*buffer_used, dec, "ERROR: Failed to read inter-partition from dec file");
        gwrite(buffer, 1, 2048*buffer_used, game, md, &curr_sector);
      }
      /*partition*/
      fputs("[Write partition]\n", stderr);
      fread_chk(buffer, 1, 2048*64, dec, "ERROR: Failed to read partition header from dec file");
      gwrite(buffer, 1, 2048*64, game, md, &curr_sector);
      /*read Ticket*/
      common_key= buffer[497]==1 ? common_key_kor : common_key_nor;
      memcpy(curr_iv, buffer+476, 8);
      memset(curr_iv+8, 0, 8);
      decrypt_block(buffer+447, curr_key, common_key, curr_iv, 16);
      curr_num_clusters=char_arr_BE_to_uint(buffer+700)/8192;/*use only for console output*/
      curr_num_groups=curr_num_clusters/64;
      if(curr_num_clusters%64!=0)
        ++curr_num_groups;


      {

      }

      group_byte=0;
      j=0;
      while(0==0)
      {
#ifdef CHANGE_TITLE
        if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %7u/%u] [Partition %u/%u] [Group %u/%u]", game_num, game_tot, curr_sector, iso_size_in_sectors, i+1, partition_count, j, curr_num_groups)<0)
          abort_err("ERROR: sprintf failed");
        SetConsoleTitle(console_title_builder);
        if((curr_sector/229400)==chunk)
        {
          fprintf(stderr, "%s\n", console_title_builder);
          fflush(stderr);
          ++chunk;
        }
        ++j;
#endif
        prev_group_byte=group_byte;
        fread_chk(&group_byte, 1, 1, dec, "ERROR: Failed to read group byte");
        if( (prev_group_byte>>1)%2==1 )//is group in pad?
        {
          if(pad==NULL)//is pad not present?
          {
            //write zeroes for however many clusters necessary
            memset(buffer, 0, GROUP_SIZE);
            if( (group_byte>>7)%2==1 )//is last?
            {
              gwrite(buffer, 1, CLUSTER_SIZE*(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)), game, md, &curr_sector);
              break;
            }
            else
            {
              gwrite(buffer, 1, GROUP_SIZE, game, md, &curr_sector);
            }
          }
          else
          {
            if( (group_byte>>7)%2==1 )//is last?
            {
              if(group_byte%2==1)//is verbatim?
              {
                fread_chk(buffer, 1, CLUSTER_SIZE*(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)), pad, "ERROR: Failed to read last group + hashes from pad file");
                k=0;
                while(k<(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)))
                {
                  memcpy( buffer2+(k*DEC_CLUSTER_SIZE), buffer+(k*CLUSTER_SIZE)+1024, DEC_CLUSTER_SIZE );
                  encrypt_block(buffer+(k*CLUSTER_SIZE)+1024, buffer2+(k*DEC_CLUSTER_SIZE), curr_key,  buffer+(k*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
                  ++k;
                }
              }
              else
              {
                memset(buffer2, 0, DEC_GROUP_SIZE);
                fread_chk(buffer2, 1, DEC_CLUSTER_SIZE*(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)), pad, "ERROR: Failed to read last group from pad file");
                encrypt_group(buffer2, buffer, curr_key);
              }
              gwrite(buffer, 1, CLUSTER_SIZE*(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)), game, md, &curr_sector);
              break;
            }
            else
            {
              if(group_byte%2==1)//is verbatim?
              {
                fread_chk(buffer, 1, GROUP_SIZE, pad, "ERROR: Failed to read group + hashes from pad file");
                k=0;
                while(k<64)
                {
                  memcpy( buffer2+(k*DEC_CLUSTER_SIZE), buffer+(k*CLUSTER_SIZE)+1024, DEC_CLUSTER_SIZE );
                  encrypt_block(buffer+(k*CLUSTER_SIZE)+1024, buffer2+(k*DEC_CLUSTER_SIZE), curr_key,  buffer+(k*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
                  ++k;
                }
              }
              else
              {
                fread_chk(buffer2, 1, DEC_GROUP_SIZE, pad, "ERROR: Failed to read group from pad file");
                encrypt_group(buffer2, buffer, curr_key);
              }
              gwrite(buffer, 1, GROUP_SIZE, game, md, &curr_sector);
            }
          }
        }
        else//group in dec
        {
          if( (group_byte>>7)%2==1 )//is last?
          {
            if( group_byte%2==1)//is verbatim?
            {
              fread_chk(buffer, 1, CLUSTER_SIZE*(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)), dec, "ERROR: Failed to read last group + hashes from dec file");
              k=0;
              while(k<(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)))
              {
                memcpy( buffer2+(k*DEC_CLUSTER_SIZE), buffer+(k*CLUSTER_SIZE)+1024, DEC_CLUSTER_SIZE );
                encrypt_block(buffer+(k*CLUSTER_SIZE)+1024, buffer2+(k*DEC_CLUSTER_SIZE), curr_key,  buffer+(k*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
                ++k;
              }
            }
            else
            {
              fread_chk(buffer2, 1, DEC_CLUSTER_SIZE*(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)), dec, "ERROR: Failed to read last group from dec file");
              encrypt_group(buffer2, buffer, curr_key);
            }
            gwrite(buffer, 1, CLUSTER_SIZE*(((group_byte>>1)%64)==0?64:((group_byte>>1)%64)), game, md, &curr_sector);
            break;
          }
          else
          {
            if( group_byte%2==1)//is verbatim?
            {
              fread_chk(buffer, 1, GROUP_SIZE, dec, "ERROR: Failed to read group + hashes from dec file");
              k=0;
              while(k<64)
              {
                memcpy( buffer2+(k*DEC_CLUSTER_SIZE), buffer+(k*CLUSTER_SIZE)+1024, DEC_CLUSTER_SIZE );
                encrypt_block(buffer+(k*CLUSTER_SIZE)+1024, buffer2+(k*DEC_CLUSTER_SIZE), curr_key,  buffer+(k*CLUSTER_SIZE)+976, DEC_CLUSTER_SIZE);
                ++k;
              }
            }
            else
            {
              fread_chk(buffer2, 1, DEC_GROUP_SIZE, dec, "ERROR: Failed to read group from dec file");
              encrypt_group(buffer2, buffer, curr_key);
            }
            gwrite(buffer, 1, GROUP_SIZE, game, md, &curr_sector);
          }
        }
      }

 
      ++i;
    }
    if(curr_sector<iso_size_in_sectors)
      fputs("[Write trailing sectors]\n\n", stderr);
    while( curr_sector<iso_size_in_sectors )
    {
#ifdef CHANGE_TITLE
      if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %7u/%u] [Trailing Sectors]", game_num, game_tot, curr_sector, iso_size_in_sectors)<0)
        abort_err("ERROR: sprintf failed");
      if(curr_sector%100==0)
        SetConsoleTitle(console_title_builder);
      if((curr_sector/229400)==chunk)
      {
        fprintf(stderr, "%s\n", console_title_builder);
        fflush(stderr);
        ++chunk;
      }
#endif
      buffer_used = (iso_size_in_sectors-curr_sector)>=4096 ? 4096 : (iso_size_in_sectors-curr_sector);
      fread_chk(buffer, 1, 2048*buffer_used, dec, "ERROR: Failed to read trailing sectors");
      gwrite(buffer, 1, 2048*buffer_used, game, md, &curr_sector);
    }

#ifdef POLAR_SSL
    sha1_finish(md, hash);
#endif
    fread_chk(hash_raw,   1, 20, dec, "ERROR: Failed to read original hash from dec file");
    fread_chk(hash_scrub, 1, 20, dec, "ERROR: Failed to read scrubbed hash from dec file");

    write_to_chk_file(chk, hash, hash_scrub, hash_raw);
  }

  free(md);

  free(buffer);
  free(buffer2);
  free(zeroes);

  free(partition_mask_offset);
  free(partition_mask_length);
  free(curr_iv);
  free(curr_hash);
  free(curr_key);
  free(partition_table_entries);

#ifdef CHANGE_TITLE
  free(console_title_builder);
#endif

  fputs("\n", stderr);

  return EXIT_SUCCESS;
}
int encode_game_xbox(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size)
{
#ifdef CHANGE_TITLE
  unsigned int chunk=0;
  unsigned char* console_title_builder=NULL;
#endif

  unsigned int sector_count;
  unsigned char image_type;

  char* volume_desc_magic="MICROSOFT*XBOX*MEDIA";
  char* burn=NULL;//to use as anything at any point
  char* used_map=NULL;
  char* used_map_bit=NULL;
  char* curr_name=NULL;
  char* dir_table=NULL;//[start_sector total_filesize] LE

  unsigned int dir_table_read_loc=0;
  unsigned int dir_table_write_loc=1;

  unsigned int count_used=0;
  unsigned int i;

  my_int64 curr_seek_val;
  unsigned int curr_size_val;
  my_int64 xdvdfs_loc;
  my_int64 size_chk;
  my_int64 root_directory_loc;
  my_int64 root_directory_size;
  my_int64 stream_start_marker;
  my_int64 stream_end_marker;
  my_int64 curr_pos=0;
  my_int64 new_mark;

  unsigned char* zeroes=NULL;
  unsigned char* buffer=NULL;
  unsigned int buffer_loc=0;
  unsigned char* dec_buffer=NULL;
  unsigned char* pad_buffer=NULL;//TODO

#ifdef POLAR_SSL
  sha1_context* md_raw;
  sha1_context* md_scrub;
  md5_context* md5=NULL;

  unsigned char hash_raw[20];
  unsigned char hash_scrub[20];
  unsigned char hash_md5[16];

  md_raw=malloc(sizeof(sha1_context));
  md_scrub=malloc(sizeof(sha1_context));
  if(grp!=NULL)
    md5=malloc(sizeof(md5_context));

  sha1_starts(md_raw);
  sha1_starts(md_scrub);
  if(grp!=NULL)
    md5_starts(md5);
#endif

#ifdef CHANGE_TITLE
  console_title_builder=malloc(8192);
#endif

  my_fseek(game, 0, SEEK_END);

  size_chk=my_ftell(game);
  size_chk/=2048;
  if(size_chk!=XBOX_SECTOR_COUNT && size_chk!=XBOXOLD_SECTOR_COUNT)
    abort_err("ERROR: Image is the incorrect size for an Xbox image");
  sector_count=(unsigned int) size_chk;

  /*byte defining whether old size or new size*/
  image_type = sector_count==XBOX_SECTOR_COUNT?2:1;

  buffer=malloc(BUFFER_SIZE);
  burn=malloc(8192);
  used_map=malloc(sector_count);
  used_map_bit=malloc((sector_count)/8);
  memset(used_map, '0', sector_count);
  curr_name=malloc(257);
  dir_table=malloc(8*1048576);//up to a million directories

  zeroes=malloc(2048);
  memset(zeroes, 0, 2048);

  /*mark extra video partition as data if present*/
  if(sector_count==XBOX_SECTOR_COUNT)
  {
    new_mark=2048;
    new_mark*=XBOXOLD_SECTOR_COUNT;
    mark_used(used_map, new_mark, 2048*(XBOX_SECTOR_COUNT - XBOXOLD_SECTOR_COUNT), 2048, '1');
  }

  my_fseek(game, 198176*2048 , SEEK_SET);
  fread_chk(buffer, 1, 2048, game, "ERROR: Failed to read volume descriptor from image");
  if(memcmp(buffer, volume_desc_magic, 20)!=0 || memcmp(buffer+0x7ec, volume_desc_magic, 20)!=0)
    abort_err("ERROR: Could not find volume descriptor, not an Xbox image (or unsupported)");

  xdvdfs_loc=198144*2048;
  mark_used(used_map, 0, xdvdfs_loc, 2048, '1');//dvd-video
  mark_used(used_map, xdvdfs_loc+(32*2048), 4096, 2048, '1');//volume descriptor

  memcpy(dir_table, buffer+20, 8);//copy root directory info to dir_table

  root_directory_loc=char_arr_LE_to_uint(buffer+20);
  root_directory_loc*=2048;
  root_directory_loc+=xdvdfs_loc;
  root_directory_size=char_arr_LE_to_uint(buffer+24);
  mark_used(used_map, root_directory_loc, root_directory_size, 2048, '1');//root dir table

  //printf("%zd, %zd\n", root_directory_loc, root_directory_size);

  while(dir_table_read_loc<dir_table_write_loc)
  {
    curr_seek_val=char_arr_LE_to_uint(dir_table+(dir_table_read_loc*8));
    curr_seek_val*=2048;
    curr_seek_val+=xdvdfs_loc;
    curr_size_val=char_arr_LE_to_uint(dir_table+(dir_table_read_loc*8)+4);
    //no need to  mark table, it's been done already when dir is added to dir_table
    my_fseek(game, curr_seek_val, SEEK_SET);//seek to dir table
    fread_chk(buffer, 1, curr_size_val, game, "ERROR: Failed to read directory table from image");//read dir table into buffer
    read_dir_entry(buffer, 0, dir_table, &dir_table_write_loc, used_map, xdvdfs_loc);//read this dir table

    ++dir_table_read_loc;
  }

  /*write byte defining whether iso is old size of new size*/
  fwrite_chk(&image_type, 1, 1, dec, "ERROR: Could not write image type to dec file");
  
  //write used_map_bit to dec as it is, as a placeholder
  stream_start_marker=my_ftell(dec);
  fwrite_chk(used_map_bit, 1, (sector_count/8), dec, "ERROR: Could not write map placeholder to dec file");

  my_fseek(game, 0, SEEK_SET);
  curr_pos=0;
  i=0;
  while(i<sector_count)
  {
#ifdef CHANGE_TITLE
    if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %7u/%7u]", game_num, game_tot, i, sector_count)<0)
      abort_err("ERROR: sprintf failed");
    if(i%100==0)
      SetConsoleTitle(console_title_builder);
    if((i/(sector_count/13))==chunk)
    {
      fprintf(stderr, "%s\n", console_title_builder);
      fflush(stderr);
      ++chunk;
    }
#endif
    fread_chk(buffer, 1, 2048, game, "ERROR: Failed to read data from image");
#ifdef POLAR_SSL
    sha1_update(md_raw, buffer, 2048);
    if(grp!=NULL)
      md5_update(md5, buffer, 2048);
#endif
    if(used_map[i]=='0' && is_sec_filled(buffer)!=EXIT_SUCCESS )
    {
      //padding
#ifdef POLAR_SSL
      sha1_update(md_scrub, zeroes, 2048);
#endif
      if(pad!=NULL)
        fwrite_chk(buffer, 1, 2048, pad, "ERROR: Could not write data to pad file");
      pad_size[0]+=2048;
    }
    else
    {
#ifdef POLAR_SSL
      sha1_update(md_scrub, buffer, 2048);
#endif
      used_map[i]='1';
      fwrite_chk(buffer, 1, 2048, dec, "ERROR: Could not write data to dec file");
    }
    ++i;
  }

  /*go back to map and rewrite it, some sectors may have moved from padding to dec*/
  stream_end_marker=my_ftell(dec);
  my_fseek(dec, stream_start_marker, SEEK_SET);
  used_map_crunch(used_map, used_map_bit, sector_count);
  fwrite_chk(used_map_bit, 1, (sector_count/8), dec, "ERROR: Could not write final map to dec file");
  my_fseek(dec, stream_end_marker, SEEK_SET);

#ifdef POLAR_SSL
  sha1_finish(md_raw, hash_raw);
  sha1_finish(md_scrub, hash_scrub);
  if(grp!=NULL)
    md5_finish(md5, hash_md5);
#endif
  fwrite_chk(hash_raw, 1, 20, dec, "ERROR: Could not write raw hash to dec file");
  fwrite_chk(hash_scrub, 1, 20, dec, "ERROR: Could not write scrubbed hash to dec file");

  if(grp!=NULL)
  {
    free(md5);
    fprintf(stderr, "Original MD5:");print_key(hash_md5, 16);
  }
  fprintf(stderr, "Original SHA1:");print_key(hash_raw, 20);
  fprintf(stderr, "Scrubbed SHA1:");print_key(hash_scrub, 20);
  free(md_raw);
  free(md_scrub);

  if(grp!=NULL)
    write_element_to_grp(grp, filename, sector_count, hash_md5, hash_raw);

  free(used_map);
  free(used_map_bit);
  free(curr_name);
  free(zeroes);

  free(buffer);
  free(burn);
  free(dir_table);
#ifdef CHANGE_TITLE
  free(console_title_builder);
#endif

  return EXIT_SUCCESS;
}
int decode_game_xbox(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename)
{
  unsigned char image_type;
  fread_chk(&image_type, 1, 1, dec, "ERROR: Could not read image type from dec");
  if(image_type!=1 && image_type!=2)
    abort_err("ERROR: Image type unsupported (maybe using incompatible version of GParse)");
  return decode_game_simple_map(game, pad, dec, chk, game_num, game_tot, filename, image_type==1?XBOXOLD_SECTOR_COUNT:XBOX_SECTOR_COUNT);
}


#ifdef FUTURE_STUFF
int encode_game_xbox360(FILE* game, FILE* pad, FILE* dec, FILE* grp, unsigned int game_num, unsigned int game_tot, char* filename, my_int64* pad_size)
{
  /*TODO*/
  abort_err("ERROR: Unsupported operation [2]");
  return EXIT_FAIL;
}
int decode_game_xbox360(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename)
{
  /*TODO*/
  abort_err("ERROR: Unsupported operation. dec_file is invalid, or was created with a newer version of this program [3]");
  return EXIT_FAIL;
}
#endif
int decode_game_simple_map(FILE* game, FILE* pad, FILE* dec, FILE* chk, unsigned int game_num, unsigned int game_tot, char* filename, unsigned int sector_count)
{
#ifdef CHANGE_TITLE
  unsigned int chunk=0;
  unsigned char* console_title_builder=NULL;
#endif

  unsigned char* used_map=NULL;
  unsigned char* used_map_bit=NULL;

  unsigned char* buffer=NULL;
  unsigned int buffer_loc=0;

  unsigned char* zeroes=NULL;

  unsigned int i;

#ifdef POLAR_SSL
  sha1_context* md;
  unsigned char hash[20];
  unsigned char hash_raw[20];
  unsigned char hash_scrub[20];

  md=malloc(sizeof(sha1_context));

  sha1_starts(md);
#endif

#ifdef CHANGE_TITLE
  console_title_builder=malloc(8192);
#endif

  used_map_bit=malloc(sector_count/8);
  used_map=malloc(sector_count);
  fread_chk(used_map_bit, 1, sector_count/8, dec, "ERROR: Failed to read map from dec file");

  zeroes=malloc(2048);
  memset(zeroes, 0, 2048);

  //inflate used_map
  used_map_uncrunch(used_map, used_map_bit, sector_count);

  buffer=malloc(NGC_BUFFER_SIZE);

  i=0;
  while(i<sector_count)
  {
#ifdef CHANGE_TITLE
    if(sprintf(console_title_builder, "GParse [Game %u/%u] [Sector %7u/%7u]", game_num, game_tot, i, sector_count)<0)
      abort_err("ERROR: sprintf failed");
    if(i%100==0)
      SetConsoleTitle(console_title_builder);
    if((i/(sector_count/13))==chunk)
    {
      fprintf(stderr, "%s\n", console_title_builder);
      fflush(stderr);
      ++chunk;
    }
#endif
    if(used_map[i]=='0')
    {
      //padding
      if(pad!=NULL)
        fread_chk(buffer+buffer_loc, 1, 2048, pad, "ERROR: Failed to read data from pad file");
      else
        memcpy(buffer+buffer_loc, zeroes, 2048);
    }
    else//game_data
      fread_chk(buffer+buffer_loc, 1, 2048, dec, "ERROR: Failed to read data from dec file");
    buffer_loc+=2048;
    if(buffer_loc==NGC_BUFFER_SIZE)
    {
#ifdef POLAR_SSL
      sha1_update(md, buffer, NGC_BUFFER_SIZE);
#endif
      fwrite_chk(buffer, 1, NGC_BUFFER_SIZE, game, "ERROR: Could not write to image file (simple)");
      buffer_loc=0;
    }
    ++i;
  }
  if(buffer_loc>0)
  {
#ifdef POLAR_SSL
    sha1_update(md, buffer, buffer_loc);
#endif
    fwrite_chk(buffer, 1, buffer_loc, game, "ERROR: Could not write last of image to file (simple)");
  }

#ifdef POLAR_SSL
  sha1_finish(md, hash);
#endif
  fread_chk(hash_raw,   1, 20, dec, "ERROR: Failed to read original hash from dec file");
  fread_chk(hash_scrub, 1, 20, dec, "ERROR: Failed to read scrubbed hash from dec file");

  write_to_chk_file(chk, hash, hash_scrub, hash_raw);

#ifdef CHANGE_TITLE
  free(console_title_builder);
#endif

  free(md);
  free(used_map);
  free(used_map_bit);
  free(zeroes);
  free(buffer);

  return EXIT_SUCCESS;
}


