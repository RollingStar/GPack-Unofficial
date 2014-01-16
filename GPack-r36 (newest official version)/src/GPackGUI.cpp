#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#ifndef WIN32
#define WIN32
#endif

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Choice.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Menu_Item.H>

extern "C"
{

unsigned int err_messages_count=117;
char* err_messages[118]={
"All seems ok",
"ERROR: Unspecific error",
"ERROR: fread seemed to fail",
"ERROR: fopen seemed to fail",
"ERROR: fwrite seemed to fail",
"ERROR: Failed to write to def file",
"ERROR: Failed to allocate memory",
"ERROR: Failed to open cat file for writing",
"ERROR: options.ini does not define everything it should",
"ERROR: Failed to write .grp file",
"ERROR: Unspecific malformed option",
"ERROR: Malformed option. GPack root required but not supplied",
"ERROR: Malformed option. Input file(s) required but not supplied",
"ERROR: Malformed option. Output file required but not supplied",
"ERROR: Malformed option. Generic name required but not supplied",
"ERROR: Malformed option. Directory of merge required but not supplied",
"ERROR: Malformed option. Character after hash not recognised",
"ERROR: Not enough args",
"ERROR: Directory containing binaries is missing some files",
"ERROR: arg parse error, can't find path to go with -o",
"ERROR: Encoding not supported on linux",
"ERROR: Encoding not supported on os x",
"ERROR: Could not resolve relative path to input directory",
"ERROR: Failed to open .grp for reading",
"ERROR: Failed to read .grp file",
"ERROR: Unpack list empty",
"ERROR: Unpack list cannot contain duplicate elements",
"ERROR: Unpack list value out of range",
"ERROR: Invalid unpack method defined",
"ERROR: Could not open 7zip to read the header",
"ERROR: 7zip file is too small",
"ERROR: tak decompressor seemed to fail",
"ERROR: Failed to open 2352.srep file for writing",
"ERROR: Failed to open 2352.wav file for reading",
"ERROR: Error reading 2352.wav, but EOF not set",
"ERROR: 7z decompressor seemed to fail",
"ERROR: srep seemed to fail",
"ERROR: GParse execution failed",
"ERROR: GParse failed chk_file tests",
"ERROR: Failed to seek in 2048 sector bucket",
"ERROR: Failed to seek in 2324 sector bucket",
"ERROR: Failed to seek in 2336 sector bucket",
"ERROR: Failed to seek in 2352 sector bucket",
"ERROR: Failed to seek in 96 sector bucket",
"ERROR: Unexpected filename in .grp (.grp invalid)",
"ERROR: Failed to open image file for writing",
"ERROR: Hash does not match expected",
"ERROR: Failed to read sector from sector store",
"ERROR: Failed to write sector to iso file",
"ERROR: Failed to write sector to sub file",
"ERROR: Failed to read sector from cat file",
"ERROR: Failed to write sector to embedded file",
"ERROR: Could not find any files to merge",
"ERROR: Failed to open image file for reading",
"ERROR: Encountered unexpected file extension",
"ERROR: 7z compressor seemed to fail",
"ERROR: Sox seemed to fail",
"ERROR: Tak compressor seemed to fail",
"ERROR: Could not open grp file for writing",
"ERROR: fork() failed, failed to execute external program",
"ERROR: execvp() failed, failed to execute external program",
"ERROR: Undefined symbol in def file",
"ERROR: Failed to initialise 2048 buffer",
"ERROR: Failed to initialise 2324 buffer",
"ERROR: Failed to initialise 2336 buffer",
"ERROR: Failed to initialise 2352 buffer",
"ERROR: Failed to read from .def file",
"ERROR: Def file defines a 2048 byte data chunk, but no such data store exists",
"ERROR: Def file defines a 2324 byte data chunk, but no such data store exists",
"ERROR: Def file defines a 2336 byte data chunk, but no such data store exists",
"ERROR: Def file defines a 2352 byte data chunk, but no such data store exists",
"ERROR: Failed to read audio sectors",
"ERROR: Failed to generate cd_sync for mode 0 sector",
"ERROR: Failed to generate MSF for mode 0 sector",
"ERROR: Failed to generate mode for mode 0 sector",
"ERROR: Failed to read data for mode 0 sector",
"ERROR: Failed to generate cd_sync for mode 1 sector",
"ERROR: Failed to generate MSF for mode 1 sector",
"ERROR: Failed to generate mode for mode 1 sector",
"ERROR: Failed to read data for mode 1 sector",
"ERROR: Failed to generate zerofill for mode 1 sector",
"ERROR: Failed to generate edc for mode 1 sector",
"ERROR: Failed to generate edc for mode 1 sector",
"ERROR: Failed to generate edc for mode 1 sector",
"ERROR: Failed to generate cd_sync for mode 2 sector",
"ERROR: Failed to generate MSF for mode 2 sector",
"ERROR: Failed to generate mode for mode 2 sector",
"ERROR: Failed to read data for mode 2 sector",
"ERROR: Failed to generate cd_sync for mode 2 form 1 sector",
"ERROR: Failed to generate MSF for mode 2 form 1 sector",
"ERROR: Failed to generate mode for mode 2 form 1 sector",
"ERROR: Failed to read data for mode 2 form 1 sector",
"ERROR: Failed to generate subheader for mode 2 form 1 sector",
"ERROR: Failed to generate edc for mode 2 form 1 sector",
"ERROR: Failed to generate p for mode 2 form 1 sector",
"ERROR: Failed to generate q for mode 2 form 1 sector",
"ERROR: Failed to generate cd_sync for mode 2 form 2 sector",
"ERROR: Failed to generate MSF for mode 2 form 2 sector",
"ERROR: Failed to generate mode for mode 2 form 2 sector",
"ERROR: Failed to read data for mode 2 form 2 sector",
"ERROR: Failed to generate subheader for mode 2 form 2 sector",
"ERROR: Failed to generate edc for mode 2 form 2 sector",
"ERROR: Could not write to image file",
"ERROR: Failed to write to cat file from iso",
"ERROR: Failed to write to cat file from special file",
"ERROR: Invalid sector type, logic error",
"ERROR: Failed to determine sector type",
"ERROR: Failed to write to sector store",
"ERROR: Could not find any path delimiters. Files cannot be on root of device",
"ERROR: Key file incorrect size",
"ERROR: Key not supplied",
"ERROR: AES encryption key initialisation failed for d1 -> key",
"ERROR: AES encrypt failed for d1 -> key",
"ERROR: Sense GPack exit code failed",
"ERROR: Image wrong size for its supposed type",
"ERROR: Image is empty",
"ERROR: Path not found",
"ERROR: Input could not be read as directory (encode) or grp file (decode)"
};

char* program;
char* params;

char* exe_root;

char* enc_filename;
char* dec_filename;

char* enc_outname;
char* dec_outname;

char* enc_working;
char* dec_working;

Fl_Window *window;

Fl_Box *d_filename_box;
Fl_Box *e_filename_box;

Fl_Choice *e_comp_choice;

Fl_Box *dec_outname_box;
Fl_Box *enc_outname_box;

Fl_Button *d_exec_b;
Fl_Button *e_exec_b;

Fl_Text_Display *d_disp;
Fl_Text_Display *e_disp;

Fl_Text_Buffer *decode_buff;
Fl_Text_Buffer *encode_buff;
Fl_Text_Buffer *help_buff;

#define COMP_NONE 0
#define COMP_QUICK 1
#define COMP_NORMAL 2
#define COMP_SLOW 3

char comp_settings=COMP_NONE;

HANDLE exec_thread_handle;
char exec_thread_in_progress=0;

void d_filename_f_ret(Fl_File_Chooser *fc, void *data);
void d_outname_f_ret(Fl_File_Chooser *fc, void *data);
void e_filename_f_ret(Fl_File_Chooser *fc, void *data);
void e_outname_f_ret(Fl_File_Chooser *fc, void *data);

void about_gpackgui_f(Fl_Widget *, void *);
void about_gpack_f(Fl_Widget *, void *);
void about_gparse_f(Fl_Widget *, void *);
void about_supportedformats_f(Fl_Widget *, void *);

DWORD execute(char* program, char* params);

/*execution thread function*/
DWORD WINAPI d_exec_thread()
{
  DWORD exit_code=execute(program, params);
  Fl::lock();
  if(exit_code<=err_messages_count && exit_code>=0)
  {
    decode_buff->text( err_messages[exit_code] );
  }
  else
    decode_buff->text( "Status: Exit code could not be mapped to known value" );
  d_exec_b->activate();
  e_exec_b->activate();
  Fl::check();
  Fl::unlock();

  return exit_code;
}

DWORD WINAPI e_exec_thread()
{
  DWORD exit_code=execute(program, params);
  Fl::lock();
  if(exit_code<=err_messages_count && exit_code>=0)
  {
    encode_buff->text( err_messages[exit_code] );
  }
  else
    encode_buff->text( "Status: Exit code could not be mapped to known value" );
  d_exec_b->activate();
  e_exec_b->activate();
  Fl::check();
  Fl::unlock();

  return exit_code;
}

DWORD execute(char* program, char* params)
{
  DWORD exit_code;
  HINSTANCE h=0;
  HANDLE handle;
  SHELLEXECUTEINFO ex;
  memset(&ex, 0, sizeof(SHELLEXECUTEINFO) );
  ex.cbSize=sizeof(SHELLEXECUTEINFO);
  ex.fMask=SEE_MASK_NOCLOSEPROCESS;
  ex.hwnd=GetDesktopWindow();
  ex.lpVerb="open";
  ex.lpFile=program;
  ex.lpParameters=params;
  ex.lpDirectory=NULL;
  ex.nShow=SW_SHOWMINNOACTIVE;/*start external cmd windows minimised*/
  ex.hInstApp=h;
  ex.lpIDList=NULL;
  ex.lpClass=NULL;
  ex.hkeyClass=NULL;
  ex.dwHotKey=0;
  ex.hIcon=NULL;
  ex.hProcess=&handle;

  if( ShellExecuteEx( &ex )==TRUE )
  {
    SetPriorityClass(ex.hProcess, BELOW_NORMAL_PRIORITY_CLASS);/*set to low priority*/
    WaitForSingleObject(ex.hProcess, INFINITE);
    if( GetExitCodeProcess( ex.hProcess, &exit_code) == 0 )/*check exit code*/
      return 113;
    else
    {
      return exit_code;
    }
  }
  else
      return 1;
  
  return EXIT_SUCCESS;
}

void d_filename_f(Fl_Widget *, void *)
{
  Fl_File_Chooser *choose = new Fl_File_Chooser(".", "GPack files (*.grp)", Fl_File_Chooser::SINGLE, "Select an existing merge");
  choose->preview(0);
  choose->show();
  choose->callback(d_filename_f_ret);
}

void d_filename_f_ret(Fl_File_Chooser *fc,	// I - File chooser
            void            *data)	// I - Data
{
  char* pprint;
  if( fc->shown() || fc->value()==NULL )
    return;

  pprint=(char*) malloc(70);

  sprintf( dec_filename, "%s", fc->value() );
  if(strlen(dec_filename)>60)
    sprintf(pprint, "'... %s'", &dec_filename[strlen(dec_filename)-60]);
  else
    sprintf(pprint, "'%s'", dec_filename);
  d_filename_box->copy_label(pprint);
}

void e_filename_f(Fl_Widget *, void *)
{
  Fl_File_Chooser *choose = new Fl_File_Chooser(".", "*", Fl_File_Chooser::DIRECTORY, "Select input directory");
  choose->preview(0);
  choose->show();
  choose->callback(e_filename_f_ret);
}

void e_filename_f_ret(Fl_File_Chooser *fc,	// I - File chooser
            void            *data)	// I - Data
{
  char* pprint;
  if( fc->shown() || fc->value()==NULL )
    return;

  pprint=(char*) malloc(70);

  sprintf( enc_filename, "%s", fc->value() );
  if(strlen(enc_filename)>60)
    sprintf(pprint, "'... %s'", &enc_filename[strlen(enc_filename)-60]);
  else
    sprintf(pprint, "'%s'", enc_filename);
  e_filename_box->copy_label(pprint);
}

void d_outname_f(Fl_Widget *, void *)
{
  Fl_File_Chooser *choose = new Fl_File_Chooser(".", "*", Fl_File_Chooser::DIRECTORY, "Select output directory");
  choose->preview(0);
  choose->show();
  choose->callback(d_outname_f_ret);
}

void d_outname_f_ret(Fl_File_Chooser *fc,	// I - File chooser
            void            *data)	// I - Data
{
  char* pprint;
  if( fc->shown() || fc->value()==NULL )
    return;

  pprint=(char*) malloc(70);

  sprintf( dec_outname, "%s", fc->value() );
  if(strlen(dec_outname)>60)
    sprintf(pprint, "'... %s'", &dec_outname[strlen(dec_outname)-60]);
  else
    sprintf(pprint, "'%s'", dec_outname);
  dec_outname_box->copy_label(pprint);
}

void e_outname_f(Fl_Widget *, void *)
{
  Fl_File_Chooser *choose = new Fl_File_Chooser(".", "*", Fl_File_Chooser::DIRECTORY, "Select output directory");
  choose->preview(0);
  choose->show();
  choose->callback(e_outname_f_ret);
}

void e_outname_f_ret(Fl_File_Chooser *fc,	// I - File chooser
            void            *data)	// I - Data
{
  char* pprint;
  if( fc->shown() || fc->value()==NULL )
    return;

  pprint=(char*) malloc(70);

  sprintf( enc_outname, "%s", fc->value() );
  if(strlen(enc_outname)>60)
    sprintf(pprint, "'... %s'", &enc_outname[strlen(enc_outname)-60]);
  else
    sprintf(pprint, "'%s'", enc_outname);
  enc_outname_box->copy_label(pprint);
}

void d_exec_f(Fl_Widget *, void *)
{

  if(dec_filename[0]==0)
  {
    Fl::lock();
    decode_buff->text(
"You must define a grp file\n"
);
    Fl::unlock();
    return;
  }

  sprintf(program, "%s\\GPack.exe", exe_root);
  if(dec_outname[0]==0)
    sprintf(params, "--script \"%s\"", dec_filename);
  else
    sprintf(params, "--script -o \"%s\" \"%s\"", dec_outname, dec_filename);

  fprintf(stderr, "Program: %s\n", program);
  fprintf(stderr, "Params: %s\n", params);

  Fl::lock();
  d_exec_b->deactivate();
  e_exec_b->deactivate();
  decode_buff->text(
"Status: Pending.\n"
"\n"
"Don't close GPackGUI or any console windows that could be popping up. Execution\n"
"could take a long time, depending on input, settings and computer spec.\n"
"\n"
"The result should end up here one way or the other.\n"
);
  Fl::unlock();


  /*spawn a new thread to do the execution to avoid gui lockup*/
  exec_thread_handle=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)d_exec_thread,NULL,0,NULL);
}

void e_exec_f(Fl_Widget *, void *)
{

  int params_loc;

  if(enc_filename[0]==0)
  {
    Fl::lock();
    encode_buff->text(
"You must define a directory containing images\n"
);
    Fl::unlock();
    return;
  }

  params_loc=sprintf(params, "--script");

  switch( e_comp_choice->value() )
  {
    case COMP_NONE:
      params_loc+=sprintf(params+params_loc, " --nocomp");
    break;
    case COMP_QUICK:
      params_loc+=sprintf(params+params_loc, " --quick");
    break;
    case COMP_NORMAL:
      params_loc+=sprintf(params+params_loc, " --normal");
    break;
    case COMP_SLOW:
      params_loc+=sprintf(params+params_loc, " --slow");
    break;
    default:
      printf("enc setting undefined\n");
    break;
  }

  sprintf(program, "%s\\GPack.exe", exe_root);
  if(enc_outname[0]==0)
    params_loc+=sprintf(params+params_loc, " \"%s\"", enc_filename);
  else
    params_loc+=sprintf(params+params_loc, " -o \"%s\" \"%s\"", enc_outname, enc_filename);

  fprintf(stderr, "Program: %s\n", program);
  fprintf(stderr, "Params: %s\n", params);

  Fl::lock();
  d_exec_b->deactivate();
  e_exec_b->deactivate();
  encode_buff->text(
"Status: Pending.\n"
"\n"
"Don't close GPackGUI or any console windows that could be popping up. Execution\n"
"could take a long time, depending on input, settings and computer spec.\n"
"\n"
"The result should end up here one way or the other.\n"
);
  Fl::unlock();


  /*spawn a new thread to do the execution to avoid gui lockup*/
  exec_thread_handle=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)e_exec_thread,NULL,0,NULL);
}

void e_comp_choice_f(Fl_Widget *, void *)
{
  printf("value: %s\n", e_comp_choice->label());
}



int main(int argc, char ** argv)
{
  program=(char*)malloc(4096);
  params=(char*)malloc(40960);

  exe_root=(char*)malloc(4096);
  if( GetModuleFileName(NULL, exe_root, 4096)!=0)//os specific method win32
  {
    if(strrchr(exe_root, '\\')!=NULL)
      (strrchr(exe_root, '\\'))[0]=0;
  }
  else
  {
    puts("Failed to find exe_root");
    return 1;
  }

  enc_filename=(char*)malloc(4096);
  dec_filename=(char*)malloc(4096);

  enc_outname=(char*)malloc(4096);
  dec_outname=(char*)malloc(4096);

  enc_working=(char*)malloc(4096);
  dec_working=(char*)malloc(4096);

  memset(enc_filename, 0, 4096);
  memset(dec_filename, 0, 4096);

  memset(enc_outname, 0, 4096);
  memset(dec_outname, 0, 4096);

  memset(enc_working, 0, 4096);
  memset(dec_working, 0, 4096);

  window = new Fl_Window(640,480, "GPackGUI r1");/*lazy, rev goes here*/

  Fl_Tabs *tabs = new Fl_Tabs(0, 0, 640, 480);
  Fl_Group *dec_tab = new Fl_Group(0, 30, 640, 450, "Decode");
  Fl_Group *enc_tab = new Fl_Group(0, 30, 640, 450, "Encode");
  Fl_Group *help_tab = new Fl_Group(0, 30, 640, 450, "Help");
  tabs->add(dec_tab);
  tabs->add(enc_tab);
  tabs->add(help_tab);

  /*decode tab*****************************************************************/
  /*setup text display*/
  decode_buff = new Fl_Text_Buffer();
  d_disp = new Fl_Text_Display(0, 310, 640, 170, "");
  d_disp->buffer(decode_buff);
  //window->resizable(*d_disp);
  decode_buff->text(
"\n"
);
  dec_tab->add(d_disp);

  /*dec_filename description*/
  Fl_Box *d_filename_desc = new Fl_Box(20, 70, 600, 25, "1) Select a grp file from an existing merge");
  dec_tab->add(d_filename_desc);

  /*dec_filename button*/
  Fl_Button *d_filename_b = new Fl_Button(20, 100, 80, 25, "Browse");
  d_filename_b->callback(d_filename_f,0);
  dec_tab->add(d_filename_b);

  /*dec_filename path*/
  d_filename_box = new Fl_Box(105, 100, 535, 25, "No grp file selected");
  dec_tab->add(d_filename_box);

  /*dec_outname description*/
  Fl_Box *d_outname_desc = new Fl_Box(20, 160, 600, 25, "2) Select an output directory (optional)");
  dec_tab->add(d_outname_desc);

  /*dec_filename button*/
  Fl_Button *d_outname_b = new Fl_Button(20, 190, 80, 25, "Browse");
  d_outname_b->callback(d_outname_f,0);
  dec_tab->add(d_outname_b);

  /*dec_filename path*/
  dec_outname_box = new Fl_Box(105, 190, 535, 25, "Default to same directory as merged files");
  dec_tab->add(dec_outname_box);

  /*dec_exec description*/
  Fl_Box *d_exec_desc = new Fl_Box(20, 250, 600, 25, "3) Execute");
  dec_tab->add(d_exec_desc);

  /*dec_exec button*/
  d_exec_b = new Fl_Button(20, 280, 80, 25, "Make it so");
  d_exec_b->callback(d_exec_f,0);
  dec_tab->add(d_exec_b);

  /*encode tab*****************************************************************/
  /*setup text display*/
  encode_buff = new Fl_Text_Buffer();
  e_disp = new Fl_Text_Display(0, 400, 640, 80, "");
  e_disp->buffer(encode_buff);
  //window->resizable(*e_disp);
  encode_buff->text(
"\n"
);
  enc_tab->add(e_disp);

  /*enc_filename description*/
  Fl_Box *e_filename_desc = new Fl_Box(20, 70, 600, 25, "1) Select a directory containing images to merge");
  enc_tab->add(e_filename_desc);

  /*enc_filename button*/
  Fl_Button *e_filename_b = new Fl_Button(20, 100, 80, 25, "Browse");
  e_filename_b->callback(e_filename_f,0);
  enc_tab->add(e_filename_b);

  /*enc_filename path*/
  e_filename_box = new Fl_Box(105, 100, 535, 25, "No directory selected");
  enc_tab->add(e_filename_box);

  /*enc_outname description*/
  Fl_Box *e_outname_desc = new Fl_Box(20, 160, 600, 25, "2) Select an output directory (optional)");
  enc_tab->add(e_outname_desc);

  /*enc_outname button*/
  Fl_Button *e_outname_b = new Fl_Button(20, 190, 80, 25, "Browse");
  e_outname_b->callback(e_outname_f,0);
  enc_tab->add(e_outname_b);

  /*enc_outname path*/
  enc_outname_box = new Fl_Box(105, 190, 535, 25, "Default to same directory as merged files");
  enc_tab->add(enc_outname_box);

  /*compression option*/
  Fl_Box *e_comp_desc = new Fl_Box(20, 250, 600, 25, "3) Select a compression profile");
  enc_tab->add(e_comp_desc);

  /*enc_outname button*/
  e_comp_choice = new Fl_Choice(20, 280, 600,25);
  e_comp_choice->add("No compression (srep)");
  e_comp_choice->add("Quick compression (srep + 7zip (mx1) + tak)");
  e_comp_choice->add("Normal compression (srep + 7zip (mx5) + tak)");
  e_comp_choice->add("Slow compression (srep + 7zip (mx9) + tak)");
  e_comp_choice->value(COMP_NORMAL);
  enc_tab->add(e_comp_choice);

  /*enc_exec description*/
  Fl_Box *e_exec_desc = new Fl_Box(20, 340, 600, 25, "4) Execute");
  enc_tab->add(e_exec_desc);

  /*enc_exec button*/
  e_exec_b = new Fl_Button(20, 370, 80, 25, "Make it so");
  e_exec_b->callback(e_exec_f,0);
  enc_tab->add(e_exec_b);

  /*about tab******************************************************************/
  /*setup text display*/
  help_buff = new Fl_Text_Buffer();
  Fl_Text_Display *h_disp = new Fl_Text_Display(0, 65, 640, 415, "");
  h_disp->buffer(help_buff);
  //window->resizable(*h_disp);
  help_buff->text(
"GPack is a commandline compressor for related disc images, GPackGUI is a simple\n"
"GUI interface to GPack.\n"
"\n"
"This GUI does not expose every option available in GPack. For full functionality\n"
"use GPack from the commandline.\n"
);
  help_tab->add(h_disp);

  Fl_Button *about_gpackgui_b = new Fl_Button(5, 35, 150, 25, "About GPackGUI");
  about_gpackgui_b->callback(about_gpackgui_f,0);
  help_tab->add(about_gpackgui_b);

  Fl_Button *about_gpack_b = new Fl_Button(165, 35, 150, 25, "About GPack");
  about_gpack_b->callback(about_gpack_f,0);
  help_tab->add(about_gpack_b);

  Fl_Button *about_gparse_b = new Fl_Button(325, 35, 150, 25, "About GParse");
  about_gparse_b->callback(about_gparse_f,0);
  help_tab->add(about_gparse_b);

  Fl_Button *about_supportedformats_b = new Fl_Button(485, 35, 150, 25, "Supported Formats");
  about_supportedformats_b->callback(about_supportedformats_f,0);
  help_tab->add(about_supportedformats_b);

  window->end();
  window->show(argc,argv);

  Fl::lock();
  return Fl::run();
}

void about_gpackgui_f(Fl_Widget *, void *)
{
  Fl::lock();
  help_buff->text(
"GPack is a commandline compressor for related disc images, GPackGUI is a simple\n"
"GUI interface to GPack.\n"
"\n"
"This GUI does not expose every option available in GPack. For full functionality\n"
"use GPack from the commandline.\n"
);
  Fl::unlock();
}

void about_gpack_f(Fl_Widget *, void *)
{
  Fl::lock();
  help_buff->text(
"GPack is a commandline compressor for related disc images.\n"
"\n"
"GPack farms many tasks to external programs where all of the real work happens.\n"
"GPack acts as controller, handling the input smartly. The aim is to strike a\n"
"good balance between compression and speed.\n"
"\n"
"Features:\n"
" * Binary compatibility when the same settings are used (get matching results from matching runs)\n"
" * Allows renames without having to repack\n"
" * Reasonable ram usage (aiming for under 1GiB in most circumstances)\n"
" * User definable compression settings (none, quick, normal, slow)\n"
" * For some systems with padding, don't compress the padding\n"
" * For some systems with encryption, decrypt before compressing\n"
"\n"
"External programs (without which GPack could not function):\n"
" * 7zip (general compressor)\n"
" * gparse (process systems with special requirements like encryption and padding)\n"
" * srep (quick long range compressor)\n"
" * tak (audio compressor)\n"
" * sox (deal with audio headers)\n"
" * ffmpeg (tak decoder for linux)\n"
);
  Fl::unlock();
}

void about_gparse_f(Fl_Widget *, void *)
{
  Fl::lock();
  help_buff->text(
"GParse is a commandline processor for images from special systems. It's not a\n"
"compressor as it doesn't compress (filesize may actually go up slightly from\n"
"minor overheads), the goal is to convert the input into a form which is more\n"
"compressible.\n"
"\n"
"By system we mean computer system, typically a console. A special system is one\n"
"that can be handled in a non-standard way. Special systems implement one or more\n"
"of the following:\n"
"  * Non-standard file system\n"
"  * Encryption\n"
"  * Padding (filling unused areas with random garbage)\n"
"\n"
"Supported systems: \n"
"  * GameCube\n"
"  * PS3\n"
"  * Wii\n"
"  * Xbox (1)\n"
"\n"
"For systems with padding, the input is split into a data file and a pad file.\n"
"The data file is compressible, the pad file isn't (unless input has been\n"
"previously 'scrubbed'). The pad file can be deleted if you don't want to\n"
"recreate the original file (output will instead be a 'scrubbed' image).\n"
);
  Fl::unlock();
}

void about_supportedformats_f(Fl_Widget *, void *)
{
  Fl::lock();
  help_buff->text(
"Many common image formats are supported:\n"
"\n"
"    *.iso containing 2048 byte per sector data\n"
"    *.bin containing 2352 byte per sector data\n"
"    *.raw containing 2352 byte per sector data\n"
"    *.img, clonecd format containing 2352 byte per sector data\n"
"    *.sub, clonecd format containing 96 byte per sector data \n"
"    *.ccd, clonecd format descriptor files\n"
);
  Fl::unlock();
}

}
