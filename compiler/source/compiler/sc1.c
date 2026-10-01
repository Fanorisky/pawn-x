/*  Pawn compiler
 *
 *  Function and variable definition and declaration, statement parser.
 *
 *  Copyright (c) ITB CompuPhase, 1997-2006
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 *
 *  Version: $Id: sc1.c 3664 2006-11-08 12:09:25Z thiadmer $
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined	__WIN32__ || defined _WIN32 || defined __MSDOS__
  #include <conio.h>
  #include <io.h>
#endif

#if defined LINUX || defined __FreeBSD__ || defined __OpenBSD__
  #include <sclinux.h>
  #include <binreloc.h> /* from BinReloc, see www.autopackage.org */
#endif

#if defined LINUX || defined __APPLE__
  #include <unistd.h>
#endif

#if defined FORTIFY
  #include <alloc/fortify.h>
#endif

#if defined __BORLANDC__ || defined __WATCOMC__
  #include <dos.h>
  static unsigned total_drives; /* dummy variable */
  #define dos_setdrive(i)       _dos_setdrive(i,&total_drives)
#elif defined _MSC_VER && defined _WIN32
  #include <direct.h>           /* for _chdrive() */
  #define dos_setdrive(i)       _chdrive(i)
  #define stricmp  _stricmp
  #define chdir    _chdir
  #define access   _access
  #define snprintf _snprintf
#endif
#if defined __BORLANDC__
  #include <dir.h>              /* for chdir() */
#elif defined __WATCOMC__
  #include <direct.h>           /* for chdir() */
#endif
#if defined __WIN32__ || defined _WIN32 || defined _Windows
  #include <windows.h>
#endif

#include "lstring.h"
#include "sc.h"
#include "version.h"

static void resetglobals(void);
static void hook_reset(void);
static void hook_emit_dispatchers(void);
static void timers_reset(void);
static void timers_emit(void);
static void dotask(int perplayer);
static void callhook_reset(void);
static void callhook_emit(void);
static void generator_emit_helper(void);
static void initglobals(void);
static char *get_extension(char *filename);
static void setopt(int argc,char **argv,char *oname,char *ename,char *pname,
                   char *codepage);
static void setconfig(char *root);
static void setcaption(void);
static void about(void);
static void invalid_option(const char *opt);
static void usage(void);
static void setconstants(void);
static void setstringconstants(void);
static void parse(void);
static void dumplits(void);
static void dumpzero(int count);
static void declfuncvar(int fpublic,int fstatic,int fstock,int fconst);
static void declglb(char *firstname,int firsttag,int fpublic,int fstatic,
                    int fstock,int fconst);
static int declloc(int fstatic);
static void decl_const(int vclass);
static void decl_enum(int vclass,int fstatic);
static cell needsub(int *tag,constvalue_root **enumroot);
static void initials(int ident,int tag,cell *size,int dim[],int numdim,
                     constvalue_root *enumroot,int *explicit_init);
static cell initarray(int ident,int tag,int dim[],int numdim,int cur,
                      int startlit,int counteddim[],constvalue_root *lastdim,
                      constvalue_root *enumroot,int *errorfound);
static cell initvector(int ident,int tag,cell size,int startlit,int fillzero,
                       constvalue_root *enumroot,int *errorfound);
static cell init(int ident,int *tag,int *errorfound);
static int getstates(const char *funcname);
static void attachstatelist(symbol *sym, int state_id);
static void funcstub(int fnative);
static int newfunc(char *firstname,int firsttag,int fpublic,int fstatic,int stock);
static int declargs(symbol *sym,int chkshadow);
static void doarg(char *name,int ident,int offset,int tags[],int numtags,
                  int fpublic,int fconst,int written,int chkshadow,arginfo *arg);
static void make_report(symbol *root,FILE *log,char *sourcefile);
static void reduce_referrers(symbol *root);
static long max_stacksize(symbol *root,int *recursion);
static int testsymbols(symbol *root,int level,int testlabs,int testconst);
static void scanloopvariables(symstate **loopvars,int dowhile);
static void testloopvariables(symstate *loopvars,int dowhile,int line);
static void destructsymbols(symbol *root,int level);
static constvalue *find_constval_byval(constvalue_root *table,cell val);
static symbol *fetchlab(char *name);
static void statement(int *lastindent,int allow_decl);
static void compound(int stmt_sameline,int starttok);
static int test(int label,int parens,int invert);
static int doexpr(int comma,int chkeffect,int allowarray,int mark_endexpr,
                  int *tag,symbol **symptr,int chkfuncresult,cell *val);
static void doassert(void);
static void doexit(void);
static int doif(void);
static int dowhile(void);
static int dodo(void);
static int dofor(void);
static int doforeach(void);
static int doswitch(void);
static void docase(int isdefault);
static int dogoto(void);
static void dolabel(void);
static void emit_invalid_token(int expected_token,int found_token);
static regid emit_findreg(char *opname);
static int emit_getlval(int *identptr,emit_outval *p,int *islocal,
                        regid reg,int allow_char,int allow_const,
                        int store_pri,int store_alt,int *ispushed);
static int emit_getrval(int *identptr,cell *val);
static int emit_param_any_internal(emit_outval *p,int expected_tok,
                                   int allow_nonint,int allow_expr);
static void emit_param_any(emit_outval *p);
static void emit_param_integer(emit_outval *p);
static void emit_param_index(emit_outval *p,int isrange,
                             const cell *valid_values,int numvalues);
static void emit_param_nonneg(emit_outval *p);
static void emit_param_shift(emit_outval *p);
static void emit_param_data(emit_outval *p);
static void emit_param_local(emit_outval *p,int allow_ref);
static void emit_param_label(emit_outval *p);
static void emit_param_function(emit_outval *p,int isnative);
static void emit_noop(char *name);
static void emit_parm0(char *name);
static void emit_parm1_any(char *name);
static void emit_parm1_integer(char *name);
static void emit_parm1_nonneg(char *name);
static void emit_parm1_shift(char *name);
static void emit_parm1_data(char *name);
static void emit_parm1_local(char *name);
static void emit_parm1_local_noref(char *name);
static void emit_parm1_label(char *name);
static void emit_do_casetbl(char *name);
static void emit_do_case(char *name);
static void emit_do_lodb_strb(char *name);
static void emit_do_align(char *name);
static void emit_do_call(char *name);
static void emit_do_sysreq_c(char *name);
static void emit_do_sysreq_n(char *name);
static void emit_do_const(char *name);
static void emit_do_const_s(char *name);
static void emit_do_load_both(char *name);
static void emit_do_load_s_both(char *name);
static void emit_do_pushn_c(char *name);
static void emit_do_pushn(char *name);
static void emit_do_pushn_s_adr(char *name);
static void emit_do_load_u_pri_alt(char *name);
static void emit_do_stor_u_pri_alt(char *name);
static void emit_do_addr_u_pri_alt(char *name);
static void emit_do_push_u(char *name);
static void emit_do_push_u_adr(char *name);
static void emit_do_zero_u(char *name);
static void emit_do_inc_dec_u(char *name);
static int emit_findopcode(const char *instr);
static int isterminal(int tok);
static void doreturn(void);
static void dobreak(void);
static void docont(void);
static void dosleep(void);
static void dostate(void);
static void addwhile(int *ptr);
static void delwhile(void);
static int *readwhile(void);
static char *parsestringparam(int onlycheck,int *bck_litidx);
static void dopragma(void);
static void pragma_apply(symbol *sym);

typedef void (*OPCODE_PROC)(char *name);
typedef struct {
  char *name;
  OPCODE_PROC func;
} EMIT_OPCODE;

enum {
  TEST_PLAIN,           /* no parentheses */
  TEST_THEN,            /* '(' <expr> ')' or <expr> 'then' */
  TEST_DO,              /* '(' <expr> ')' or <expr> 'do' */
  TEST_OPT,             /* '(' <expr> ')' or <expr> */
};
static int lastst     = 0;      /* last executed statement type */
static int endlessloop= 0;      /* nesting level of endless loop */
static int rettype    = 0;      /* the type that a "return" expression should have */
static int skipinput  = 0;      /* number of lines to skip from the first input file */
static int optproccall = TRUE;  /* support "procedure call" */
static int verbosity  = 1;      /* verbosity level, 0=quiet, 1=normal, 2=verbose */
static int sc_reparse = 0;      /* needs 3th parse because of changed prototypes? */
/* one-shot reparse gate for a forward-referenced async callee. "await asyncFn()"
 * and "__async_start(asyncFn,...)" decide their whole codegen by findglb()+uASYNC
 * on the callee. If the callee is declared AFTER the call site, the (single)
 * addressing pass sees findglb==NULL and picks the normal-call path, while the
 * write pass sees the persisted uASYNC symbol and picks the ergonomic/start path
 * -- divergent code_idx/labels -> silent miscompile (segfault), no diagnostic
 * (the phase-error assert is compiled out in Release). Like "yield" (uGENERATOR)
 * and the call-hook seen-set, we force ONE extra addressing pass so the callee
 * symbol (which persists across passes with uASYNC; reduce_referrers/delete_symbols
 * keep non-native functions) is resolvable on the reparse and both passes emit the
 * same path. This flag is monotonic and set at most once per compilation, so a
 * genuinely-undefined callee cannot spin the reparse loop: after that one extra
 * pass every async function that CAN be resolved IS, and any still-unresolved name
 * falls through to the ordinary undefined-symbol / non-async error path. */
static int pc_async_fwd_reparsed = FALSE;
static int sc_parsenum = 0;     /* number of the extra parses */
static int wq[wqTABSZ];         /* "while queue", internal stack for nested loops */
static int *wqptr;              /* pointer to next entry */
static time_t now;              /* current timestamp, for built-in constants "__time" and "__timestamp" */
static char reportname[_MAX_PATH];/* report file name */
#if !defined SC_LIGHT
  static char sc_rootpath[_MAX_PATH];
  static char *sc_documentation=NULL;/* main documentation */
#endif
#if defined	__WIN32__ || defined _WIN32 || defined _Windows
  static HWND hwndFinish = 0;
#endif

#if !defined NO_MAIN

#if defined __TURBOC__ && !defined __32BIT__
  extern unsigned int _stklen = 0x2000;
#endif

int main(int argc, char *argv[])
{
  return pc_compile(argc,argv);
}

/* pc_printf()
 * Called for general purpose "console" output. This function prints general
 * purpose messages; errors go through pc_error(). The function is modelled
 * after printf().
 */
int pc_printf(const char *message,...)
{
  int ret;
  va_list argptr;

  va_start(argptr,message);
  ret=vprintf(message,argptr);
  va_end(argptr);

  return ret;
}

/* pc_error()
 * Called for producing error output.
 *    number      the error number (as documented in the manual)
 *    message     a string describing the error with embedded %d and %s tokens
 *    filename    the name of the file currently being parsed
 *    firstline   the line number at which the expression started on which
 *                the error was found, or -1 if there is no "starting line"
 *    lastline    the line number at which the error was detected
 *    argptr      a pointer to the first of a series of arguments (for macro
 *                "va_arg")
 * Return:
 *    If the function returns 0, the parser attempts to continue compilation.
 *    On a non-zero return value, the parser aborts.
 */
int pc_error(int number,char *message,char *filename,int firstline,int lastline,va_list argptr)
{
static char *prefix[3]={ "error", "fatal error", "warning" };

  if (number!=0) {
    char *pre;

    pre=prefix[number/100];
    if (number>=253 || (number>=200 && pc_geterrorwarnings())){
      pre=prefix[0];      /* error numbers 253 and up are errors, although */
    }                     /* they share the number range of the warnings */
    if (firstline>=0)
      fprintf(stderr,"%s(%d -- %d) : %s %03d: ",filename,firstline,lastline,pre,number);
    else
      fprintf(stderr,"%s(%d) : %s %03d: ",filename,lastline,pre,number);
  } /* if */
  vfprintf(stderr,message,argptr);
  fflush(stderr);
  return 0;
}

/* pc_opensrc()
 * Opens a source file (or include file) for reading. The "file" does not have
 * to be a physical file, one might compile from memory.
 *    filename    the name of the "file" to read from
 * Return:
 *    The function must return a pointer, which is used as a "magic cookie" to
 *    all I/O functions. When failing to open the file for reading, the
 *    function must return NULL.
 * Note:
 *    Several "source files" may be open at the same time. Specifically, one
 *    file can be open for reading and another for writing.
 */
void *pc_opensrc(char *filename)
{
  return fopen(filename,"r");
}

/* pc_createsrc()
 * Creates/overwrites a source file for writing. The "file" does not have
 * to be a physical file, one might compile from memory.
 *    filename    the name of the "file" to create
 * Return:
 *    The function must return a pointer, which is used as a "magic cookie" to
 *    all I/O functions. When failing to open the file for reading, the
 *    function must return NULL.
 * Note:
 *    Several "source files" may be open at the same time. Specifically, one
 *    file can be open for reading and another for writing.
 */
void *pc_createsrc(char *filename)
{
  return fopen(filename,"w");
}

/* pc_createtmpsrc()
 * Creates a temporary source file with a unique name for writing.
 * Return:
 *    The function must return a pointer, which is used as a "magic cookie" to
 *    all I/O functions. When failing to open the file for reading, the
 *    function must return NULL.
 */
void *pc_createtmpsrc(char **filename)
{
  char *tname=NULL;
  FILE *ftmp=NULL;

  #if defined	__WIN32__ || defined _WIN32
    tname=_tempnam(NULL,"pawn");
    ftmp=fopen(tname,"wt");
  #elif defined __MSDOS__ || defined _Windows
    tname=tempnam(NULL,"pawn");
    ftmp=fopen(tname,"wt");
  #else
    static const char template[]="/tmp/pawnXXXXXX";
    if ((tname=malloc(sizeof(template)))!=NULL) {
      int fdtmp;
      strncpy(tname,template,arraysize(template));
      if ((fdtmp=mkstemp(tname)) >= 0) {
        ftmp=fdopen(fdtmp,"wt");
      } else {
        free(tname);
        tname=NULL;
      } /* if */
    } /* if */
  #endif
  if (filename!=NULL)
    *filename=tname;
  return ftmp;
}

/* pc_closesrc()
 * Closes a source file (or include file). The "handle" parameter has the
 * value that pc_opensrc() returned in an earlier call.
 */
void pc_closesrc(void *handle)
{
  assert(handle!=NULL);
  fclose((FILE*)handle);
}

/* pc_resetsrc()
 * "position" may only hold a pointer that was previously obtained from
 * pc_getpossrc()
 */
void pc_resetsrc(void *handle,void *position)
{
  assert(handle!=NULL);
  fsetpos((FILE*)handle,(fpos_t *)position);
}

/* pc_readsrc()
 * Reads a single line from the source file (or up to a maximum number of
 * characters if the line in the input file is too long).
 */
char *pc_readsrc(void *handle,unsigned char *target,int maxchars)
{
  return fgets((char*)target,maxchars,(FILE*)handle);
}

/* pc_writesrc()
 * Writes to to the source file. There is no automatic line ending; to end a
 * line, write a "\n".
 */
int pc_writesrc(void *handle,unsigned char *source)
{
  return fputs((char*)source,(FILE*)handle) >= 0;
}

void *pc_getpossrc(void *handle)
{
  static fpos_t lastpos;  /* may need to have a LIFO stack of such positions */

  fgetpos((FILE*)handle,&lastpos);
  return &lastpos;
}

int pc_eofsrc(void *handle)
{
  return feof((FILE*)handle);
}

/* should return a pointer, which is used as a "magic cookie" to all I/O
 * functions; return NULL for failure
 */
void *pc_openasm(char *filename)
{
  #if defined __MSDOS__ || defined SC_LIGHT
    return fopen(filename,"w+");
  #else
    return mfcreate(filename);
  #endif
}

void pc_closeasm(void *handle, int deletefile)
{
  #if defined __MSDOS__ || defined SC_LIGHT
    if (handle!=NULL)
      fclose((FILE*)handle);
    if (deletefile)
      remove(outfname);
  #else
    if (handle!=NULL) {
      if (!deletefile)
        mfdump((MEMFILE*)handle);
      mfclose((MEMFILE*)handle);
    } /* if */
  #endif
}

void pc_resetasm(void *handle)
{
  assert(handle!=NULL);
  #if defined __MSDOS__ || defined SC_LIGHT
    fflush((FILE*)handle);
    fseek((FILE*)handle,0,SEEK_SET);
  #else
    mfseek((MEMFILE*)handle,0,SEEK_SET);
  #endif
}

int pc_writeasm(void *handle,char *string)
{
  #if defined __MSDOS__ || defined SC_LIGHT
    return fputs(string,(FILE*)handle) >= 0;
  #else
    return mfputs((MEMFILE*)handle,string);
  #endif
}

char *pc_readasm(void *handle, char *string, int maxchars)
{
  #if defined __MSDOS__ || defined SC_LIGHT
    return fgets(string,maxchars,(FILE*)handle);
  #else
    return mfgets((MEMFILE*)handle,string,maxchars);
  #endif
}

/* Should return a pointer, which is used as a "magic cookie" to all I/O
 * functions; return NULL for failure.
 */
void *pc_openbin(char *filename)
{
  FILE *fbin;

  fbin=fopen(filename,"wb");
  setvbuf(fbin,NULL,_IOFBF,1UL<<20);
  return fbin;
}

void pc_closebin(void *handle,int deletefile)
{
  fclose((FILE*)handle);
  if (deletefile)
    remove(binfname);
}

/* pc_resetbin()
 * Can seek to any location in the file.
 * The offset is always from the start of the file.
 */
void pc_resetbin(void *handle,long offset)
{
  fflush((FILE*)handle);
  fseek((FILE*)handle,offset,SEEK_SET);
}

int pc_writebin(void *handle,void *buffer,int size)
{
  return (int)fwrite(buffer,1,size,(FILE*)handle) == size;
}

long pc_lengthbin(void *handle)
{
  return ftell((FILE*)handle);
}

#endif  /* !defined NO_MAIN */


/*  "main" of the compiler
 */
#if defined __cplusplus
  extern "C"
#endif
int pc_compile(int argc, char *argv[])
{
  int entry,i,jmpcode;
  int retcode;
  char incfname[_MAX_PATH];
  char codepage[MAXCODEPAGE+1];
  FILE *binf;
  void *inpfmark;
  int lcl_packstr,lcl_needsemicolon,lcl_tabsize;
  #if !defined SC_LIGHT
    int hdrsize=0;
  #endif
  char *ptr;
  char *tname=NULL;

  /* set global variables to their initial value */
  binf=NULL;
  initglobals();
  errorset(sRESET,0);
  errorset(sEXPRRELEASE,0);
  lexinit();

  /* make sure that we clean up on a fatal error; do this before the first
   * call to error(). */
  if ((jmpcode=setjmp(errbuf))!=0)
    goto cleanup;

  /* allocate memory for fixed tables */
  inpfname=(char*)malloc(_MAX_PATH);
  if (inpfname==NULL)
    error(103);         /* insufficient memory */
  litq=(cell*)malloc(litmax*sizeof(cell));
  if (litq==NULL)
    error(103);         /* insufficient memory */

  /* inptfname may be used in error(), fill it with zeros */
  memset(inpfname,0,_MAX_PATH);

  setopt(argc,argv,outfname,errfname,incfname,codepage);
  strcpy(binfname,outfname);
  ptr=get_extension(binfname);
  if (ptr!=NULL && stricmp(ptr,".asm")==0)
    set_extension(binfname,".amx",TRUE);
  else
    set_extension(binfname,".amx",FALSE);
  /* set output names that depend on the input name */
  if (sc_listing)
    set_extension(outfname,".lst",TRUE);
  else
    set_extension(outfname,".asm",TRUE);
  if (!strempty(errfname))
    remove(errfname);   /* delete file on startup */
  else if (verbosity>0)
    setcaption();
  setconfig(argv[0]);   /* the path to the include and codepage files */
  sc_ctrlchar_org=sc_ctrlchar;
  lcl_packstr=sc_packstr;
  lcl_needsemicolon=sc_needsemicolon;
  lcl_tabsize=sc_tabsize;
  #if !defined NO_CODEPAGE
    if (!cp_set(codepage))      /* set codepage */
      error(108);               /* codepage mapping file not found */
  #endif
  /* optionally create a temporary input file that is a collection of all
   * input files
   */
  assert(get_sourcefile(0)!=NULL);  /* there must be at least one source file */
  if (get_sourcefile(1)!=NULL) {
    /* there are at least two or more source files */
    char *sname;
    FILE *ftmp,*fsrc;
    int fidx;
    ftmp=pc_createtmpsrc(&tname);
    for (fidx=0; (sname=get_sourcefile(fidx))!=NULL; fidx++) {
      unsigned char tstring[128];
      fsrc=(FILE*)pc_opensrc(sname);
      if (fsrc==NULL) {
        strcpy(inpfname,sname); /* avoid invalid filename */
        error(100,sname);
      } /* if */
      pc_writesrc(ftmp,(unsigned char*)"#file \"");
      pc_writesrc(ftmp,(unsigned char*)sname);
      pc_writesrc(ftmp,(unsigned char*)"\"\n");
      while (pc_readsrc(fsrc,tstring,arraysize(tstring))!=NULL) {
        pc_writesrc(ftmp,tstring);
      } /* while */
      pc_writesrc(ftmp,(unsigned char*)"\n");
      pc_closesrc(fsrc);
    } /* for */
    pc_closesrc(ftmp);
    strcpy(inpfname,tname);
  } else {
    strcpy(inpfname,get_sourcefile(0));
  } /* if */
  inpf_org=(FILE*)pc_opensrc(inpfname);
  if (inpf_org==NULL)
    error(100,inpfname);
  freading=TRUE;
  outf=(FILE*)pc_openasm(outfname); /* first write to assembler file (may be temporary) */
  if (outf==NULL)
    error(101,outfname);
  /* immediately open the binary file, for other programs to check */
  if (sc_asmfile || sc_listing) {
    binf=NULL;
  } else {
    binf=(FILE*)pc_openbin(binfname);
    if (binf==NULL)
      error(101,binfname);
  } /* if */
  setconstants();               /* set predefined constants and tagnames */
  for (i=0; i<skipinput; i++)   /* skip lines in the input file */
    if (pc_readsrc(inpf_org,pline,sLINEMAX)!=NULL)
      fline++;                  /* keep line number up to date */
  skipinput=fline;
  sc_status=statFIRST;
  /* do the first pass through the file (or possibly two or more "first passes") */
  sc_parsenum=0;
  pc_async_fwd_reparsed=FALSE;   /* reset the forward-async reparse gate once per compilation */
  inpfmark=pc_getpossrc(inpf_org);
  do {
    /* reset "defined" flag of all functions and global variables */
    reduce_referrers(&glbtab);
    delete_symbols(&glbtab,0,TRUE,FALSE);
    delete_heaplisttable();
    #if !defined NO_DEFINE
      delete_substtable();
    #endif
    resetglobals();
    sc_ctrlchar=sc_ctrlchar_org;
    sc_packstr=lcl_packstr;
    sc_needsemicolon=lcl_needsemicolon;
    sc_tabsize=lcl_tabsize;
    errorset(sRESET,0);
    /* reset the source file */
    inpf=inpf_org;
    freading=TRUE;
    pc_resetsrc(inpf,inpfmark); /* reset file position */
    fline=skipinput;            /* reset line number */
    sc_reparse=FALSE;           /* assume no extra passes */
    sc_status=statFIRST;        /* resetglobals() resets it to IDLE */
    setstringconstants();
    setfileconst(inpfname);
    if (!strempty(incfname)) {
      if (strcmp(incfname,sDEF_PREFIX)==0) {
        plungefile(incfname,FALSE,TRUE);    /* parse "default.inc" */
      } else {
        if (!plungequalifiedfile(incfname,1)) /* parse "prefix" include file */
          error(100,incfname);          /* cannot read from ... (fatal error) */
      } /* if */
    } /* if */
    warnstack_init();
    preprocess();                       /* fetch first line */
    parse();                            /* process all input */
    timers_emit();                      /* synthesise @yt_init timer registration (addressing pass) */
    hook_emit_dispatchers();            /* synthesise native-hook dispatchers (addressing pass) */
    callhook_emit();                    /* synthesise call-site hook wrappers/dispatchers (addressing pass) */
    generator_emit_helper();            /* synthesise @yield.emit for coroutine generators */
    warnstack_cleanup();
    sc_parsenum++;
  } while (sc_reparse);

  /* second (or third) pass */
  if (sc_listing)
    sc_status=statSECOND;
  else
    sc_status=statWRITE;                  /* set, to enable warnings */
  state_conflict(&glbtab);

  /* write a report, if requested */
  #if !defined SC_LIGHT
    if (sc_makereport) {
      FILE *frep=stdout;
      if (!strempty(reportname))
        frep=fopen(reportname,"wb");    /* avoid translation of \n to \r\n in DOS/Windows */
      if (frep!=NULL) {
        make_report(&glbtab,frep,get_sourcefile(0));
        if (!strempty(reportname))
          fclose(frep);
      } /* if */
      if (sc_documentation!=NULL) {
        free(sc_documentation);
        sc_documentation=NULL;
      } /* if */
    } /* if */
  #endif

  sc_ctrlchar=sc_ctrlchar_org;
  sc_packstr=lcl_packstr;
  sc_needsemicolon=lcl_needsemicolon;
  sc_tabsize=lcl_tabsize;

  /*if (sc_listing)
    goto cleanup;*/
  /* write starting options (from the command line or the configuration file) */
  if (sc_listing) {
    char string[150];
    sprintf(string,"#pragma ctrlchar 0x%02x\n"
                   "#pragma pack %s\n"
                   "#pragma semicolon %s\n"
                   "#pragma tabsize %d\n",
            sc_ctrlchar,
            sc_packstr ? "true" : "false",
            sc_needsemicolon ? "true" : "false",
            sc_tabsize);
    pc_writeasm(outf,string);
  } /* if */
  setfiledirect(inpfname);

  /* ??? for re-parsing the listing file instead of the original source
   * file (and doing preprocessing twice):
   * - close input file, close listing file
   * - re-open listing file for reading (inpf)
   * - open assembler file (outf)
   */

  /* reset "defined" flag of all functions and global variables */
  reduce_referrers(&glbtab);
  delete_symbols(&glbtab,0,TRUE,FALSE);
  #if !defined NO_DEFINE
    delete_substtable();
  #endif
  resetglobals();
  errorset(sRESET,0);
  /* reset the source file */
  inpf=inpf_org;
  freading=TRUE;
  pc_resetsrc(inpf,inpfmark);   /* reset file position */
  fline=skipinput;              /* reset line number */
  lexinit();                    /* clear internal flags of lex() */
  if (sc_listing)
    sc_status=statSECOND;
  else
    sc_status=statWRITE;          /* allow to write --this variable was reset by resetglobals() */
  writeleader(&glbtab);
  setstringconstants();
  setfileconst(inpfname);
  insert_dbgfile(inpfname);
  if (!strempty(incfname)) {
    if (strcmp(incfname,sDEF_PREFIX)==0)
      plungefile(incfname,FALSE,TRUE);  /* parse "default.inc" (again) */
    else
    {
        
    }
      plungequalifiedfile(incfname,1);    /* parse implicit include file (again) */
  } /* if */
  warnstack_init();
  preprocess();                         /* fetch first line */
  parse();                              /* process all input */
  timers_emit();                        /* synthesise @yt_init timer registration (code-emission pass) */
  hook_emit_dispatchers();              /* synthesise native-hook dispatchers (code-emission pass) */
  callhook_emit();                      /* synthesise call-site hook wrappers/dispatchers (code-emission pass) */
  generator_emit_helper();              /* synthesise @yield.emit for coroutine generators */
  warnstack_cleanup();
  if (sc_listing)
    goto cleanup;
  /* inpf is already closed when readline() attempts to pop of a file */
  writetrailer();                       /* write remaining stuff */

  entry=testsymbols(&glbtab,0,TRUE,FALSE);  /* test for unused or undefined
                                             * functions and variables */
  if (!entry)
    error(13);                  /* no entry point (no public functions) */

cleanup:
  if (inpf!=NULL) {             /* main source file is not closed, do it now */
    pc_closesrc(inpf);
    inpf=NULL;
  }
  /* write the binary file (the file is already open) */
  if (!(sc_asmfile || sc_listing) && errnum==0 && jmpcode==0) {
    assert(binf!=NULL);
    pc_resetasm(outf);          /* flush and loop back, for reading */
    #if !defined SC_LIGHT
      hdrsize=
    #endif
    assemble(binf,outf);        /* assembler file is now input */
  } /* if */
  if (outf!=NULL) {
    pc_closeasm(outf,!(sc_asmfile || sc_listing));
    outf=NULL;
  } /* if */
  if (binf!=NULL) {
    pc_closebin(binf,errnum!=0);
    binf=NULL;
  } /* if */

  #if !defined SC_LIGHT
    if (errnum==0 && strempty(errfname)) {
      int recursion;
      long stacksize=max_stacksize(&glbtab,&recursion);
      int flag_exceed=FALSE;
      if (pc_amxlimit>0) {
        long totalsize=hdrsize+code_idx;
        if (pc_amxram==0)
          totalsize+=(glb_declared+pc_stksize)*sizeof(cell);
        if (totalsize>=pc_amxlimit)
          flag_exceed=TRUE;
      } /* if */
      if (pc_amxram>0 && (glb_declared+pc_stksize)*sizeof(cell)>=(unsigned long)pc_amxram)
        flag_exceed=TRUE;
      if ((sc_debug & sSYMBOLIC)!=0 || verbosity>=2 || stacksize+32>=(long)pc_stksize || flag_exceed) {
        pc_printf("Header size:       %8ld bytes\n", (long)hdrsize);
        pc_printf("Code size:         %8ld bytes\n", (long)code_idx);
        pc_printf("Data size:         %8ld bytes\n", (long)glb_declared*sizeof(cell));
        pc_printf("Stack/heap size:   %8ld bytes; ", (long)pc_stksize*sizeof(cell));
        pc_printf("estimated max. usage");
        if (recursion)
          pc_printf(": unknown, due to recursion\n");
        else if ((pc_memflags & suSLEEP_INSTR)!=0)
          pc_printf(": unknown, due to the \"sleep\" instruction\n");
        else
          pc_printf("=%ld cells (%ld bytes)\n",stacksize,stacksize*sizeof(cell));
        pc_printf("Total requirements:%8ld bytes\n", (long)hdrsize+(long)code_idx+(long)glb_declared*sizeof(cell)+(long)pc_stksize*sizeof(cell));
      } /* if */
      if (flag_exceed)
        error(106,pc_amxlimit+pc_amxram); /* this causes a jump back to label "cleanup" */
    } /* if */
  #endif

  if (get_sourcefile(1)!=NULL && tname!=NULL) {
    remove(tname);         /* the "input file" was in fact a temporary file */
    free(tname);
  } /* if */
  free(inpfname);
  free(litq);
  stgbuffer_cleanup();
  clearstk();
  assert(jmpcode!=0 || loctab.next==NULL);/* on normal flow, local symbols
                                           * should already have been deleted */
  delete_symbols(&loctab,0,TRUE,TRUE);    /* delete local variables if not yet
                                           * done (i.e. on a fatal error) */
  delete_symbols(&glbtab,0,TRUE,TRUE);
  line_sym=NULL;
  free(pc_deprecate);
  pc_deprecate=NULL;
  free(pc_recstr);
  pc_recstr=NULL;
  hashtable_term(&symbol_cache_ht);
  delete_consttable(&tagname_tab);
  delete_consttable(&libname_tab);
  delete_consttable(&sc_automaton_tab);
  delete_consttable(&sc_state_tab);
  state_deletetable();
  delete_aliastable();
  delete_pathtable();
  delete_sourcefiletable();
  delete_dbgstringtable();
  #if !defined NO_DEFINE
    delete_substtable();
  #endif
  #if !defined SC_LIGHT
    delete_docstringtable();
    free(sc_documentation);
  #endif
  delete_autolisttable();
  delete_heaplisttable();
  if (errnum!=0) {
    if (strempty(errfname))
      pc_printf("\n%d Error%s.\n",errnum,(errnum>1) ? "s" : "");
    retcode=1;
  } else if (warnnum!=0){
    if (strempty(errfname))
      pc_printf("\n%d Warning%s.\n",warnnum,(warnnum>1) ? "s" : "");
    retcode=0;          /* use "0", so that MAKE and similar tools continue */
  } else {
    retcode=jmpcode;
    if (retcode==0 && verbosity>=2)
      pc_printf("\nDone.\n");
  } /* if */
  #if defined	__WIN32__ || defined _WIN32 || defined _Windows
    if (IsWindow(hwndFinish))
      PostMessage(hwndFinish,RegisterWindowMessage("PawnNotify"),retcode,0L);
  #endif
  #if defined FORTIFY
    Fortify_ListAllMemory();
  #endif
  return retcode;
}

#if defined __cplusplus
  extern "C"
#endif
int pc_addconstant(char *name,cell value,int tag)
{
  errorset(sFORCESET,0);        /* make sure error engine is silenced */
  sc_status=statIDLE;
  add_constant(name,value,sGLOBAL,tag);
  return 1;
}

#if defined __cplusplus
  extern "C"
#endif
int pc_addtag(char *name)
{
  cell val;
  constvalue *ptr;
  int last,tag;

  if (name==NULL) {
    /* no tagname was given, check for one */
    if (lex(&val,&name)!=tLABEL) {
      lexpush();
      return 0;         /* untagged */
    } /* if */
  } /* if */

  assert(strchr(name,':')==NULL); /* colon should already have been stripped */
  last=0;
  ptr=tagname_tab.first;
  while (ptr!=NULL) {
    tag=(int)(ptr->value & TAGMASK);
    if (strcmp(name,ptr->name)==0)
      return tag;       /* tagname is known, return its sequence number */
    tag &= (int)~FIXEDTAG;
    if (tag>last)
      last=tag;
    ptr=ptr->next;
  } /* while */

  /* tagname currently unknown, add it */
  tag=last+1;           /* guaranteed not to exist already */
  if (isupper(*name))
    tag |= (int)FIXEDTAG;
  append_constval(&tagname_tab,name,(cell)tag,0);
  return tag;
}

static void resetglobals(void)
{
  /* reset the subset of global variables that is modified by the first pass */
  curfunc=NULL;         /* pointer to current function */
  lastst=0;             /* last executed statement type */
  pc_nestlevel=0;       /* number of active (open) compound statements */
  rettype=0;            /* the type that a "return" expression should have */
  litidx=0;             /* index to literal table */
  stgidx=0;             /* index to the staging buffer */
  sc_labnum=0;          /* top value of (internal) labels */
  staging=FALSE;        /* true if staging output */
  declared=0;           /* number of local cells declared */
  glb_declared=0;       /* number of global cells declared */
  code_idx=0;           /* number of bytes with generated code */
  ntv_funcid=0;         /* incremental number of native function */
  curseg=0;             /* 1 if currently parsing CODE, 2 if parsing DATA */
  freading=FALSE;       /* no input file ready yet */
  fline=0;              /* the line number in the current file */
  fnumber=0;            /* the file number in the file table (debugging) */
  fcurrent=0;           /* current file being processed (debugging) */
  sc_intest=FALSE;      /* true if inside a test */
  pc_sideeffect=FALSE;  /* true if an expression causes a side-effect */
  pc_ovlassignment=FALSE;/* true if an expression contains an overloaded assignment */
  stmtindent=0;         /* current indent of the statement */
  indent_nowarn=FALSE;  /* do not skip warning "217 loose indentation" */
  sc_allowtags=TRUE;    /* allow/detect tagnames */
  sc_status=statIDLE;
  sc_allowproccall=FALSE;
  pc_addlibtable=TRUE;  /* by default, add a "library table" to the output file */
  sc_alignnext=FALSE;
  pc_docexpr=FALSE;
  free(pc_deprecate);
  pc_deprecate=NULL;
  sc_curstates=0;
  pc_memflags=0;
  pc_naked=FALSE;
  pc_retexpr=FALSE;
  pc_attributes=0;
  pc_loopcond=0;
  emit_flags=0;
  emit_stgbuf_idx=-1;
  hook_reset();         /* clear the per-pass native-hook registry */
  callhook_reset();     /* clear the per-pass call-site hook registry */
  timers_reset();       /* clear the per-pass timer-task registry */
}

static void initglobals(void)
{
  resetglobals();

  sc_asmfile=FALSE;      /* do not create .ASM file */
  sc_listing=FALSE;      /* do not create .LST file */
  skipinput=0;           /* number of lines to skip from the first input file */
  sc_ctrlchar=CTRL_CHAR; /* the escape character */
  litmax=sDEF_LITMAX;    /* current size of the literal table */
  litgrow=sDEF_LITMAX;   /* amount to increase the literal table by */
  errnum=0;              /* number of errors */
  warnnum=0;             /* number of warnings */
  optproccall=TRUE;      /* support "procedure call" */
  verbosity=1;           /* verbosity level, no copyright banner */
  sc_debug=sCHKBOUNDS;   /* by default: bounds checking+assertions */
  pc_optimize=sOPTIMIZE_NOMACRO;
  sc_packstr=FALSE;      /* strings are unpacked by default */
  #if AMX_COMPACTMARGIN > 2
    sc_compress=TRUE;    /* compress output bytecodes */
  #else
    sc_compress=FALSE;
  #endif
  sc_needsemicolon=FALSE;   /* semicolon required to terminate expressions? */
  sc_dataalign=sizeof(cell);
  pc_stksize=sDEF_AMXSTACK; /* default stack size */
  pc_amxlimit=0;         /* no limit on size of the abstract machine */
  pc_amxram=0;           /* no limit on data size of the abstract machine */
  sc_tabsize=8;          /* assume a TAB is 8 spaces */
  sc_rationaltag=0;      /* assume no support for rational numbers */
  rational_digits=0;     /* number of fractional digits */

  outfname[0]='\0';      /* output file name */
  errfname[0]='\0';      /* error file name */
  inpf=NULL;             /* file read from */
  inpfname=NULL;         /* pointer to name of the file currently read from */
  outf=NULL;             /* file written to */
  litq=NULL;             /* the literal queue */
  glbtab.next=NULL;      /* clear global variables/constants table */
  loctab.next=NULL;      /*   "   local      "    /    "       "   */
  hashtable_init(&symbol_cache_ht, sizeof(symbol *),(16384/3*2),NULL); /* 16384 slots */
  tagname_tab.first=tagname_tab.last=NULL; /* tagname table */
  libname_tab.first=libname_tab.last=NULL; /* library table (#pragma library "..." syntax) */

  pline[0]='\0';         /* the line read from the input file */
  lptr=NULL;             /* points to the current position in "pline" */
  curlibrary=NULL;       /* current library */
  inpf_org=NULL;         /* main source file */

  wqptr=wq;              /* initialize while queue pointer */
  reportname[0]='\0';    /* report file name */

#if !defined SC_LIGHT
  sc_documentation=NULL;
  sc_makereport=FALSE;   /* do not generate a cross-reference report */
#endif
}

static char *get_extension(char *filename)
{
  char *ptr;

  assert(filename!=NULL);
  ptr=strrchr(filename,'.');
  if (ptr!=NULL) {
    /* ignore extension on a directory or at the start of the filename */
    if (strchr(ptr,DIRSEP_CHAR)!=NULL || ptr==filename || *(ptr-1)==DIRSEP_CHAR)
      ptr=NULL;
  } /* if */
  return ptr;
}

/* set_extension
 * Set the default extension, or force an extension. To erase the
 * extension of a filename, set "extension" to an empty string.
 */
SC_FUNC void set_extension(char *filename,char *extension,int force)
{
  char *ptr;

  assert(extension!=NULL && (*extension=='\0' || *extension=='.'));
  assert(filename!=NULL);
  ptr=get_extension(filename);
  if (force && ptr!=NULL)
    *ptr='\0';          /* set zero terminator at the position of the period */
  if (force || ptr==NULL)
    strcat(filename,extension);
}

static const char *option_value(const char *optptr)
{
  return (*(optptr+1)=='=' || *(optptr+1)==':') ? optptr+2 : optptr+1;
}

static int toggle_option(const char *optptr, int option)
{
  switch (*option_value(optptr)) {
  case '\0':
    option=TRUE;
    break;
  case '-':
    option=FALSE;
    break;
  case '+':
    option=TRUE;
    break;
  default:
    invalid_option(optptr);
  } /* switch */
  return option;
}

/* Parsing command line options is indirectly recursive: parseoptions()
 * calls parserespf() to handle options in a a response file and
 * parserespf() calls parseoptions() at its turn after having created
 * an "option list" from the contents of the file.
 */
static void parserespf(char *filename,char *oname,char *ename,char *pname,
                       char *codepage);

static void parseoptions(int argc,char **argv,char *oname,char *ename,char *pname,
                         char *codepage)
{
  char str[_MAX_PATH],*name;
  const char *ptr;
  int arg,i,isoption;

  for (arg=1; arg<argc; arg++) {
    #if DIRSEP_CHAR=='/'
      isoption= argv[arg][0]=='-';
    #else
      isoption= argv[arg][0]=='/' || argv[arg][0]=='-';
    #endif
    if (isoption) {
      ptr=&argv[arg][1];
      switch (*ptr) {
      case 'A':
        i=atoi(option_value(ptr));
        if ((i % sizeof(cell))==0)
          sc_dataalign=i;
        else
          invalid_option(ptr);
        break;
      case 'a':
        if (*(ptr+1)!='\0')
          invalid_option(ptr);
        sc_asmfile=TRUE;        /* skip last pass of making binary file */
        if (verbosity>1)
          verbosity=1;
        break;
      case 'C':
        #if AMX_COMPACTMARGIN > 2
          sc_compress=toggle_option(ptr,sc_compress);
        #else
          invalid_option(ptr);
        #endif
        break;
      case 'c':
        strlcpy(codepage,option_value(ptr),MAXCODEPAGE);  /* set name of codepage */
        break;
      case 'D':                 /* set active directory */
        ptr=option_value(ptr);
#if defined dos_setdrive
        if (ptr[1]==':')
          dos_setdrive(toupper(*ptr)-'A'+1);    /* set active drive */
#endif
        if (chdir(ptr)==-1)
          ; /* silently ignore chdir() errors */
        break;
      case 'd': {
        int debug;
        switch (*option_value(ptr)) {
        case '0':
          sc_debug=0;
          break;
        case '1':
          sc_debug=sCHKBOUNDS;  /* assertions and bounds checking */
          break;
        case '2':
          sc_debug=sCHKBOUNDS | sSYMBOLIC;  /* also symbolic info */
          break;
        case '3':
          sc_debug=sCHKBOUNDS | sSYMBOLIC;
          pc_optimize=sOPTIMIZE_NONE;
          /* also avoid peephole optimization */
          break;
        default:
          invalid_option(ptr);
        } /* switch */
        debug=0;
        if ((sc_debug & (sCHKBOUNDS | sSYMBOLIC))==(sCHKBOUNDS | sSYMBOLIC))
          debug=2;
        else if ((sc_debug & sCHKBOUNDS)==sCHKBOUNDS)
          debug=1;
        add_builtin_constant("debug",debug,sGLOBAL,0);
        break;
      } /* case */
      case 'e':
        if (ename)
          strlcpy(ename,option_value(ptr),_MAX_PATH); /* set name of error file */
        break;
#if defined	__WIN32__ || defined _WIN32 || defined _Windows
      case 'H':
        #if defined __64BIT__
          hwndFinish=(HWND)atoll(option_value(ptr));
        #else
          hwndFinish=(HWND)atoi(option_value(ptr));
        #endif
        if (!IsWindow(hwndFinish))
          hwndFinish=(HWND)0;
        break;
#endif
      case 'i':
        strlcpy(str,option_value(ptr),arraysize(str));  /* set name of include directory */
        i=strlen(str);
        if (i>0) {
          if (str[i-1]!=DIRSEP_CHAR) {
            str[i]=DIRSEP_CHAR;
            str[i+1]='\0';
          } /* if */
          insert_path(str);
        } /* if */
        break;
      case 'l':
        if (*(ptr+1)!='\0')
          invalid_option(ptr);
        sc_listing=TRUE;        /* skip second pass & code generation */
        break;
      case 'o':
        if (oname)
          strlcpy(oname,option_value(ptr),_MAX_PATH); /* set name of (binary) output file */
        break;
      case 'O':
        pc_optimize=*option_value(ptr) - '0';
        if (pc_optimize<sOPTIMIZE_NONE || pc_optimize>=sOPTIMIZE_NUMBER)
          invalid_option(ptr);
        add_builtin_constant("__optimization", pc_optimize, sGLOBAL, 0);
        add_builtin_constant("__optimisation", pc_optimize, sGLOBAL, 0);
        break;
      case 'p':
        if (pname)
          strlcpy(pname,option_value(ptr),_MAX_PATH); /* set name of implicit include file */
        break;
      case 'R':
        pc_recursion=toggle_option(ptr,pc_recursion);
        break;
#if !defined SC_LIGHT
      case 'r':
        strlcpy(reportname,option_value(ptr),_MAX_PATH); /* set name of report file */
        sc_makereport=TRUE;
        if (!strempty(reportname)) {
          set_extension(reportname,".xml",FALSE);
        } else if ((name=get_sourcefile(0))!=NULL) {
          assert(strempty(reportname));
          assert(strlen(name)<_MAX_PATH);
          if ((ptr=strrchr(name,DIRSEP_CHAR))!=NULL)
            ptr++;          /* strip path */
          else
            ptr=name;
          assert(strlen(ptr)<_MAX_PATH);
          strcpy(reportname,ptr);
          set_extension(reportname,".xml",TRUE);
        } /* if */
        break;
#endif
      case 'S':
        i=atoi(option_value(ptr));
        if (i>32)
          pc_stksize=(cell)i;   /* stack size has minimum size */
        else
          invalid_option(ptr);
        break;
      case 's':
        skipinput=atoi(option_value(ptr));
        break;
      case 't':
        i=atoi(option_value(ptr));
        if (i>0)
          sc_tabsize=i;
        else
          invalid_option(ptr);
        break;
      case 'v':
        verbosity= isdigit(*option_value(ptr)) ? atoi(option_value(ptr)) : 2;
        if (sc_asmfile && verbosity>1)
          verbosity=1;
        break;
      case 'E':
        switch (*option_value(ptr)) {
        case '+':
          pc_seterrorwarnings(1);
          break;
        case '-':
          pc_seterrorwarnings(0);
          break;
        default:
          pc_seterrorwarnings(2);
          break;
        }
        break;
      case 'w':
        i=(int)strtol(option_value(ptr),(char **)&ptr,10);
        if (*ptr=='-')
          pc_enablewarning(i,warnDISABLE);
        else if (*ptr=='+')
          pc_enablewarning(i,warnENABLE);
        else if (*ptr=='\0')
          pc_enablewarning(i,warnTOGGLE);
        break;
      case 'X':
        if (*(ptr+1)=='D') {
          i=atoi(option_value(ptr+1));
          if (i>64)
            pc_amxram=(cell)i;  /* abstract machine data/stack has minimum size */
          else
            invalid_option(ptr);
        } else {
          i=atoi(option_value(ptr));
          if (i>64)
            pc_amxlimit=(cell)i;/* abstract machine has minimum size */
          else
            invalid_option(ptr);
        } /* if */
        break;
      case 'Z': {
        symbol *sym;
        pc_compat=toggle_option(ptr,pc_compat);
        sym=findconst("__compat",NULL);
        if (sym!=NULL) {
          assert(sym!=NULL);
          sym->addr=pc_compat;
        } /* if */
        break;
      } /* case */
      case '\\':                /* use \ instead for escape characters */
        sc_ctrlchar='\\';
        break;
      case '^':                 /* use ^ instead for escape characters */
        sc_ctrlchar='^';
        break;
      case ';':
        sc_needsemicolon=toggle_option(ptr,sc_needsemicolon);
        break;
      case '(':
        optproccall=!toggle_option(ptr,!optproccall);
        break;
      default:                  /* wrong option */
        invalid_option(ptr);
      } /* switch */
    } else if (argv[arg][0]=='@') {
      #if !defined SC_LIGHT
        parserespf(&argv[arg][1],oname,ename,pname,codepage);
      #endif
    } else if ((ptr=strchr(argv[arg],'='))!=NULL) {
      i=(int)(ptr-argv[arg]);
      if (i>sNAMEMAX) {
        i=sNAMEMAX;
        error(200,argv[arg],sNAMEMAX);  /* symbol too long, truncated to sNAMEMAX chars */
      } /* if */
      strlcpy(str,argv[arg],i+1);       /* str holds symbol name */
      i=atoi(ptr+1);
      add_builtin_constant(str,i,sGLOBAL,0);
    } else if (oname) {
      strlcpy(str,argv[arg],arraysize(str)-2); /* -2 because default extension is ".p" */
      set_extension(str,".p",FALSE);
      insert_sourcefile(str);
      /* The output name is the first input name with a different extension,
       * but it is stored in a different directory
       */
      if (strempty(oname)) {
        if ((ptr=strrchr(str,DIRSEP_CHAR))!=NULL)
          ptr++;          /* strip path */
        else
          ptr=str;
        assert(strlen(ptr)<_MAX_PATH);
        strcpy(oname,ptr);
      } /* if */
      set_extension(oname,".asm",TRUE);
#if !defined SC_LIGHT
      if (sc_makereport && strempty(reportname)) {
        if ((ptr=strrchr(str,DIRSEP_CHAR))!=NULL)
          ptr++;          /* strip path */
        else
          ptr=str;
        assert(strlen(ptr)<_MAX_PATH);
        strcpy(reportname,ptr);
        set_extension(reportname,".xml",TRUE);
      } /* if */
#endif
    } /* if */
  } /* for */
}

void parsesingleoption(char *argv)
{
  /* argv[0] is the program, which we don't need here */
  char *args[2] = { 0, argv };
  char codepage[MAXCODEPAGE+1] = { 0 };
  codepage[0] = '\0';
  parseoptions(2, args, NULL, NULL, NULL, codepage);
  /* need explicit support for codepages */
  if (codepage[0] && !cp_set(codepage))
    error(108);         /* codepage mapping file not found */
}

#if !defined SC_LIGHT
static void parserespf(char *filename,char *oname,char *ename,char *pname,
                       char *codepage)
{
#define MAX_OPTIONS     100
  FILE *fp;
  char *string, *ptr, **argv;
  int argc;
  long size;

  if ((fp=fopen(filename,"rb"))==NULL)
    error(100,filename);        /* error reading input file */
  /* load the complete file into memory */
  fseek(fp,0L,SEEK_END);
  size=ftell(fp);
  fseek(fp,0L,SEEK_SET);
  assert(size<INT_MAX);
  if ((string=(char *)malloc((int)size+1))==NULL)
    error(103);                 /* insufficient memory */
  /* fill with zeros; in MS-DOS, fread() may collapse CR/LF pairs to
   * a single '\n', so the string size may be smaller than the file
   * size. */
  memset(string,0,(int)size+1);
  if (fread(string,1,(int)size,fp)<(size_t)size)
    error(100,filename);        /* error reading input file */
  fclose(fp);
  /* allocate table for option pointers */
  if ((argv=(char **)malloc(MAX_OPTIONS*sizeof(char*)))==NULL)
    error(103);                 /* insufficient memory */
  /* fill the options table */
  ptr=strtok(string," \t\r\n");
  for (argc=1; argc<MAX_OPTIONS && ptr!=NULL; argc++) {
    /* note: the routine skips argv[0], for compatibility with main() */
    argv[argc]=ptr;
    ptr=strtok(NULL," \t\r\n");
  } /* for */
  if (ptr!=NULL)
    error(102,"option table");   /* table overflow */
  /* parse the option table */
  parseoptions(argc,argv,oname,ename,pname,codepage);
  /* free allocated memory */
  free(argv);
  free(string);
}
#endif

static void setopt(int argc,char **argv,char *oname,char *ename,char *pname,
                   char *codepage)
{
  delete_sourcefiletable(); /* make sure it is empty */
  *oname='\0';
  *ename='\0';
  *pname='\0';
  *codepage='\0';
  strcpy(pname,sDEF_PREFIX);

  #if 0 /* needed to test with BoundsChecker for DOS (it does not pass
         * through arguments) */
    insert_sourcefile("test.p");
    strcpy(oname,"test.asm");
  #endif

  #if !defined SC_LIGHT
    /* first parse a "config" file with default options */
    if (argv[0]!=NULL) {
      char cfgfile[_MAX_PATH];
      char *ext;
      strcpy(cfgfile,argv[0]);
      if ((ext=strrchr(cfgfile,DIRSEP_CHAR))!=NULL) {
        *(ext+1)='\0';          /* strip the program filename */
        strcat(cfgfile,"pawn.cfg");
      } else {
        strcpy(cfgfile,"pawn.cfg");
      } /* if */
      if (access(cfgfile,4)==0)
        parserespf(cfgfile,oname,ename,pname,codepage);
    } /* if */
  #endif
  parseoptions(argc,argv,oname,ename,pname,codepage);
  if (get_sourcefile(0)==NULL)
    about();
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
static void setconfig(char *root)
{
  #if defined macintosh
    insert_path(":include:");
  #else
    char path[_MAX_PATH];
    char *ptr,*base;
    int len;

    /* add the default "include" directory */
    #if defined __WIN32__ || defined _WIN32
      GetModuleFileName(NULL,path,_MAX_PATH);
    #elif defined LINUX || defined __FreeBSD__ || defined __OpenBSD__
      /* see www.autopackage.org for the BinReloc module */
      br_init_lib(NULL);
      ptr=br_find_exe("/opt/Pawn/bin/pawncc");
      strlcpy(path,ptr,arraysize(path));
      free(ptr);
    #else
      if (root!=NULL)
        strlcpy(path,root,arraysize(path)); /* path + filename (hopefully) */
    #endif
    #if defined __MSDOS__
      /* strip the options (appended to the path + filename) */
      if ((ptr=strpbrk(path," \t/"))!=NULL)
        *ptr='\0';
    #endif
    /* terminate just behind last \ or : */
    if ((ptr=strrchr(path,DIRSEP_CHAR))!=NULL || (ptr=strchr(path,':'))!=NULL) {
      /* If there is no "\" or ":", the string probably does not contain the
       * path; so we just don't add it to the list in that case
       */
      *(ptr+1)='\0';
      base=ptr;
      strcat(path,"include");
      len=strlen(path);
      path[len]=DIRSEP_CHAR;
      path[len+1]='\0';
      /* see if it exists */
      if (access(path,0)!=0 && *base==DIRSEP_CHAR) {
        /* There is no "include" directory below the directory where the compiler
         * is found. This typically means that the compiler is in a "bin" sub-directory
         * and the "include" is below the *parent*. So find the parent...
         */
        *base='\0';
        if ((ptr=strrchr(path,DIRSEP_CHAR))!=NULL) {
          *(ptr+1)='\0';
          strcat(path,"include");
          len=strlen(path);
          path[len]=DIRSEP_CHAR;
          path[len+1]='\0';
        } else {
          *base=DIRSEP_CHAR;
        } /* if */
      } /* if */
      insert_path(path);
      /* same for the codepage root */
      #if !defined NO_CODEPAGE
        if (ptr!=NULL)
          *ptr='\0';
        if (!cp_path(path,"codepage"))
          error(109,path);        /* codepage path */
      #endif
      /* also copy the root path (for the XML documentation) */
      #if !defined SC_LIGHT
        if (ptr!=NULL)
          *ptr='\0';
        strcpy(sc_rootpath,path);
      #endif
    } /* if */
  #endif /* macintosh */
}

static void setcaption(void)
{
  pc_printf("PawnX " PAWNX_VERSION "  (Pawn " PAWN_BASE_STR " language base)\n"
            "Copyright (c) 1997-2006 ITB CompuPhase\n"
            "PawnX extensions, 2026\n\n");
}

static void about(void)
{
  usage();
  longjmp(errbuf,3);        /* user abort */
}

static void invalid_option(const char *opt)
{
  usage();
  pc_printf("\nInvalid or unsupported option: -%s\n",opt);
  longjmp(errbuf,3);        /* user abort */
}

static void usage(void)
{
  if (strempty(errfname)) {
    setcaption();
    pc_printf("Usage:   pawncc <filename> [filename...] [options]\n\n");
    pc_printf("Options:\n");
    pc_printf("         -A<num>  alignment in bytes of the data segment and the stack\n");
    pc_printf("         -a       output assembler code\n");
#if AMX_COMPACTMARGIN > 2
    pc_printf("         -C[+/-]  compact encoding for output file (default=%c)\n", sc_compress ? '+' : '-');
#endif
    pc_printf("         -c<name> codepage name or number; e.g. 1252 for Windows Latin-1\n");
    pc_printf("         -Dpath   active directory path\n");
    pc_printf("         -d<num>  debugging level (default=-d%d)\n",sc_debug);
    pc_printf("             0    no symbolic information, no run-time checks\n");
    pc_printf("             1    run-time checks, no symbolic information\n");
    pc_printf("             2    full debug information and dynamic checking\n");
    pc_printf("             3    same as -d2, but implies -O0\n");
    pc_printf("         -e<name> set name of error file (quiet compile)\n");
#if defined	__WIN32__ || defined _WIN32 || defined _Windows
    pc_printf("         -H<hwnd> window handle to send a notification message on finish\n");
#endif
    pc_printf("         -i<name> path for include files\n");
    pc_printf("         -l       create list file (preprocess only)\n");
    pc_printf("         -o<name> set base name of (P-code) output file\n");
    pc_printf("         -O<num>  optimization level (default=-O%d)\n",pc_optimize);
    pc_printf("             0    no optimization\n");
    pc_printf("             1    JIT-compatible optimizations only\n");
    pc_printf("             2    full optimizations\n");
    pc_printf("         -p<name> set name of \"prefix\" file\n");
    pc_printf("         -R[+/-]  add detailed recursion report with call chains (default=%c)\n",pc_recursion ? '+' : '-');
#if !defined SC_LIGHT
    pc_printf("         -r[name] write cross reference report to console or to specified file\n");
#endif
    pc_printf("         -S<num>  stack/heap size in cells (default=%d)\n",(int)pc_stksize);
    pc_printf("         -s<num>  skip lines from the input file\n");
    pc_printf("         -t<num>  TAB indent size (in character positions, default=%d)\n",sc_tabsize);
    pc_printf("         -v<num>  verbosity level; 0=quiet, 1=normal, 2=verbose (default=%d)\n",verbosity);
    pc_printf("         -w<num>  disable a specific warning by its number\n");
    pc_printf("         -X<num>  abstract machine size limit in bytes\n");
    pc_printf("         -XD<num> abstract machine data/stack size limit in bytes\n");
    pc_printf("         -Z[+/-]  run in compatibility mode (default=%c)\n",pc_compat ? '+' : '-');
    pc_printf("         -E[+/-]  turn warnings in to errors\n");
    pc_printf("         -\\       use '\\' for escape characters\n");
    pc_printf("         -^       use '^' for escape characters\n");
    pc_printf("         -;[+/-]  require a semicolon to end each statement (default=%c)\n", sc_needsemicolon ? '+' : '-');
    pc_printf("         -([+/-]  require parentheses for function invocation (default=%c)\n", optproccall ? '-' : '+');
    pc_printf("         sym=val  define constant \"sym\" with value \"val\"\n");
    pc_printf("         sym=     define constant \"sym\" with value 0\n");
#if defined	__WIN32__ || defined _WIN32 || defined _Windows || defined __MSDOS__
    pc_printf("\nOptions may start with a dash or a slash; the options \"-d0\" and \"/d0\" are\n");
    pc_printf("equivalent.\n");
#endif
    pc_printf("\nOptions with a value may optionally separate the value from the option letter\n");
    pc_printf("with a colon (\":\") or an equal sign (\"=\"). That is, the options \"-d0\", \"-d=0\"\n");
    pc_printf("and \"-d:0\" are all equivalent.\n");
  } /* if */
}

static void setconstants(void)
{
  int debug;
  time_t loctime;
  struct tm loctm,utctm;

  assert(sc_status==statIDLE);
  append_constval(&tagname_tab,"_",0,0);/* "untagged" */
  append_constval(&tagname_tab,"bool",BOOLTAG,0);

  add_builtin_constant("true",1,sGLOBAL,BOOLTAG);/* boolean flags */
  add_builtin_constant("false",0,sGLOBAL,BOOLTAG);
  add_builtin_constant("__PawnX",100,sGLOBAL,0); /* pawn-x compiler detect (version*100): `#if defined __PawnX` is true only on this compiler, independent of any include */
  add_builtin_constant("EOS",0,sGLOBAL,0);      /* End Of String, or '\0' */
  #if PAWN_CELL_SIZE==16
  add_builtin_constant("cellbits",16,sGLOBAL,0);
    #if defined _I16_MAX
      add_builtin_constant("cellmax",_I16_MAX,sGLOBAL,0);
      add_builtin_constant("cellmin",_I16_MIN,sGLOBAL,0);
    #else
      add_builtin_constant("cellmax",SHRT_MAX,sGLOBAL,0);
      add_builtin_constant("cellmin",SHRT_MIN,sGLOBAL,0);
    #endif
  #elif PAWN_CELL_SIZE==32
    add_builtin_constant("cellbits",32,sGLOBAL,0);
    #if defined _I32_MAX
      add_builtin_constant("cellmax",_I32_MAX,sGLOBAL,0);
      add_builtin_constant("cellmin",_I32_MIN,sGLOBAL,0);
    #else
      add_builtin_constant("cellmax",INT_MAX,sGLOBAL,0);
      add_builtin_constant("cellmin",INT_MIN,sGLOBAL,0);
    #endif
  #elif PAWN_CELL_SIZE==64
    #if !defined _I64_MIN
      #define _I64_MIN  (-9223372036854775807ULL - 1)
      #define _I64_MAX    9223372036854775807ULL
    #endif
    add_builtin_constant("cellbits",64,sGLOBAL,0);
    add_builtin_constant("cellmax",_I64_MAX,sGLOBAL,0);
    add_builtin_constant("cellmin",_I64_MIN,sGLOBAL,0);
  #else
    #error Unsupported cell size
  #endif
  add_builtin_constant("charbits",sCHARBITS,sGLOBAL,0);
  add_builtin_constant("charmin",0,sGLOBAL,0);
  add_builtin_constant("charmax",~((ucell)-1 << sCHARBITS) - 1,sGLOBAL,0);
  add_builtin_constant("ucharmax",(1 << (sizeof(cell)-1)*8)-1,sGLOBAL,0);

  add_builtin_constant("__Pawn",VERSION_INT,sGLOBAL,0);
  add_builtin_constant("__PawnBuild",VERSION_BUILD,sGLOBAL,0);
  line_sym=add_builtin_constant("__line",0,sGLOBAL,0);
  add_builtin_constant("__compat",pc_compat,sGLOBAL,0);

  now=time(NULL);
  loctm=*localtime(&now);
  utctm=*gmtime(&now);
  loctime=now+(loctm.tm_sec-utctm.tm_sec)+(loctm.tm_min-utctm.tm_min)*60
          +(loctm.tm_hour-utctm.tm_hour)*60*60+(loctm.tm_mday-utctm.tm_mday)*60*60*24;
  add_builtin_constant("__timestamp",(cell)loctime,sGLOBAL,0);

  debug=0;
  if ((sc_debug & (sCHKBOUNDS | sSYMBOLIC))==(sCHKBOUNDS | sSYMBOLIC))
    debug=2;
  else if ((sc_debug & sCHKBOUNDS)==sCHKBOUNDS)
    debug=1;
  add_builtin_constant("debug",debug,sGLOBAL,0);
  add_builtin_constant("__optimization", pc_optimize,sGLOBAL,0);
  add_builtin_constant("__optimisation", pc_optimize,sGLOBAL,0);

  append_constval(&sc_automaton_tab,"",0,0);    /* anonymous automaton */
}

static void setstringconstants(void)
{
  char timebuf[arraysize("11:22:33")];
  char datebuf[arraysize("10 Jan 2017")];

  assert(sc_status!=statIDLE);
  add_builtin_string_constant("__file","",sGLOBAL);

  strftime(timebuf,arraysize(timebuf),"%H:%M:%S",localtime(&now));
  add_builtin_string_constant("__time",timebuf,sGLOBAL);
  strftime(datebuf,arraysize(datebuf),"%d %b %Y",localtime(&now));
  add_builtin_string_constant("__date",datebuf,sGLOBAL);
}

static int getclassspec(int initialtok,int *fpublic,int *fstatic,int *fstock,int *fconst)
{
  int tok,err;
  cell val;
  char *str;

  assert(fconst!=NULL);
  assert(fstock!=NULL);
  assert(fstatic!=NULL);
  assert(fpublic!=NULL);
  *fconst=FALSE;
  *fstock=FALSE;
  *fstatic=FALSE;
  *fpublic=FALSE;
  switch (initialtok) {
  case tCONST:
    *fconst=TRUE;
    break;
  case tSTOCK:
    *fstock=TRUE;
    break;
  case tSTATIC:
    *fstatic=TRUE;
    break;
  case tPUBLIC:
    *fpublic=TRUE;
    break;
  } /* switch */

  err=0;
  do {
    tok=lex(&val,&str);  /* read in (new) token */
    switch (tok) {
    case tCONST:
      if (*fconst)
        err=42;          /* invalid combination of class specifiers */
      *fconst=TRUE;
      break;
    case tSTOCK:
      if (*fstock)
        err=42;          /* invalid combination of class specifiers */
      *fstock=TRUE;
      break;
    case tSTATIC:
      if (*fstatic)
        err=42;          /* invalid combination of class specifiers */
      *fstatic=TRUE;
      break;
    case tPUBLIC:
      if (*fpublic)
        err=42;          /* invalid combination of class specifiers */
      *fpublic=TRUE;
      break;
    default:
      lexpush();
      tok=0;             /* force break out of loop */
    } /* switch */
  } while (tok && err==0);

  /* extra checks */
  if (*fstatic && *fpublic) {
    err=42;              /* invalid combination of class specifiers */
    *fstatic=*fpublic=FALSE;
  } /* if */

  if (err)
    error(err);
  return err==0;
}

/* set while parsing a function that was prefixed with the "iterfunc" keyword,
 * so newfunc() can tag its symbol with uITERFUNC (a lazy generator that
 * foreach drives with a call-loop; see doforeach) */
static int pc_iterfunc=FALSE;

/* set while parsing a function that was prefixed with the "async" keyword
 * (experiment 012 spike), so newfunc() tags its symbol uITERFUNC|uASYNC: it is
 * compiled on the coroutine-generator engine but its suspend point is "await"
 * and a scheduler (not "foreach") resumes it. */
static int pc_async=FALSE;

/* set while parsing a coroutine generator: an "iterfunc" that "foreach" drives
 * by resuming it where it last suspended (see the "yield" support below). It is
 * decided at the top of the function (from the uGENERATOR flag or the declared
 * argument count) and, in the addressing pass that first discovers a "yield",
 * while the body is being parsed. */
static int pc_generator=FALSE;
/* the value of "declared" right after a generator's prologue reserves its cells
 * -- the baseline at which the body begins, with every scalar local/param lifted
 * into the state block and so NOT counted here. A "yield" is only sound while
 * "declared" is still at this baseline: any live stack storage beyond it (e.g. a
 * nested "foreach"'s loop/hidden cells) is discarded by the suspend and rebuilt
 * as garbage on resume, so doyield() rejects a "yield" seen above it (error 099).
 * Set in generator_emit_prologue(), which runs on every pass whose addresses
 * matter, so the guard is two-pass stable. */
static cell pc_gen_baseline=0;

/* native "hook" support (experiment 004): registry of hooked callbacks for the
 * current parse pass, plus the routines that register a hook and synthesise the
 * dispatchers at end-of-parse. See the hookgroup comment in sc.h. */
static hookgroup *hook_registry=NULL;
/* per-callback default return value (YSI HOOK_RET analogue): a "hook default
 * Name = N;" declaration makes the dispatcher return N on fall-through instead
 * of the last chain value. Kept in a small separate list so it is independent
 * of hookgroup lifecycle / declaration order. */
typedef struct s_hookdefault {
  struct s_hookdefault *next;
  char name[sNAMEMAX+1];
  cell value;
} hookdefault;
static hookdefault *hook_defaults=NULL;
static void hook_register(const char *callback,symbol *hidden,int argcount,int tag,int prio,int hasstate,cell statevar,cell stateval);
static void hook_set_default(const char *callback,cell value);
static int hook_get_default(const char *callback,cell *value);
static void hook_set_prototype(symbol *disp,symbol *proto);
static void dohook(void);

/* native CALL-SITE "hook" support (experiment 010): registry of hooked call
 * targets ("hook function/native/stock Name") for the current parse pass. See
 * the callhookgroup comment in sc.h. Rebuilt each pass in callhook_reset(). */
static callhookgroup *callhook_registry=NULL;
/* set while declargs() parses a call-hook body's parameter list: it prepends a
 * hidden leading "idx" (chain-index) parameter, so the user's declared args land
 * at frame offsets 16.. and the body reads idx at frame offset 12 (param 0). */
static int callhook_inject_idx=0;
static callhookgroup *callhook_find(const char *name);
static void callhook_parse(int modifier,int prio);

/* Persistent set of target names that have been hooked at least once during
 * THIS compile -- ANY kind (function/stock/native), generalised from Task 3's
 * native-only set. Unlike callhook_registry (rebuilt every pass by
 * callhook_reset) this list survives resetglobals, and it does two jobs:
 *
 *   1. Gate a one-shot reparse: the first time a target is hooked we force one
 *      extra addressing pass, after which the name is already "seen" and no
 *      further reparse is requested (monotonic -> converges).
 *   2. Drive FORWARD-REFERENCE redirection: callhook_target_wrapper consults
 *      this set, so a call site that appears LEXICALLY BEFORE its "hook"
 *      declaration still redirects to the wrapper -- on the second (and later)
 *      passes the name is already "seen", even though the registry group has
 *      not been (re)built yet at the call site this pass.
 *
 * For a native this set is essential: the native symbol is freed between passes
 * (delete_symbols, mustdelete|=uNATIVE), so uCALLHOOK cannot ride it; being in
 * the set is what makes both the final addressing pass and the write pass emit
 * "call wrapper" (2 cells) at a pre-hook native call, instead of a 4-cell
 * "sysreq" in one pass and a 2-cell "call" in the other (silent address
 * corruption). For a pawn function/stock uCALLHOOK does persist across passes,
 * but the set still covers a genuine forward reference within a single pass. */
typedef struct s_chookpersist {
  struct s_chookpersist *next;
  char name[sNAMEMAX+1];
} chookpersist;
static chookpersist *chook_seen=NULL;
static int chook_is_seen(const char *name)
{
  chookpersist *p;
  for (p=chook_seen; p!=NULL; p=p->next)
    if (strcmp(p->name,name)==0)
      return TRUE;
  return FALSE;
}
static void chook_mark_seen(const char *name)
{
  chookpersist *p;
  if (chook_is_seen(name))
    return;
  p=(chookpersist*)malloc(sizeof(chookpersist));
  if (p==NULL)
    return;                     /* out of memory: skip the reparse gate, not fatal */
  strcpy(p->name,name);
  p->next=chook_seen;
  chook_seen=p;
}

/* coroutine generator ("yield") support; see the comment block above
 * generator_emit_helper() */
static symbol *generator_helper(void);
static void generator_emit_helper(void);
static void generator_emit_prologue(void);
static void doyield(void);

/*  parse       - process all input text
 *
 *  At this level, only static declarations and function definitions are legal.
 */
static void parse(void)
{
  int tok,fconst,fstock,fstatic,fpublic;
  cell val;
  char *str;

  while (freading){
    /* first try whether a declaration possibly is native or public */
    tok=lex(&val,&str);  /* read in (new) token */
    switch (tok) {
    case 0:
      /* ignore zero's */
      break;
    case t__EMIT:
      begcseg();
      emit_flags |= efGLOBAL;
      lex(&val,&str);
      emit_parse_line();
      needtoken(';');
      emit_flags &= ~efGLOBAL;
      break;
    case tNEW:
      if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst))
        declglb(NULL,0,fpublic,fstatic,fstock,fconst);
      break;
    case tSTATIC:
      if (matchtoken(tENUM)) {
        decl_enum(sGLOBAL,TRUE);
      } else {
        /* This can be a static function or a static global variable; we know
         * which of the two as soon as we have parsed up to the point where an
         * opening parenthesis of a function would be expected. To back out after
         * deciding it was a declaration of a static variable after all, we have
         * to store the symbol name and tag.
         */
        if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst)) {
          assert(!fpublic);
          declfuncvar(fpublic,fstatic,fstock,fconst);
        } /* if */
      } /* if */
      break;
    case tCONST:
      decl_const(sGLOBAL);
      break;
    case tENUM:
      decl_enum(sGLOBAL,matchtoken(tSTATIC));
      break;
    case tPUBLIC:
      /* This can be a public function or a public variable; see the comment
       * above (for static functions/variables) for details.
       */
      if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst)) {
        assert(!fstatic);
        declfuncvar(fpublic,fstatic,fstock,fconst);
      } /* if */
      break;
    case tSTOCK:
      /* This can be a stock function or a stock *global*) variable; see the
       * comment above (for static functions/variables) for details.
       */
      if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst)) {
        assert(fstock);
        declfuncvar(fpublic,fstatic,fstock,fconst);
      } /* if */
      break;
    case t__PRAGMA:
    case tLABEL:
    case tSYMBOL:
    case tOPERATOR:
      lexpush();
      if (!newfunc(NULL,-1,FALSE,FALSE,FALSE)) {
        error(10);              /* illegal function or declaration */
        lexclr(TRUE);           /* drop the rest of the line */
        litidx=0;               /* drop the literal queue too */
      } /* if */
      break;
    case tNATIVE:
      funcstub(TRUE);           /* create a dummy function */
      break;
    case tFORWARD:
      funcstub(FALSE);
      break;
    case tITERFUNC:
      /* "iterfunc" prefixes an ordinary function declaration and flags its
       * symbol as a lazy generator (uITERFUNC). It may be followed by a normal
       * class specifier ("iterfunc stock/static/public Name(...)"), which is
       * honored as usual; the function is otherwise parsed like any other, and
       * only foreach treats a call to it specially. */
      pc_iterfunc=TRUE;
      tok=lex(&val,&str);
      switch (tok) {
      case tSTOCK:
      case tSTATIC:
      case tPUBLIC:
        if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst))
          declfuncvar(fpublic,fstatic,fstock,fconst);
        break;
      default:
        lexpush();              /* no class specifier -- hand the name to newfunc */
        if (!newfunc(NULL,-1,FALSE,FALSE,FALSE)) {
          error(10);            /* illegal function or declaration */
          lexclr(TRUE);         /* drop the rest of the line */
          litidx=0;             /* drop the literal queue too */
        } /* if */
      } /* switch */
      pc_iterfunc=FALSE;
      break;
    case tASYNC:
      /* "async Name(args) body" (experiment 012 spike): parsed exactly like an
       * "iterfunc" (it is compiled on the coroutine-generator engine), but tagged
       * uASYNC so its suspend point is "await" and a scheduler resumes it. */
      pc_async=TRUE;
      tok=lex(&val,&str);
      switch (tok) {
      case tSTOCK:
      case tSTATIC:
      case tPUBLIC:
        if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst))
          declfuncvar(fpublic,fstatic,fstock,fconst);
        break;
      default:
        lexpush();              /* no class specifier -- hand the name to newfunc */
        if (!newfunc(NULL,-1,FALSE,FALSE,FALSE)) {
          error(10);            /* illegal function or declaration */
          lexclr(TRUE);
          litidx=0;
        } /* if */
      } /* switch */
      pc_async=FALSE;
      break;
    case tHOOK:
      /* "hook Name(args) body": one of possibly several handlers for callback
       * "Name" in this compilation unit. Each body compiles to a hidden
       * ordinary function ("_hook.Name.<seq>"); the dispatcher public "Name"
       * that chains them is synthesised at end-of-parse (hook_emit_dispatchers).
       * No class specifier is accepted (like the tests): "Name" is an ordinary
       * identifier. */
      dohook();
      break;
    case tTASK:
      /* "task Name[interval]() body": a repeating timer. The body compiles to a
       * hidden public ("@yt.Name"); the compiler auto-registers a SetTimer for
       * it at end-of-parse (timers_emit) by chaining a synthesised handler onto
       * OnGameModeInit/OnFilterScriptInit -- no runtime public-table scan. */
      dotask(FALSE);
      break;
    case tPTASK:
      /* "ptask Name[interval](playerid) body": like "task", but fires for every
       * connected player each interval (the synthesised handler loops the
       * <players> set and calls the body per id). */
      dotask(TRUE);
      break;
    case t__STATIC_ASSERT:
    case t__STATIC_CHECK: {
      int use_warning=(tok==t__STATIC_CHECK);
      do_static_check(use_warning);
      needtoken(';');
      break;
    } /* case */
    case '}':
      error(54);                /* unmatched closing brace */
      break;
    case '{':
      error(55);                /* start of function body without function header */
      break;
    default:
      if (freading) {
        error(10);              /* illegal function or declaration */
        lexclr(TRUE);           /* drop the rest of the line */
        litidx=0;               /* drop any literal arrays (strings) */
      } /* if */
    } /* switch */
  } /* while */
}

/*  hook_find    - locate the registry group for a hooked callback (or NULL) */
static hookgroup *hook_find(const char *callback)
{
  hookgroup *grp;
  for (grp=hook_registry; grp!=NULL; grp=grp->next)
    if (strcmp(grp->name,callback)==0)
      return grp;
  return NULL;
}

/*  hook_reset   - free the per-pass hook registry (called from resetglobals) */
static void hook_reset(void)
{
  hookgroup *grp,*next;
  hookdefault *hd,*hdnext;
  for (grp=hook_registry; grp!=NULL; grp=next) {
    next=grp->next;
    free(grp->slots);
    free(grp);
  } /* for */
  hook_registry=NULL;
  for (hd=hook_defaults; hd!=NULL; hd=hdnext) {
    hdnext=hd->next;
    free(hd);
  } /* for */
  hook_defaults=NULL;
}

/*  hook_set_default - record "hook default <callback> = <value>" (updates the
 *  entry if the callback already has one) */
static void hook_set_default(const char *callback,cell value)
{
  hookdefault *hd;
  for (hd=hook_defaults; hd!=NULL; hd=hd->next) {
    if (strcmp(hd->name,callback)==0) {
      hd->value=value;
      return;
    } /* if */
  } /* for */
  hd=(hookdefault*)malloc(sizeof(hookdefault));
  if (hd==NULL) {
    error(103);                 /* insufficient memory */
    return;
  } /* if */
  assert(strlen(callback)<=sNAMEMAX);
  strcpy(hd->name,callback);
  hd->value=value;
  hd->next=hook_defaults;
  hook_defaults=hd;
}

/*  hook_get_default - TRUE (and fills *value) if the callback has a declared
 *  default return */
static int hook_get_default(const char *callback,cell *value)
{
  hookdefault *hd;
  for (hd=hook_defaults; hd!=NULL; hd=hd->next) {
    if (strcmp(hd->name,callback)==0) {
      if (value!=NULL)
        *value=hd->value;
      return TRUE;
    } /* if */
  } /* for */
  return FALSE;
}

/*  hook_argshape_match - TRUE if two argument lists are structurally identical
 *  (same count, and per position the same class -- iVARIABLE/iREFERENCE/
 *  iREFARRAY/iVARARGS -- and the same array dimensions). Argument NAMES are
 *  ignored (they are forwarded by position), but the shape must match so that
 *  the dispatcher forwards each slot the way every hook expects it. */
static int hook_argshape_match(arginfo *a,arginfo *b)
{
  int i,level;
  for (i=0; a[i].ident!=0 && b[i].ident!=0; i++) {
    if (a[i].ident!=b[i].ident)
      return FALSE;
    if (a[i].numdim!=b[i].numdim)
      return FALSE;
    for (level=0; level<a[i].numdim; level++)
      if (a[i].dim[level]!=b[i].dim[level])
        return FALSE;
  } /* for */
  return a[i].ident==b[i].ident;        /* both lists must end together */
}

/*  hook_register - record a hidden hook function for callback "callback", in
 *  source order. The first hook of a callback fixes the shared signature; a
 *  later hook whose argument list differs in count or shape is an error (256). */
static void hook_register(const char *callback,symbol *hidden,int argcount,int tag,int prio,int hasstate,cell statevar,cell stateval)
{
  hookgroup *grp=hook_find(callback);
  if (grp==NULL) {
    grp=(hookgroup*)malloc(sizeof(hookgroup));
    if (grp==NULL) {
      error(103);               /* insufficient memory */
      return;
    } /* if */
    memset(grp,0,sizeof(hookgroup));
    assert(strlen(callback)<=sNAMEMAX);
    strcpy(grp->name,callback);
    grp->argcount=argcount;
    grp->tag=tag;
    grp->capacity=4;
    grp->slots=(hookslot*)malloc(grp->capacity*sizeof(hookslot));
    if (grp->slots==NULL) {
      free(grp);
      error(103);               /* insufficient memory */
      return;
    } /* if */
    grp->next=hook_registry;
    hook_registry=grp;
  } else {
    /* compare against the first hook's full argument shape, not just the count,
     * so a value/reference/array mismatch cannot slip through and misforward */
    if (grp->count==0
        || !hook_argshape_match(grp->slots[0].fn->dim.arglist,hidden->dim.arglist))
      error(256,callback);      /* hooks for one callback must share a signature */
    if (grp->count>=grp->capacity) {
      hookslot *grown;
      grp->capacity*=2;
      grown=(hookslot*)realloc(grp->slots,grp->capacity*sizeof(hookslot));
      if (grown==NULL) {
        error(103);             /* insufficient memory */
        return;
      } /* if */
      grp->slots=grown;
    } /* if */
  } /* if */
  grp->slots[grp->count].fn=hidden;
  grp->slots[grp->count].prio=prio;
  grp->slots[grp->count].hasstate=hasstate;
  grp->slots[grp->count].statevar=statevar;
  grp->slots[grp->count].stateval=stateval;
  grp->count++;
}

/*  dohook       - parse a "hook Name(args) body" declaration
 *
 *  The body is compiled to a hidden ordinary function "_hook.Name.<seq>" (seq
 *  in source order); the callback name is registered so the dispatcher public
 *  "Name" can be synthesised at end-of-parse (hook_emit_dispatchers). Because
 *  the hidden names differ, the normal duplicate-definition gate (error 021)
 *  never fires for repeated hooks of one callback.
 */
/* --- native "task" timers (y_timers periodic-task replacement) -------------
 * "task Name[interval]() body" is a repeating timer. The body is hoisted into a
 * hidden PUBLIC "@yt_Name" (SetTimer resolves it by name at run time); the task
 * is recorded here, and at end-of-parse timers_emit() synthesises one "@yt_init"
 * that SetTimer-registers them all and chains itself onto OnGameModeInit /
 * OnFilterScriptInit via the hook machinery -- no runtime public-table scan. */
typedef struct timertask {
  struct timertask *next;
  char pubname[sNAMEMAX+1];     /* body public: task "@yt_Name", ptask "@pt_Name" */
  cell interval;
  int perplayer;                /* ptask: dispatch to the body per connected player */
} timertask;
static timertask *timer_registry=NULL;

/* timers_reset - free the per-pass timer registry (called from resetglobals) */
static void timers_reset(void)
{
  timertask *t,*n;
  for (t=timer_registry; t!=NULL; t=n) {
    n=t->next;
    free(t);
  } /* for */
  timer_registry=NULL;
}

/*  dotask - parse "task Name[interval]() body" or, when perplayer, "ptask
 *  Name[interval](playerid) body". The body compiles to a hidden public
 *  ("@yt_Name" for task, "@pt_Name" for ptask); the task is recorded for
 *  timers_emit() to auto-register (ptask gets a synthesised per-player
 *  dispatcher "@ptd_Name"). */
static void dotask(int perplayer)
{
  char name[sNAMEMAX+1];
  char hidden[sNAMEMAX+1];
  cell val,interval;
  char *str;
  int argcount;
  symbol *hsym;
  timertask *t;

  if (!needtoken(tSYMBOL)) {
    lexclr(TRUE);
    return;
  } /* if */
  tokeninfo(&val,&str);
  assert(strlen(str)<=sNAMEMAX);
  strcpy(name,str);

  if (!needtoken('[')) {
    lexclr(TRUE);
    return;
  } /* if */
  interval=0;
  constexpr(&interval,NULL,NULL);   /* the repeat interval in ms */
  needtoken(']');

  /* reserve room for the LONGEST mangled name: "@yt_<name>" (task, 4-char
   * prefix) or "@ptd_<name>" (ptask registration dispatcher, 5-char prefix).
   * Under-reserving for ptask overflows the regname buffer in timers_emit and
   * registers the timer under a truncated name (silently never fires). */
  if (strlen(name)+(perplayer ? 5 : 4)>sNAMEMAX) {
    error(200,name,sNAMEMAX);       /* symbol too long once mangled */
    lexclr(TRUE);
    return;
  } /* if */
  sprintf(hidden,perplayer ? "@pt_%s" : "@yt_%s",name);

  /* pre-create the hidden public and mark it read so its body is not dropped
   * as dead code before @yt_init (emitted later) references it */
  hsym=fetchfunc(hidden,0);
  if (hsym!=NULL)
    hsym->usage|=uREAD|uPUBLIC|uFORWARD;   /* uFORWARD: it is its own prototype (no warning 235) */

  /* parse "(params) body" under the hidden name, as a public */
  if (!newfunc(hidden,0,TRUE,FALSE,FALSE)) {
    error(10);                      /* illegal function or declaration */
    lexclr(TRUE);
    litidx=0;
    return;
  } /* if */
  hsym=findglb(hidden,sGLOBAL);
  if (hsym==NULL || hsym->ident!=iFUNCTN)
    return;                         /* body was only a prototype or was rejected */
  hsym->usage|=uPUBLIC|uREAD;
  argcount=0;
  while (hsym->dim.arglist[argcount].ident!=0)
    argcount++;
  if (perplayer) {
    /* ptask takes exactly one ordinary (by-value) parameter: the player id */
    if (argcount!=1)
      error(25);                    /* function heading differs: ptask takes (playerid) */
    else if (hsym->dim.arglist[0].ident!=iVARIABLE)
      error(35,1);                  /* argument type mismatch: playerid must be a plain cell */
  } else {
    if (argcount!=0)
      error(25);                    /* function heading differs: task takes no args */
  } /* if */

  t=(timertask*)malloc(sizeof(timertask));
  if (t==NULL) {
    error(103);                     /* insufficient memory */
    return;
  } /* if */
  memset(t,0,sizeof(timertask));
  strcpy(t->pubname,hidden);
  t->interval=interval;
  t->perplayer=perplayer;
  t->next=timer_registry;
  timer_registry=t;
}

/*  timers_emit - at end-of-parse (both passes, before hook_emit_dispatchers),
 *  synthesise "@yt_init": for each task, SetTimer("@yt_Name", interval, true);
 *  then chain @yt_init onto OnGameModeInit / OnFilterScriptInit so it runs at
 *  startup. Modelled on generator_emit_helper() (function synthesis) + the
 *  per-function literal flush for the name strings. */
static void timers_emit(void)
{
  symbol *sym,*savedfunc,*rt,*disp,*ptsym;
  timertask *t;
  int i,len,anyperplayer;
  cell litstart,nameaddr,recaddr;
  char regname[sNAMEMAX+1];

  if (timer_registry==NULL)
    return;

  rt=findglb("SetTimer",sGLOBAL);
  if (rt==NULL || rt->ident!=iFUNCTN) {
    error(17,"SetTimer");         /* undefined symbol: include the SA-MP/open.mp SDK */
    return;
  } /* if */
  /* NB: do NOT mark SetTimer uREAD yet -- see the uREAD-after-ffcall note below. */
  anyperplayer=0;
  for (t=timer_registry; t!=NULL; t=t->next)
    if (t->perplayer)
      anyperplayer=1;
  disp=NULL;
  if (anyperplayer) {
    disp=findglb("__ptask_dispatch",sGLOBAL);
    if (disp==NULL || disp->ident!=iFUNCTN) {
      error(17,"__ptask_dispatch");  /* ptask needs #include <ptask> (or <pawn-x>) */
      return;
    } /* if */
  } /* if */

  savedfunc=curfunc;

  /* (1) one no-arg dispatcher "@ptd_Name" per ptask: it hands a {entry=@pt_Name,
   * link=0} Callback record (in the data segment) to the plugin-free
   * __ptask_dispatch helper, which loops the <players> set and calls the body
   * per connected id. SetTimer registers this dispatcher (not the body). */
  for (t=timer_registry; t!=NULL; t=t->next) {
    if (!t->perplayer)
      continue;
    ptsym=findglb(t->pubname,sGLOBAL);       /* "@pt_Name" body */
    if (ptsym==NULL || ptsym->ident!=iFUNCTN)
      continue;
    litstart=litidx;
    litadd(ptsym->addr);                     /* record[0] = entry address */
    litadd(0);                               /* record[1] = static link (a public has none) */
    recaddr=(litstart+glb_declared)*(cell)sizeof(cell);
    sprintf(regname,"@ptd_%s",t->pubname+4); /* base = pubname past "@pt_" */
    sym=fetchfunc(regname,0);
    if (sym==NULL)
      continue;
    sym->usage|=uREAD|uPUBLIC|uDEFINE|uPROTOTYPED; /* a DEFINED public: SetTimer
                                     * resolves it by name at run time. uDEFINE is
                                     * required -- the publics table only lists
                                     * uPUBLIC|uDEFINE symbols (sc6.c), so a
                                     * uFORWARD-only dispatcher is silently absent
                                     * and the ptask never fires. */
    sym->addr=code_idx;
    curfunc=sym;
    begcseg();
    startfunc(regname,TRUE);
    ldconst(recaddr,sPRI);                   /* &record -> the Callback value */
    pushreg(sPRI);
    pushval(1*(cell)sizeof(cell));           /* one argument */
    ffcall(disp,NULL,1);                     /* __ptask_dispatch(&record) */
    markusage(disp,uREAD);                   /* keep the helper live */
    ldconst(0,sPRI);
    ffret(TRUE);
    endfunc();
    sym->codeaddr=code_idx;
  } /* for */

  /* (2) "@yt_init": SetTimer each timer at its registration name
   * (task -> "@yt_Name", ptask -> "@ptd_Name"), then chain onto the init hooks. */
  sym=fetchfunc("@yt_init",0);
  if (sym==NULL) {
    curfunc=savedfunc;
    return;
  } /* if */
  sym->usage|=uREAD;
  sym->addr=code_idx;             /* address of the "proc" that follows */
  curfunc=sym;
  begcseg();
  startfunc("@yt_init",TRUE);     /* emit "proc" */

  for (t=timer_registry; t!=NULL; t=t->next) {
    if (t->perplayer)
      sprintf(regname,"@ptd_%s",t->pubname+4);
    else
      strcpy(regname,t->pubname);
    len=(int)strlen(regname);
    litstart=litidx;
    for (i=0; i<len; i++)
      litadd((cell)regname[i]);
    litadd(0);                    /* NUL-terminate the name string */
    nameaddr=(litstart+glb_declared)*(cell)sizeof(cell);
    /* SetTimer(name, interval, true): args pushed last-to-first, then the
     * argument count in bytes; ffcall emits the call + (native) stack cleanup */
    ldconst(1,sPRI);              /* repeating = true */
    pushreg(sPRI);
    ldconst(t->interval,sPRI);    /* interval (ms) */
    pushreg(sPRI);
    ldconst(nameaddr,sPRI);       /* address of the public-name string */
    pushreg(sPRI);
    pushval(3*(cell)sizeof(cell));/* argument count, in bytes */
    ffcall(rt,NULL,3);
    /* For a NATIVE SetTimer, mark it read AFTER the first ffcall: the first call
     * (uREAD still clear) assigns its sysreq id, the rest reuse it, and its
     * library gets listed even in a task-only script with no manual call site.
     * Guard on uNATIVE -- x.lib is only valid for natives (union), and a pawn
     * stock target (e.g. a test stub) takes ffcall's plain "call" branch and is
     * kept live by its own call sites, so it needs neither. */
    if ((rt->usage & uNATIVE)!=0) {
      markusage(rt,uREAD);
      if (rt->x.lib!=NULL)
        rt->x.lib->value+=1;
    } /* if */
  } /* for */

  ldconst(1,sPRI);                /* HOOK_CONTINUE: let other init hooks run */
  ffret(TRUE);
  endfunc();
  sym->codeaddr=code_idx;

  /* flush all synthesised literals (Callback records + name strings) at once */
  glb_declared+=litidx;
  begdseg();
  dumplits();
  litidx=0;

  curfunc=savedfunc;

  /* run @yt_init at startup by chaining it onto the init callbacks */
  hook_register("OnGameModeInit",sym,0,0,0,0,0,0);
  hook_register("OnFilterScriptInit",sym,0,0,0,0,0,0);
}

static void dohook(void)
{
  char callback[sNAMEMAX+1];
  char hidden[sNAMEMAX+32];     /* room for "_hook.<name>.<seq>" before the
                                 * length check below; seq may be many digits */
  cell val;
  char *str;
  int tok,seq,argcount,prio,hasstate;
  cell statevar,stateval;
  symbol *hsym;
  hookgroup *grp;
  int rettag;

  /* default-return declaration: "hook default <Callback> = <const>;" makes the
   * dispatcher return <const> on fall-through (all hooks CONTINUE) instead of the
   * last chain value — pawn-x's HOOK_RET analogue (e.g. OnPlayerCommandText = 0).
   * A hook may still HOOK_STOP/HOOK_STOP_1 to force 0/1. */
  if (matchtoken(tDEFAULT)) {
    int neg;
    cell dval;
    if (!needtoken(tSYMBOL)) {
      lexclr(TRUE);
      return;
    } /* if */
    tokeninfo(&val,&str);
    strcpy(callback,str);
    needtoken('=');
    neg=matchtoken('-');
    tok=lex(&val,&str);
    if (tok!=tNUMBER) {
      error(1,"-integer-",str);   /* expected a constant after "hook default X =" */
      lexclr(TRUE);
      return;
    } /* if */
    dval= neg ? -val : val;
    needtoken(';');
    hook_set_default(callback,dval);
    return;
  } /* if */

  /* optional priority: "hook:N Name(...)" or "hook:-N Name(...)". Higher N runs
   * earlier in the chain; equal priority keeps source order; default is 0. This
   * lets independent includes order their hooks without relying on include order
   * (pawn-x's answer to YSI's PRE_HOOK/CHAIN_ORDER, with no bytecode scan). */
  prio=0;
  if (matchtoken(':')) {
    int neg=matchtoken('-');
    tok=lex(&val,&str);
    if (tok!=tNUMBER) {
      error(1,"-integer-",str);   /* expected a priority number after "hook:" */
      lexclr(TRUE);
      return;
    } /* if */
    prio= neg ? -(int)val : (int)val;
  } /* if */

  /* optional state scope: "hook <state> Name(...)" or "hook <automaton:state> ..."
   * makes this hook fire only while the automaton is in <state> (the dispatcher
   * checks the automaton's state variable at runtime and skips the call
   * otherwise). pawn-x's answer to YSI's state-scoped hooks; the state is placed
   * before the name so it is parsed here, not by newfunc (which would otherwise
   * make the hidden function itself state-conditioned). */
  hasstate=0;
  statevar=0;
  stateval=0;
  if (matchtoken('<')) {
    constvalue *automaton,*state;
    /* issue #13 side-note: guard the NULL case. sc_getstateid can report success
     * yet leave automaton/state NULL on malformed input (e.g. an empty "<>"); in a
     * release build (NDEBUG) the assert is gone and automaton->value then segfaults.
     * Require both non-NULL, so a bad state list is a clean skip, never a crash. */
    automaton=NULL;
    state=NULL;
    if (sc_getstateid(&automaton,&state) && automaton!=NULL && state!=NULL) {
      statevar=automaton->value;   /* address of the automaton's state variable */
      stateval=state->value;       /* the required state's index */
      hasstate=1;
    } /* if */
    needtoken('>');
  } /* if */

  /* optional return tag: "hook Float:OnFoo(...)" / "hook bool:OnBar(...)" -- the
   * hooked callback may carry a return tag, exactly like an ordinary public.
   * pc_addtag(NULL) consumes a leading "Tag:" label if present (and only then:
   * the "native"/"stock"/"function" modifiers are not labels, so a call-site
   * hook is unaffected and its tag comes from the target). The tag is applied to
   * the synthesised body below and flows to the dispatcher via hook_register. */
  rettag=pc_addtag(NULL);

  tok=lex(&val,&str);
  /* call-site hook? "hook [<prio>] native|function|stock Name(args) body" hooks
   * an ordinary call target (experiment 010), distinct from the callback hook
   * below. "native"/"stock" are keywords; "function" is not, so it arrives as a
   * symbol -- treat it as the modifier only when a target name follows (otherwise
   * it is an ordinary callback that happens to be named "function"). */
  if (tok==tNATIVE || tok==tSTOCK) {
    if (hasstate)
      error(262);               /* state-scoped call hooks are not supported */
    callhook_parse((tok==tNATIVE) ? CHOOK_NATIVE : CHOOK_STOCK,prio);
    return;
  } /* if */
  /* Capture the symbol name NOW, before the lookahead below: matchtoken()'s
   * internal lex() overwrites the buffer "str" points at (_lexstr), so a
   * callback literally named "function" would otherwise be copied as an empty
   * name on the non-modifier fall-through. */
  if (tok==tSYMBOL) {
    assert(strlen(str)<=sNAMEMAX);
    strcpy(callback,str);
  } /* if */
  if (tok==tSYMBOL && strcmp(callback,"function")==0 && matchtoken(tSYMBOL)) {
    lexpush();                  /* put the target name back for callhook_parse() */
    if (hasstate)
      error(262);               /* state-scoped call hooks are not supported */
    callhook_parse(CHOOK_FUNCTION,prio);
    return;
  } /* if */
  if (tok!=tSYMBOL) {
    error(20,str);              /* invalid symbol name */
    lexclr(TRUE);
    return;
  } /* if */
  /* callback name already captured above */

  grp=hook_find(callback);
  seq= (grp!=NULL) ? grp->count : 0;
  sprintf(hidden,"_hook.%s.%d",callback,seq);
  if (strlen(hidden)>sNAMEMAX) {
    /* the readable hidden name "_hook.<callback>.<seq>" does not fit the
     * symbol-name limit (a very long callback name, e.g. a streamer callback,
     * or an astronomical hook count). Fall back to a HASHED hidden name that
     * always fits -- the same technique inline_hidden_name() uses -- so a long
     * callback name is hookable rather than a hard failure. The hash is taken
     * over the callback name; the readable "<seq>" suffix is kept so multiple
     * hooks on one long-named callback still mint distinct bodies. The
     * dispatcher references this body by symbol pointer (hook_register), never
     * by reconstructing the name, so a hashed name needs no change elsewhere,
     * and the hash is deterministic so both parse passes agree. */
    unsigned long h=5381UL;
    const char *q;
    for (q=callback; *q!='\0'; ++q)
      h=h*33UL+(unsigned char)*q;
    sprintf(hidden,"_hook.%08lx.%d",h&0xffffffffUL,seq);
  } /* if */

  /* pre-create the hidden symbol and mark it "read" so that, in the write
   * pass, its body is not skipped as dead code before the dispatcher (emitted
   * later) records the reference to it. The body returns control sentinels
   * (HOOK_CONTINUE/HOOK_STOP...), so it stays UNTAGGED even for a tagged
   * callback; the return tag lives on the DISPATCHER (via hook_register), which
   * is the public the host actually calls. */
  hsym=fetchfunc(hidden,0);
  if (hsym!=NULL)
    hsym->usage|=uREAD;

  /* parse the body exactly like an ordinary function, but under the hidden
   * name -- "hook" carries no class specifier, so the callback name is handed
   * straight to newfunc(). The body is untagged (it yields control sentinels);
   * "rettag" is applied to the dispatcher below. */
  if (!newfunc(hidden,0,FALSE,FALSE,FALSE)) {
    error(10);                  /* illegal function or declaration */
    lexclr(TRUE);
    litidx=0;
    return;
  } /* if */

  /* re-fetch (newfunc may have created it if the pre-fetch failed) and register */
  hsym=findglb(hidden,sGLOBAL);
  if (hsym==NULL || hsym->ident!=iFUNCTN)
    return;                     /* body was only a prototype or was rejected */
  argcount=0;
  while (hsym->dim.arglist[argcount].ident!=0)
    argcount++;
  /* the dispatcher (the public the host calls) carries the callback's return
   * tag "rettag"; the body is untagged. hook_get_default/state/prio unchanged. */
  hook_register(callback,hsym,argcount,rettag,prio,hasstate,statevar,stateval);
}

/*  doinline - parse an "inline [const] Name(params) { body }" nested-function
 *  declaration at statement position (experiment 015). The body is hoisted into a
 *  hidden top-level function "_inline.<parent>.<Name>" compiled right here; the
 *  enclosing function jumps over the emitted body at run time so it is reached only
 *  through its entry address. "using inline Name" later resolves the hidden name and
 *  yields that entry address as a Callback: value; a receiver calls it indirectly.
 *
 *  Milestone 0: no closure capture yet -- the body sees only its own parameters and
 *  globals (the enclosing locals are detached during compilation). Capture via a
 *  static link is the next milestone. */
/*  inline_hidden_name - the mangled top-level name of an "inline" named `iname`
 *  defined inside function `fname`. It hashes (fname, iname) rather than
 *  embedding the literal parent name, so the result always fits in sNAMEMAX even
 *  when the enclosing function has a long mangled name (e.g. a hook body). Both
 *  the definition (doinline) and the use ("using inline") build the identical
 *  name from the same pair, so they resolve to the same symbol. */
SC_FUNC void inline_hidden_name(char *dst,const char *fname,const char *iname)
{
  unsigned long h=5381UL;
  const char *p;
  for (p=fname; *p!='\0'; ++p)
    h=h*33UL+(unsigned char)*p;
  h=h*33UL+(unsigned long)'@';
  for (p=iname; *p!='\0'; ++p)
    h=h*33UL+(unsigned char)*p;
  sprintf(dst,"_in.%08lx",h&0xffffffffUL);
}

static void doinline(void)
{
  char name[sNAMEMAX+1];
  char hidden[sNAMEMAX+1];
  cell val;
  char *str;
  int tok,lbl_skip;
  symbol *hsym;
  symbol *save_curfunc,*save_loc;
  int save_declared,save_gen,save_async,save_iter,save_status,save_rettype,save_iconst;
  int is_const;

  if (curfunc==NULL) {
    error(10);                  /* an inline only makes sense inside a function */
    lexclr(TRUE);
    return;
  } /* if */
  is_const=matchtoken(tCONST);  /* "inline const" -- captured locals are read-only */
  tok=lex(&val,&str);
  if (tok!=tSYMBOL) {
    error(20,str);              /* invalid symbol name */
    lexclr(TRUE);
    return;
  } /* if */
  strcpy(name,str);
  inline_hidden_name(hidden,curfunc->name,name);

  /* Flush the enclosing function's PENDING string literals to the data segment
   * before compiling the inline. newfunc() resets the shared literal queue
   * (litidx) to 0, so any literals the parent accumulated so far (e.g. an earlier
   * "print" argument) would be overwritten by the inline's and collide at the same
   * data address. Dumping them now (the same sequence newfunc uses at a function's
   * end) fixes their addresses and leaves litidx==0 as newfunc expects. */
  if (litidx) {
    glb_declared+=litidx;
    begdseg();
    dumplits();
    litidx=0;
  } /* if */
  begcseg();                    /* back to the code segment for the jump below */

  /* the enclosing function jumps over the inline body at run time */
  lbl_skip=getlabel();
  jumplabel(lbl_skip);

  /* save + neutralise enclosing parse state: newfunc asserts an empty loctab and
   * clobbers curfunc/declared/pc_* and (for an unused symbol) sc_status */
  save_curfunc=curfunc;
  save_loc=loctab.next;
  save_declared=declared;
  save_gen=pc_generator;
  save_async=pc_async;
  save_iter=pc_iterfunc;
  save_status=sc_status;
  save_rettype=rettype;
  save_iconst=pc_inline_const;
  pc_inline_const=is_const;
  /* capture: keep the enclosing locals reachable for lookup (findloc consults
   * inline_outer_loc while pc_compiling_inline) and flag them uCAPTURED so sc4.c
   * addresses them through the inline's static link rather than its own frame. */
  { symbol *s;
    for (s=save_loc; s!=NULL; s=s->next)
      s->usage|=uCAPTURED;
  }
  inline_outer_loc=save_loc;
  pc_compiling_inline=1;
  loctab.next=NULL;
  declared=0;
  pc_generator=0;
  pc_async=0;
  pc_iterfunc=0;

  hsym=fetchfunc(hidden,0);
  if (hsym!=NULL)
    hsym->usage|=uREAD;         /* keep the body alive in the write pass */
  newfunc(hidden,0,FALSE,FALSE,FALSE);  /* parses "(params){body}" from here */

  /* restore the enclosing function's parse state */
  pc_compiling_inline=0;
  inline_outer_loc=NULL;
  pc_inlinelink=0;
  { symbol *s;
    for (s=save_loc; s!=NULL; s=s->next)
      s->usage&=~uCAPTURED;
  }
  loctab.next=save_loc;
  curfunc=save_curfunc;
  declared=save_declared;
  pc_generator=save_gen;
  pc_async=save_async;
  pc_iterfunc=save_iter;
  sc_status=save_status;
  rettype=save_rettype;
  pc_inline_const=save_iconst;

  begcseg();                    /* newfunc left us in the data segment (its literal
                                 * dump); the enclosing function resumes in code */
  setlabel(lbl_skip);
}

/*  hook_free_arglist - deep-free an argument list previously built by
 *  hook_clone_arglist (mirrors the arglist portion of free_symbol) */
static void hook_free_arglist(arginfo *arglist)
{
  arginfo *arg;
  if (arglist==NULL)
    return;
  for (arg=arglist; arg->ident!=0; arg++) {
    if (arg->ident==iREFARRAY && arg->hasdefault)
      free(arg->defvalue.array.data);
    else if (arg->ident==iVARIABLE
             && ((arg->hasdefault & uSIZEOF)!=0 || (arg->hasdefault & uTAGOF)!=0))
      free(arg->defvalue.size.symname);
    free(arg->tags);
  } /* for */
  free(arglist);
}

/*  hook_set_prototype - give the dispatcher "disp" the same argument signature
 *  as the first hook "proto", so that direct calls to the dispatcher type-check
 *  like an ordinary call. The list is deep-copied (each arg owns its tag list
 *  and any default value), so the dispatcher owns it independently. */
static void hook_set_prototype(symbol *disp,symbol *proto)
{
  int n,i;
  arginfo *src,*dst;

  n=0;
  while (proto->dim.arglist[n].ident!=0)
    n++;
  dst=(arginfo*)malloc((n+1)*sizeof(arginfo));
  if (dst==NULL) {
    error(103);                 /* insufficient memory */
    return;
  } /* if */
  for (i=0; i<n; i++) {
    src=&proto->dim.arglist[i];
    dst[i]=*src;                /* shallow copy of the fixed fields */
    /* deep-copy the tag list (free_symbol frees it and asserts it is non-NULL) */
    dst[i].tags=(int*)malloc((src->numtags>0 ? src->numtags : 1)*sizeof(int));
    if (dst[i].tags==NULL) {
      while (--i>=0)
        free(dst[i].tags);
      free(dst);
      error(103);
      return;
    } /* if */
    if (src->numtags>0)
      memcpy(dst[i].tags,src->tags,src->numtags*sizeof(int));
    /* deep-copy default array data / sizeof symbol name, if any */
    if (src->ident==iREFARRAY && src->hasdefault && src->defvalue.array.data!=NULL) {
      int sz=src->defvalue.array.size;
      dst[i].defvalue.array.data=(cell*)malloc(sz*sizeof(cell));
      if (dst[i].defvalue.array.data!=NULL)
        memcpy(dst[i].defvalue.array.data,src->defvalue.array.data,sz*sizeof(cell));
    } else if (src->ident==iVARIABLE
               && ((src->hasdefault & uSIZEOF)!=0 || (src->hasdefault & uTAGOF)!=0)
               && src->defvalue.size.symname!=NULL) {
      dst[i].defvalue.size.symname=strdup(src->defvalue.size.symname);
    } /* if */
  } /* for */
  memset(&dst[n],0,sizeof(arginfo)); /* terminator */
  hook_free_arglist(disp->dim.arglist);
  disp->dim.arglist=dst;
}

/*  hook_emit_dispatchers - synthesise the "public Name(args)" dispatcher for
 *  every hooked callback, at end-of-parse.
 *
 *  This runs at the SAME point (right after parse()) in both the addressing
 *  pass (statFIRST) and the code-emission pass (statWRITE), so each dispatcher
 *  is emitted after all user code and gets a consistent address across passes,
 *  exactly like every ordinary function. The generated code is plain:
 *
 *    proc
 *    ; per hook, in source order:
 *    push.s <argN-1> ... push.s <arg0>   ; forward the dispatcher's arguments
 *    push.c <argbytes>
 *    call .<hidden hook>                  ; result in PRI
 *    move.alt                             ; ALT = result (preserved by eq.c.alt)
 *    eq.c.alt -1 / jnz ret0               ; HOOK_STOP    -> return 0
 *    eq.c.alt -2 / jnz ret1               ; HOOK_STOP_1  -> return 1
 *    load.s.pri chain / or (or and)       ; combine result into the running value
 *    ; after the last hook:
 *    retn                                 ; return the combined chain value
 *    ret0: zero.pri  / retn
 *    ret1: const.pri 1 / retn
 *
 *  No new opcodes, no bytecode scanning: it is an ordinary public function.
 */
static void hook_emit_dispatchers(void)
{
  hookgroup *grp;
  symbol *disp,*savedfunc;
  int i,a,lbl_ret0,lbl_ret1;
  cell argbytes,chainaddr;
  cell origaddr;                /* addr of a user-defined `public` body for the callback (issue #6) */
  int has_orig;
  cell defval;                  /* chain seed + combine mode from `hook default` (issue #5/#12) */
  int use_and;                  /* default 1 -> AND the returns; default 0 (implicit) -> OR */

  if (hook_registry==NULL)
    return;

  savedfunc=curfunc;
  for (grp=hook_registry; grp!=NULL; grp=grp->next) {
    disp=fetchfunc(grp->name,grp->tag);
    if (disp==NULL)
      continue;
    /* issue #6: if the user ALSO wrote a `public <callback>` body, chain it as
     * the LAST link (after every hook). At this point (pass 1, before we set
     * uDEFINE below) uDEFINE means "user defined a real body"; mark uHOOKORIG so
     * pass 2 still knows (the dispatcher itself carries uDEFINE by then). Capture
     * the body's entry address BEFORE the dispatcher overwrites disp->addr; parse
     * re-runs each pass so disp->addr is the user body's address here in both. */
    origaddr=disp->addr;
    if ((disp->usage & uDEFINE)!=0)
      disp->usage|=uHOOKORIG;
    has_orig=(disp->usage & uHOOKORIG)!=0;
    /* order the chain by priority (higher first), stable so equal priorities
     * keep source order. Done identically in both passes -> the dispatcher emits
     * its calls in the same order each pass, so addresses stay consistent. */
    for (i=1; i<grp->count; i++) {
      hookslot hs=grp->slots[i];
      int j=i-1;
      while (j>=0 && grp->slots[j].prio<hs.prio) {   /* strict < keeps equal-priority order */
        grp->slots[j+1]=grp->slots[j];
        j--;
      } /* while */
      grp->slots[j+1]=hs;
    } /* for */
    /* the dispatcher is the public implementation of the callback */
    disp->usage|=uPUBLIC|uDEFINE|uPROTOTYPED;
    if (grp->tag!=0)
      disp->usage|=uRETVALUE;
    disp->tag=grp->tag;
    if (grp->count>0)
      hook_set_prototype(disp,grp->slots[0].fn); /* type-check direct calls */
    disp->addr=code_idx;        /* address of the "proc" that follows */
    curfunc=disp;

    argbytes=(cell)grp->argcount*sizeof(cell);
    begcseg();
    startfunc(disp->name,TRUE); /* emit "proc" */

    /* one hidden local holds the running chain value; a state-scoped hook that
     * is skipped leaves it untouched, and the state test is free to clobber PRI. */
    chainaddr=-(cell)sizeof(cell);
    modstk(-(int)sizeof(cell));
    if (disp->x.stacksize<grp->argcount+4)
      disp->x.stacksize=grp->argcount+4;
    /* return-value combining (Y-Less issue #5/#12): the chain seed and combine
     * operator come from `hook default`. default 0 (implicit) ORs the returns,
     * so any hook that claims the callback (returns 1) wins; default 1 ANDs them
     * (YSI's confirm semantics). This replaces the old last-value model, where a
     * later "not handled" hook could clobber an earlier claim (the /help bug). */
    if (!hook_get_default(grp->name,&defval))
      defval=0;                 /* implicit default 0 -> OR, seed 0 */
    use_and=(defval==1);        /* default 1 -> AND, seed 1 */
    ldconst(defval,sPRI);
    stgwrite("\tstor.s.pri ");
    outval(chainaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);

    lbl_ret0=getlabel();
    lbl_ret1=getlabel();

    for (i=0; i<grp->count; i++) {
      symbol *hook=grp->slots[i].fn;
      int lbl_skip=-1;
      /* state gate: run this hook only while the automaton is in its state */
      if (grp->slots[i].hasstate) {
        lbl_skip=getlabel();
        loadreg(grp->slots[i].statevar,sALT);   /* ALT = current state */
        ldconst(grp->slots[i].stateval,sPRI);   /* PRI = required state */
        ob_eq();                                 /* PRI = (current == required) */
        jmp_eq0(lbl_skip);                       /* not in state -> skip the call */
      } /* if */
      /* forward the dispatcher's own arguments (reverse order) */
      for (a=grp->argcount-1; a>=0; a--) {
        stgwrite("\tpush.s ");
        outval((a+3)*sizeof(cell),TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* for */
      pushval(argbytes);        /* argument-count marker (in bytes) */
      markusage(hook,uREAD);    /* the dispatcher refers to the hidden hook */
      ffcall(hook,NULL,grp->argcount);  /* call; result in PRI */
      /* preserve the result in ALT (eq.c.alt does not modify ALT) */
      stgwrite("\tmove.alt\n");
      code_idx+=opcodes(1);
      /* HOOK_STOP (-1): return 0 */
      stgwrite("\teq.c.alt ");
      outval(-1,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      jmp_ne0(lbl_ret0);
      /* HOOK_STOP_1 (-2): return 1 */
      stgwrite("\teq.c.alt ");
      outval(-2,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      jmp_ne0(lbl_ret1);
      /* CONTINUE / CONTINUE_0: combine this hook's result (in ALT) into the
       * running chain value (OR, or AND when default 1), and stash it so a later
       * skipped state-hook keeps it. */
      stgwrite("\tload.s.pri ");
      outval(chainaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);   /* PRI = running chain value */
      if (use_and)
        ob_and();                       /* PRI = running & result(ALT) */
      else
        ob_or();                        /* PRI = running | result(ALT) */
      stgwrite("\tstor.s.pri ");
      outval(chainaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      if (lbl_skip>=0)
        setlabel(lbl_skip);     /* skipped hooks land here, chain value intact */
    } /* for */

    /* issue #6: the user's own `public` body is the chain's tail -- call it after
     * every hook, with the dispatcher's arguments; its return combines into the
     * running chain value like any other link (a plain public returns normal
     * values, not HOOK_STOP codes, so no STOP checks). Reached only when no hook
     * forced an early STOP. */
    if (has_orig) {
      for (a=grp->argcount-1; a>=0; a--) {
        stgwrite("\tpush.s ");
        outval((a+3)*sizeof(cell),TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* for */
      pushval(argbytes);
      ldconst(origaddr,sPRI);          /* PRI = user body entry address */
      stgwrite("\tcall.pri\n");         /* call the original (its retn cleans the args) */
      code_idx+=opcodes(1);             /* PRI = original's return */
      stgwrite("\tload.s.alt ");        /* ALT = running chain value */
      outval(chainaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      if (use_and)
        ob_and();                       /* PRI = original & running */
      else
        ob_or();                        /* PRI = original | running */
      stgwrite("\tstor.s.pri ");        /* chain value = combined */
      outval(chainaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */

    /* fell through the whole chain: return the combined running chain value (the
     * `hook default` was applied as the seed above, not as a forced override). */
    stgwrite("\tload.s.pri ");
    outval(chainaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    modstk((int)sizeof(cell));  /* release the hidden chain-value local before retn */
    ffret(TRUE);
    /* HOOK_STOP target: return 0 */
    setlabel(lbl_ret0);
    ldconst(0,sPRI);
    modstk((int)sizeof(cell));
    ffret(TRUE);
    /* HOOK_STOP_1 target: return 1 */
    setlabel(lbl_ret1);
    ldconst(1,sPRI);
    modstk((int)sizeof(cell));
    ffret(TRUE);

    endfunc();
    disp->codeaddr=code_idx;
  } /* for */
  curfunc=savedfunc;
}

/*  Native CALL-SITE hooks ("hook function/native/stock", experiment 010) -------
 *
 *  See the callhookgroup comment in sc.h for the runtime shape. These routines
 *  (a) parse a "hook <mod> Name(args) body" declaration, compiling the body as a
 *  hidden "@chook.Name.<seq>" function with a hidden leading chain-index param,
 *  and (b) synthesise, at end-of-parse in BOTH passes (stable addresses), the
 *  wrapper "@chook.Name.wrap" and chain dispatcher "@chook.Name.chain", and mark
 *  the target uCALLHOOK so its call sites redirect to the wrapper. */

/*  callhook_find - locate the registry group for a hooked target (or NULL) */
static callhookgroup *callhook_find(const char *name)
{
  callhookgroup *grp;
  for (grp=callhook_registry; grp!=NULL; grp=grp->next)
    if (strcmp(grp->name,name)==0)
      return grp;
  return NULL;
}

/*  callhook_reset - free the per-pass call-site hook registry (from resetglobals) */
static void callhook_reset(void)
{
  callhookgroup *grp,*next;
  for (grp=callhook_registry; grp!=NULL; grp=next) {
    next=grp->next;
    free(grp->slots);
    free(grp);
  } /* for */
  callhook_registry=NULL;
}

/*  callhook_target_wrapper - the redirect consulted at the call choke-point: if
 *  "sym" is a hooked target and we are not emitting the dispatcher's tail-call to
 *  the original, return the wrapper its call sites must be routed to. Resolves the
 *  group by name (not by decl order), so a call site anywhere in the unit is
 *  redirected as long as uCALLHOOK is set (which persists from the prior pass for
 *  a pawn function/stock).
 *
 *  FORWARD REFERENCES: a call site may appear LEXICALLY BEFORE the "hook"
 *  declaration, so at this point (a) uCALLHOOK may not be set yet this pass (it
 *  is set at end-of-parse in callhook_emit, or -- for a native -- at registration
 *  which is also later), and (b) the registry group has not been built yet this
 *  pass. We therefore also redirect when the target is in the persistent
 *  "seen" set (known-hooked from an earlier pass); and when no group exists yet,
 *  we route to the deterministically-named wrapper "@chook.<name>.wrap", which
 *  callhook_parse fetches by the SAME name later this pass and gives an address.
 *  The wrapper reference is a normal forward reference (backpatched), and a pawn
 *  "call" is 2 cells whether it targets the original or the wrapper, so this is
 *  size-stable; for a native the seen-set guarantees "call wrapper" (2 cells) in
 *  BOTH the final addressing pass and the write pass. */
SC_FUNC symbol *callhook_target_wrapper(const symbol *sym)
{
  callhookgroup *grp;
  char wrapname[sNAMEMAX+32];
  symbol *wrap;
  if (sym==NULL || pc_emit_orig)
    return NULL;
  if ((sym->usage & uCALLHOOK)==0 && !chook_is_seen(sym->name))
    return NULL;
  grp=callhook_find(sym->name);
  if (grp!=NULL)
    return grp->wrapper;
  /* forward reference: the "hook" declaration is later in this pass, so the group
   * is not built yet. Fetch (creating if needed) the wrapper by its fixed name;
   * callhook_parse will return this same symbol and emit its body/address. */
  sprintf(wrapname,"@chook.%s.wrap",sym->name);
  if (strlen(wrapname)>sNAMEMAX)
    return NULL;
  wrap=fetchfunc(wrapname,sym->tag);
  if (wrap!=NULL)
    wrap->usage|=uDEFINE|uPROTOTYPED|uREAD|uRETVALUE;
  return wrap;
}

/*  callhook_parse - parse "hook [<prio>] function|native|stock Name(args) body".
 *  The caller has consumed "hook", the priority and the modifier; the target name
 *  is the next token. */
static void callhook_parse(int modifier,int prio)
{
  char target[sNAMEMAX+1];
  char hidden[sNAMEMAX+32];
  char wrapname[sNAMEMAX+32];
  char chainname[sNAMEMAX+32];
  cell val;
  char *str;
  int seq,argcount;
  symbol *tsym,*body,*wrapper,*chain,*save_disp;
  callhookgroup *grp,*save_grp;

  /* Size-stability of native call sites, and the forward-reference machinery.
   *
   * Unlike a pawn function/stock -- whose call site is size-neutral under
   * redirect (2-cell "call orig" <-> 2-cell "call wrapper") -- a native call
   * site is NOT: an un-redirected native emits a 4-cell "sysreq.c"+"stack"
   * (ffcall, sc4.c), a redirected one a 2-cell "call wrapper". If the flag that
   * drives the redirect were only set at end-of-parse (callhook_emit), the
   * addressing pass would emit "sysreq" (flag unset) and the write pass "call
   * wrapper" (flag set): a 4->2 shift desynchronising every following address.
   * The yield/uGENERATOR trick of persisting the flag on the symbol across
   * passes does NOT work for a native: delete_symbols() always frees native
   * symbols between passes (sc2.c: mustdelete |= uNATIVE), so the flag cannot
   * ride the symbol. Two mechanisms keep it stable, set up below after the
   * target is resolved:
   *   - the native is marked uCALLHOOK at REGISTRATION time (this parse point),
   *     re-executed on every pass ahead of any hook-before-call call site; and
   *   - the persistent "seen" set (chook_mark_seen) records the target so that
   *     callhook_target_wrapper redirects a call site placed BEFORE the hook
   *     declaration too (forward reference), on the final addressing pass and
   *     the write pass alike -- both emit "call wrapper".
   * The first time a target is hooked we force one extra addressing pass; the
   * persistent set makes that one-shot so the initial-scan loop converges. */

  if (!needtoken(tSYMBOL)) {
    lexclr(TRUE);
    return;
  } /* if */
  tokeninfo(&val,&str);
  assert(strlen(str)<=sNAMEMAX);
  strcpy(target,str);

  /* the target must be visible: we adopt its signature for continue()'s type
   * check and to know how many args to forward. A call to the target that
   * appears BEFORE this hook (a forward reference) is redirected via the
   * persistent seen-set + a reparse (see below). */
  tsym=findglb(target,sGLOBAL);
  if (tsym==NULL || tsym->ident!=iFUNCTN) {
    error(259,target);          /* unknown hook target: no such native or function */
    lexclr(TRUE);
    return;
  } /* if */

  /* the modifier must match the target's kind, or we would miscompile the
   * original-endpoint (a native needs a SYSREQ, a pawn function a "call").
   * A "stock" is an ordinary pawn function that carries uSTOCK, so its endpoint
   * is a plain "call" -- identical to "hook function"; both refuse a native. */
  if (modifier==CHOOK_NATIVE && (tsym->usage & uNATIVE)==0) {
    error(258,target);          /* hook modifier does not match target kind */
    lexclr(TRUE);
    return;
  } /* if */
  if (modifier==CHOOK_FUNCTION && (tsym->usage & uNATIVE)!=0) {
    error(258,target);          /* hook modifier does not match target kind */
    lexclr(TRUE);
    return;
  } /* if */
  if (modifier==CHOOK_STOCK && (tsym->usage & uNATIVE)!=0) {
    error(258,target);          /* hook modifier does not match target kind */
    lexclr(TRUE);
    return;
  } /* if */

  /* variadic ("...") targets are ACCEPTED: their variadic shape is recorded on
   * the group below (grp->isvariadic / grp->fixedargs), validated against the
   * hook body, and emitted by callhook_emit like any fixed-arity group. Error
   * 260 was retired in place -- its warnmsg slot is blanked rather than removed
   * so the N-200 indexing of 261/262/263 stays intact. */

  grp=callhook_find(target);
  seq= (grp!=NULL) ? grp->count : 0;
  sprintf(hidden,"@chook.%s.%d",target,seq);
  sprintf(wrapname,"@chook.%s.wrap",target);
  sprintf(chainname,"@chook.%s.chain",target);
  if (strlen(hidden)>sNAMEMAX || strlen(wrapname)>sNAMEMAX || strlen(chainname)>sNAMEMAX) {
    error(200,target,sNAMEMAX); /* symbol too long */
    lexclr(TRUE);
    return;
  } /* if */

  if (grp==NULL) {
    grp=(callhookgroup*)malloc(sizeof(callhookgroup));
    if (grp==NULL) {
      error(103);               /* insufficient memory */
      return;
    } /* if */
    memset(grp,0,sizeof(callhookgroup));
    strcpy(grp->name,target);
    grp->modifier=modifier;
    grp->capacity=4;
    grp->slots=(callhookslot*)malloc(grp->capacity*sizeof(callhookslot));
    if (grp->slots==NULL) {
      free(grp);
      error(103);               /* insufficient memory */
      return;
    } /* if */
    grp->next=callhook_registry;
    callhook_registry=grp;
  } /* if */

  /* adopt the target's fixed-arity signature */
  argcount=0;
  if (tsym->dim.arglist!=NULL)
    while (tsym->dim.arglist[argcount].ident!=0)
      argcount++;
  grp->argcount=argcount;
  /* variadic shape: a trailing iVARARGS means the target ends in "...". The
   * fixed-arg count excludes that vararg slot (== argcount when not variadic). */
  grp->isvariadic= (argcount>0 && tsym->dim.arglist[argcount-1].ident==iVARARGS) ? 1 : 0;
  grp->fixedargs= grp->isvariadic ? argcount-1 : argcount;
  grp->tag=tsym->tag;
  grp->orig=tsym;

  /* fetch the wrapper and chain dispatcher now (their code is emitted at
   * end-of-parse). The chain adopts the target's arg signature so that
   * continue(...) type-checks; both are prototyped so calls to them resolve. */
  wrapper=fetchfunc(wrapname,tsym->tag);
  chain=fetchfunc(chainname,tsym->tag);
  if (wrapper==NULL || chain==NULL) {
    error(103);                 /* insufficient memory */
    return;
  } /* if */
  wrapper->usage|=uDEFINE|uPROTOTYPED|uREAD|uRETVALUE;
  chain->usage|=uDEFINE|uPROTOTYPED|uREAD|uRETVALUE;
  wrapper->tag=tsym->tag;
  chain->tag=tsym->tag;
  hook_set_prototype(chain,tsym); /* chain's declared args = target args (for continue) */
  grp->wrapper=wrapper;
  grp->dispatcher=chain;

  /* Register the target in the persistent seen-set and, the FIRST time it is
   * hooked, force one extra addressing pass. The seen-set is monotonic and this
   * is one-shot per target, so the initial-scan loop converges. This is what
   * makes a call site placed BEFORE the hook declaration (a forward reference)
   * redirect on the following passes: callhook_target_wrapper consults the same
   * set. It covers function/stock/native alike (Task 3 generalised).
   *
   * For a NATIVE we additionally mark uCALLHOOK NOW, at registration time (see
   * the note at the top of this function): the native symbol is freed between
   * passes, so uCALLHOOK cannot ride it; re-establishing it here each pass keeps
   * a hook-before-call native call site emitting the 2-cell "call wrapper" in
   * both the final addressing pass and the write pass. (A pawn function/stock
   * keeps uCALLHOOK across passes -- reduce_referrers only clears uREAD -- so it
   * needs no registration-time flag; callhook_emit sets it at end-of-parse.) */
  if (!chook_is_seen(target)) {
    chook_mark_seen(target);
    sc_reparse=TRUE;
  } /* if */
  if (modifier==CHOOK_NATIVE)
    tsym->usage|=uCALLHOOK;

  /* pre-mark the body "read" so its code is not skipped in the write pass before
   * the dispatcher (emitted later) records the reference to it (mirrors dohook) */
  body=fetchfunc(hidden,tsym->tag);
  if (body!=NULL)
    body->usage|=uREAD;

  /* compile the body as a hidden function with a hidden leading "idx" parameter
   * (callhook_inject_idx); inside it, continue(...) lowers to a call to the chain
   * dispatcher (pc_callhook_dispatcher). */
  callhook_inject_idx=1;
  save_disp=pc_callhook_dispatcher;
  pc_callhook_dispatcher=chain;
  save_grp=pc_callhook_group;
  pc_callhook_group=grp;
  if (!newfunc(hidden,tsym->tag,FALSE,FALSE,FALSE)) {
    error(10);                  /* illegal function or declaration */
    lexclr(TRUE);
    litidx=0;
  } /* if */
  pc_callhook_dispatcher=save_disp;
  pc_callhook_group=save_grp;
  callhook_inject_idx=0;

  body=findglb(hidden,sGLOBAL);
  if (body==NULL || body->ident!=iFUNCTN)
    return;                     /* body was only a prototype or was rejected */

  /* body-vs-target shape + fixed arity: the body carries a hidden leading "idx"
   * parameter (callhook_inject_idx), so its user-visible parameters are the
   * arglist minus that idx (and minus its own trailing "..." if variadic).
   *   1. Variadic SHAPE must match: a variadic target needs a body that ends in
   *      "...", a fixed target needs a body with no "..." -- otherwise the
   *      wrapper/chain would forward a mismatched frame.
   *   2. The FIXED portion's arity must match the target's fixedargs. */
  {
    int bodyargs=0;
    int bodyvariadic;
    int bodyfixed;
    if (body->dim.arglist!=NULL)
      while (body->dim.arglist[bodyargs].ident!=0)
        bodyargs++;
    bodyvariadic= (bodyargs>0 && body->dim.arglist[bodyargs-1].ident==iVARARGS) ? 1 : 0;
    if (bodyvariadic!=grp->isvariadic) {
      error(263,target);        /* variadic shape (body vs target) mismatch */
      lexclr(TRUE);
      return;
    } /* if */
    bodyfixed= bodyargs-1;      /* drop the injected idx */
    if (bodyvariadic)
      bodyfixed--;             /* drop the body's own trailing "..." slot */
    if (bodyfixed!=grp->fixedargs) {
      error(261,target);        /* hook body argument count does not match target */
      return;
    } /* if */
  }

  if (grp->count>=grp->capacity) {
    callhookslot *grown;
    grp->capacity*=2;
    grown=(callhookslot*)realloc(grp->slots,grp->capacity*sizeof(callhookslot));
    if (grown==NULL) {
      error(103);               /* insufficient memory */
      return;
    } /* if */
    grp->slots=grown;
  } /* if */
  grp->slots[grp->count].fn=body;
  grp->slots[grp->count].prio=prio;
  grp->count++;
}

/*  callhook_emit - synthesise the wrapper + chain dispatcher for every hooked
 *  call target, at end-of-parse. Runs at the same point in both passes (right
 *  after hook_emit_dispatchers), so the synthesised addresses are consistent.
 *  All ordinary push/call/retn; no new opcodes.
 *
 *  Frame layout (arg N at frame offset (N+3)*cell):
 *    wrapper (args)        arg a at (a+3)*cell
 *    chain   (idx,args)    idx at 3*cell, target arg a at (a+4)*cell
 *    body    (idx,args)    same as chain
 *
 *  Variadic targets (grp->isvariadic): only the "fixedargs" fixed cells are
 *  pushed statically; the trailing "..." is copied at run time with the same
 *  fwdpushloop/fwdbytecount/fwdpopnative helpers the ordinary "___" forwarding
 *  uses (sc3.c). The tail sits at the HIGHEST callee offsets, so it is pushed
 *  FIRST (deepest); idx/fixed follow. Variadic frame layout:
 *    wrapper  fixed a at (a+3)*cell, varargs at (fixedargs+3)*cell
 *    chain    idx at 3*cell, fixed a at (a+4)*cell, varargs at (fixedargs+4)*cell
 *    body     same as chain
 *  NOTE: grp->argcount is the FULL arglist count INCLUDING the "..." slot;
 *  grp->fixedargs is the fixed count (== argcount when non-variadic,
 *  == argcount-1 when variadic). Variadic pushes use fixedargs, never argcount.
 */
static void callhook_emit(void)
{
  callhookgroup *grp;
  symbol *savedfunc;
  int i,a,k;
  cell argbytes_body,argbytes_orig;

  if (callhook_registry==NULL)
    return;

  savedfunc=curfunc;
  for (grp=callhook_registry; grp!=NULL; grp=grp->next) {
    symbol *wrapper=grp->wrapper;
    symbol *chain=grp->dispatcher;
    symbol *orig=grp->orig;
    int argcount=grp->argcount;
    int isva=grp->isvariadic;
    int fixedargs=grp->fixedargs;   /* fixed cells to push (== argcount when non-variadic) */
    int *lbls;

    if (wrapper==NULL || chain==NULL || orig==NULL)
      continue;

    /* order the chain by priority (higher first), stable -> equal priorities keep
     * source order. Done identically in both passes for address stability. */
    for (i=1; i<grp->count; i++) {
      callhookslot hs=grp->slots[i];
      int j=i-1;
      while (j>=0 && grp->slots[j].prio<hs.prio) {
        grp->slots[j+1]=grp->slots[j];
        j--;
      } /* while */
      grp->slots[j+1]=hs;
    } /* for */

    /* mark the target as hooked so its call sites redirect to the wrapper; the
     * flag persists into the next pass (reduce_referrers only clears uREAD). */
    orig->usage|=uCALLHOOK;
    if ((orig->usage & uNATIVE)==0)
      markusage(orig,uREAD);    /* pawn function: dispatcher's tail-call keeps it alive.
                                 * A NATIVE is marked uREAD only AFTER its ffcall below,
                                 * so ffcall's lazy SYSREQ-id assignment (which requires
                                 * uREAD clear) still fires -- see the default case. */
    markusage(wrapper,uREAD);
    markusage(chain,uREAD);

    argbytes_body=(cell)(argcount+1)*sizeof(cell);  /* idx + target args */
    argbytes_orig=(cell)argcount*sizeof(cell);       /* target args only */

    /* ---- wrapper @chook.Name.wrap(args): return chain(0, args) ---- */
    wrapper->addr=code_idx;
    curfunc=wrapper;
    if (wrapper->x.stacksize<argcount+4+(isva?sMAXARGS:0))
      wrapper->x.stacksize=argcount+4+(isva?sMAXARGS:0);
    begcseg();
    startfunc(wrapper->name,TRUE);        /* proc */
    if (isva) {
      /* Variadic wrapper: forward this wrapper's own varargs to the chain, then
       * the fixed args, then idx=0, then a run-time byte-count marker. Push
       * order is the runtime frame order reversed: the tail lands at the
       * HIGHEST callee offsets (chain varargs at (fixedargs+4)*cell), so it is
       * pushed FIRST (deepest); idx lands at chain arg 0 (3*cell), so it is
       * pushed LAST before the marker. fixedargs (not argcount) is the fixed
       * cell count -- argcount also counts the trailing "..." slot. */
      fwdpushloop((cell)(fixedargs+3)*sizeof(cell)); /* copy wrapper varargs (deepest) */
      for (a=fixedargs-1; a>=0; a--) {    /* fixed args (reverse) */
        stgwrite("\tpush.s ");
        outval((a+3)*sizeof(cell),TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* for */
      pushval(0);                         /* idx = 0 (leading argument) */
      fwdbytecount(fixedargs+1,fixedargs);/* marker = fixed + idx + tail; skip=fixedargs
                                           * (the wrapper frame carries no idx) */
      ffcall(chain,NULL,fixedargs+1);     /* result in PRI */
    } else {
      for (a=argcount-1; a>=0; a--) {     /* forward the wrapper's own args (reverse) */
        stgwrite("\tpush.s ");
        outval((a+3)*sizeof(cell),TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* for */
      pushval(0);                         /* idx = 0 (leading argument) */
      pushval(argbytes_body);             /* argument-count marker (in bytes) */
      ffcall(chain,NULL,argcount+1);      /* result in PRI */
    } /* if */
    ffret(TRUE);                          /* return chain(0, args) */
    endfunc();
    wrapper->codeaddr=code_idx;

    /* ---- chain dispatcher @chook.Name.chain(idx, args) ---- */
    chain->addr=code_idx;
    curfunc=chain;
    if (chain->x.stacksize<argcount+4+(isva?sMAXARGS:0))
      chain->x.stacksize=argcount+4+(isva?sMAXARGS:0);
    begcseg();
    startfunc(chain->name,TRUE);          /* proc */
    lbls=(int*)malloc((grp->count>0 ? grp->count : 1)*sizeof(int));
    if (lbls==NULL) {
      error(103);                         /* insufficient memory */
      curfunc=savedfunc;
      return;
    } /* if */
    for (k=0; k<grp->count; k++)
      lbls[k]=getlabel();
    /* dispatch: if idx==k, jump to body k's call */
    for (k=0; k<grp->count; k++) {
      stgwrite("\tload.s.pri ");          /* PRI = idx (param 0) */
      outval(3*sizeof(cell),TRUE);
      code_idx+=opcodes(1)+opargs(1);
      ldconst((cell)k,sALT);              /* ALT = k */
      ob_eq();                            /* PRI = (idx == k) */
      jmp_ne0(lbls[k]);
    } /* for */
    /* default: tail-call the ORIGINAL. pc_emit_orig guards the redirect so this
     * call is not itself routed back through the wrapper (recursion guard). */
    pc_emit_orig=1;
    if (isva) {
      /* Variadic default -> original (NO idx to the original). Tail first
       * (highest offsets), then the fixed args. skip=fixedargs+1 because the
       * CHAIN's own frame carries idx before its fixed args, so the tail begins
       * fixedargs+1 cells into the frame -- this must match fwdpushloop's own
       * skip ((fixedargs+4)*cell => fixedargs+1 leading cells) and, for a
       * native, fwdpopnative below. */
      fwdpushloop((cell)(fixedargs+4)*sizeof(cell)); /* copy chain varargs */
      for (a=fixedargs-1; a>=0; a--) {
        stgwrite("\tpush.s ");
        outval((a+4)*sizeof(cell),TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* for */
      fwdbytecount(fixedargs,fixedargs+1);/* marker = fixed + tail (NO idx) */
    } else {
      for (a=argcount-1; a>=0; a--) {     /* forward the chain's target args (reverse) */
        stgwrite("\tpush.s ");
        outval((a+4)*sizeof(cell),TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* for */
      pushval(argbytes_orig);
    } /* if */
    /* For a NATIVE original, ffcall emits "sysreq.c <id>" and assigns the native
     * id lazily (sc4.c), in the write pass, only while uREAD is clear. Two cases:
     *   - uREAD clear: no non-redirected call site emitted a sysreq for this
     *     native, so this dispatcher tail is the first & only real sysreq --
     *     ffcall assigns the id here.
     *   - uREAD set: a REAL (non-redirected, e.g. lexically-before-the-hook) call
     *     site already emitted a sysreq and ffcall assigned it an id; we must
     *     REUSE that id. So we do NOT clear uREAD -- clearing it would make ffcall
     *     assign a SECOND, different id to the same native (double id -> a hole /
     *     out-of-bounds write in the natives table, sc6.c, and a runtime error 19).
     * Redirected call sites deliberately do not set uREAD on the original
     * (sc3.c), so uREAD here means exactly "a real sysreq already exists". */
    ffcall(orig,NULL,isva?fixedargs:argcount); /* pawn -> call, native -> sysreq (ffcall decides).
                                           * For a variadic native, ffcall's constant "stack"
                                           * pops only the fixedargs static cells + marker; the
                                           * forwarded tail is popped by fwdpopnative below. */
    if ((orig->usage & uNATIVE)!=0) {
      markusage(orig,uREAD);              /* count the native in the natives table */
      /* count the library this native belongs to, mirroring the ordinary call
       * site (sc3.c): the dispatcher tail is a real call, so its library must be
       * listed even if every user call site was redirected to the wrapper. The
       * value is a usage threshold (>0), so overlapping with a call site's own
       * increment is harmless. */
      if (orig->x.lib!=NULL)
        orig->x.lib->value+=1;
      if (isva)
        fwdpopnative(fixedargs+1);        /* pop the forwarded tail; skip=fixedargs+1 matches
                                           * the chain frame's idx+fixed header, so exactly the
                                           * cells fwdpushloop pushed are popped */
    } /* if */
    pc_emit_orig=0;
    ffret(TRUE);
    /* one body call per slot: body k(idx, args) */
    for (k=0; k<grp->count; k++) {
      symbol *body=grp->slots[k].fn;
      setlabel(lbls[k]);
      if (isva) {
        /* Variadic case -> body(idx, fixed..., tail...). Tail first (deepest),
         * then fixed, then idx (chain arg 0 at 3*cell). skip=fixedargs+1: the
         * chain frame carries idx before its fixed args, so the tail begins
         * fixedargs+1 cells in (matches fwdpushloop's (fixedargs+4)*cell). */
        fwdpushloop((cell)(fixedargs+4)*sizeof(cell)); /* copy chain varargs */
        for (a=fixedargs-1; a>=0; a--) {
          stgwrite("\tpush.s ");
          outval((a+4)*sizeof(cell),TRUE);
          code_idx+=opcodes(1)+opargs(1);
        } /* for */
        stgwrite("\tpush.s ");            /* forward idx as body's leading param */
        outval(3*sizeof(cell),TRUE);
        code_idx+=opcodes(1)+opargs(1);
        fwdbytecount(fixedargs+1,fixedargs+1); /* marker = idx + fixed + tail */
      } else {
        for (a=argcount-1; a>=0; a--) {   /* forward target args (reverse) */
          stgwrite("\tpush.s ");
          outval((a+4)*sizeof(cell),TRUE);
          code_idx+=opcodes(1)+opargs(1);
        } /* for */
        stgwrite("\tpush.s ");            /* forward idx as body's leading param */
        outval(3*sizeof(cell),TRUE);
        code_idx+=opcodes(1)+opargs(1);
        pushval(argbytes_body);
      } /* if */
      markusage(body,uREAD);
      ffcall(body,NULL,isva?fixedargs+1:argcount+1);
      ffret(TRUE);
    } /* for */
    free(lbls);
    endfunc();
    chain->codeaddr=code_idx;
  } /* for */
  curfunc=savedfunc;
}

/*  Coroutine generators ("yield") - experiment 009
 *
 *  An "iterfunc" whose body contains a top-level "yield return <expr>;" is a
 *  coroutine generator: "foreach" does not call it fresh on every step, it
 *  RESUMES it where it last suspended. An "iterfunc" that declares no
 *  parameters at all is a generator too -- with no argument cell to receive
 *  the running state, the re-entrant call-loop of the other "iterfunc" kinds
 *  cannot drive it (see generator_isgen()).
 *
 *  All state a running generator needs to survive across the suspend lives in
 *  one heap block, the "state block", of which "foreach" owns the lifetime:
 *
 *      B[0]        continuation to resume at (0 = never suspended yet)
 *      B[1 .. L]   the generator's lifted scalar locals (L = 0 in v1)
 *
 *  The base B is passed BY VALUE as the hidden first argument on every call,
 *  so inside the generator "arg0" (i.e. [frm+3*sizeof(cell)]) is B. The
 *  prologue loads B[0] and, if it is non-zero, jumps back into the middle of
 *  the body with "sctrl 6" (set CIP). No new opcodes are used, so the output
 *  runs on an unmodified host -- but the host must support "lctrl 6"/"sctrl 6"
 *  (CIP get/set), the same dependency YSI's yield has.
 *
 *  "yield" itself only has to record where to resume and get the value out to
 *  "foreach". It calls the single generic helper "@yield.emit(B,val)", which
 *  captures its own return address (the instruction after the call, i.e.
 *  exactly the resume point) as the continuation, then unwinds the generator's
 *  frame so that the value is returned to "foreach" just as the generator's
 *  own "retn" would have done.
 *
 *  A generator's scalar locals are "lifted" into the state block (see declloc()
 *  and rvalue()/store() in sc4.c), so they survive across a suspend; they do not
 *  occupy the stack frame, which is discarded on suspend and rebuilt on resume.
 *
 *  Not supported yet: array/string locals (only scalars fit a block slot -- they
 *  are rejected in declloc), and user arguments (a later step of experiment 009
 *  copies them into their slots on the first call).
 */

/*  the sentinel that ends a generator's sequence: cellmin, the same value the
 *  re-entrant "iterfunc" generators return (ITER_STOP) */
#define generator_iterstop ((cell)((ucell)1 << (PAWN_CELL_SIZE-1)))

/*  generator_isgen - is the current function definition a coroutine generator?
 *
 *  This must answer identically in the addressing pass (statFIRST) and the
 *  code-emission pass (statWRITE), so it is derived only from information that
 *  is stable across passes: the uGENERATOR flag of an earlier pass, and the
 *  declared parameter count.
 */
static int generator_isgen(symbol *sym)
{
  if ((sym->usage & uGENERATOR)!=0)
    return TRUE;
  if ((sym->usage & uASYNC)!=0)
    return TRUE;                /* an "async" function is always a coroutine (exp 012) */
  if ((sym->usage & uITERFUNC)==0)
    return FALSE;
  /* no parameters: there is no "cur"/"&state" argument for the re-entrant
   * call-loop to hand the function, so the coroutine protocol is the only one
   * that can drive it.
   *
   * D5 reclassification note: this reinterprets EVERY parameterless iterfunc as
   * a coroutine generator, even one that never uses "yield". A no-param iterfunc
   * therefore no longer follows the classic re-entrant iterator protocol -- its
   * "return <val>" is treated as the end of the sequence (it yields ITER_STOP,
   * == cellmin, to "foreach"), not as a per-step value. This is intentional for
   * v1 (a parameterless classic iterfunc has no state channel to drive it), but
   * any future no-param iterfunc inherits this coroutine semantics. */
  return sym->dim.arglist==NULL || sym->dim.arglist[0].ident==0;
}

/*  gen_reserved - number of reserved head cells at the base of a coroutine's
 *  state block B, i.e. the count of B slots that precede the lifted param/local
 *  slots. A "yield" generator reserves 1 (B[0] = continuation CIP). An "async"
 *  coroutine (exp 012) reserves 4: B[0] = continuation, B[1] = the await-result
 *  inbox that the scheduler writes before resuming (ASYNC_INBOX_SLOT),
 *  B[2] = the coroutine's own code entry address (ASYNC_ENTRY_SLOT), which the
 *  generic token-dispatched resume loads into PRI and calls through "call.pri"
 *  so ONE scheduler can resume ANY parked coroutine by token, and B[3] = the
 *  awaiter link (ASYNC_AWAITER_SLOT): the state block of the async coroutine
 *  that is "await"ing this one, or 0 for a top-level coroutine with no awaiter.
 *  On "return <v>" a non-top-level coroutine delivers <v> straight into its
 *  awaiter's inbox and resumes it by that B pointer (no registry lookup needed).
 *  Lifted slot i therefore lives at byte offset (gen_reserved(sym)+i)*sizeof(cell).
 *  This must be pass-stable, and it is: uASYNC is set at declaration. */
#define ASYNC_INBOX_SLOT 1      /* B[1] holds the value delivered to "await" on resume */
#define ASYNC_ENTRY_SLOT 2      /* B[2] holds the coroutine's code entry address (call.pri) */
#define ASYNC_AWAITER_SLOT 3    /* B[3] holds the awaiter's B (0 = top-level, no awaiter) */
static int gen_reserved(const symbol *sym)
{
  return (sym!=NULL && (sym->usage & uASYNC)!=0) ? 4 : 1;
}

/*  generator_helper - the symbol of the hidden "@yield.emit" helper, creating
 *  it on first use.
 *
 *  The helper is defined once per compilation unit at end-of-parse in both
 *  passes (generator_emit_helper()), exactly like the native-hook dispatchers,
 *  so its address is the same in both passes and a generator's "call
 *  .@yield.emit" resolves. Its argument list is the default empty one; it is
 *  never called through the type-checking call path.
 */
static symbol *generator_helper(void)
{
  symbol *sym;

  sym=fetchfunc("@yield.emit",0);
  if (sym==NULL)
    return NULL;                /* error 021 already given */
  sym->usage|=uDEFINE|uSTOCK;   /* synthesized, never warned about as unused */
  return sym;
}

/*  generator_emit_helper - emit the body of "@yield.emit", at end-of-parse.
 *
 *  The helper is called as "@yield.emit(B, val)" by a suspended generator, so
 *  on entry (with the usual frame layout of this compiler) it has:
 *
 *      [frm+0]  caller's frame == the generator's frame
 *      [frm+4]  return address    == the point to resume the generator at
 *      [frm+8]  number of argument bytes
 *      [frm+12] B    (first argument)
 *      [frm+16] val  (second argument)
 *
 *  It stores the resume point into B[0], then unwinds the generator's frame --
 *  "sctrl 4" sets STK back to the generator's frame and "retn" pops the
 *  generator's frame, its return address and its arguments, exactly as if the
 *  generator had returned itself -- and leaves "val" in PRI, so the value is
 *  what "foreach" receives from the generator call.
 */
static void generator_emit_helper(void)
{
  symbol *sym,*savedfunc;
  int havegenerator;
  cell argbase;

  /* emit only when a generator exists, and do so at the same point of both
   * passes (right after the hook dispatchers) so the address matches */
  havegenerator=FALSE;
  for (sym=glbtab.next; sym!=NULL; sym=sym->next)
    if (sym->ident==iFUNCTN && (sym->usage & uGENERATOR)!=0) {
      havegenerator=TRUE;
      break;
    } /* if */
  if (!havegenerator)
    return;
  sym=generator_helper();
  if (sym==NULL)
    return;

  savedfunc=curfunc;
  sym->addr=code_idx;           /* address of the "proc" that follows */
  curfunc=sym;
  begcseg();
  startfunc(sym->name,TRUE);    /* emit "proc" */

  argbase=3*sizeof(cell);       /* [frm + 3*cell] = first argument */
  stgwrite("\tload.s.alt ");    /* ALT = B */
  outval(argbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tload.s.pri ");    /* PRI = the resume point (our own return address) */
  outval(sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tstor.i\n");       /* B[0] = PRI */
  code_idx+=opcodes(1);
  stgwrite("\tload.s.pri ");    /* PRI = [frm+0] = the generator's own frame */
  outval(0,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tsctrl 4\n");      /* STK = the generator's frame: drop our frame */
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tload.s.pri ");    /* PRI = val */
  outval(argbase+sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  ffret(TRUE);                  /* return to "foreach"; pops the generator's frame */

  endfunc();
  sym->codeaddr=code_idx;
  curfunc=savedfunc;
}

/*  inline_emit_prologue - emit the static-link save at the top of an "inline" body
 *  (experiment 015). callindirect() passes the enclosing frame's FRM in ALT before
 *  "call.pri"; PROC does not touch ALT, so the first thing the body does is stash
 *  ALT into a hidden frame cell. Captured enclosing locals are then reached as
 *  *(FRM+pc_inlinelink) + addr (see sc4.c lifted_slotaddr_pri). */
static void inline_emit_prologue(void)
{
  cell linkcell;
  declared+=1;
  linkcell=-declared*(cell)sizeof(cell);
  pc_inlinelink=linkcell;               /* captured-local access indexes off this cell */
  /* The static link arrives in ALT. modstk() emits the "stack" opcode, which does
   * "alt=stk" and would DESTROY the link before we save it -- so move the link into
   * PRI first, allocate, then store PRI. */
  stgwrite("\tmove.pri\n");             /* PRI = ALT = the static link */
  code_idx+=opcodes(1);
  modstk(-(int)sizeof(cell));
  assert(curfunc!=NULL);
  if (curfunc->x.stacksize<declared+1)
    curfunc->x.stacksize=declared+1;
  stgwrite("\tstor.s.pri ");            /* linkcell = PRI = the enclosing frame's FRM */
  outval(linkcell,TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/*  generator_emit_prologue - emit the resume test at the top of a generator.
 *
 *  One hidden local cell holds the state-block base B for the whole body; it
 *  is the first local of the function, so its frame offset is always
 *  "-1*sizeof(cell)" (the body's own locals follow it).
 *
 *  B[0] is the continuation to resume at. It is 0 on the first call, which
 *  must run the body from the top; otherwise "sctrl 6" jumps straight back to
 *  the instruction after the "yield" that suspended the generator.
 *
 *  A generator's user parameters are LIFTED into block slots, exactly like its
 *  scalar locals (Task 2). The parameters take the first slots (0..P-1) and the
 *  body's locals continue from slot P in declloc(), so param slots and local
 *  slots never collide -- "genlocals" is the single running slot counter for
 *  both. This makes a parameter mutable and lets its value survive a "yield"
 *  suspend. The real calling convention is Gen(B, p0, p1, ...): B is the hidden
 *  arg0 at FRM+12, so a parameter declared by declargs() at frame offset "off"
 *  is physically passed one cell higher (at off+sizeof(cell)). On the FRESH call
 *  each parameter is copied from that incoming stack location into its block
 *  slot; on a RESUME the copies are skipped (the block already holds the state).
 */
static void generator_emit_prologue(void)
{
  int lbl_fresh;
  cell localsbase;
  symbol *sym;
  int slot,nparm,pi;
  cell parmphys[sMAXARGS];      /* incoming stack offset of each lifted param */
  cell parmslot[sMAXARGS];      /* block byte-offset of each lifted param slot */
  cell parmsize[sMAXARGS];      /* cells per lifted param: 1 (scalar) or N (copied-in array) */

  declared+=1;
  localsbase=-declared*(cell)sizeof(cell);
  pc_genlocalsbase=localsbase; /* lifted-local access indexes off this cell */
  modstk(-(int)sizeof(cell));
  assert(curfunc!=NULL);
  if (curfunc->x.stacksize<declared+1)
    curfunc->x.stacksize=declared+1;

  /* lift each user parameter into a block slot (they occupy the first slots;
   * body locals in declloc() continue after them). Only local symbols exist yet
   * (define_args() ran; no body local is declared before the prologue), so every
   * symbol in loctab is a parameter. A scalar fits one slot. An "async" coroutine
   * additionally COPIES IN a fixed-size 1-D array parameter: its cells are copied
   * from the caller's array into consecutive block slots on the fresh call (below),
   * so the coroutine owns them and they survive the suspend (the caller's frame is
   * gone by then). An UNSIZED or MULTI-DIMENSIONAL array, or a by-"&"reference
   * parameter, still cannot be lifted (no compile-time cell count / caller-frame
   * aliasing) and is rejected. */
  nparm=0;
  for (sym=loctab.next; sym!=NULL; sym=sym->next) {
    assert(sym->vclass==sLOCAL);
    if (sym->ident!=iVARIABLE) {
      if ((curfunc->usage & uASYNC)!=0 && sym->ident==iREFARRAY
          && sym->dim.array.level==0 && sym->dim.array.length>0) {
        /* fixed-size 1-D array parameter: COPY IN. Reserve "length" block cells,
         * rebind the symbol as an owned lifted array (iARRAY at the block offset),
         * and record the incoming address slot + cell count for the fresh-path copy. */
        if (nparm>=sMAXARGS-1)
          break;
        slot=curfunc->genlocals;
        parmphys[nparm]=sym->addr+(cell)sizeof(cell);  /* incoming slot holds the array ADDRESS */
        parmslot[nparm]=(cell)(slot+gen_reserved(curfunc))*sizeof(cell);
        parmsize[nparm]=sym->dim.array.length;         /* cells to copy in */
        sym->addr=parmslot[nparm];                     /* body indexes the block (address() in sc4.c) */
        sym->ident=iARRAY;                             /* now an owned, lifted array (data in B) */
        sym->usage|=uLIFTED;
        curfunc->genlocals=slot+(int)parmsize[nparm];
        nparm++;
        continue;
      } /* if */
      if ((curfunc->usage & uASYNC)!=0)
        error(268);             /* async: an unsized/multi-dim array or "&"reference
                                 * parameter points into the caller's frame, which is
                                 * freed at "await" -- it cannot be copied into the
                                 * coroutine block (no compile-time cell count / it
                                 * aliases the caller), so reject cleanly. */
      else if (sym->ident==iREFERENCE)
        error(98);              /* a generator cannot combine "yield" with a reference/cur parameter */
      else
        error(255,"a generator (\"yield\") may not take an array parameter yet");
      continue;
    } /* if */
    if (nparm>=sMAXARGS-1)
      break;                    /* declargs()/the caller already capped the count
                                 * at sMAXARGS-1 (B takes one slot); keep the two
                                 * bounds symmetric */
    slot=curfunc->genlocals;
    parmphys[nparm]=sym->addr+(cell)sizeof(cell); /* B shifts every user arg up one cell */
    parmslot[nparm]=(cell)(slot+gen_reserved(curfunc))*sizeof(cell);  /* head slots precede locals */
    parmsize[nparm]=1;          /* scalar */
    sym->addr=parmslot[nparm];  /* body access now indexes the block (see sc4.c) */
    sym->usage|=uLIFTED;
    curfunc->genlocals=slot+1;
    nparm++;
  } /* for */

  stgwrite("\tload.s.pri ");    /* PRI = arg0 = B (the state block) */
  outval(3*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tstor.s.pri ");    /* localsbase = B */
  outval(localsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tload.i\n");       /* PRI = B[0] = resume point (0 => fresh) */
  code_idx+=opcodes(1);
  lbl_fresh=getlabel();
  jmp_eq0(lbl_fresh);           /* never suspended: run the body from the top */
  stgwrite("\tsctrl 6\n");      /* resume: CIP = B[0] */
  code_idx+=opcodes(1)+opargs(1);
  setlabel(lbl_fresh);

  /* FRESH path only, ASYNC coroutines: authoritatively record this coroutine's
   * own code entry in B[ASYNC_ENTRY_SLOT], so a later generic Async_Resume can
   * dispatch back into it via "call.pri". The STARTER (doasyncstart / the
   * ergonomic await in doawait) also writes this slot, but its value is a raw
   * const.pri of the callee's address that is STALE for a forward-referenced
   * callee (declared after the start site): the write pass places the callee at
   * a different address than the addressing pass, and a plain const operand is
   * not relocated. Here we overwrite it from curfunc->addr -- read in the
   * coroutine's OWN body, so it is the current pass's real entry every pass and
   * is correct regardless of declaration order. This runs on the fresh call
   * before any suspend, hence before any resume can read the slot; on a resume
   * the prologue "sctrl 6" jumps past this to the continuation, which is fine
   * because the fresh call already set the (pass-stable) value. It is emitted
   * only for uASYNC coroutines -- a plain "yield"/foreach generator is never
   * resumed by call.pri and has no entry slot (gen_reserved==1). */
  if ((curfunc->usage & uASYNC)!=0) {
    stgwrite("\tload.s.pri ");  /* PRI = B */
    outval(localsbase,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tadd.c ");       /* PRI = &B[entry] */
    outval((cell)ASYNC_ENTRY_SLOT*sizeof(cell),TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tmove.alt\n");   /* ALT = &B[entry] (destination) */
    code_idx+=opcodes(1);
    stgwrite("\tconst.pri ");   /* PRI = this coroutine's own entry (same-pass, never stale) */
    outval(curfunc->addr,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tstor.i\n");     /* B[entry] = own code address */
    code_idx+=opcodes(1);
  } /* if */

  /* FRESH path only: copy each incoming user parameter into its block slot, so
   * it is readable in the body and survives resumption. ALT/PRI are free here.
   *   scalar:  ALT = B + slot (destination);   PRI = incoming arg;   *(ALT) = PRI
   *   array:   PRI = caller array address (the incoming slot holds it);  ALT = B +
   *            slot (destination);  movs size -- copy the cells IN so the coroutine
   *            owns them (the caller's array is gone after the first suspend). */
  for (pi=0; pi<nparm; pi++) {
    if (parmsize[pi]!=1) {
      stgwrite("\tload.s.pri ");  /* PRI = incoming array ADDRESS (source) */
      outval(parmphys[pi],TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tpush.pri\n");   /* save source address */
      code_idx+=opcodes(1);
      stgwrite("\tload.s.pri ");  /* PRI = B */
      outval(localsbase,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tadd.c ");       /* PRI = B + slot (destination) */
      outval(parmslot[pi],TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tmove.alt\n");   /* ALT = B + slot (destination) */
      code_idx+=opcodes(1);
      stgwrite("\tpop.pri\n");    /* PRI = source address */
      code_idx+=opcodes(1);
      memcopy(parmsize[pi]*(cell)sizeof(cell));   /* movs: copy [PRI] -> [ALT], size bytes */
      continue;
    } /* if */
    stgwrite("\tload.s.pri ");  /* PRI = B */
    outval(localsbase,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tadd.c ");       /* PRI = B + slot (the block slot address) */
    outval(parmslot[pi],TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tmove.alt\n");   /* ALT = B + slot (destination) */
    code_idx+=opcodes(1);
    stgwrite("\tload.s.pri ");  /* PRI = incoming parameter value */
    outval(parmphys[pi],TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tstor.i\n");     /* *(B + slot) = parameter */
    code_idx+=opcodes(1);
  } /* for */

  /* the body begins here: "declared" now counts only the one prologue cell
   * (lifted params/locals live in the state block, off-stack). Record it as the
   * "no live stack storage" baseline the "yield" guard in doyield() checks. */
  pc_gen_baseline=declared;
}

/*  doyield - parse "yield return <expr>;" and emit the suspend.
 *
 *  The value of <expr> is left in PRI by "expression()" (a constant is not
 *  loaded, so it is forced there), and the state block is loaded from the
 *  hidden first argument. Both, plus the argument count, are pushed for
 *  "@yield.emit" -- the value first, so that B is the FIRST argument -- and
 *  the call follows. The instruction after the call is where the generator
 *  resumes on the next step; the helper finds it as its own return address.
 */
static void doyield(void)
{
  int ident,tag;
  cell val;
  symbol *sym,*helper;
  int localstaging,index;

  if (curfunc==NULL || (curfunc->usage & uITERFUNC)==0) {
    error(95);          /* "yield" is only valid inside an iterfunc generator */
    lexclr(TRUE);
    return;
  } /* if */
  if (!pc_generator) {
    /* the first "yield" of this generator, seen while its body is parsed: the
     * prologue was not emitted (the addressing pass did not know yet), so have
     * the addressing pass redone. That is what "sc_reparse" is for; the next
     * run sees uGENERATOR at the top of the function and emits the prologue
     * there, identically to the code-emission pass. */
    pc_generator=TRUE;
    curfunc->usage|=uGENERATOR;
    sc_reparse=TRUE;
  } /* if */
  if (pc_generator && declared>pc_gen_baseline) {
    /* a live stack cell exists beyond the prologue's single reserved cell -- a
     * construct inside the body (e.g. a nested "foreach") allocated loop/hidden
     * cells that are NOT lifted into the state block. The suspend discards the
     * frame and the resume rebuilds it with only the prologue cell, so that
     * storage would be garbage on resume (a silent runtime hang). Reject it
     * rather than miscompile; the compile fails, so no broken binary is emitted.
     * error() suppresses this (number<100) outside the statWRITE pass, and the
     * comparison is derived from prologue-set, pass-stable data, so the guard
     * fires identically in both passes. */
    error(99);
    lexclr(TRUE);
    return;
  } /* if */
  if (!needtoken(tRETURN)) {
    lexclr(TRUE);
    return;
  } /* if */

  /* Stage-buffer the yielded expression and the "@yield.emit" call, exactly as
   * doexpr()/doreturn()/test() stage any other statement expression. Without
   * staging the peephole optimizer and, crucially, the stgdel() that plnge2()
   * uses to scratch a pushed left operand when the right operand is a constant
   * are no-ops, so "<lifted local> OP <constant>" (e.g. "i * 10") emits broken
   * code. Staging here makes a "yield return" expression behave like any other. */
  localstaging=FALSE;
  if (!staging) {
    stgset(TRUE);                   /* start stage-buffering */
    localstaging=TRUE;
    assert(stgidx==0);
  } /* if */
  index=stgidx;
  errorset(sEXPRMARK,0);

  sym=NULL;
  ident=expression(&val,&tag,&sym,FALSE);   /* the yielded value, in PRI */
  if (ident==iARRAY || ident==iREFARRAY) {
    error(33,(sym!=NULL) ? sym->name : "-unknown-");  /* array must be indexed */
    ldconst(0,sPRI);
  } else if (ident==iCONSTEXPR) {
    ldconst(val,sPRI);                      /* a constant is not auto-loaded */
  } /* if */

  helper=generator_helper();
  if (helper!=NULL) {
    pushreg(sPRI);                  /* val (second argument: pushed first) */
    stgwrite("\tload.s.pri ");      /* PRI = B (hidden first argument) */
    outval(3*sizeof(cell),TRUE);
    code_idx+=opcodes(1)+opargs(1);
    pushreg(sPRI);                  /* B */
    pushval(2*sizeof(cell));        /* argument count, in bytes */
    markusage(helper,uREAD);
    ffcall(helper,NULL,2);
  } /* if */

  markexpr(sEXPR,NULL,0);           /* end of the yield expression/statement */
  errorset(sEXPRRELEASE,0);
  if (localstaging) {
    stgout(index);
    stgset(FALSE);                  /* stop staging */
  } /* if */
  needtoken(tTERM);
}

/*  async_emit_suspend - emit an async coroutine's suspend + resume-value read.
 *
 *  On entry PRI holds the value to yield to the coroutine's caller (the token in
 *  the plain-await case; ignored by the start glue / a resuming awaiter). It
 *  suspends through the shared "@yield.emit(B,val)" helper -- which records the
 *  resume CIP in B[0] and unwinds the frame back to the caller -- and then emits
 *  the RESUME landing code: PRI = *(B + ASYNC_INBOX_SLOT), the value the scheduler
 *  (or a returning awaitee) delivered, which becomes the value of "await". Shared
 *  by the plain-await and the ergonomic "await asyncFn(args)" paths so both
 *  suspend identically. B is the hidden arg0 at FRM+3*cell on the incoming frame,
 *  and also lives in the localsbase cell (pc_genlocalsbase) for the resume read. */
static void async_emit_suspend(symbol *helper)
{
  if (helper!=NULL) {
    pushreg(sPRI);                  /* yield value (second argument: pushed first) */
    stgwrite("\tload.s.pri ");      /* PRI = B (hidden first argument) */
    outval(3*sizeof(cell),TRUE);
    code_idx+=opcodes(1)+opargs(1);
    pushreg(sPRI);                  /* B */
    pushval(2*sizeof(cell));
    markusage(helper,uREAD);
    ffcall(helper,NULL,2);
  } /* if */
  /* RESUME lands here (this is @yield.emit's saved return address). Deliver the
   * value stored in the inbox slot: PRI = *(B + ASYNC_INBOX_SLOT). */
  stgwrite("\tload.s.pri ");        /* PRI = B (localsbase cell) */
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");             /* PRI = B + inbox */
  outval((cell)ASYNC_INBOX_SLOT*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tload.i\n");           /* PRI = B[inbox] = awaited value */
  code_idx+=opcodes(1);
}

/*  async_emit_arena_alloc - emit the arena-backed allocation of a coroutine state
 *  block B, shared by doasyncstart (top-level start) and doawait (composed inner
 *  await).  It emits:  B = Async_Alloc(blkcells);  stash B in the hidden cell
 *  "btemp";  if B==-1 (arena full or the block does not fit a slot) jump to
 *  "lbl_full" and start nothing;  else zero the (possibly reused) block and store
 *  B[ASYNC_ENTRY_SLOT] = fsym's code entry (for call.pri resume).  The caller then
 *  writes B[ASYNC_AWAITER_SLOT] (which differs: -1 for top-level, the awaiter's B
 *  for a composed inner) and does the fresh call.  Routing BOTH paths through the
 *  same arena is what makes every coroutine's B reclaimable -- the composed inner
 *  no longer leaks on the LIFO heap.  Returns 1 on success, 0 if Async_Alloc is
 *  not visible (caller reports the error).  Every emitted instruction is
 *  fixed-length, so two-pass address stability holds. */
static int async_emit_arena_alloc(symbol *fsym,cell btemp,int blkcells,int lbl_full)
{
  symbol *allocsym=findglb("Async_Alloc",sGLOBAL);
  if (allocsym==NULL || allocsym->ident!=iFUNCTN)
    return 0;
  markusage(allocsym,uREAD);
  pushval((cell)blkcells);          /* arg0 = blkcells (bounds-checked in Async_Alloc) */
  pushval((cell)sizeof(cell));      /* 1 argument */
  ffcall(allocsym,NULL,1);          /* PRI = B (arena block address), or -1 if full/too big */
  stgwrite("\tstor.s.pri ");        /* stash B in the hidden cell */
  outval(btemp,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  /* arena full / block too big (B==-1): skip the whole start. The sentinel is -1,
   * not 0, because a valid block can sit at data address 0; test "B+1==0". */
  stgwrite("\tadd.c ");             /* PRI = B + 1 (so the -1 sentinel becomes 0) */
  outval((cell)1,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  jmp_eq0(lbl_full);                /* B==-1 -> skip */
  /* zero the block (a reused slot holds stale state): ALT=B, PRI=0, fill blkcells */
  stgwrite("\tload.s.pri ");        /* PRI = B */
  outval(btemp,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tmove.alt\n");         /* ALT = B (fill destination) */
  code_idx+=opcodes(1);
  ldconst(0,sPRI);                  /* PRI = 0 (fill value) */
  stgwrite("\tfill ");
  outval((cell)blkcells*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  /* B[ASYNC_ENTRY_SLOT] = fsym code entry (raw const.pri for length stability).
   * NOTE: fsym->addr is a RAW literal here, so for a FORWARD-referenced callee
   * (declared after this start site) it is the callee's address from the previous
   * pass, which is stale -- the write pass lays the callee out at a different
   * address than the addressing pass (dead-code elimination shrinks the write
   * pass), and there is no relocation for a plain const operand (only "call
   * .name" is resolved at assemble time; cf. __addressof rejecting forward refs).
   * This store is therefore NOT authoritative: the callee re-establishes its own
   * B[ASYNC_ENTRY_SLOT] from curfunc->addr (a same-pass, always-correct value) at
   * the top of its prologue (see generator_emit_prologue), before any generic
   * call.pri resume can read it. Keeping this store keeps the emitted sequence
   * (and thus code_idx) identical for the declared-before and forward cases. */
  stgwrite("\tload.s.pri ");        /* PRI = B */
  outval(btemp,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");             /* PRI = &B[entry] */
  outval((cell)ASYNC_ENTRY_SLOT*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tmove.alt\n");         /* ALT = &B[entry] (destination) */
  code_idx+=opcodes(1);
  stgwrite("\tconst.pri ");         /* PRI = fsym code address (provisional; see above) */
  outval(fsym->addr,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tstor.i\n");           /* B[entry] = code address */
  code_idx+=opcodes(1);
  return 1;
}

/*  async_emit_free_self - when the CURRENT "async" coroutine completes, return its
 *  arena slot to the free pool by its own B address (Async_FreeByAddr computes the
 *  slot from the address and frees it; a non-arena or already-free address is a
 *  no-op).  Emitted at BOTH completion points -- the return-to-awaiter block in
 *  doreturn (explicit "return") and the fall-off-the-end epilogue -- so a
 *  coroutine that finishes via the nested call.pri chain (never observed "done" by
 *  a top-level Async_Resume) still frees its slot exactly once.  Clobbers PRI/ALT,
 *  so it must be emitted where PRI is dead (before the return/iterstop value is
 *  loaded).  No-op for non-async functions or when <async> is not included. */
static void async_emit_free_self(void)
{
  symbol *freesym;
  if (curfunc==NULL || (curfunc->usage & uASYNC)==0)
    return;
  freesym=findglb("Async_FreeByAddr",sGLOBAL);
  if (freesym==NULL || freesym->ident!=iFUNCTN)
    return;                         /* <async> not included: nothing to free */
  markusage(freesym,uREAD);
  stgwrite("\tpush.s ");            /* arg0 = this coroutine's own B */
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  pushval((cell)sizeof(cell));      /* 1 argument */
  ffcall(freesym,NULL,1);
}

/*  async_emit_complete_self - richer completion than async_emit_free_self: hand the
 *  coroutine's final value to the include's __async_complete(B, retval), which
 *  records the result (task_keep / Async_Result), fires any bound callback
 *  (task_bind), then frees the slot unless it is kept. Emitted at BOTH completion
 *  points, replacing the bare Async_FreeByAddr. "retval_cell" is the stack cell
 *  holding the return value (explicit "return"), or 0 to pass a constant 0 (the
 *  fall-off-the-end path has no explicit value). Falls back to async_emit_free_self
 *  when an older <async> without __async_complete is included. Clobbers PRI/ALT, so
 *  it must be emitted where PRI is dead. No-op for non-async functions. */
static void async_emit_complete_self(cell retval_cell)
{
  symbol *csym;
  if (curfunc==NULL || (curfunc->usage & uASYNC)==0)
    return;
  csym=findglb("__async_complete",sGLOBAL);
  if (csym==NULL || csym->ident!=iFUNCTN) {
    async_emit_free_self();           /* older <async>: free only, no result capture */
    return;
  }
  markusage(csym,uREAD);
  /* Read B into ALT BEFORE pushing anything. At the fall-off-the-end completion
   * site the frame locals -- including the localsbase cell that holds B -- have
   * already been popped (modstk before this point), so STK sits at that cell; a
   * "push" here would overwrite the localsbase memory, and a later push.s of it
   * would read back the just-pushed value instead of B. Loading B first sidesteps
   * that entirely, and is harmless on the "return" path where the cell is still live. */
  stgwrite("\tload.s.pri ");          /* PRI = B (this coroutine's block) */
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tmove.alt\n");           /* ALT = B (survives the retval load/push below) */
  code_idx+=opcodes(1);
  /* arguments are pushed right-to-left: param1 = retval first, then param0 = B */
  if (retval_cell!=0) {
    stgwrite("\tload.s.pri ");        /* PRI = the return value (hidden cell) */
    outval(retval_cell,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tpush.pri\n");         /* param1 = return value */
    code_idx+=opcodes(1);
  } else {
    pushval((cell)0);                 /* param1 = 0 (fall-off-the-end has no value) */
  } /* if */
  stgwrite("\tpush.alt\n");           /* param0 = B */
  code_idx+=opcodes(1);
  pushval((cell)(2*sizeof(cell)));    /* 2 arguments */
  ffcall(csym,NULL,2);
}

/*  async_emit_clearfault - reset the include's fault channel (g_asyncFailed /
 *  g_asyncErr) on the return-to-awaiter resume path, so a composed
 *  "await asyncFn()" whose inner returns NORMALLY does not leave a stale fault
 *  for its awaiter to observe (the inner may have handled a leaf fault and
 *  returned a good value). Mirrors async_emit_free_self's one-argument call
 *  emission (the arg is ignored by __async_clearfault). No-op when <async> is not
 *  included / the fault channel is unused. Clobbers PRI/ALT, so it must be emitted
 *  where PRI is dead. */
static void async_emit_clearfault(void)
{
  symbol *clrsym;
  if (curfunc==NULL || (curfunc->usage & uASYNC)==0)
    return;
  clrsym=findglb("__async_clearfault",sGLOBAL);
  if (clrsym==NULL || clrsym->ident!=iFUNCTN)
    return;                         /* <async> not included: no fault channel to clear */
  markusage(clrsym,uREAD);
  stgwrite("\tpush.s ");            /* arg0 = our B (ignored by the callee) */
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  pushval((cell)sizeof(cell));      /* 1 argument */
  ffcall(clrsym,NULL,1);
}

/* async_bcell_addr - PRI = &B[slot] (data address of the slot-th cell of this
 * coroutine's block), via the reloaded localsbase. */
static void async_bcell_addr(int slot)
{
  stgwrite("\tload.s.pri ");
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");
  outval((cell)(gen_reserved(curfunc)+slot)*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/* async_bcell_load - PRI = B[slot]. */
static void async_bcell_load(int slot)
{
  async_bcell_addr(slot);
  stgwrite("\tload.i\n");
  code_idx+=opcodes(1);
}

/* async_bcell_store - B[slot] = PRI (clobbers ALT; preserves nothing else). */
static void async_bcell_store(int slot)
{
  stgwrite("\tpush.pri\n");        /* save the value */
  code_idx+=opcodes(1);
  async_bcell_addr(slot);          /* PRI = &B[slot] */
  stgwrite("\tmove.alt\n");        /* ALT = &B[slot] */
  code_idx+=opcodes(1);
  stgwrite("\tpop.pri\n");         /* PRI = value */
  code_idx+=opcodes(1);
  stgwrite("\tstor.i\n");          /* B[slot] = value */
  code_idx+=opcodes(1);
}

/* async_emit_boundary - PRI = FRM - pc_gen_baseline*cell: the address just above
 * the live operand-stack temporaries (the bottom of the lifted frame's reserved
 * cells). Everything in [STK, boundary) is a live temporary at this point. */
static void async_emit_boundary(void)
{
  stgwrite("\tlctrl 5\n");         /* PRI = FRM */
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");            /* PRI = FRM - baseline*cell */
  outval(-(cell)pc_gen_baseline*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/* async_emit_spill - MID-EXPRESSION await support. Copy the live operand-stack
 * temporaries (all cells in [STK, boundary)) into B, RUNTIME-EXACT via a cell loop
 * bounded by STK/FRM (never a static count, which over/under-shoots and corrupts).
 * B[spillslot]=bytecount (for restore), B[spillslot+1]=src, B[spillslot+2]=dst,
 * B[spillslot+3..]=the copied cells. PRI/ALT are free here (the await operand is
 * ignored by the async suspend). */
static void async_emit_spill(int spillslot)
{
  int lblcopy=getlabel(),lbldone=getlabel();
  int cnt=spillslot,src=spillslot+1,dst=spillslot+2,data=spillslot+3;

  /* bytecount = boundary - STK */
  stgwrite("\tlctrl 4\n");         /* PRI = STK */
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tmove.alt\n");        /* ALT = STK */
  code_idx+=opcodes(1);
  async_emit_boundary();           /* PRI = boundary */
  stgwrite("\tsub\n");             /* PRI = boundary - STK = bytecount */
  code_idx+=opcodes(1);
  async_bcell_store(cnt);
  /* src = STK */
  stgwrite("\tlctrl 4\n");         /* PRI = STK */
  code_idx+=opcodes(1)+opargs(1);
  async_bcell_store(src);
  /* dst = &B[data] */
  async_bcell_addr(data);          /* PRI = &B[data] */
  async_bcell_store(dst);
  /* loop: while src < boundary: *dst = *src; src+=cell; dst+=cell */
  setlabel(lblcopy);
  async_bcell_load(src);           /* PRI = src */
  stgwrite("\tpush.pri\n");        /* save src */
  code_idx+=opcodes(1);
  async_emit_boundary();           /* PRI = boundary */
  stgwrite("\tmove.alt\n");        /* ALT = boundary */
  code_idx+=opcodes(1);
  stgwrite("\tpop.pri\n");         /* PRI = src */
  code_idx+=opcodes(1);
  stgwrite("\tjsgeq ");            /* if src >= boundary -> done */
  outval(lbldone,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  async_bcell_load(src);           /* PRI = src */
  stgwrite("\tload.i\n");          /* PRI = *src */
  code_idx+=opcodes(1);
  stgwrite("\tpush.pri\n");        /* save value */
  code_idx+=opcodes(1);
  async_bcell_load(dst);           /* PRI = dst */
  stgwrite("\tmove.alt\n");        /* ALT = dst */
  code_idx+=opcodes(1);
  stgwrite("\tpop.pri\n");         /* PRI = value */
  code_idx+=opcodes(1);
  stgwrite("\tstor.i\n");          /* *dst = value */
  code_idx+=opcodes(1);
  async_bcell_load(src);           /* PRI = src */
  stgwrite("\tadd.c ");            /* PRI = src + cell */
  outval((cell)sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  async_bcell_store(src);
  async_bcell_load(dst);           /* PRI = dst */
  stgwrite("\tadd.c ");            /* PRI = dst + cell */
  outval((cell)sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  async_bcell_store(dst);
  stgwrite("\tjump ");
  outval(lblcopy,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  setlabel(lbldone);
}

/* async_emit_restore - inverse of async_emit_spill: after the resume, the temps
 * were discarded (STK is at boundary). Lower STK by the saved bytecount, copy the
 * cells back from B[spillslot+3..], then leave the resume value (inbox) in PRI. */
static void async_emit_restore(int spillslot)
{
  int lblcopy=getlabel(),lbldone=getlabel();
  int cnt=spillslot,src=spillslot+1,dst=spillslot+2,data=spillslot+3;

  /* STK = boundary - bytecount  (re-allocate the temp region) */
  async_emit_boundary();           /* PRI = boundary */
  stgwrite("\tmove.alt\n");        /* ALT = boundary */
  code_idx+=opcodes(1);
  async_bcell_load(cnt);           /* PRI = bytecount */
  stgwrite("\tsub.alt\n");         /* PRI = boundary - bytecount (ALT - PRI) */
  code_idx+=opcodes(1);
  stgwrite("\tsctrl 4\n");         /* STK = PRI */
  code_idx+=opcodes(1)+opargs(1);
  /* src = &B[data]; dst = new STK */
  async_bcell_addr(data);          /* PRI = &B[data] */
  async_bcell_store(src);
  stgwrite("\tlctrl 4\n");         /* PRI = STK */
  code_idx+=opcodes(1)+opargs(1);
  async_bcell_store(dst);
  /* loop: while dst < boundary: *dst = *src; src+=cell; dst+=cell */
  setlabel(lblcopy);
  async_bcell_load(dst);           /* PRI = dst */
  stgwrite("\tpush.pri\n");
  code_idx+=opcodes(1);
  async_emit_boundary();           /* PRI = boundary */
  stgwrite("\tmove.alt\n");        /* ALT = boundary */
  code_idx+=opcodes(1);
  stgwrite("\tpop.pri\n");         /* PRI = dst */
  code_idx+=opcodes(1);
  stgwrite("\tjsgeq ");            /* if dst >= boundary -> done */
  outval(lbldone,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  async_bcell_load(src);           /* PRI = src */
  stgwrite("\tload.i\n");          /* PRI = *src */
  code_idx+=opcodes(1);
  stgwrite("\tpush.pri\n");
  code_idx+=opcodes(1);
  async_bcell_load(dst);           /* PRI = dst */
  stgwrite("\tmove.alt\n");        /* ALT = dst */
  code_idx+=opcodes(1);
  stgwrite("\tpop.pri\n");         /* PRI = value */
  code_idx+=opcodes(1);
  stgwrite("\tstor.i\n");          /* *dst = value */
  code_idx+=opcodes(1);
  async_bcell_load(src);
  stgwrite("\tadd.c ");
  outval((cell)sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  async_bcell_store(src);
  async_bcell_load(dst);
  stgwrite("\tadd.c ");
  outval((cell)sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  async_bcell_store(dst);
  stgwrite("\tjump ");
  outval(lblcopy,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  setlabel(lbldone);
  /* PRI = B[inbox] = resume value (the value of the await expression) */
  stgwrite("\tload.s.pri ");
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");
  outval((cell)ASYNC_INBOX_SLOT*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tload.i\n");
  code_idx+=opcodes(1);
}


/*  async_check_array_arg - diagnostic for an array argument passed to an "async"
 *  coroutine via doasyncstart (Async_Start) or a composed "await asyncFn(args)".
 *  An async array PARAMETER is COPIED IN at its DECLARED length by the prologue, so
 *  a shorter passed array would be over-read from the caller's frame. The ordinary
 *  call path (callfunction) reports error 47 for a size mismatch; these two async
 *  entry paths bypass that path, so mirror the check here. Only the common 1-D
 *  fixed-size case is checked (unsized/multi-dim array params are rejected 268). */
static void async_check_array_arg(symbol *fsym,int argidx,int id,symbol *argsym)
{
  arginfo *arg;
  int i;
  if (fsym==NULL || fsym->dim.arglist==NULL || argidx<0)
    return;
  arg=fsym->dim.arglist;
  for (i=0; i<argidx; i++)
    if (arg[i].ident==0 || arg[i].ident==iVARARGS)
      return;                       /* fewer fixed params than args -- not our case */
  if (arg[argidx].ident==iREFARRAY && arg[argidx].numdim==1 && arg[argidx].dim[0]>0
      && (id==iARRAY || id==iREFARRAY) && argsym!=NULL
      && argsym->dim.array.length>0
      && argsym->dim.array.length!=arg[argidx].dim[0])
    error(47);                      /* array sizes do not match */
}


/*  doawait - parse "await <expr>" and emit the suspend (experiment 012 spike).
 *
 *  "await" is the async coroutine's suspend point, the analogue of "yield" but
 *  used as an EXPRESSION. It evaluates its operand (an awaitable that arranges
 *  its own later completion and leaves a token in PRI), then suspends through the
 *  SAME shared "@yield.emit" helper the generator engine already provides: the
 *  helper records the resume CIP in B[0] and unwinds the frame, so the token is
 *  returned to whatever called the coroutine (the scheduler's start glue). On
 *  RESUME the scheduler has written the awaited value into B[ASYNC_INBOX_SLOT];
 *  the code emitted right after the suspend loads it, so it becomes the value of
 *  the "await" expression. Leaves that value in PRI (a non-lvalue rvalue).
 *
 *  Ergonomic form "await asyncFn(args)" (Task 2): when the operand is a DIRECT
 *  call to an "async" function, this composes coroutines -- it starts the inner
 *  coroutine, links it back to this one (inner B[ASYNC_AWAITER_SLOT] = our B),
 *  and suspends; the inner's "return" later resumes us with its value. The
 *  detection is a single-token lookahead scoped strictly to the await operand;
 *  the general call path is not touched.
 */
SC_FUNC int doawait(value *lval)
{
  int ident,tag,localstaging,index;
  int spillcount,spillslot;
  cell val;
  symbol *sym,*helper;

  if (curfunc==NULL || (curfunc->usage & uASYNC)==0) {
    error(265);                 /* "await" is only valid inside an "async" function */
    ldconst(0,sPRI);
    return FALSE;
  } /* if */
  if (!pc_generator) {          /* mirror doyield()'s first-suspend reparse */
    pc_generator=TRUE;
    curfunc->usage|=uGENERATOR;
    sc_reparse=TRUE;
  } /* if */
  if (pc_generator && declared>pc_gen_baseline) {
    error(99);                  /* live stack storage cannot span a suspend (mirror yield) */
    ldconst(0,sPRI);
    return FALSE;
  } /* if */
  /* Multiple awaits in one statement ARE supported now: "await" binds at unary
   * precedence (expression_unary below), so "await A() + await B()" is two
   * independent suspends the enclosing operator combines, each spilling its live
   * operand temporaries across its own suspend. The one shape that still does not
   * work is a LEAF await that must carry a COMPOSED await's result across its
   * suspend (compose-then-leaf in one expression, e.g. "await Inner() + await X()"):
   * the compose returns through the awaiter call.pri chain and a following leaf
   * suspend does not unwind back to a resumable point. That case is rejected in the
   * leaf path below (pc_await_composed); everything else -- leaf+leaf, compose+compose,
   * leaf+compose -- runs correctly. */
  /* MID-EXPRESSION await (operand-stack spill): in an "async" body every local is
   * LIFTED into B, so "declared" never rises and the check above is blind to
   * OPERATOR temporaries (the "base" pushed by "base + await F()"). Those temps
   * would be discarded by @yield.emit's "sctrl 4" unwind. To support them, the
   * general path SPILLS the live temporaries (the runtime region [STK, boundary))
   * into B before the suspend and RESTORES them after -- a runtime-exact cell copy
   * (async_emit_spill/restore), correct for any expression shape. pc_exprtemp (the
   * pushreg/popreg balance for this statement) is a safe UPPER BOUND on the temp
   * count, used only to RESERVE B cells (3 scratch + pc_exprtemp data); the copy
   * itself is bounded by STK/FRM at runtime, so an over-count only over-reserves.
   * Composed "await asyncFn()" buffers its own hidden inner-start cells BELOW these
   * operator temporaries and frees them before the suspend, so it uses the SAME
   * spill/restore (the enclosing temporaries are all that remain live). */
  if (pc_generator && getcallnesting()>0 && getcallargvariadic()>0) {
    /* "await" inside a VARIADIC call's argument list (e.g. printf("%d", await F())).
     * Reverse-order emission pushes the arguments AFTER the await before it at run
     * time, so they are live across the suspend -- but the spill reserve is captured
     * HERE, before those trailing args are parsed, and a variadic callee's count is
     * not bounded by its fixed parameter list, so there is no safe compile-time
     * reserve (the runtime-exact spill would overflow B and corrupt a neighbouring
     * coroutine's block). Reject cleanly; hoist the await to a statement:
     * "new v = await F(); printf(\"%d\", v);". A leaf await in a FIXED-ARITY call is
     * supported -- getcallargbound reserves the full parameter footprint, which
     * covers the trailing siblings wherever the await sits. */
    error(99);
    lexclr(TRUE);
    ldconst(0,sPRI);
    if (lval!=NULL)
      lval->ident=iEXPRESSION;
    return FALSE;
  } /* if */
  spillcount=(int)pc_exprtemp;
  /* Leaf await inside a FIXED-ARITY call-argument list: add the callee parameter
   * footprint (getcallargbound) to the reserve. A fixed-arity call pushes at most
   * its parameter count, so this bounds the reverse-emitted trailing siblings live
   * across the suspend regardless of where the await sits; pc_exprtemp alone would
   * miss them (it is captured before they are emitted). The runtime spill copy is
   * STK/FRM-bounded, so an over-count only over-reserves. (Variadic enclosing calls
   * are rejected above -- no safe bound.) */
  if (pc_generator && getcallnesting()>0)
    spillcount+=(int)getcallargbound();
  spillslot=0;
  if (pc_generator && spillcount>0) {
    spillslot=curfunc->genlocals;
    curfunc->genlocals=spillslot+spillcount+3;   /* 3 scratch (cnt/src/dst) + data */
  } /* if */
  /* NOTE: "await" inside a while/for/do loop is SUPPORTED. The completion detection
   * in doasyncresume tests PRI==generator_iterstop (the epilogue's sentinel), not
   * the old "B[0] unchanged" heuristic, so a body that loops back to the same
   * "await" re-suspends correctly instead of being mis-read as complete. Loop-
   * carried leaf AND composed awaits are covered by tests (async_loop*). Only
   * mid-expression temporaries across a suspend remain rejected (error 099 above). */

  /* Stage the whole await (both the ergonomic and general operand paths) so the
   * peephole optimizer's stgdel() works: without an active stage buffer, an
   * operand sub-expression of the form "<lifted local> OP <constant>" emits a
   * stray push (see doyield()'s note). doexpr() normally has staging on already
   * when it reaches here; this guard covers the rare non-staged caller. */
  localstaging=FALSE;
  if (!staging) {
    stgset(TRUE);
    localstaging=TRUE;
    assert(stgidx==0);
  } /* if */
  index=stgidx;

  /* Ergonomic "await asyncFn(args)": a single-token lookahead peeks the operand.
   * If it is a direct call to an "async" function, compose: start that inner
   * coroutine here (allocate + zero its state block, record its entry address,
   * link inner B[ASYNC_AWAITER_SLOT] = OUR B so its "return" resumes us), do the
   * FRESH call so it runs to its first suspend, then suspend the outer. Anything
   * that is not a direct async-function call is pushed back to the general
   * expression path below, so the normal await surface is untouched. */
  {
    char *fstr;
    cell ftval;
    int ftok;
    symbol *fsym;
    ftok=lex(&ftval,&fstr);
    fsym=(ftok==tSYMBOL) ? findglb(fstr,sGLOBAL) : NULL;
    /* Forward-referenced async callee: if, during an addressing pass, the operand
     * names something we cannot yet resolve as an async function (findglb==NULL,
     * or a function symbol whose "async" declaration has not been parsed yet so
     * uASYNC is not set), request ONE extra addressing pass. On the reparse the
     * callee symbol persists (reduce_referrers/delete_symbols keep non-native
     * functions across passes) with uASYNC, so this pass then emits the SAME
     * ergonomic path the write pass will -- no code_idx/label divergence. The
     * gate is monotonic (set at most once per compilation), so a genuinely
     * undefined or non-async operand does not loop: after that single pass it
     * falls through to the ordinary general-await/undefined-symbol path below.
     * A resolved non-function operand (a normal awaitable variable) can never
     * become async, so it is excluded and awaits normally without a reparse. */
    if (sc_status==statFIRST && !pc_async_fwd_reparsed && ftok==tSYMBOL
        && (fsym==NULL || (fsym->ident==iFUNCTN && (fsym->usage & uASYNC)==0))) {
      pc_async_fwd_reparsed=TRUE;
      sc_reparse=TRUE;
    } /* if */
    if (fsym!=NULL && fsym->ident==iFUNCTN && (fsym->usage & uASYNC)!=0) {
      cell argaddr[sMAXARGS],save_decl,btemp;
      int nuser,ai,blkcells,lbl_full;
      markusage(fsym,uREAD);
      pc_await_composed=1;      /* a composed await suspends here; a following LEAF
                                 * await in this expression that must carry this
                                 * result across its own suspend is rejected below */
      /* MID-EXPRESSION "await asyncFn()" (spillcount>0) IS supported: the compose
       * path allocates its hidden inner-start cells BELOW the enclosing operator
       * temporaries and FREES them before the suspend, so at the suspend point only
       * those operator temporaries are live -- identical to a leaf mid-expression
       * await. They are spilled into B before the suspend and restored after (see
       * the async_emit_spill/restore calls at the suspend below). spillslot was
       * reserved above from pc_exprtemp, the same upper bound the leaf path uses. */
      /* COMPOSED "await asyncFn()" inside a while/for/do loop IS supported. Each
       * iteration allocates the inner B from the arena and frees it when the inner
       * completes (return-to-awaiter), and the hidden inner-start cells are freed
       * within the iteration before the suspend, so the operand stack balances
       * across the loop back-edge. (This was once rejected with error 269 as a
       * conservative measure after the "spurious completion after one iteration"
       * miscompile; that root cause was the B[0]-unchanged completion heuristic,
       * fixed by testing PRI==generator_iterstop in doasyncresume -- which repairs
       * the composed loop as well as the leaf loop. Covered by async_loop_compose.) */
      save_decl=declared;
      /* MID-EXPRESSION compose: the enclosing operator temporaries (the "base" in
       * "base + await F()") sit on the operand stack BELOW the last "declared"
       * cell, but pushreg never bumped "declared" for them. The compose's hidden
       * inner-start cells are addressed FRM-relative as -declared*cell, which would
       * land ON those temporaries and corrupt them. So before any hidden allocation:
       * spill the temps into B (runtime-exact copy of [STK, boundary)), then raise
       * STK to the boundary to clear them off the operand stack. The compose then
       * runs on a clean stack exactly as in statement position, and async_emit_restore
       * (after the suspend) copies the temps back and reloads the inbox into PRI. The
       * discard is STK=boundary (runtime-exact), never a spillcount-based modstk, so
       * an over-counted reserve can never over-pop the frame. */
      if (spillcount>0) {
        async_emit_spill(spillslot);   /* B[spillslot..] = the live operand temps */
        async_emit_boundary();         /* PRI = FRM - baseline*cell (top of temps) */
        stgwrite("\tsctrl 4\n");       /* STK = boundary: drop the temps */
        code_idx+=opcodes(1)+opargs(1);
      } /* if */
      needtoken('(');
      /* user args: evaluate ONCE, buffer each in a hidden stack cell (as
       * doasyncstart does), so they can be pushed in reverse just before the
       * fresh call. */
      nuser=0;
      if (!matchtoken(')')) {
        do {
          int id;
          symbol *argsym=NULL;
          if (nuser>=sMAXARGS-1) { error(45); break; }
          id=expression(&val,NULL,&argsym,FALSE);
          if (id==iCONSTEXPR)
            ldconst(val,sPRI);
          async_check_array_arg(fsym,nuser,id,argsym);  /* array-arg size guard (see doasyncstart) */
          declared+=1;
          argaddr[nuser]=-declared*(cell)sizeof(cell);
          modstk(-(int)sizeof(cell));
          if (curfunc->x.stacksize<declared+1)
            curfunc->x.stacksize=declared+1;
          stgwrite("\tstor.s.pri ");
          outval(argaddr[nuser],TRUE);
          code_idx+=opcodes(1)+opargs(1);
          nuser++;
        } while (matchtoken(','));
        needtoken(')');
      } /* if */

      /* a hidden cell to hold the inner B across the fresh call */
      declared+=1;
      btemp=-declared*(cell)sizeof(cell);
      modstk(-(int)sizeof(cell));
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=declared+1;

      /* allocate the inner state block from the SAME fixed arena as top-level
       * starts (task 3 fix): B = Async_Alloc(blkcells); zero it; store its entry.
       * This replaces the old raw modheap, which leaked HEA on every composed
       * "await asyncFn()". On arena-full/too-big (B==-1) jump past the start. */
      blkcells=gen_reserved(fsym)+fsym->genlocals;
      lbl_full=getlabel();
      if (!async_emit_arena_alloc(fsym,btemp,blkcells,lbl_full)) {
        error(255,"await: Async_Alloc() not found -- #include <async>");
        lexclr(TRUE);
        ldconst(0,sPRI);
        if (lval!=NULL)
          lval->ident=iEXPRESSION;
        return FALSE;
      } /* if */

      /* inner B[ASYNC_AWAITER_SLOT] = OUR B: the inner "return" resumes us by this
       * B pointer directly (no registry lookup needed for chained awaits). */
      stgwrite("\tload.s.pri ");     /* PRI = inner B */
      outval(btemp,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tadd.c ");          /* PRI = &innerB[awaiter] */
      outval((cell)ASYNC_AWAITER_SLOT*sizeof(cell),TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tmove.alt\n");      /* ALT = &innerB[awaiter] (destination) */
      code_idx+=opcodes(1);
      stgwrite("\tload.s.pri ");     /* PRI = OUR B (localsbase cell) */
      outval(pc_genlocalsbase,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tstor.i\n");        /* innerB[awaiter] = our B */
      code_idx+=opcodes(1);

      /* FRESH call innerFn(inner B, args...): args pushed in reverse, B last */
      for (ai=nuser-1; ai>=0; ai--) {
        stgwrite("\tpush.s ");
        outval(argaddr[ai],TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* for */
      stgwrite("\tpush.s ");         /* inner B = arg0 */
      outval(btemp,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      pushval((cell)(nuser+1)*sizeof(cell));
      ffcall(fsym,NULL,nuser+1);
      /* B==-1 (arena full) lands here having started nothing; the outer still
       * suspends below and will observe a 0 inbox on resume. */
      setlabel(lbl_full);

      /* the inner ran to its first "await" and parked; free our hidden cells
       * BEFORE the suspend so the outer suspends at the generator baseline (the
       * resume rebuilds the frame with only the prologue cell). Any enclosing
       * operator temporaries were already spilled and cleared above. */
      if (declared>save_decl) {
        modstk((int)((declared-save_decl)*sizeof(cell)));
        declared=save_decl;
      } /* if */

      /* suspend the OUTER; on resume the inner's "return" has written our inbox
       * with its result, which async_emit_suspend()/restore delivers into PRI. */
      ldconst(0,sPRI);               /* yield value (ignored by our caller) */
      helper=generator_helper();
      async_emit_suspend(helper);
      if (spillcount>0)
        async_emit_restore(spillslot);  /* re-establish the temps; inbox -> PRI */
      if (localstaging) {
        stgout(index);
        stgset(FALSE);
      } /* if */
      if (lval!=NULL)
        lval->ident=iEXPRESSION;
      return FALSE;                  /* delivered value in PRI; not an lvalue */
    } /* if */
    lexpush();                       /* not a direct async call: back to expression() */
  }

  /* Reject a LEAF await that must carry a COMPOSED await's result across its own
   * suspend (compose-then-leaf in one expression, e.g. "await Inner() + await X()").
   * A composed await returns through the awaiter call.pri chain; a leaf suspend that
   * follows it, with that result live on the operand stack (spillcount>0), does not
   * unwind back to a resumable point, so the coroutine would hang. Every other
   * multi-await shape (leaf+leaf, compose+compose, leaf+compose) works. Reorder so
   * the leaf await comes first, or split into separate statements. */
  if (pc_generator && spillcount>0 && pc_await_composed) {
    error(99);
    lexclr(TRUE);
    ldconst(0,sPRI);
    if (localstaging) {
      stgout(index);
      stgset(FALSE);
    } /* if */
    if (lval!=NULL)
      lval->ident=iEXPRESSION;
    return FALSE;
  } /* if */

  errorset(sEXPRMARK,0);

  sym=NULL;
  ident=expression_unary(&val,&tag,&sym);   /* the awaitable -> token in PRI (UNARY
                                             * precedence: "await" binds tightly, so
                                             * "await A() + await B()" is two awaits) */
  if (ident==iARRAY || ident==iREFARRAY) {
    error(33,(sym!=NULL) ? sym->name : "-unknown-");
    ldconst(0,sPRI);
  } else if (ident==iCONSTEXPR) {
    ldconst(val,sPRI);
  } /* if */

  helper=generator_helper();
  if (spillcount>0)
    async_emit_spill(spillslot);     /* mid-expression: save live temps into B */
  async_emit_suspend(helper);
  if (spillcount>0)
    async_emit_restore(spillslot);   /* ...and bring them back (leaves inbox in PRI) */

  errorset(sEXPRRELEASE,0);
  if (localstaging) {
    stgout(index);
    stgset(FALSE);
  } /* if */
  if (lval!=NULL)
    lval->ident=iEXPRESSION;
  return FALSE;                     /* not an lvalue */
}

/*  doasyncstart - "__async_start(AsyncFunc, args...)" -> B (exp 012).
 *
 *  The scheduler-side START glue, in EXPRESSION position: it allocates and
 *  zero-fills a coroutine state block B on the heap (size gen_reserved +
 *  genlocals cells), records AsyncFunc's code entry address in B[ASYNC_ENTRY_SLOT]
 *  (so a later generic resume can dispatch to it via "call.pri"), does the FRESH
 *  call AsyncFunc(B, args...) -- which runs the body to its first "await", where
 *  it suspends back here -- and leaves B in PRI as the expression's value. The
 *  caller (the in-script registry's Async_Start) stores B in a token->B table.
 *
 *  Unlike the spike's statement form, no explicit handle variable is taken:
 *  returning B lets "new t = Async_Register(__async_start(Fn, args))" work, so
 *  the registry can be a normal in-script function. B is parked on the heap and
 *  is NOT freed on return (its lifetime is owned by the scheduler); a production
 *  version would fold this into the normal call path.
 *
 *  Restriction (v1): usable only as a bare/first sub-expression (e.g. the sole
 *  argument of a call), because the hidden arg-buffer cells it allocates are
 *  addressed off "declared" and would shift if live temporaries were already on
 *  the stack mid-expression. That is exactly how it is used via Async_Start.
 */
SC_FUNC int doasyncstart(value *lval)
{
  symbol *fsym;
  cell val,argaddr[sMAXARGS],btemp;
  char *str;
  int tok,nuser,ai,blkcells,lbl_full;
  cell save_decl;

  save_decl=declared;
  needtoken('(');
  tok=lex(&val,&str);           /* the async function */
  fsym=(tok==tSYMBOL) ? findglb(str,sGLOBAL) : NULL;
  if (fsym==NULL || fsym->ident!=iFUNCTN || (fsym->usage & uASYNC)==0) {
    /* Forward-referenced async callee (same two-pass hazard as ergonomic await):
     * if the operand is a name we cannot yet resolve as an async function during
     * an addressing pass, request ONE reparse instead of erroring, so a later
     * "async Fn(...)" declaration is resolvable on the next pass (its symbol
     * persists with uASYNC). The gate is monotonic, so a genuinely undefined or
     * non-async first argument falls through to the hard error on the reparse
     * (or immediately if the operand is not even a symbol) rather than looping.
     * error(255) is >=100 and thus NOT suppressed in an addressing pass, so it
     * must be skipped while we are only deferring to the reparse. */
    if (sc_status==statFIRST && !pc_async_fwd_reparsed && tok==tSYMBOL) {
      /* Consume the rest of the "__async_start(...)" argument list with balanced
       * parentheses -- NOT lexclr(TRUE), which clears to end-of-statement and
       * would swallow the closing ")" of the enclosing "__async_tok(...)" macro
       * wrapper, unbalancing it and provoking a FATAL cascade on this pass before
       * the reparse can run. We have already consumed "(" and the name token, so
       * the paren depth is 1; consume through its matching ")". The stub code is
       * discarded because sc_reparse re-runs this pass with Fn now resolvable. */
      int depth=1,t;
      pc_async_fwd_reparsed=TRUE;
      sc_reparse=TRUE;
      while (depth>0) {
        t=lex(&val,&str);
        if (t==0)                 /* EOF safety: never spin */
          break;
        if (t=='(')
          depth++;
        else if (t==')')
          depth--;
      } /* while */
    } else {
      error(255,"__async_start: first argument must name an \"async\" function");
      lexclr(TRUE);
    } /* if */
    ldconst(0,sPRI);
    if (lval!=NULL)
      lval->ident=iEXPRESSION;
    return FALSE;
  } /* if */
  markusage(fsym,uREAD);

  /* user args: evaluate ONCE, buffer each in a hidden stack cell (as doforeach
   * does), so they can be pushed in reverse just before the call. */
  nuser=0;
  if (matchtoken(',')) {
    do {
      int id;
      symbol *argsym=NULL;
      if (nuser>=sMAXARGS-1) { error(45); break; }
      id=expression(&val,NULL,&argsym,FALSE);
      if (id==iCONSTEXPR)
        ldconst(val,sPRI);
      /* array-argument size guard: an async array parameter is COPIED IN by the
       * prologue at its DECLARED length, so a smaller passed array would be
       * over-read. The ordinary call path reports 047 for this; mirror it here. */
      async_check_array_arg(fsym,nuser,id,argsym);
      declared+=1;
      argaddr[nuser]=-declared*(cell)sizeof(cell);
      modstk(-(int)sizeof(cell));
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=declared+1;
      stgwrite("\tstor.s.pri ");
      outval(argaddr[nuser],TRUE);
      code_idx+=opcodes(1)+opargs(1);
      nuser++;
    } while (matchtoken(','));
  } /* if */
  needtoken(')');

  /* a hidden cell to hold B across the fresh call (the call clobbers PRI/ALT,
   * and the fresh call's "await" returns its own token in PRI, not B) */
  declared+=1;
  btemp=-declared*(cell)sizeof(cell);
  modstk(-(int)sizeof(cell));
  if (curfunc->x.stacksize<declared+1)
    curfunc->x.stacksize=declared+1;

  /* Obtain the state block B from the fixed ARENA (task 3) instead of a raw,
   * never-freed "modheap": B = Async_Alloc(blkcells); zero it; store its entry.
   * The shared helper also guards the arena-full/too-big sentinel (B==-1) to
   * lbl_full. This is what makes B's lifetime reclaimable: the block is a slot
   * that Async_Free() returns to the pool on completion, so the LIFO heap cursor
   * is never asked to release a non-top block (which it cannot do safely). */
  blkcells=gen_reserved(fsym)+fsym->genlocals;
  lbl_full=getlabel();
  if (!async_emit_arena_alloc(fsym,btemp,blkcells,lbl_full)) {
    error(255,"__async_start: Async_Alloc() not found -- #include <async>");
    lexclr(TRUE);
    ldconst(0,sPRI);
    if (lval!=NULL)
      lval->ident=iEXPRESSION;
    return FALSE;
  } /* if */

  /* B[ASYNC_AWAITER_SLOT] = -1: this coroutine is TOP-LEVEL (started externally
   * by Async_Start), so it has no awaiter. The sentinel is -1, not the zero-fill
   * default of 0, because a real awaiter block can sit at data address 0 (the
   * arena may be the first global); "return" tests awaiter==-1 for "no awaiter". */
  stgwrite("\tload.s.pri ");    /* PRI = B */
  outval(btemp,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");         /* PRI = &B[awaiter] */
  outval((cell)ASYNC_AWAITER_SLOT*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tmove.alt\n");     /* ALT = &B[awaiter] (destination) */
  code_idx+=opcodes(1);
  ldconst(-1,sPRI);             /* PRI = -1 (no awaiter) */
  stgwrite("\tstor.i\n");       /* B[awaiter] = -1 */
  code_idx+=opcodes(1);

  /* call AsyncFunc(B, args...): user args pushed in reverse, B (arg0) pushed last */
  for (ai=nuser-1; ai>=0; ai--) {
    stgwrite("\tpush.s ");
    outval(argaddr[ai],TRUE);
    code_idx+=opcodes(1)+opargs(1);
  } /* for */
  stgwrite("\tpush.s ");        /* B = arg0 */
  outval(btemp,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  pushval((cell)(nuser+1)*sizeof(cell));
  ffcall(fsym,NULL,nuser+1);

  /* the fresh call suspended at the first "await" and returned its token in PRI;
   * reload B (0 on the arena-full path) so it becomes this expression's value */
  setlabel(lbl_full);
  stgwrite("\tload.s.pri ");
  outval(btemp,TRUE);
  code_idx+=opcodes(1)+opargs(1);

  /* free the hidden arg-buffer + B cells; the call already popped its own args */
  if (declared>save_decl) {
    modstk((int)((declared-save_decl)*sizeof(cell)));
    declared=save_decl;
  } /* if */
  if (lval!=NULL)
    lval->ident=iEXPRESSION;
  return FALSE;                 /* B is in PRI; not an lvalue */
}

/*  doasyncresume - "__async_resume(B, value)" -> completed? (exp 012).
 *
 *  The GENERIC scheduler-side RESUME glue, in EXPRESSION position: given a parked
 *  state block B (a scalar variable) and a value, it writes "value" into the
 *  inbox slot (B[ASYNC_INBOX_SLOT], where doawait() reads it) and resumes the
 *  coroutine by loading its entry address from B[ASYNC_ENTRY_SLOT] into PRI and
 *  calling through "call.pri" (opcode 50, indirect call). Because the entry
 *  address travels in B, one pump can resume ANY parked coroutine by token,
 *  regardless of which "async" function it belongs to -- there is no statically
 *  named "call". No new opcode is introduced.
 *
 *  It returns 1 if the coroutine COMPLETED on this resume (ran off its end), else
 *  0 (it suspended again at another "await"). Completion is detected without a
 *  VM change: @yield.emit rewrites B[0] to a fresh resume point on every suspend,
 *  so if B[0] is unchanged across the call the coroutine did not suspend -- it
 *  returned. The in-script registry uses this to deactivate the token so a later
 *  resume of a completed token is a safe no-op. (Limitation: a body that loops
 *  back to the SAME "await" reports completion; the linear MVP bodies do not.)
 */
SC_FUNC int doasyncresume(value *lval)
{
  symbol *hsym;
  cell val;
  char *str;
  int tok,lbl_done,lbl_end;

  needtoken('(');
  tok=lex(&val,&str);           /* the state block B (a scalar variable) */
  hsym=NULL;
  if (tok==tSYMBOL) {
    hsym=findloc(str);
    if (hsym==NULL)
      hsym=findglb(str,sGLOBAL);
  } /* if */
  if (hsym==NULL || hsym->ident!=iVARIABLE) {
    error(255,"__async_resume: first argument must be a scalar block variable");
    lexclr(TRUE);
    ldconst(0,sPRI);
    if (lval!=NULL)
      lval->ident=iEXPRESSION;
    return FALSE;
  } /* if */
  markusage(hsym,uREAD);
  needtoken(',');

  /* compute &B[inbox] and stash it, then evaluate the value, then store it there */
  if (hsym->vclass==sLOCAL)     /* PRI = B */
    stgwrite("\tload.s.pri ");
  else
    stgwrite("\tload.pri ");
  outval(hsym->addr,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");         /* PRI = B + inbox */
  outval((cell)ASYNC_INBOX_SLOT*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  pushreg(sPRI);                /* save &B[inbox] */
  {
    int id=expression(&val,NULL,NULL,FALSE);   /* value -> PRI */
    if (id==iCONSTEXPR)
      ldconst(val,sPRI);        /* a constant is not auto-loaded */
  }
  stgwrite("\tpop.alt\n");      /* ALT = &B[inbox] */
  code_idx+=opcodes(1);
  stgwrite("\tstor.i\n");       /* B[inbox] = value */
  code_idx+=opcodes(1);
  needtoken(')');

  /* generic resume: push B (arg0), then dispatch through B[ASYNC_ENTRY_SLOT]. */
  if (hsym->vclass==sLOCAL)
    stgwrite("\tpush.s ");      /* arg0 = B */
  else
    stgwrite("\tpush ");
  outval(hsym->addr,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  pushval((cell)sizeof(cell));  /* 1 argument */
  if (hsym->vclass==sLOCAL)
    stgwrite("\tload.s.pri ");  /* PRI = B */
  else
    stgwrite("\tload.pri ");
  outval(hsym->addr,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");         /* PRI = B + entry slot */
  outval((cell)ASYNC_ENTRY_SLOT*sizeof(cell),TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tload.i\n");       /* PRI = B[entry] = coroutine code address */
  code_idx+=opcodes(1);
  stgwrite("\tcall.pri\n");     /* indirect call (opcode 50): resume the coroutine */
  code_idx+=opcodes(1);

  /* completed? The coroutine leaves generator_iterstop in PRI when it runs off
   * its end (the epilogue loads that sentinel), and the awaited value (never the
   * sentinel) in PRI when it suspends again at another "await" (@yield.emit leaves
   * "val" in PRI). So PRI == generator_iterstop iff the coroutine completed. This
   * is robust for a body that LOOPS back to the same "await" -- the old heuristic
   * (B[0] unchanged across the call) mis-read that as completion, because
   * @yield.emit rewrites B[0] to the SAME resume point each iteration. PRI is live
   * straight out of call.pri, so the check reads it before any clobber. (The
   * sentinel also survives local DESTRUCTORS at the coroutine's "return":
   * destructsymbols() saves/restores PRI around the "~" operator calls -- sc1.c
   * ~7935/7980 -- exactly so a return value survives, and this sentinel rides that
   * same preservation.) */
  stgwrite("\tconst.alt ");     /* ALT = the completion sentinel */
  outval(generator_iterstop,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  lbl_done=getlabel();
  lbl_end=getlabel();
  stgwrite("\tjeq ");           /* PRI == generator_iterstop -> completed */
  outval(lbl_done,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  ldconst(0,sPRI);              /* still parked */
  stgwrite("\tjump ");
  outval(lbl_end,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  setlabel(lbl_done);
  ldconst(1,sPRI);              /* completed */
  setlabel(lbl_end);

  if (lval!=NULL)
    lval->ident=iEXPRESSION;
  return FALSE;                 /* completion flag is in PRI; not an lvalue */
}

/*  doasyncself - "__async_self()" -> current coroutine's state block B (exp 012).
 *
 *  In EXPRESSION position inside an "async" body, yields the running coroutine's
 *  own state block B in PRI. B lives in the prologue's localsbase cell
 *  (pc_genlocalsbase), so this is a single load of that cell. Its purpose is
 *  self-registration: a coroutine that "await"s an externally-completed value
 *  can hand its B to the in-script scheduler (Async_Register(__async_self())) so
 *  the event source can resume it by token. It takes no arguments. Using it
 *  outside an "async" function is an error (there is no coroutine B to name).
 */
SC_FUNC int doasyncself(value *lval)
{
  if (curfunc==NULL || (curfunc->usage & uASYNC)==0) {
    error(255,"\"__async_self\" is only valid inside an \"async\" function");
    ldconst(0,sPRI);
    if (lval!=NULL)
      lval->ident=iEXPRESSION;
    return FALSE;
  } /* if */
  needtoken('(');
  needtoken(')');
  /* PRI = B (the localsbase cell the prologue set to arg0 = the state block) */
  stgwrite("\tload.s.pri ");
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  if (lval!=NULL)
    lval->ident=iEXPRESSION;
  return FALSE;                 /* B is in PRI; not an lvalue */
}

/*  dumplits
 *
 *  Dump the literal pool (strings etc.)
 *
 *  Global references: litidx (referred to only)
 */
static void dumplits(void)
{
  int i,j;
  static const int row_len=16;

  if (sc_status==statSKIP)
    return;

  i=0;
  while (i<litidx) {
    /* should be in the data segment */
    assert(curseg==2);
    j=i+1;
    while (j<litidx && litq[j]==litq[i])
      j++;
    if (j-i>=row_len-1) {
      int count=j-i;
      defcompactstorage();
      outval(litq[i],FALSE);
      stgwrite(" ");
      outval(count,TRUE);
      i+=count;
    } else {
      defstorage();
      j=row_len;       /* 16 values per line */
      while (j && i<litidx){
        outval(litq[i],FALSE);
        stgwrite(" ");
        i++;
        j--;
        if (j==0 || i>=litidx)
          stgwrite("\n");         /* force a newline after 10 dumps */
        /* Note: stgwrite() buffers a line until it is complete. It recognizes
         * the end of line as a sequence of "\n\0", so something like "\n\t"
         * so should not be passed to stgwrite().
         */
      } /* while */
    } /* if */
  } /* while */
}

/*  dumpzero
 *
 *  Dump zero's for default initial values
 */
static void dumpzero(int count)
{
  if (sc_status==statSKIP || count<=0)
    return;
  assert(curseg==2);
  defcompactstorage();
  outval(0, FALSE);
  stgwrite(" ");
  outval(count, TRUE);
}

static void aligndata(int numbytes)
{
  assert(numbytes % sizeof(cell) == 0);   /* alignment must be a multiple of
                                           * the cell size */
  assert(numbytes!=0);

  if ((((glb_declared+litidx)*sizeof(cell)) % numbytes)!=0) {
    while ((((glb_declared+litidx)*sizeof(cell)) % numbytes)!=0)
      litadd(0);
  } /* if */

}

#if !defined SC_LIGHT
/* sc_attachdocumentation()
 * appends documentation comments to the passed-in symbol, or to a global
 * string if "sym" is NULL.
 */
void sc_attachdocumentation(symbol *sym)
{
  int line;
  size_t length;
  char *str,*doc;

  if (!sc_makereport || sc_status!=statFIRST || sc_parsenum>0) {
    /* just clear the entire table */
    delete_docstringtable();
    return;
  } /* if */
  /* in the case of state functions, multiple documentation sections may
   * appear; we should concatenate these
   * (with forward declarations, this is also already the case, so the assertion
   * below is invalid)
   */
  // assert(sym==NULL || sym->documentation==NULL || sym->states!=NULL);

  /* first check the size */
  length=0;
  for (line=0; (str=get_docstring(line))!=NULL && *str!=sDOCSEP; line++) {
    if (str[0]!='\0') {
      if (length>0)
        length++;   /* count 1 extra for a separating space */
      length+=strlen(str);
    }
  } /* for */
  if (length>0) {
    if (sym==NULL && sc_documentation!=NULL) {
      length += strlen(sc_documentation) + 1 + 4; /* plus 4 for "<p/>" */
      assert(length > strlen(sc_documentation));
    } else if (sym!=NULL && sym->documentation!=NULL) {
      length+=strlen(sym->documentation) + 1 + 4;/* plus 4 for "<p/>" */
      assert(length > strlen(sym->documentation));
    } /* if */

    /* allocate memory for the documentation */
    doc=(char*)malloc((length+1)*sizeof(char));
    if (doc!=NULL) {
      /* initialize string or concatenate */
      if (sym==NULL && sc_documentation!=NULL) {
        strcpy(doc,sc_documentation);
        strcat(doc,"<p/>");
        free(sc_documentation);
        sc_documentation=NULL;
      } else if (sym!=NULL && sym->documentation!=NULL) {
        strcpy(doc,sym->documentation);
        strcat(doc,"<p/>");
        free(sym->documentation);
        sym->documentation=NULL;
      } else {
        doc[0]='\0';
      } /* if */
      /* collect all documentation */
      while ((str=get_docstring(0))!=NULL && *str!=sDOCSEP) {
        if (str[0]!='\0') {
          if (doc[0]!='\0')
            strcat(doc," ");
          strcat(doc,str);
        }
        delete_docstring(0);
      } /* while */
      if (str!=NULL) {
        /* also delete the separator */
        assert(*str==sDOCSEP);
        delete_docstring(0);
      } /* if */
      if (sym==NULL) {
        assert(sc_documentation==NULL);
        sc_documentation=doc;
      } else {
        assert(sym->documentation==NULL);
        sym->documentation=doc;
      } /* if */
    } /* if */
  } else {
    /* delete an empty separator, if present */
    if ((str=get_docstring(0))!=NULL && *str==sDOCSEP)
      delete_docstring(0);
  } /* if */
}

static void insert_docstring_separator(void)
{
  char sep[2]={sDOCSEP,'\0'};
  insert_docstring(sep);
}
#else
  #define sc_attachdocumentation(s)      (void)(s)
  #define insert_docstring_separator()
#endif

static void declfuncvar(int fpublic,int fstatic,int fstock,int fconst)
{
  char name[sNAMEMAX+11];
  int tok,tag;
  char *str;
  cell val;
  int invalidfunc;

  tag=pc_addtag(NULL);
  tok=lex(&val,&str);
  /* if we arrived here, this may not be a declaration of a native function
   * or variable
   */
  if (tok==tNATIVE) {
    error(42);          /* invalid combination of class specifiers */
    return;
  } /* if */

  if (tok==t__PRAGMA) {
    dopragma();
    tok=lex(&val,&str);
  } /* if */

  if (tok!=tSYMBOL && tok!=tOPERATOR) {
    lexpush();
    needtoken(tSYMBOL);
    lexclr(TRUE);       /* drop the rest of the line */
    litidx=0;           /* drop the literal queue too */
    return;
  } /* if */
  if (tok==tOPERATOR) {
    lexpush();          /* push "operator" keyword back (for later analysis) */
    if (!newfunc(NULL,tag,fpublic,fstatic,fstock)) {
      error(10);        /* illegal function or declaration */
      lexclr(TRUE);     /* drop the rest of the line */
      litidx=0;         /* drop the literal queue too */
    } /* if */
  } else {
    /* so tok is tSYMBOL */
    assert(strlen(str)<=sNAMEMAX);
    strcpy(name,str);
    /* only variables can be "const" or both "public" and "stock" */
    invalidfunc= fconst || (fpublic && fstock);
    if (invalidfunc || !newfunc(name,tag,fpublic,fstatic,fstock)) {
      /* if not a function, try a global variable */
      declglb(name,tag,fpublic,fstatic,fstock,fconst);
    } /* if */
  } /* if */
}

/*  declglb     - declare global symbols
 *
 *  Declare a static (global) variable. Global variables are stored in
 *  the DATA segment.
 *
 *  global references: glb_declared     (altered)
 */
static void declglb(char *firstname,int firsttag,int fpublic,int fstatic,int fstock,int fconst)
{
  int ident,tag,ispublic;
  int idxtag[sDIMEN_MAX];
  char name[sNAMEMAX+1];
  cell val,size,cidx;
  ucell address;
  int glb_incr;
  char *str;
  int dim[sDIMEN_MAX];
  int numdim;
  int explicit_init;
  short filenum;
  symbol *sym;
  constvalue_root *enumroot=NULL;
  #if !defined NDEBUG
    cell glbdecl=0;
  #endif

  assert(!fpublic || !fstatic);         /* may not both be set */
  insert_docstring_separator();         /* see comment in newfunc() */
  filenum=fcurrent;                     /* save file number at the start of the declaration */
  do {
    size=1;                             /* single size (no array) */
    numdim=0;                           /* no dimensions */
    ident=iVARIABLE;
    if (firstname!=NULL) {
      assert(strlen(firstname)<=sNAMEMAX);
      strcpy(name,firstname);           /* save symbol name */
      tag=firsttag;
      firstname=NULL;
    } else {
      tag=pc_addtag(NULL);
      if (matchtoken(t__PRAGMA))
        dopragma();
      if (lex(&val,&str)!=tSYMBOL)      /* read in (new) token */
        error_suggest(20,str,NULL,estSYMBOL,esfFUNCTION);   /* invalid symbol name */
      assert(strlen(str)<=sNAMEMAX);
      strcpy(name,str);                 /* save symbol name */
    } /* if */
    ispublic=fpublic;
    if (name[0]==PUBLIC_CHAR) {
      ispublic=TRUE;                    /* implicitly public variable */
      assert(!fstatic);
    } /* if */
    while (matchtoken('[')) {
      ident=iARRAY;
      if (numdim == sDIMEN_MAX) {
        error(53);                      /* exceeding maximum number of dimensions */
        return;
      } /* if */
      size=needsub(&idxtag[numdim],&enumroot);  /* get size; size==0 for "var[]" */
      #if INT_MAX < LONG_MAX
        if (size > INT_MAX)
          error(105);                   /* overflow, exceeding capacity */
      #endif
      if (ispublic)
        error(56,name);                 /* arrays cannot be public */
      dim[numdim++]=(int)size;
    } /* while */
    assert(sc_curstates==0);
    sc_curstates=getstates(name);
    if (sc_curstates<0) {
      error(85,name);           /* empty state list on declaration */
      sc_curstates=0;
    } else if (sc_curstates>0 && ispublic) {
      error(88,name);           /* public variables may not have states */
      sc_curstates=0;
    } /* if */
    sym=findconst(name,NULL);
    if (sym==NULL) {
      sym=findglb(name,sSTATEVAR);
      /* if a global variable without states is found and this declaration has
       * states, the declaration is okay
       */
      if (sym!=NULL && sym->states==NULL && sc_curstates>0)
        sym=NULL;               /* set to NULL, we found the global variable */
      if (sc_curstates>0 && findglb(name,sGLOBAL)!=NULL)
        error(233,name);        /* state variable shadows a global variable */
    } /* if */
    /* we have either:
     * a) not found a matching variable (or rejected it, because it was a shadow)
     * b) found a global variable and we were looking for that global variable
     * c) found a state variable in the automaton that we were looking for
     */
    assert(sym==NULL
           || (sym->states==NULL && sc_curstates==0)
           || (sym->states!=NULL && sym->states->first!=NULL && sym->states->first->index==sc_curstates));
    /* a state variable may only have a single id in its list (so either this
     * variable has no states, or it has a single list)
     */
    assert(sym==NULL || sym->states==NULL || sym->states->first->next==NULL);
    /* it is okay for the (global) variable to exist, as long as it belongs to
     * a different automaton
     */
    if (sym!=NULL && (sym->usage & uDEFINE)!=0)
      error(21,name);                   /* symbol already defined */
    /* if this variable is never used (which can be detected only in the
     * second stage), shut off code generation
     */
    cidx=0;             /* only to avoid a compiler warning */
    if (sc_status==statWRITE && sym!=NULL && (sym->usage & (uREAD | uWRITTEN | uPUBLIC))==0) {
      sc_status=statSKIP;
      cidx=code_idx;
      #if !defined NDEBUG
        glbdecl=glb_declared;
      #endif
    } /* if */
    begdseg();          /* real (initialized) data in data segment */
    assert(litidx==0);  /* literal queue should be empty */
    if (sc_alignnext) {
      litidx=0;
      aligndata(sc_dataalign);
      dumplits();       /* dump the literal queue */
      sc_alignnext=FALSE;
      litidx=0;         /* global initial data is dumped, so restart at zero */
    } /* if */
    assert(litidx==0);  /* literal queue should be empty (again) */
    initials(ident,tag,&size,dim,numdim,enumroot,&explicit_init);/* stores values in the literal queue */
    assert(size>=litidx);
    if (numdim==1)
      dim[0]=(int)size;
    /* before dumping the initial values (or zeros) check whether this variable
     * overlaps another
     */
    if (sc_curstates>0) {
      unsigned char *map;

      if (litidx!=0)
        error(89,name); /* state variables may not be initialized */
      /* find an appropriate address for the state variable */
      /* assume that it cannot be found */
      address=sizeof(cell)*glb_declared;
      glb_incr=(int)size;
      /* use a memory map in which every cell occupies one bit */
      if (glb_declared>0 && (map=(unsigned char*)malloc((glb_declared+7)/8))!=NULL) {
        int fsa=state_getfsa(sc_curstates);
        symbol *sweep;
        cell sweepsize,addr;
        memset(map,0,(glb_declared+7)/8);
        assert(fsa>=0);
        /* fill in all variables belonging to this automaton */
        for (sweep=glbtab.next; sweep!=NULL; sweep=sweep->next) {
          if (sweep->parent!=NULL || sweep->states==NULL || sweep==sym)
            continue;   /* hierarchical type, or no states, or same as this variable */
          if (sweep->ident!=iVARIABLE && sweep->ident!=iARRAY)
            continue;   /* a function or a constant */
          if ((sweep->usage & uDEFINE)==0)
            continue;   /* undefined variable, ignore */
          if (fsa!=state_getfsa(sweep->states->first->index))
            continue;   /* wrong automaton */
          /* when arrived here, this is a global variable, with states and
           * belonging to the same automaton as the variable we are declaring
           */
          sweepsize=(sweep->ident==iVARIABLE) ? 1 : array_totalsize(sweep);
          assert(sweep->addr % sizeof(cell) == 0);
          addr=sweep->addr/sizeof(cell);
          /* mark this address range */
          while (sweepsize-->0) {
            map[addr/8] |= (unsigned char)(1 << (addr % 8));
            addr++;
          } /* while */
        } /* for */
        /* go over it again, clearing any ranges that have conflicts */
        for (sweep=glbtab.next; sweep!=NULL; sweep=sweep->next) {
          if (sweep->parent!=NULL || sweep->states==NULL || sweep==sym)
            continue;   /* hierarchical type, or no states, or same as this variable */
          if (sweep->ident!=iVARIABLE && sweep->ident!=iARRAY)
            continue;   /* a function or a constant */
          if ((sweep->usage & uDEFINE)==0)
            continue;   /* undefined variable, ignore */
          if (fsa!=state_getfsa(sweep->states->first->index))
            continue;   /* wrong automaton */
          /* when arrived here, this is a global variable, with states and
           * belonging to the same automaton as the variable we are declaring
           */
          /* if the lists of states of the existing variable and the new
           * variable have a non-empty intersection, this is not a suitable
           * overlap point -> wipe the address range
           */
          if (state_conflict_id(sc_curstates,sweep->states->first->index)) {
            sweepsize=(sweep->ident==iVARIABLE) ? 1 : array_totalsize(sweep);
            assert(sweep->addr % sizeof(cell) == 0);
            addr=sweep->addr/sizeof(cell);
            /* mark this address range */
            while (sweepsize-->0) {
              map[addr/8] &= (unsigned char)(~(1 << (addr % 8)));
              addr++;
            } /* while */
          } /* if */
        } /* for */
        /* now walk through the map and find a starting point that is big enough */
        sweepsize=0;
        for (addr=0; addr<glb_declared; addr++) {
          if ((map[addr/8] & (1 << (addr % 8)))==0)
            continue;
          for (sweepsize=addr+1; sweepsize<glb_declared; sweepsize++) {
            if ((map[sweepsize/8] & (1 << (sweepsize % 8)))==0)
              break;    /* zero bit found, skip this range */
            if (sweepsize-addr>=size)
              break;    /* fitting range found, no need to search further */
          } /* for */
          if (sweepsize-addr>=size)
            break;      /* fitting range found, no need to search further */
          addr=sweepsize;
        } /* for */
        free(map);
        if (sweepsize-addr>=size) {
          address=sizeof(cell)*addr;    /* fitting range found, set it */
          glb_incr=0;
        } /* if */
      } /* if */
    } else {
      address=sizeof(cell)*glb_declared;
      glb_incr=(int)size;
    } /* if */
    if (address==sizeof(cell)*glb_declared) {
      dumplits();       /* dump the literal queue */
      dumpzero((int)size-litidx);
    } /* if */
    litidx=0;
    if (sym==NULL) {    /* define only if not yet defined */
      sym=addvariable(name,address,ident,sGLOBAL,tag,dim,numdim,idxtag,0);
      if (sc_curstates>0)
        attachstatelist(sym,sc_curstates);
    } else {            /* if declared but not yet defined, adjust the variable's address */
      assert(sym->states==NULL && sc_curstates==0
             || sym->states->first!=NULL && sym->states->first->index==sc_curstates && sym->states->first->next==NULL);
      sym->addr=address;
      sym->codeaddr=code_idx;
      sym->usage|=uDEFINE;
    } /* if */
    assert(sym!=NULL);
    sc_curstates=0;
    if (ispublic)
      sym->usage|=uPUBLIC;
    if (fconst) {
      symbol *cur=sym;
      do {
        cur->usage|=uCONST;
      } while ((cur=cur->child)!=NULL);
    } /* if */
    if (fstock)
      sym->usage|=uSTOCK;
    if (fstatic)
      sym->fnumber=filenum;
    if (explicit_init)
      markinitialized(sym,TRUE,FALSE);
    sc_attachdocumentation(sym);/* attach any documentation to the variable */
    if (sc_status==statSKIP) {
      sc_status=statWRITE;
      code_idx=cidx;
      assert(glb_declared==glbdecl);
    } else {
      glb_declared+=glb_incr;   /* add total number of cells (if added to the end) */
    } /* if */
    if (matchtoken(t__PRAGMA))
      dopragma();
    pragma_apply(sym);
  } while (matchtoken(',')); /* enddo */   /* more? */
  needtoken(tTERM);    /* if not comma, must be semicolon */
}

/*  declloc     - declare local symbols
 *
 *  Declare local (automatic) variables. Since these variables are relative
 *  to the STACK, there is no switch to the DATA segment. These variables
 *  cannot be initialized either.
 *
 *  global references: declared   (altered)
 *                     funcstatus (referred to only)
 */
static int declloc(int fstatic)
{
  int ident,tag;
  int idxtag[sDIMEN_MAX];
  char name[sNAMEMAX+1];
  symbol *sym;
  constvalue_root *enumroot=NULL;
  cell val,size;
  char *str;
  value lval = {0};
  int cur_lit=0;
  int dim[sDIMEN_MAX];
  int numdim;
  int fconst;
  int staging_start;
  int explicit_init;    /* is the variable explicitly initialized? */
  int suppress_w240=FALSE;

  fconst=matchtoken(tCONST);
  do {
    ident=iVARIABLE;
    size=1;
    numdim=0;                           /* no dimensions */
    if (matchtoken(t__PRAGMA))
      dopragma();
    tag=pc_addtag(NULL);
    if (!needtoken(tSYMBOL)) {
      lexclr(TRUE);                     /* drop the rest of the line... */
      return 0;                         /* ...and quit */
    } /* if */
    tokeninfo(&val,&str);
    assert(strlen(str)<=sNAMEMAX);
    strcpy(name,str);                   /* save symbol name */
    if (name[0]==PUBLIC_CHAR)
      error(56,name);                   /* local variables cannot be public */
    /* Note: block locals may be named identical to locals at higher
     * compound blocks (as with standard C); so we must check (and add)
     * the "nesting level" of local variables to verify the
     * multi-definition of symbols.
     */
    if ((sym=findloc(name))!=NULL && sym->compound==pc_nestlevel)
      error(21,name);                   /* symbol already defined */
    /* Although valid, a local variable whose name is equal to that
     * of a global variable or to that of a local variable at a lower
     * level might indicate a bug.
     */
    if (((sym=findloc(name))!=NULL && sym->compound!=pc_nestlevel) || findglb(name,sGLOBAL)!=NULL)
      error(219,name);                  /* variable shadows another symbol */
    while (matchtoken('[')){
      ident=iARRAY;
      if (numdim == sDIMEN_MAX) {
        error(53);                      /* exceeding maximum number of dimensions */
        return ident;
      } /* if */
      size=needsub(&idxtag[numdim],&enumroot); /* get size; size==0 for "var[]" */
      #if INT_MAX < LONG_MAX
        if (size > INT_MAX)
          error(105);                   /* overflow, exceeding capacity */
      #endif
      dim[numdim++]=(int)size;
    } /* while */
    if (getstates(name))
      error(88,name);           /* local variables may not have states */
    if (ident==iARRAY || fstatic) {
      if (sc_alignnext) {
        aligndata(sc_dataalign);
        sc_alignnext=FALSE;
      } /* if */
      cur_lit=litidx;           /* save current index in the literal table */
      initials(ident,tag,&size,dim,numdim,enumroot,&explicit_init);
      if (size==0)
        return ident;           /* error message already given */
      if (numdim==1)
        dim[0]=(int)size;
    } /* if */
    /* reserve memory (on the stack) for the variable */
    if (fstatic) {
      /* write zeros for uninitialized fields */
      while (litidx<cur_lit+size)
        litadd(0);
      sym=addvariable(name,(cur_lit+glb_declared)*sizeof(cell),ident,sSTATIC,
                      tag,dim,numdim,idxtag,pc_nestlevel);
    } else if (pc_generator && ident==iVARIABLE) {
      /* a scalar local of a coroutine generator is "lifted" into the state
       * block instead of being allocated on the stack, so its value survives
       * across a "yield"/"await" suspend (the stack frame is discarded on
       * suspend and rebuilt on resume). It gets the next free block slot; "addr"
       * is the slot's byte offset within the block. The head slots (B[0..], see
       * gen_reserved()) precede the lifted slots: slot 0 is B[1] for a "yield"
       * generator, B[4] for an "async" coroutine (gen_reserved==4). "declared"/the stack are left
       * untouched, so the frame stays balanced across the suspend. Access is
       * emitted against the hidden "localsbase" cell (see rvalue()/store() in
       * sc4.c). */
      int slot=curfunc->genlocals;
      sym=addvariable(name,(slot+gen_reserved(curfunc))*sizeof(cell),ident,sLOCAL,
                      tag,dim,numdim,idxtag,pc_nestlevel);
      sym->usage|=uLIFTED;
      curfunc->genlocals=slot+1;
      /* the initializer still stores through the lifted path below; open the
       * staging buffer for it exactly as the on-stack case does */
      assert(!staging);
      stgset(TRUE);
      assert(stgidx==0);
      staging_start=stgidx;
    } else if (pc_generator && ident==iARRAY) {
      /* A fixed-size array/string local of ANY coroutine generator ("yield" or
       * "async") is LIFTED into the state block, exactly like a scalar local --
       * it just occupies "size" consecutive block cells instead of one. Because
       * the block B is a stable arena slot in the data segment, the array's
       * cells LIVE there permanently: they need NO save/restore across a suspend
       * (the discarded stack frame never held them), so no stack snapshot is
       * required. address() in sc4.c computes a lifted symbol's base as B+addr,
       * an absolute data address, so indexing/passing/fill/copy all work
       * unchanged; only the initializer path (fillarray/copyarray) is taught the
       * lifted base below. "declared"/the stack are untouched, so the frame stays
       * at the generator baseline and the "live stack storage across a suspend"
       * guard is happy.
       *
       * This was gated to "async" only; extending it to "yield" generators
       * (2026-09-29) lifts the old error 096 ("a local array cannot span a
       * yield") -- the block model is identical for the two (only gen_reserved()
       * differs), so the same lift works. Verified for 1-D, multi-dimensional,
       * and large arrays.
       *
       * MULTI-DIMENSIONAL arrays lift too: "size" is the FULL flattened cell
       * count (indirection vector + data), so reserving "size" block cells
       * covers the whole array. The indirection vector holds byte offsets
       * RELATIVE TO THE ARRAY BASE (adjust_indirectiontables()) -- position
       * independent -- and is built into the block by the same copyarray() path
       * as the data, so indexing grid[i][j] follows with no further work. */
      int slot=curfunc->genlocals;
      sym=addvariable(name,(slot+gen_reserved(curfunc))*sizeof(cell),ident,sLOCAL,
                      tag,dim,numdim,idxtag,pc_nestlevel);
      sym->usage|=uLIFTED;
      curfunc->genlocals=slot+(int)size;   /* reserve "size" block cells (flattened) */
      /* fall through to the array initializer path (fillarray/copyarray), which
       * now emits against the lifted base; do NOT allocate on the stack. */
    } else {
      declared+=(int)size;      /* variables are put on stack, adjust "declared" */
      sym=addvariable(name,-declared*sizeof(cell),ident,sLOCAL,
                      tag,dim,numdim,idxtag,pc_nestlevel);
      if (ident==iVARIABLE) {
        assert(!staging);
        stgset(TRUE);           /* start stage-buffering */
        assert(stgidx==0);
        staging_start=stgidx;
      } /* if */
      markexpr(sLDECL,name,-declared*sizeof(cell)); /* mark for better optimization */
      modstk(-(int)size*sizeof(cell));
      assert(curfunc!=NULL);
      assert((curfunc->usage & uNATIVE)==0);
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=declared+1;  /* +1 for PROC opcode */
    } /* if */
    /* now that we have reserved memory for the variable, we can proceed
     * to initialize it */
    assert(sym!=NULL);          /* we declared it, it must be there */
    if (fconst) {
      symbol *cur=sym;
      do {
        cur->usage|=uCONST;
      } while ((cur=cur->child)!=NULL);
    } /* if */
    if (!fstatic) {             /* static variables already initialized */
      if (ident==iVARIABLE) {
        /* simple variable, also supports initialization */
        int ctag = tag;         /* set to "tag" by default */
        explicit_init=FALSE;
        if (matchtoken('=')) {
          int initexpr_ident;
          sym->usage &= ~uDEFINE;   /* temporarily mark the variable as undefined to prevent
                                     * possible self-assignment through its initialization expression */
          initexpr_ident=doexpr(FALSE,FALSE,FALSE,FALSE,&ctag,NULL,TRUE,&val);
          sym->usage |= uDEFINE;
          explicit_init=TRUE;
          suppress_w240=(initexpr_ident==iCONSTEXPR && val==0);
        } else {
          ldconst(0,sPRI);      /* uninitialized variable, set to zero */
        } /* if */
        /* now try to save the value (still in PRI) in the variable */
        lval.sym=sym;
        lval.ident=iVARIABLE;
        lval.constval=0;
        lval.tag=tag;
        suppress_w240 |= check_userop(NULL,ctag,lval.tag,2,NULL,&ctag);
        store(&lval);
        markexpr(sEXPR,NULL,0); /* full expression ends after the store */
        assert(staging);        /* end staging phase (optimize expression) */
        stgout(staging_start);
        stgset(FALSE);
        check_tagmismatch(tag,ctag,TRUE,-1);
        /* if the variable was not explicitly initialized, reset the
         * "uWRITTEN" flag that store() set */
        if (!explicit_init)
          sym->usage &= ~uWRITTEN;
      } else {
        /* an array */
        assert(cur_lit>=0 && cur_lit<=litidx && litidx<=litmax);
        assert(size>0 && size>=sym->dim.array.length);
        assert(numdim>1 || size==sym->dim.array.length);
        /* final literal values that are zero make no sense to put in the literal
         * pool, because values get zero-initialized anyway; we check for this,
         * because users often explicitly initialize strings to ""
         */
        while (litidx>cur_lit && litq[litidx-1]==0)
          litidx--;
        /* if the array is not completely filled, set all values to zero first */
        if (litidx-cur_lit<size && (ucell)size<CELL_MAX)
          fillarray(sym,size*sizeof(cell),0);
        if (cur_lit<litidx) {
          /* check whether the complete array is set to a single value; if
           * it is, more compact code can be generated */
          cell first=litq[cur_lit];
          int i;
          for (i=cur_lit; i<litidx && litq[i]==first; i++)
            /* nothing */;
          if (i==litidx) {
            /* all values are the same */
            fillarray(sym,(litidx-cur_lit)*sizeof(cell),first);
            litidx=cur_lit;     /* reset literal table */
          } else {
            /* copy the literals to the array */
            ldconst((cur_lit+glb_declared)*sizeof(cell),sPRI);
            copyarray(sym,(litidx-cur_lit)*sizeof(cell));
          } /* if */
        } /* if */
      } /* if */
    } /* if */
    if (explicit_init)
      markinitialized(sym,!suppress_w240,FALSE);
    if (pc_ovlassignment)
      sym->usage |= uREAD;
    if (matchtoken(t__PRAGMA))
      dopragma();
    pragma_apply(sym);
  } while (matchtoken(',')); /* enddo */   /* more? */
  needtoken(tTERM);    /* if not comma, must be semicolon */
  return ident;
}

/* this function returns the maximum value for a cell in case of an error
 * (invalid dimension).
 */
static cell calc_arraysize(int dim[],int numdim,int cur)
{
  cell subsize;
  ucell newsize;

  /* the return value is in cells, not bytes */
  assert(cur>=0 && cur<=numdim);
  if (cur==numdim)
    return 0;
  subsize=calc_arraysize(dim,numdim,cur+1);
  newsize=dim[cur]+dim[cur]*subsize;
  if (newsize==0)
    return 0;
  if ((ucell)subsize>=CELL_MAX || newsize>=CELL_MAX || newsize<(ucell)subsize
      || newsize*sizeof(cell)>=CELL_MAX)
    return CELL_MAX;
  return newsize;
}

static void adjust_indirectiontables(int dim[],int numdim,int startlit,
                                     constvalue_root *lastdim,int *skipdim)
{
static int base;
  int cur;
  int i,d;
  cell accum;
  cell size;

  assert(startlit==-1 || (startlit>=0 && startlit<=litidx));
  base=startlit;
  size=1;
  for (cur=0; cur<numdim-1; cur++) {
    /* 2 or more dimensions left, fill in an indirection vector */
    if (dim[cur+1]>0) {
      for (i=0; i<size; i++)
        for (d=0; d<dim[cur]; d++)
          litq[base++]=(size*dim[cur]+(dim[cur+1]-1)*(dim[cur]*i+d)) * sizeof(cell);
    } else {
      /* final dimension is variable length */
      constvalue *ld;
      assert(dim[cur+1]==0);
      assert(lastdim!=NULL);
      assert(skipdim!=NULL);
      accum=0;
      for (i=0; i<size; i++) {
        /* skip the final dimension sizes for all earlier major dimensions */
        for (d=0,ld=lastdim->first; d<*skipdim; d++,ld=ld->next) {
          assert(ld!=NULL);
        } /* for */
        for (d=0; d<dim[cur]; d++) {
          assert(ld!=NULL);
          assert(strtol(ld->name,NULL,16)==d);
          litq[base++]=(size*dim[cur]+accum) * sizeof(cell);
          accum+=ld->value-1;
          *skipdim+=1;
          ld=ld->next;
        } /* for */
      } /* for */
    } /* if */
    size*=dim[cur];
  } /* for */
}

/*  initials
 *
 *  Initialize global objects and local arrays.
 *    size==array cells (count), if 0 on input, the routine counts the number of elements
 *    tag==required tagname id (not the returned tag)
 *
 *  Global references: litidx (altered)
 */
static void initials(int ident,int tag,cell *size,int dim[],int numdim,
                     constvalue_root *enumroot,int *explicit_init)
{
  int ctag;
  cell tablesize;
  int curlit=litidx;
  int err=0;
  int i;

  if (explicit_init!=NULL)
    *explicit_init=FALSE;
  if (!matchtoken('=')) {
    assert(ident!=iARRAY || numdim>0);
    if (ident==iARRAY) {
      assert(numdim>0 && numdim<=sDIMEN_MAX);
      for (i=0; i<numdim; i++) {
        if (dim[i]==0) {
          /* declared like "myvar[];" which is senseless (note: this *does* make
           * sense in the case of a iREFARRAY, which is a function parameter)
           */
          error(9); /* array has zero length -> invalid size */
          return;
        } /* if */
      } /* for */
      *size=calc_arraysize(dim,numdim,0);
      if (*size==(cell)CELL_MAX) {
        error(9);       /* array is too big -> invalid size */
        return;
      } /* if */
      /* first reserve space for the indirection vectors of the array, then
       * adjust it to contain the proper values
       * (do not use dumpzero(), as it bypasses the literal queue)
       */
      for (tablesize=calc_arraysize(dim,numdim-1,0); tablesize>0; tablesize--)
        litadd(0);
      if (dim[numdim-1]!=0)     /* error 9 has already been given */
        adjust_indirectiontables(dim,numdim,curlit,NULL,NULL);
    } /* if */
    return;
  } /* if */

  if (explicit_init!=NULL)
    *explicit_init=TRUE;
  if (ident==iVARIABLE) {
    assert(*size==1);
    init(ident,&ctag,NULL);
    check_tagmismatch(tag,ctag,TRUE,-1);
  } else {
    assert(numdim>0);
    if (numdim==1) {
      *size=initvector(ident,tag,dim[0],litidx,FALSE,enumroot,NULL);
    } else {
      int errorfound=FALSE;
      int counteddim[sDIMEN_MAX];
      int idx;
      constvalue_root lastdim = { NULL, NULL};     /* sizes of the final dimension */
      int skipdim=0;

      /* check if size specified for all dimensions */
      for (idx=0; idx<numdim; idx++)
        if (dim[idx]==0)
          break;
      /* already reserve space for the indirection tables (for an array with
       * known dimensions)
       * (do not use dumpzero(), as it bypasses the literal queue)
       */
      if(idx==numdim)
        *size=calc_arraysize(dim,numdim,0);
      else
        *size=0; /* size of one or more dimensions is unknown */
      for (tablesize=calc_arraysize(dim,numdim-1,0); tablesize>0; tablesize--)
        litadd(0);
      /* now initialize the sub-arrays */
      memset(counteddim,0,sizeof counteddim);
      initarray(ident,tag,dim,numdim,0,curlit,counteddim,&lastdim,enumroot,&errorfound);
      /* check the specified array dimensions with the initialler counts */
      for (idx=0; idx<numdim-1; idx++) {
        if (dim[idx]==0) {
          dim[idx]=counteddim[idx];
        } else if (counteddim[idx]<dim[idx]) {
          error(52);            /* array is not fully initialized */
          err++;
        } else if (counteddim[idx]>dim[idx]) {
          error(18);            /* initialization data exceeds declared size */
          err++;
        } /* if */
      } /* for */
      if (numdim>1 && dim[numdim-1]==0) {
        /* also look whether, by any chance, all "counted" final dimensions are
         * the same value; if so, we can store this
         */
        constvalue *ld=lastdim.first;
        int d,match;
        for (d=0; d<dim[numdim-2]; d++) {
          assert(ld!=NULL);
          assert(strtol(ld->name,NULL,16)==d);
          if (d==0)
            match=ld->value;
          else if (match!=ld->value)
            break;
          ld=ld->next;
        } /* for */
        if (d==dim[numdim-2])
          dim[numdim-1]=match;
      } /* if */
      /* after all arrays have been initialized, we know the (major) dimensions
       * of the array and we can properly adjust the indirection vectors
       */
      if (err==0)
        adjust_indirectiontables(dim,numdim,curlit,&lastdim,&skipdim);
      delete_consttable(&lastdim);  /* clear list of minor dimension sizes */
    } /* if */
  } /* if */

  if (*size==0)
    *size=litidx-curlit;        /* number of elements defined */
}

static cell initarray(int ident,int tag,int dim[],int numdim,int cur,
                      int startlit,int counteddim[],constvalue_root *lastdim,
                      constvalue_root *enumroot,int *errorfound)
{
  cell dsize,totalsize;
  int idx,idx_ellips,vidx,do_insert;
  int abortparse;
  int curlit;
  int prev1_idx=-1,prev2_idx=-1;

  assert(cur>=0 && cur<numdim);
  assert(startlit>=0);
  assert(cur+2<=numdim);        /* there must be 2 dimensions or more to do */
  assert(errorfound!=NULL && *errorfound==FALSE);
  totalsize=0;
  needtoken('{');
  for (do_insert=FALSE,idx=0; idx<=cur; idx++) {
    if (dim[idx]==0) {
      do_insert=TRUE;
      break;
    } /* if */
  } /* for */
  for (idx=0,abortparse=FALSE; !abortparse; idx++) {
    /* In case the major dimension is zero, we need to store the offset
     * to the newly detected sub-array into the indirection table; i.e.
     * this table needs to be expanded and updated.
     * In the current design, the indirection vectors for a multi-dimensional
     * array are adjusted after parsing all initiallers. Hence, it is only
     * necessary at this point to reserve space for an extra cell in the
     * indirection vector.
     */
    if (do_insert) {
      litinsert(0,startlit);
    } else if (idx>=dim[cur]) {
      error(18);                /* initialization data exceeds array size */
      break;
    } /* if */
    if (cur+2<numdim) {
      dsize=initarray(ident,tag,dim,numdim,cur+1,startlit,counteddim,
                      lastdim,enumroot,errorfound);
    } else {
      curlit=litidx;
      if (matchtoken(tELLIPS)!=0) {
        /* found an ellipsis; fill up the rest of the array with a series
         * of one-dimensional arrays ("2d ellipsis")
         */
        if (prev1_idx!=-1) {
          for (idx_ellips=1; idx < dim[cur]; idx++, idx_ellips++) {
            for (vidx=0; vidx < dsize; vidx++) {
              if (prev2_idx!=-1)
                litadd(litq[prev1_idx+vidx]+idx_ellips*(litq[prev1_idx+vidx]-litq[prev2_idx+vidx]));
              else
                litadd(litq[prev1_idx+vidx]);
            } /* for */
            append_constval(lastdim,itoh(idx),dsize,0);
          } /* for */
          idx--;
        } else
          error(41);            /* invalid ellipsis, array size unknown */
      } else {
        prev2_idx=prev1_idx;
        prev1_idx=litidx;
        dsize=initvector(ident,tag,dim[cur+1],curlit,TRUE,enumroot,errorfound);
        /* The final dimension may be variable length. We need to save the
         * lengths of the final dimensions in order to set the indirection
         * vectors for the next-to-last dimension.
         */
        append_constval(lastdim,itoh(idx),dsize,0);
      } /* if */
    } /* if */
    totalsize+=dsize;
    if (*errorfound || !matchtoken(','))
      abortparse=TRUE;
  } /* for */
  needtoken('}');
  assert(counteddim!=NULL);
  if (counteddim[cur]>0) {
    if (idx<counteddim[cur])
      error(52);                /* array is not fully initialized */
    else if (idx>counteddim[cur])
      error(18);                /* initialization data exceeds declared size */
  } /* if */
  counteddim[cur]=idx;

  return totalsize+dim[cur];    /* size of sub-arrays + indirection vector */
}

/*  initvector
 *  Initialize a single dimensional array
 */
static cell initvector(int ident,int tag,cell size,int startlit,int fillzero,
                       constvalue_root *enumroot,int *errorfound)
{
  cell prev1=0,prev2=0;
  int ellips=FALSE;
  int rtag,ctag;

  assert(ident==iARRAY || ident==iREFARRAY);
  if (matchtoken('{')) {
    constvalue *enumfield=(enumroot!=NULL) ? enumroot->first : NULL;
    do {
      int fieldlit=litidx;
      int matchbrace,i;
      if (matchtoken('}')) {    /* to allow for trailing ',' after the initialization */
        lexpush();
        break;
      } /* if */
      if ((ellips=matchtoken(tELLIPS))!=0)
        break;
      /* for enumeration fields, allow another level of braces ("{...}") */
      matchbrace=0;             /* preset */
      ellips=0;
      if (enumfield!=NULL)
        matchbrace=matchtoken('{');
      for ( ;; ) {
        prev2=prev1;
        prev1=init(ident,&ctag,errorfound);
        if (!matchbrace)
          break;
        if ((ellips=matchtoken(tELLIPS))!=0)
          break;
        if (!matchtoken(',')) {
          needtoken('}');
          break;
        } /* if */
      } /* for */
      /* if this array is based on an enumeration, fill the "field" up with
       * zeros, and toggle the tag
       */
      if (enumroot!=NULL && enumfield==NULL)
        error(227);             /* more initiallers than enum fields */
      rtag=tag;                 /* preset, may be overridden by enum field tag */
      if (enumfield!=NULL) {
        cell step;
        int cmptag=enumfield->index;
        symbol *symfield=findconst(enumfield->name,&cmptag);
        if (cmptag>1)
          error(91,enumfield->name); /* ambiguous constant, needs tag override */
        assert(symfield!=NULL);
        assert(fieldlit<litidx);
        if (litidx-fieldlit>symfield->dim.array.length)
          error(228);           /* length of initialler exceeds size of the enum field */
        if (ellips) {
          step=prev1-prev2;
        } else {
          step=0;
          prev1=0;
        } /* if */
        for (i=litidx-fieldlit; i<symfield->dim.array.length; i++) {
          prev1+=step;
          litadd(prev1);
        } /* for */
        rtag=symfield->x.tags.index;  /* set the expected tag to the index tag */
        enumfield=enumfield->next;
      } /* if */
      check_tagmismatch(rtag,ctag,TRUE,-1);
    } while (matchtoken(',')); /* do */
    needtoken('}');
  } else {
    init(ident,&ctag,errorfound);
    check_tagmismatch(tag,ctag,TRUE,-1);
  } /* if */
  /* fill up the literal queue with a series */
  if (ellips) {
    cell step=((litidx-startlit)==1) ? (cell)0 : prev1-prev2;
    if (size==0 || (litidx-startlit)==0)
      error(41);                /* invalid ellipsis, array size unknown */
    else if ((litidx-startlit)==(int)size)
      error(18);                /* initialisation data exceeds declared size */
    while ((litidx-startlit)<(int)size) {
      prev1+=step;
      litadd(prev1);
    } /* while */
  } /* if */
  if (fillzero && size>0) {
    while ((litidx-startlit)<(int)size)
      litadd(0);
  } /* if */
  if (size==0) {
    size=litidx-startlit;         /* number of elements defined */
  } else if (litidx-startlit>(int)size) { /* e.g. "myvar[3]={1,2,3,4};" */
    error(18);                  /* initialisation data exceeds declared size */
    litidx=(int)size+startlit;    /* avoid overflow in memory moves */
  } /* if */
  return size;
}

/*  init
 *
 *  Evaluate one initializer.
 */
static cell init(int ident,int *tag,int *errorfound)
{
  cell i = 0;

  if (matchtoken(tSTRING)){
    /* lex() automatically stores strings in the literal table (and
     * increases "litidx")
     */
    if (ident==iVARIABLE) {
      error(6);         /* must be assigned to an array */
      litidx=1;         /* reset literal queue */
    } /* if */
    *tag=0;
  } else if (constexpr(&i,tag,NULL)){
    litadd(i);          /* store expression result in literal table */
  } else {
    if (errorfound!=NULL)
      *errorfound=TRUE;
  } /* if */
  return i;
}

/*  needsub
 *
 *  Get required array size
 */
static cell needsub(int *tag,constvalue_root **enumroot)
{
  cell val;
  symbol *sym;

  assert(tag!=NULL);
  *tag=0;
  if (enumroot!=NULL)
    *enumroot=NULL;         /* preset */
  if (matchtoken(']'))      /* we have already seen "[" */
    return 0;               /* zero size (like "char msg[]") */

  constexpr(&val,tag,&sym); /* get value (must be constant expression) */
  if (val<=0) {
    error(9);               /* negative array size is invalid; assumed zero */
    val=0;
  } /* if */
  needtoken(']');

  if (enumroot!=NULL) {
    /* get the field list for an enumeration */
    assert(*enumroot==NULL);/* should have been preset */
    assert(sym==NULL || sym->ident==iCONSTEXPR);
    if (sym!=NULL && (sym->usage & uENUMROOT)==uENUMROOT) {
      assert(sym->dim.enumlist!=NULL);
      *enumroot=sym->dim.enumlist;
    } /* if */
  } /* if */

  return val;               /* return array size */
}

/*  decl_const  - declare a single constant
 *
 */
static void decl_const(int vclass)
{
  char constname[sNAMEMAX+1];
  cell val;
  char *str;
  int tag,exprtag;
  int symbolline;
  int fstatic;
  symbol *sym;

  fstatic=(vclass==sGLOBAL && matchtoken(tSTATIC));
  insert_docstring_separator();         /* see comment in newfunc() */
  do {
    tag=pc_addtag(NULL);
    if (lex(&val,&str)!=tSYMBOL)        /* read in (new) token */
      error(20,str);                    /* invalid symbol name */
    symbolline=fline;                   /* save line where symbol was found */
    strcpy(constname,str);              /* save symbol name */
    needtoken('=');
    constexpr(&val,&exprtag,NULL);      /* get value */
    /* add_constant() checks for duplicate definitions */
    check_tagmismatch(tag,exprtag,FALSE,symbolline);
    sym=add_constant(constname,val,vclass,tag);
    if (sym!=NULL) {
      if (fstatic)
        sym->fnumber=fcurrent;
      sc_attachdocumentation(sym);/* attach any documentation to the constant */
    } /* if */
  } while (matchtoken(',')); /* enddo */   /* more? */
  needtoken(tTERM);
}

/*  decl_enum   - declare enumerated constants
 *
 */
static void decl_enum(int vclass,int fstatic)
{
  char enumname[sNAMEMAX+1],constname[sNAMEMAX+1];
  cell val,value,size;
  char *str;
  int tag,explicittag;
  int unique;
  int inctok;
  int warn_overflow,warn_noeffect;
  cell increment;
  constvalue_root *enumroot=NULL;
  symbol *enumsym=NULL;
  symbol *noeffect_sym=NULL;
  short filenum;

  filenum=fcurrent;

  /* get an explicit tag, if any (we need to remember whether an explicit
   * tag was passed, even if that explicit tag was "_:", so we cannot call
   * pc_addtag() here
   */
  if (lex(&val,&str)==tLABEL) {
    tag=pc_addtag(str);
    explicittag=TRUE;
  } else {
    lexpush();
    tag=0;
    explicittag=FALSE;
  } /* if */

  /* get optional enum name (also serves as a tag if no explicit tag was set) */
  if (lex(&val,&str)==tSYMBOL) {        /* read in (new) token */
    strcpy(enumname,str);               /* save enum name (last constant) */
    if (!explicittag)
      tag=pc_addtag(enumname);
  } else {
    lexpush();                          /* analyze again */
    enumname[0]='\0';
  } /* if */

  /* get the increment */
  increment=1;
  inctok=taADD;
  if (matchtoken('(')) {
    int tok=lex(&val,&str);
    if (tok==taADD || tok==taMULT || tok==taSHL) {
      inctok=tok;
      constexpr(&increment,NULL,NULL);
      if (tok==taSHL) {
        if (increment<0 || increment>=PAWN_CELL_SIZE)
          error(241);                   /* negative or too big shift count */
        if (increment<0)
          increment=0;
      } /* if */
    } else {
      lexpush();
    } /* if */
    needtoken(')');
  } /* if */

  if (!strempty(enumname)) {
    /* already create the root symbol, so the fields can have it as their "parent" */
    enumsym=add_constant(enumname,0,vclass,tag);
    if (enumsym!=NULL) {
      enumsym->usage |= uENUMROOT;
      unique=0;
      if (fstatic)
        enumsym->fnumber=filenum;
      /* if we redefined a root symbol of another enum, then we need to delete
       * the previous list of enum elements, otherwise it would be leaked */
      if (enumsym->dim.enumlist!=NULL)
        delete_consttable(enumsym->dim.enumlist);
    } /* if */
    /* start a new list for the element names */
    if ((enumroot=(constvalue_root*)malloc(sizeof(constvalue_root)))==NULL)
      error(103);                       /* insufficient memory (fatal error) */
    memset(enumroot,0,sizeof(constvalue_root));
  } /* if */

  needtoken('{');
  /* go through all constants */
  value=0;                              /* default starting value */
  warn_overflow=warn_noeffect=FALSE;
  do {
    int idxtag,fieldtag;
    int symline;
    symbol *sym;
    if (matchtoken('}')) {              /* quick exit if '}' follows ',' */
      lexpush();
      break;
    } /* if */
    symline=fline;
    idxtag=(enumname[0]=='\0') ? tag : pc_addtag(NULL); /* optional explicit item tag */
    if (needtoken(tSYMBOL)) {           /* read in (new) token */
      tokeninfo(&val,&str);             /* get the information */
      strcpy(constname,str);            /* save symbol name */
    } else {
      constname[0]='\0';
    } /* if */
    size=(inctok==taADD) ? increment : 1;/* default increment of 'val' */
    fieldtag=0;                         /* default field tag */
    if (matchtoken('[')) {
      constexpr(&size,&fieldtag,NULL);  /* get size */
      needtoken(']');
    } /* if */
    if (matchtoken('=')) {
      constexpr(&value,NULL,NULL);      /* get value */
      warn_overflow=warn_noeffect=FALSE;
    } else {
      if (warn_overflow) {
        int num=(inctok==taSHL) ? 242   /* shift overflow in enum element declaration */
                                : 246;  /* multiplication overflow in enum element declaration */
        errorset(sSETPOS,symline);
        error(num,constname);
        errorset(sSETPOS,-1);
        /* don't reset "warn_overflow" yet, we'll need to use it later */
      } /* if */
      if (warn_noeffect) {
        const char *name=noeffect_sym->name;
        str=sc_tokens[inctok-tFIRST];
        errorset(sSETPOS,noeffect_sym->lnumber);
        error(245,str,increment,name);  /* enum increment has no effect on zero value */
        errorset(sSETPOS,-1);
        warn_noeffect=FALSE;
      } /* if */
    } /* if */
    /* add_constant() checks whether a variable (global or local) or
     * a constant with the same name already exists
     */
    sym=add_constant(constname,value,vclass,tag);
    if (sym==NULL)
      continue;                         /* error message already given */
    /* modify the symbol only if it's not the current enum root symbol
     * being redefined by the user */
    if (sym!=enumsym) {
      /* clear the "enum root" flag and delete the list of enum elements,
       * in case we redefined a root symbol of another enum */
      if ((sym->usage & uENUMROOT)!=0) {
        sym->usage &= ~uENUMROOT;
        delete_consttable(sym->dim.enumlist);
      } /* if */
      /* set the item tag and the item size, for use in indexing arrays */
      sym->x.tags.index=idxtag;
      sym->x.tags.field=fieldtag;
      sym->dim.array.length=size;
      sym->dim.array.level=0;
      sym->parent=enumsym;
      if (enumsym)
        enumsym->child=sym;

      if (fstatic)
        sym->fnumber=filenum;

      if (enumroot!=NULL && find_constval_byval(enumroot,value)==NULL)
        unique++;

      /* add the constant to a separate list as well */
      if (enumroot!=NULL) {
        sym->usage |= uENUMFIELD;
        append_constval(enumroot,constname,value,tag);
      } /* if */
    } /* if */
    if (inctok!=taADD && value==0 && increment!=0
        && noeffect_sym==NULL && warn_overflow==FALSE) {
      warn_noeffect=TRUE;
      noeffect_sym=sym;
    } /* if */
    warn_overflow=FALSE;
    if (inctok==taADD) {
      value+=size;
    } else if (inctok==taMULT) {
#if PAWN_CELL_SIZE<64
      /* use a bigger type to detect overflow */
      int64_t t=(int64_t)value*(int64_t)size*(int64_t)increment;
      if (t>(int64_t)CELL_MAX || t<(~(int64_t)CELL_MAX))
#else
      /* casting to a bigger type isn't possible as we don't have int128_t,
       * so we'll have to use slower division */
      cell t=size*increment;
      if (value!=0 && (value*t)/value!=t)
#endif
        warn_overflow=TRUE;
      value*=(size*increment);
    } else { // taSHL
      if (increment>0 && increment<PAWN_CELL_SIZE
          && ((ucell)value>=((ucell)1 << (PAWN_CELL_SIZE-increment))))
        warn_overflow=TRUE;
      value*=(size << increment);
    } /* if */
  } while (matchtoken(','));
  needtoken('}');       /* terminates the constant list */
  matchtoken(';');      /* eat an optional ; */

  /* set the enum name to the "next" value (typically the last value plus one) */
  if (enumsym!=NULL) {
    assert((enumsym->usage & uENUMROOT)!=0);
    enumsym->addr=value;
    enumsym->x.tags.unique=unique;
    /* assign the constant list */
    assert(enumroot!=NULL);
    enumsym->dim.enumlist=enumroot;
    sc_attachdocumentation(enumsym);  /* attach any documentation to the enumeration */
  } /* if */
}

static int getstates(const char *funcname)
{
  char fsaname[sNAMEMAX+1],statename[sNAMEMAX+1];
  cell val;
  char *str;
  constvalue *automaton;
  constvalue *state;
  int fsa,islabel;
  int *list;
  int count,listsize,state_id;

  if (!matchtoken('<'))
    return 0;
  if (matchtoken('>'))
    return -1;          /* special construct: all other states (fall-back) */

  count=0;
  listsize=0;
  list=NULL;
  fsa=-1;

  do {
    if (!(islabel=matchtoken(tLABEL)) && !needtoken(tSYMBOL))
      break;
    tokeninfo(&val,&str);
    assert(strlen(str)<arraysize(fsaname));
    strcpy(fsaname,str);  /* assume this is the name of the automaton */
    if (islabel || matchtoken(':')) {
      /* token is an automaton name, add the name and get a new token */
      if (!needtoken(tSYMBOL))
        break;
      tokeninfo(&val,&str);
      assert(strlen(str)<arraysize(statename));
      strcpy(statename,str);
    } else {
      /* the token was the state name (part of an anynymous automaton) */
      assert(strlen(fsaname)<arraysize(statename));
      strcpy(statename,fsaname);
      fsaname[0]='\0';
    } /* if */
    if (fsa<0 || fsaname[0]!='\0') {
      automaton=automaton_add(fsaname);
      assert(automaton!=NULL);
      if (fsa>=0 && automaton->index!=fsa)
        error(83,funcname); /* multiple automatons for a single function/variable */
      fsa=automaton->index;
    } /* if */
    state=state_add(statename,fsa);
    /* add this state to the state combination list (it will be attached to the
     * automaton later) */
    state_buildlist(&list,&listsize,&count,(int)state->value);
  } while (matchtoken(','));
  needtoken('>');

  if (count>0) {
    assert(automaton!=NULL);
    assert(fsa>=0);
    state_id=state_addlist(list,count,fsa);
    assert(state_id>0);
  } else {
    /* error is already given */
    state_id=0;
  } /* if */
  free(list);

  return state_id;
}

static void attachstatelist(symbol *sym, int state_id)
{
  assert(sym!=NULL);

  if (state_id!=0) {
    /* add the state list id */
    constvalue *stateptr;
    if (sym->states==NULL) {
      if ((sym->states=(constvalue_root*)malloc(sizeof(constvalue_root)))==NULL)
        error(103);             /* insufficient memory (fatal error) */
      memset(sym->states,0,sizeof(constvalue_root));
    } /* if */
    /* see whether the id already exists (add new state only if it does not
     * yet exist
     */
    assert(sym->states!=NULL);
    for (stateptr=sym->states->first; stateptr!=NULL && stateptr->index!=state_id; stateptr=stateptr->next)
      /* nothing */;
    assert(state_id<=SHRT_MAX);
    if (stateptr==NULL)
      append_constval(sym->states,"",code_idx,(short)state_id);
    else if (stateptr->value==0)
      stateptr->value=code_idx;
    else
      error(84,sym->name);
    /* also check for another conflicting situation: a fallback function
     * without any states
     */
    if (state_id==-1 && sc_status!=statFIRST) {
      /* in the second round, all states should have been accumulated */
      assert(sym->states!=NULL);
      for (stateptr=sym->states->first; stateptr!=NULL && stateptr->index==-1; stateptr=stateptr->next)
        /* nothing */;
      if (stateptr==NULL)
        error(85,sym->name);      /* no states are defined for this function */
    } /* if */
  } /* if */
}

/*
 *  Finds a function in the global symbol table or creates a new entry.
 *  It does some basic processing and error checking.
 */
SC_FUNC symbol *fetchfunc(char *name,int tag)
{
  symbol *sym;

  if ((sym=findglb(name,sGLOBAL))!=NULL) {/* already in symbol table? */
    if (sym->ident!=iFUNCTN) {
      error(21,name);                     /* yes, but not as a function */
      return NULL;                        /* make sure the old symbol is not damaged */
    } else if ((sym->usage & uNATIVE)!=0) {
      error(21,name);                     /* yes, and it is a native */
    } /* if */
    assert(sym->vclass==sGLOBAL);
    if ((sym->usage & uPROTOTYPED)!=0 && sym->tag!=tag)
      error(25);                          /* mismatch from earlier prototype */
    if ((sym->usage & uDEFINE)==0) {
      /* as long as the function stays undefined, update the address and the tag */
      if (sym->states==NULL)
        sym->addr=code_idx;
      sym->tag=tag;
    } /* if */
  } else {
    /* don't set the "uDEFINE" flag; it may be a prototype */
    sym=addsym(name,code_idx,iFUNCTN,sGLOBAL,tag,0);
    assert(sym!=NULL);          /* fatal error 103 must be given on error */
    /* assume no arguments */
    sym->dim.arglist=(arginfo*)calloc(1,sizeof(arginfo));
    /* set library ID to NULL (only for native functions) */
    sym->x.lib=NULL;
    /* set the required stack size to zero (only for non-native functions) */
    sym->x.stacksize=1;         /* 1 for PROC opcode */
  } /* if */
  pragma_deprecated(sym);

  return sym;
}

/* This routine adds symbolic information for each argument.
 */
static void define_args(void)
{
  symbol *sym;

  /* At this point, no local variables have been declared. All
   * local symbols are function arguments.
   */
  sym=loctab.next;
  while (sym!=NULL) {
    assert(sym->ident!=iLABEL);
    assert(sym->vclass==sLOCAL);
    markexpr(sLDECL,sym->name,sym->addr); /* mark for better optimization */
    sym=sym->next;
  } /* while */
}

static int operatorname(char *name)
{
  int opertok;
  char *str;
  cell val;

  assert(name!=NULL);

  /* check the operator */
  opertok=lex(&val,&str);
  switch (opertok) {
  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
  case '>':
  case '<':
  case '!':
  case '~':
  case '=':
    name[0]=(char)opertok;
    name[1]='\0';
    break;
  case tINC:
    strcpy(name,"++");
    break;
  case tDEC:
    strcpy(name,"--");
    break;
  case tlEQ:
    strcpy(name,"==");
    break;
  case tlNE:
    strcpy(name,"!=");
    break;
  case tlLE:
    strcpy(name,"<=");
    break;
  case tlGE:
    strcpy(name,">=");
    break;
  default:
    name[0]='\0';
    error(7);           /* operator cannot be redefined (or bad operator name) */
    return 0;
  } /* switch */

  return opertok;
}

static int operatoradjust(int opertok,symbol *sym,char *opername,int resulttag)
{
  int tags[2]={0,0};
  int count=0;
  arginfo *arg;
  char tmpname[sNAMEMAX+1];
  symbol *oldsym;

  if (opertok==0)
    return TRUE;

  assert(sym!=NULL && sym->ident==iFUNCTN && sym->dim.arglist!=NULL);
  /* count arguments and save (first two) tags */
  while (arg=&sym->dim.arglist[count], arg->ident!=0) {
    if (count<2) {
      if (arg->numtags>1)
        error(65,count+1);  /* function argument may only have a single tag */
      else if (arg->numtags==1)
        tags[count]=arg->tags[0];
    } /* if */
    if (opertok=='~' && count==0) {
      if (arg->ident!=iREFARRAY)
        error(73,arg->name);/* must be an array argument */
    } else {
      if (arg->ident!=iVARIABLE)
        error(66,arg->name);/* must be non-reference argument */
    } /* if */
    if (arg->hasdefault)
      error(59,arg->name);  /* arguments of an operator may not have a default value */
    count++;
  } /* while */

  /* for '!', '++' and '--', count must be 1
   * for '-', count may be 1 or 2
   * for '=', count must be 1, and the resulttag is also important
   * for all other (binary) operators and the special '~' operator, count must be 2
   */
  switch (opertok) {
  case '!':
  case '=':
  case tINC:
  case tDEC:
    if (count!=1)
      error(62);      /* number or placement of the operands does not fit the operator */
    break;
  case '-':
    if (count!=1 && count!=2)
      error(62);      /* number or placement of the operands does not fit the operator */
    break;
  default:
    if (count!=2)
      error(62);      /* number or placement of the operands does not fit the operator */
  } /* switch */

  if (tags[0]==0 && ((opertok!='=' && tags[1]==0) || (opertok=='=' && resulttag==0)))
    error(64);        /* cannot change predefined operators */

  /* change the operator name */
  assert(!strempty(opername));
  operator_symname(tmpname,opername,tags[0],tags[1],count,resulttag);
  if ((oldsym=findglb(tmpname,sGLOBAL))!=NULL) {
    int i;
    if ((oldsym->usage & uDEFINE)!=0) {
      char errname[2*sNAMEMAX+16];
      funcdisplayname(errname,tmpname);
      error(21,errname);        /* symbol already defined */
    } /* if */
    sym->usage|=oldsym->usage;  /* copy flags from the previous definition */
    for (i=0; i<oldsym->numrefers; i++)
      if (oldsym->refer[i]!=NULL)
        refer_symbol(sym,oldsym->refer[i]);
    delete_symbol(&glbtab,oldsym);
  } /* if */
  rename_symbol(sym,tmpname);

  /* operators should return a value, except the '~' operator */
  if (opertok!='~')
    sym->usage |= uRETVALUE;

  return TRUE;
}

static int check_operatortag(int opertok,int resulttag,char *opername)
{
  assert(opername!=NULL && !strempty(opername));
  switch (opertok) {
  case '!':
  case '<':
  case '>':
  case tlEQ:
  case tlNE:
  case tlLE:
  case tlGE:
    if (resulttag!=BOOLTAG) {
      error(63,opername,"bool:"); /* operator X requires a "bool:" result tag */
      return FALSE;
    } /* if */
    break;
  case '~':
    if (resulttag!=0) {
      error(63,opername,"_:");    /* operator "~" requires a "_:" result tag */
      return FALSE;
    } /* if */
    break;
  } /* switch */
  return TRUE;
}

static char *tag2str(char *dest,int tag)
{
  tag &= TAGMASK;
  assert(tag>=0);
  sprintf(dest,"0%x",tag);
  return isdigit(dest[1]) ? &dest[1] : dest;
}

SC_FUNC char *operator_symname(char *symname,char *opername,int tag1,int tag2,int numtags,int resulttag)
{
  char tagstr1[10], tagstr2[10];
  int opertok;

  assert(numtags>=1 && numtags<=2);
  opertok= (opername[1]=='\0') ? opername[0] : 0;
  if (opertok=='=')
    sprintf(symname,"%s%s%s",tag2str(tagstr1,resulttag),opername,tag2str(tagstr2,tag1));
  else if (numtags==1 || opertok=='~')
    sprintf(symname,"%s%s",opername,tag2str(tagstr1,tag1));
  else
    sprintf(symname,"%s%s%s",tag2str(tagstr1,tag1),opername,tag2str(tagstr2,tag2));
  return symname;
}

static int parse_funcname(char *fname,int *tag1,int *tag2,char *opname)
{
  char *ptr,*name;
  int unary;

  /* tags are only positive, so if the function name starts with a '-',
   * the operator is an unary '-' or '--' operator.
   */
  if (*fname=='-') {
    *tag1=0;
    unary=TRUE;
    ptr=fname;
  } else {
    *tag1=(int)strtol(fname,&ptr,16);
    unary= ptr==fname;  /* unary operator if it doesn't start with a tag name */
  } /* if */
  assert(!unary || *tag1==0);
  assert(*ptr!='\0');
  for (name=opname; !isdigit(*ptr); )
    *name++ = *ptr++;
  *name='\0';
  *tag2=(int)strtol(ptr,NULL,16);
  return unary;
}

static constvalue *find_tag_byval(int tag)
{
  constvalue *tagsym;
  tagsym=find_constval_byval(&tagname_tab,tag & ~PUBLICTAG);
  if (tagsym==NULL)
    tagsym=find_constval_byval(&tagname_tab,tag | PUBLICTAG);
  return tagsym;
}

SC_FUNC void check_index_tagmismatch(char *symname,int expectedtag,int actualtag,int allowcoerce,int errline)
{
  assert(symname!=NULL);
  if (!matchtag(expectedtag,actualtag,allowcoerce)) {
    constvalue *tagsym;
    char expected_tagname[sNAMEMAX+3]="none (\"_\"),",actual_tagname[sNAMEMAX+2]="none (\"_\")"; /* two extra characters for quotes */
    if(expectedtag!=0) {
      tagsym=find_tag_byval(expectedtag);
      sprintf(expected_tagname,"\"%s\",",(tagsym!=NULL) ? tagsym->name : "-unknown-");
    } /* if */
    if(actualtag!=0) {
      tagsym=find_tag_byval(actualtag);
      sprintf(actual_tagname,"\"%s\"",(tagsym!=NULL) ? tagsym->name : "-unknown-");
    } /* if */
    if(errline>0)
      errorset(sSETPOS,errline);
    error(229,symname,expected_tagname,actual_tagname); /* index tag mismatch */
    if(errline>0)
      errorset(sSETPOS,-1);
  } /* if */
}

SC_FUNC void check_tagmismatch(int formaltag,int actualtag,int allowcoerce,int errline)
{
  if (!matchtag(formaltag,actualtag,allowcoerce)) {
    constvalue *tagsym;
    char formal_tagname[sNAMEMAX+3]="none (\"_\"),",actual_tagname[sNAMEMAX+2]="none (\"_\")"; /* two extra characters for quotes */
    if(formaltag!=0) {
      tagsym=find_tag_byval(formaltag);
      sprintf(formal_tagname,"\"%s\",",(tagsym!=NULL) ? tagsym->name : "-unknown-");
    } /* if */
    if(actualtag!=0) {
      tagsym=find_tag_byval(actualtag);
      sprintf(actual_tagname,"\"%s\"",(tagsym!=NULL) ? tagsym->name : "-unknown-");
    } /* if */
    if(errline>0)
      errorset(sSETPOS,errline);
    error(213,"tag",formal_tagname,actual_tagname); /* tag mismatch */
    if(errline>0)
      errorset(sSETPOS,-1);
  } /* if */
}

SC_FUNC void check_tagmismatch_multiple(int formaltags[],int numtags,int actualtag,int errline)
{
  if (!checktag(formaltags, numtags, actualtag)) {
    int i;
    constvalue *tagsym;
    char formal_tagnames[sLINEMAX+1]="",actual_tagname[sNAMEMAX+2]="none (\"_\")";
    int notag_allowed=FALSE,add_comma=FALSE;
    size_t size;
    for (i=0; i<numtags; i++) {
      if(formaltags[i]!=0) {
        if((i+1)==numtags && add_comma==TRUE && notag_allowed==FALSE)
          strlcat(formal_tagnames,", or ",arraysize(formal_tagnames));
        else if(add_comma)
          strlcat(formal_tagnames,", ",arraysize(formal_tagnames));
        add_comma=TRUE;
        tagsym=find_tag_byval(formaltags[i]);
        size=snprintf(formal_tagnames,
                      sizeof(formal_tagnames),
                      "%s\"%s\"",
                      formal_tagnames,
                      (tagsym!=NULL) ? tagsym->name : "-unknown-");
        if(size>=sizeof(formal_tagnames))
          break;
      } else {
        notag_allowed=TRUE;
      } /* if */
    } /* for */
    if(notag_allowed==TRUE) {
      if(add_comma==TRUE)
        strlcat(formal_tagnames,", or ",arraysize(formal_tagnames));
      strlcat(formal_tagnames,"none (\"_\")",arraysize(formal_tagnames));
    } /* if */
    strlcat(formal_tagnames,(numtags==1) ? "," : ";",arraysize(formal_tagnames));
    if(actualtag!=0) {
      tagsym=find_tag_byval(actualtag);
      sprintf(actual_tagname,"\"%s\"",(tagsym!=NULL) ? tagsym->name : "-unknown-");
    } /* if */
    if(errline>0)
      errorset(sSETPOS,errline);
    error(213,(numtags==1) ? "tag" : "tags",formal_tagnames,actual_tagname); /* tag mismatch */
    if(errline>0)
      errorset(sSETPOS,-1);
  } /* if */
}

SC_FUNC char *funcdisplayname(char *dest,char *funcname)
{
  int tags[2];
  char opname[10];
  constvalue *tagsym[2];
  int unary;

  if (isalpha(*funcname) || *funcname=='_' || *funcname==PUBLIC_CHAR || *funcname=='\0') {
    if (dest!=funcname)
      strcpy(dest,funcname);
    return dest;
  } /* if */

  unary=parse_funcname(funcname,&tags[0],&tags[1],opname);
  tagsym[1]=find_tag_byval(tags[1]);
  assert(tagsym[1]!=NULL);
  if (unary) {
    sprintf(dest,"operator%s(%s:)",opname,tagsym[1]->name);
  } else {
    tagsym[0]=find_tag_byval(tags[0]);
    assert(tagsym[0]!=NULL);
    /* special case: the assignment operator has the return value as the 2nd tag */
    if (opname[0]=='=' && opname[1]=='\0')
      sprintf(dest,"%s:operator%s(%s:)",tagsym[0]->name,opname,tagsym[1]->name);
    else
      sprintf(dest,"operator%s(%s:,%s:)",opname,tagsym[0]->name,tagsym[1]->name);
  } /* if */
  return dest;
}

static void check_reparse(symbol *sym)
{
  /* if the function was used before being declared, and it has a tag for the
   * result, add a third pass (as second "skimming" parse) because the function
   * result may have been used with user-defined operators, which have now
   * been incorrectly flagged (as the return tag was unknown at the time of
   * the call)
   */
  if ((sym->usage & (uPROTOTYPED | uREAD))==uREAD && sym->tag!=0) {
    int curstatus=sc_status;
    sc_status=statWRITE;  /* temporarily set status to WRITE, so the warning isn't blocked */
    error(208);
    sc_status=curstatus;
    sc_reparse=TRUE;      /* must add another pass to "initial scan" phase */
  } /* if */
}

static void funcstub(int fnative)
{
  int tok,tag,fpublic;
  char *str;
  cell val,size;
  char symbolname[sNAMEMAX+1];
  int idxtag[sDIMEN_MAX];
  int dim[sDIMEN_MAX];
  int numdim;
  symbol *sym,*sub;
  int opertok;
  unsigned int bck_attributes;
  char *bck_deprecate;  /* in case the user tries to use __pragma("deprecated")
                         * on a function argument */

  opertok=0;
  lastst=0;
  litidx=0;                     /* clear the literal pool */
  assert(loctab.next==NULL);    /* local symbol table should be empty */

  tag=pc_addtag(NULL);			/* get the tag of the return value */
  numdim=0;
  while (matchtoken('[')) {
    /* the function returns an array, get this tag for the index and the array
     * dimensions
     */
    if (numdim == sDIMEN_MAX) {
      error(53);                /* exceeding maximum number of dimensions */
      return;
    } /* if */
    size=needsub(&idxtag[numdim],NULL); /* get size; size==0 for "var[]" */
    if (size==0)
      error(9);                 /* invalid array size */
    #if INT_MAX < LONG_MAX
      if (size > INT_MAX)
        error(105);             /* overflow, exceeding capacity */
    #endif
    dim[numdim++]=(int)size;
  } /* while */

  tok=lex(&val,&str);
  fpublic=(tok==tPUBLIC) || (tok==tSYMBOL && str[0]==PUBLIC_CHAR);
  if (fnative) {
    if (fpublic || tok==tSTOCK || tok==tSTATIC || (tok==tSYMBOL && *str==PUBLIC_CHAR))
      error(42);                /* invalid combination of class specifiers */
  } else {
    if (tok==tPUBLIC || tok==tSTOCK || tok==tSTATIC)
      tok=lex(&val,&str);
  } /* if */

  if (tok==t__PRAGMA) {
    dopragma();
    tok=lex(&val,&str);
  } /* if */

  if (tok==tOPERATOR) {
    if (numdim!=0) {
      error(10);                /* invalid function or declaration */
      numdim=0;                 /* ignore the array size specification */
    } /* if */
    opertok=operatorname(symbolname);
    if (opertok==0)
      return;                   /* error message already given */
    check_operatortag(opertok,tag,symbolname);
  } else {
    if (tok!=tSYMBOL && freading) {
      error(10);                /* illegal function or declaration */
      return;
    } /* if */
    strcpy(symbolname,str);
  } /* if */
  needtoken('(');               /* only functions may be native/forward */

  sym=fetchfunc(symbolname,tag);/* get a pointer to the function entry */
  if (sym==NULL)
    return;
  if (fnative) {
    sym->usage=(uNATIVE | uRETVALUE | uDEFINE | (sym->usage & uPROTOTYPED));
    sym->x.lib=curlibrary;
  } else if (fpublic && opertok==0) {
    sym->usage|=uPUBLIC;
  } /* if */
  sym->usage|=uFORWARD;
  check_reparse(sym);

  bck_attributes=pc_attributes;
  bck_deprecate=pc_deprecate;
  pc_attributes=0;
  pc_deprecate=NULL;

  declargs(sym,FALSE);
  /* "declargs()" found the ")" */
  sc_attachdocumentation(sym);  /* attach any documentation to the function */
  if (!operatoradjust(opertok,sym,symbolname,tag))
    sym->usage &= ~uDEFINE;
  if (fpublic && opertok!=0) {
    char symname[2*sNAMEMAX+16];  /* allow space for user defined operators */
    funcdisplayname(symname,sym->name);
    error(56,symname);  /* operators cannot be public */
  } /* if */

  if (getstates(symbolname)!=0) {
    if (fnative || opertok!=0)
      error(82);                /* native functions and operators may not have states */
    else
      error(231);               /* ignoring state specifications on forward declarations */
  } /* if */

  /* for a native operator, also need to specify an "exported" function name;
   * for a native function, this is optional
   */
  if (fnative) {
    if ((opertok!=0) ? needtoken('=') : matchtoken('=')) {
      /* allow number or symbol */
      if (matchtoken(tSYMBOL)) {
        tokeninfo(&val,&str);
        insert_alias(sym->name,str);
      } else {
        constexpr(&val,NULL,NULL);
        sym->addr=val;
        /* At the moment, I have assumed that this syntax is only valid if
         * val < 0. To properly mix "normal" native functions and indexed
         * native functions, one should use negative indices anyway.
         * Special code for a negative index in sym->addr exists in SC4.C
         * (ffcall()) and in SC6.C (the loops for counting the number of native
         * variables and for writing them).
         */
      } /* if */
    } /* if */
  } /* if */

  pc_deprecate=bck_deprecate;
  pc_attributes=bck_attributes;
  if (matchtoken(t__PRAGMA))
    dopragma();
  pragma_apply(sym);

  needtoken(tTERM);

  if (numdim>0) {
    if (sym->child==NULL) {
      /* attach the array to the function symbol */
      assert(sym!=NULL);
      assert(curfunc==NULL);
      curfunc=sym;
      sub=addvariable(symbolname,0,iREFARRAY,sGLOBAL,tag,dim,numdim,idxtag,0);
      curfunc=NULL;
      sub->parent=sym;
      if (sym!=NULL)
        sym->child=sub;
    } else {
      /* the array is already created and attached to the function (which means
       * this is not the first compilation pass), but we need to make sure the
       * current dimensions match the dimensions declared at the first pass, as
       * we can't rely on the code being the same on all passes (mainly because
       * of conditional compilation, e.g. '#if defined <function name>') */
      if (numdim!=sym->child->dim.array.level+1) {
        error(25);              /* function heading differs from prototype */
      } else {
        int i=0;
        sub=sym->child;
        do {
          if (dim[i]!=sub->dim.array.length) {
            error(25);          /* function heading differs from prototype */
            break;
          } /* if */
          sub=sub->child;
        } while (++i<numdim);
      } /* if */
    } /* if */
  } /* if */

  litidx=0;                     /* clear the literal pool */
  delete_symbols(&loctab,0,TRUE,TRUE);/* clear local variables queue */
}

/*  newfunc    - begin a function
 *
 *  This routine is called from "parse" and tries to make a function
 *  out of the following text
 *
 *  Global references: funcstatus,lastst,litidx
 *                     rettype  (altered)
 *                     curfunc  (altered)
 *                     declared (altered)
 *                     glb_declared (altered)
 *                     sc_alignnext (altered)
 */
static int newfunc(char *firstname,int firsttag,int fpublic,int fstatic,int stock)
{
  symbol *sym,*lvar,*depend;
  int argcnt,tok,tag,funcline,i;
  int opertok,opererror;
  char symbolname[sNAMEMAX+1];
  char *str;
  cell val,cidx,glbdecl;
  short filenum;
  int state_id;
  unsigned int bck_attributes;
  char *bck_deprecate;  /* in case the user tries to use __pragma("deprecated")
                         * on a function argument */

  assert(litidx==0);    /* literal queue should be empty */
  litidx=0;             /* clear the literal pool (should already be empty) */
  opertok=0;
  lastst=0;             /* no statement yet */
  cidx=0;               /* just to avoid compiler warnings */
  glbdecl=0;
  assert(loctab.next==NULL);    /* local symbol table should be empty */
  filenum=fcurrent;     /* save file number at the start of the declaration */

  if (firstname!=NULL) {
    assert(strlen(firstname)<=sNAMEMAX);
    strcpy(symbolname,firstname);       /* save symbol name */
    tag=firsttag;
  } else {
    tag= (firsttag>=0) ? firsttag : pc_addtag(NULL);
    tok=lex(&val,&str);
    if (tok==tNATIVE || (tok==tPUBLIC && stock))
      error(42);                /* invalid combination of class specifiers */
    if (tok==t__PRAGMA) {
      dopragma();
      tok=lex(&val,&str);
    } /* if */
    if (tok==tOPERATOR) {
      opertok=operatorname(symbolname);
      if (opertok==0)
        return TRUE;            /* error message already given */
      check_operatortag(opertok,tag,symbolname);
    } else {
      if (tok!=tSYMBOL && freading) {
        error(20,str);          /* invalid symbol name */
        return FALSE;
      } /* if */
      assert(strlen(str)<=sNAMEMAX);
      strcpy(symbolname,str);
    } /* if */
  } /* if */
  /* check whether this is a function or a variable declaration */
  if (!matchtoken('(')) {
    /* "async" prefixes a FUNCTION declaration; if what follows is not a function
     * (no argument list) the async marker is meaningless. Reject it here so it is
     * caught for both the class-specifier path (declfuncvar->newfunc) and the
     * bare "async Name" path -- both reach this early return when there is no
     * "(". pc_async is still set (it is cleared only after the whole declaration
     * is parsed). */
    if (pc_async)
      error(266);               /* "async" can only be applied to a function */
    return FALSE;
  } /* if */
  /* so it is a function, proceed */
  funcline=fline;               /* save line at which the function is defined */
  if (symbolname[0]==PUBLIC_CHAR && !callhook_inject_idx) {
    /* the '@' prefix normally marks a public function; a synthesised call-hook
     * body (experiment 010) borrows the "@chook.*" namespace but is an ordinary
     * hidden function, not a public one (callhook_inject_idx is set only while
     * such a body is being compiled). */
    fpublic=TRUE;               /* implicitly public function */
    if (stock)
      error(42);                /* invalid combination of class specifiers */
  } /* if */
  sym=fetchfunc(symbolname,tag);/* get a pointer to the function entry */
  if (sym==NULL || (sym->usage & uNATIVE)!=0)
    return TRUE;                /* it was recognized as a function declaration, but not as a valid one */
  if (pc_iterfunc)
    sym->usage|=uITERFUNC;      /* declared with the "iterfunc" keyword: a lazy generator */
  if (pc_async)
    sym->usage|=uITERFUNC|uASYNC; /* "async Name(...)": coroutine driven by a scheduler, not foreach */
  if (fpublic && opertok==0)
    sym->usage|=uPUBLIC;
  if (fstatic)
    sym->fnumber=filenum;
  check_reparse(sym);
  /* we want public functions to be explicitly prototyped, as they are called
   * from the outside
   */
  if (fpublic && (sym->usage & uFORWARD)==0 && opertok==0)
    error(235,symbolname);
  bck_attributes=pc_attributes;
  bck_deprecate=pc_deprecate;
  pc_attributes=0;
  pc_deprecate=NULL;
  /* declare all arguments */
  argcnt=declargs(sym,TRUE);
  opererror=!operatoradjust(opertok,sym,symbolname,tag);
  if (fpublic && opertok!=0) {
    char symname[2*sNAMEMAX+16];  /* allow space for user defined operators */
    funcdisplayname(symname,sym->name);
    error(56,symname);  /* operators cannot be public */
  } /* if */
  if (strcmp(symbolname,uMAINFUNC)==0 || strcmp(symbolname,uENTRYFUNC)==0) {
    if (argcnt>0)
      error(5);         /* "main()" and "entry()" functions may not have any arguments */
    sym->usage|=uREAD;  /* "main()" is the program's entry point: always used */
  } /* if */
  state_id=getstates(symbolname);
  if (state_id>0 && (opertok!=0 || strcmp(symbolname,uMAINFUNC)==0))
    error(82);          /* operators may not have states, main() may neither */
  pc_deprecate=bck_deprecate;
  pc_attributes=bck_attributes;
  if (matchtoken(t__PRAGMA))
    dopragma();
  pragma_apply(sym);
  /* "declargs()" found the ")"; if a ";" appears after this, it was a
   * prototype */
  if (matchtoken(';')) {
    sym->usage|=uFORWARD;
    if (!sc_needsemicolon)
      error(218);       /* old style prototypes used with optional semicolons */
    if (state_id!=0)
      error(231);       /* state specification on forward declaration is ignored */
    delete_symbols(&loctab,0,TRUE,TRUE);  /* prototype is done; forget everything */
    return TRUE;
  } /* if */
  attachstatelist(sym,state_id);
  /* so it is not a prototype, proceed */
  /* if this is a function that is not referred to (this can only be detected
   * in the second stage), shut code generation off */
  if (sc_status==statWRITE && (sym->usage & uREAD)==0 && !fpublic) {
    sc_status=statSKIP;
    cidx=code_idx;
    glbdecl=glb_declared;
  } /* if */
  if ((sym->usage & uDEFINE)!=0 && (sym->states==NULL || state_id==0))
    error(21,sym->name); /* function already defined, either without states or the current definition has no states */
  if ((sym->flags & flagDEPRECATED)!=0 && fpublic) {
    char *ptr= (sym->documentation!=NULL) ? sym->documentation : "";
    error(234,symbolname,ptr);  /* deprecated (definitely a public function) */
  } /* if */
  if (pc_naked) {
    sym->flags|=flagNAKED;
    pc_naked=FALSE;
  } /* if */
  begcseg();
  sym->usage|=uDEFINE;  /* set the definition flag */
  if (stock)
    sym->usage|=uSTOCK;
  if (opertok!=0 && opererror)
    sym->usage &= ~uDEFINE;
  /* if the function has states, dump the label to the start of the function */
  if (state_id!=0) {
    constvalue *ptr=sym->states->first;
    while (ptr!=NULL) {
      assert(sc_status!=statWRITE || !strempty(ptr->name));
      if (ptr->index==state_id) {
        setlabel((int)strtol(ptr->name,NULL,16));
        break;
      } /* if */
      ptr=ptr->next;
    } /* while */
  } /* if */
  startfunc(sym->name,(sym->flags & flagNAKED)==0); /* creates stack frame */
  insert_dbgline(funcline);
  setline(FALSE);
  if (sc_alignnext) {
    alignframe(sc_dataalign);
    sc_alignnext=FALSE;
  } /* if */
  declared=0;           /* number of local cells */
  rettype=(sym->usage & uRETVALUE);      /* set "return type" variable */
  curfunc=sym;
  define_args();        /* add the symbolic info for the function arguments */
  /* decide whether this is a coroutine generator ("yield") and, if so, emit
   * its prologue. The decision is pass-stable (see generator_isgen()); in the
   * addressing pass that first discovers a "yield" the flag is set later, while
   * the body is parsed, and "sc_reparse" then re-runs that pass. */
  pc_generator=generator_isgen(sym);
  if (pc_generator) {
    sym->usage|=uGENERATOR;
    sym->genlocals=0;   /* recount lifted locals from scratch in this pass */
    generator_emit_prologue();
  } /* if */
  pc_inlinelink=0;
  if (pc_compiling_inline) {
    /* exp 015: this is an "inline" body -- save the static link (the enclosing
     * frame's FRM, passed in ALT by callindirect) into a hidden cell so captured
     * enclosing locals can be reached through it (see sc4.c lifted_slotaddr_pri). */
    inline_emit_prologue();
  } /* if */
  #if !defined SC_LIGHT
    if (matchtoken('{')) {
      lexpush();
    } else {
      /* Insert a separator so that comments following the statement will not
       * be attached to this function; they should be attached to the next
       * function. This is not a problem for functions having a compound block,
       * because the closing brace is an explicit "end token" for the function.
       * With single statement functions, the preprocessor may overread the
       * source code before the parser determines an "end of statement".
       */
      insert_docstring_separator();
    } /* if */
  #endif
  sc_curstates=state_id;/* set state id, for accessing global state variables */
  statement(NULL,FALSE);
  sc_curstates=0;
  if ((rettype & uRETVALUE)!=0)
    sym->usage|=uRETVALUE;
  if (declared!=0 && (curfunc->flags & flagNAKED)==0) {
    /* This happens only in a very special (and useless) case, where a function
     * has only a single statement in its body (no compound block) and that
     * statement declares a new variable
     */
    modstk((int)declared*sizeof(cell)); /* remove all local variables */
    declared=0;
  } /* if */
  if (!isterminal(lastst) && lastst!=tGOTO && (sym->flags & flagNAKED)==0) {
    destructsymbols(&loctab,0);
    /* an "async" coroutine that runs off its end has COMPLETED: hand its (absent)
     * return value 0 to __async_complete, which records the result, fires any bound
     * callback, and reclaims its arena slot (unless kept) by its own B address -- a
     * top-level coroutine resumed through the nested return-to-awaiter call.pri chain
     * finishes here, never via a top-level Async_Resume. Clobbers PRI, so it precedes
     * the sentinel load. No-op for a plain function or a "yield" generator. */
    async_emit_complete_self(0);
    /* falling off the end of a generator ends the sequence, so it returns the
     * same sentinel to "foreach" as an explicit "return" does */
    ldconst(pc_generator ? generator_iterstop : 0,sPRI);
    ffret(strcmp(sym->name,uENTRYFUNC)!=0);
    if ((sym->usage & uRETVALUE)!=0 && !pc_generator) {
      char symname[2*sNAMEMAX+16];  /* allow space for user defined operators */
      funcdisplayname(symname,sym->name);
      error(209,symname);       /* function should return a value */
    } /* if */
  } /* if */
  endfunc();
  sym->codeaddr=code_idx;
  sc_attachdocumentation(sym);  /* attach collected documentation to the function */
  if (litidx) {                 /* if there are literals defined */
    glb_declared+=litidx;
    begdseg();                  /* flip to DATA segment */
    dumplits();                 /* dump literal strings */
    litidx=0;
  } /* if */
  for (i=0; i<argcnt; i++) {
    if (sym->dim.arglist[i].ident==iREFARRAY
        && (lvar=findloc(sym->dim.arglist[i].name))!=NULL) {
      if ((sym->dim.arglist[i].usage & uWRITTEN)==0) {
        /* check if the argument was written in this definition */
        for (depend=lvar; depend!=NULL; depend=depend->child) {
          if ((depend->usage & uWRITTEN)!=0) {
            sym->dim.arglist[i].usage|=depend->usage & uWRITTEN;
            break;
          } /* if */
        } /* for */
      } /* if */
      /* mark argument as written if it was written in another definition */
      lvar->usage|=sym->dim.arglist[i].usage & uWRITTEN;
    } /* if */
  } /* for */

  testsymbols(&loctab,0,TRUE,TRUE);     /* test for unused arguments and labels */
  delete_symbols(&loctab,0,TRUE,TRUE);  /* clear local variables queue */
  assert(loctab.next==NULL);
  curfunc=NULL;
  if (sc_status==statSKIP) {
    sc_status=statWRITE;
    code_idx=cidx;
    glb_declared=glbdecl;
  } /* if */
  return TRUE;
}

static int argcompare(arginfo *a1,arginfo *a2)
{
  int result,level,i;

  result= strcmp(a1->name,a2->name)==0;     /* name */
  if (result)
    result= a1->ident==a2->ident;           /* type/class */
  if (result)
    result= a1->usage==a2->usage;           /* "const" flag */
  if (result)
    result= a1->numtags==a2->numtags;       /* tags (number and names) */
  for (i=0; result && i<a1->numtags; i++)
    result= a1->tags[i]==a2->tags[i];
  if (result)
    result= a1->numdim==a2->numdim;         /* array dimensions & index tags */
  for (level=0; result && level<a1->numdim; level++)
    result= a1->dim[level]==a2->dim[level];
  for (level=0; result && level<a1->numdim; level++)
    result= a1->idxtag[level]==a2->idxtag[level];
  if (result)
    result= a1->hasdefault==a2->hasdefault; /* availability of default value */
  if (a1->hasdefault) {
    if (a1->ident==iREFARRAY) {
      if (result)
        result= a1->defvalue.array.size==a2->defvalue.array.size;
      if (result)
        result= a1->defvalue.array.arraysize==a2->defvalue.array.arraysize;
      if (result)
        result=(memcmp(a1->defvalue.array.data,a2->defvalue.array.data,a1->defvalue.array.size*sizeof(cell))==0);
    } else {
      if (result) {
        if ((a1->hasdefault & uSIZEOF)!=0 || (a1->hasdefault & uTAGOF)!=0)
          result= a1->hasdefault==a2->hasdefault
                  && strcmp(a1->defvalue.size.symname,a2->defvalue.size.symname)==0
                  && a1->defvalue.size.level==a2->defvalue.size.level;
        else if ((a1->hasdefault & uTAGOF_TAG)!=0)
          a1->defvalue.val=a2->defvalue.val;
        else
          result= a1->defvalue.val==a2->defvalue.val;
      } /* if */
    } /* if */
    if (result)
      result= a1->defvalue_tag==a2->defvalue_tag;
  } /* if */
  return result;
}

/*  declargs()
 *
 *  This routine adds an entry in the local symbol table for each argument
 *  found in the argument list. It returns the number of arguments.
 */
static int declargs(symbol *sym,int chkshadow)
{
  #define MAXTAGS 256
  char *ptr;
  int argcnt,oldargcnt,tok,tags[MAXTAGS],numtags;
  cell val;
  arginfo arg, *arglist;
  char name[sNAMEMAX+1];
  int ident,fpublic,fconst,fpragma;
  int idx;

  /* if the function is already defined earlier, get the number of arguments
   * of the existing definition
   */
  oldargcnt=0;
  if ((sym->usage & uPROTOTYPED)!=0)
    while (sym->dim.arglist[oldargcnt].ident!=0)
      oldargcnt++;
  argcnt=0;                             /* zero arguments up to now */
  ident=iVARIABLE;
  numtags=0;
  fconst=fpragma=FALSE;
  fpublic= (sym->usage & uPUBLIC)!=0;
  /* call-site hook body (experiment 010): prepend a hidden leading "idx" (the
   * chain index) parameter, so the user's declared args land at frame offsets
   * 16.. and the body reads idx at offset 12 (param 0). No local symbol is
   * created for it -- it is never referenced by name; continue()'s lowering reads
   * the fixed offset directly. The arglist entry is added once (first/addressing
   * pass) and matched on later passes, like any prototyped argument. */
  if (callhook_inject_idx) {
    callhook_inject_idx=0;              /* only the leading parameter of this body */
    if ((sym->usage & uPROTOTYPED)==0) {
      arginfo *na=(arginfo*)realloc(sym->dim.arglist,2*sizeof(arginfo));
      if (na==NULL) {
        error(103);                     /* insufficient memory */
      } else {
        sym->dim.arglist=na;
        memset(&sym->dim.arglist[0],0,sizeof(arginfo));
        memset(&sym->dim.arglist[1],0,sizeof(arginfo)); /* keep the list terminated */
        strcpy(sym->dim.arglist[0].name,"__chook_idx");
        sym->dim.arglist[0].ident=iVARIABLE;
        sym->dim.arglist[0].usage=uCONST;
        sym->dim.arglist[0].numtags=1;
        sym->dim.arglist[0].tags=(int*)malloc(sizeof(int));
        if (sym->dim.arglist[0].tags!=NULL)
          sym->dim.arglist[0].tags[0]=0;
      } /* if */
    } /* if */
    argcnt=1;                           /* user args continue at slot 1 (offset 16) */
  } /* if */
  /* the '(' parentheses has already been parsed */
  if (!matchtoken(')')){
    do {                                /* there are arguments; process them */
      /* any legal name increases argument count (and stack offset) */
      tok=lex(&val,&ptr);
      switch (tok) {
      case 0:
        /* nothing */
        break;
      case '&':
        if (ident!=iVARIABLE || numtags>0)
          error(1,sc_tokens[tSYMBOL-tFIRST],"&");
        if (fconst)
          error(238, "const reference"); /* meaningless combination of class specifiers */
        ident=iREFERENCE;
        break;
      case tCONST:
        if (ident!=iVARIABLE || numtags>0 || fpragma)
          error(1,sc_tokens[tSYMBOL-tFIRST],sc_tokens[tCONST-tFIRST]);
        fconst=TRUE;
        break;
      case t__PRAGMA:
        if (ident!=iVARIABLE || numtags>0)
          error(1,sc_tokens[tSYMBOL-tFIRST],sc_tokens[t__PRAGMA-tFIRST]);
        dopragma();
        fpragma=TRUE;
        break;
      case tLABEL:
        if (numtags>0)
          error(1,sc_tokens[tSYMBOL-tFIRST],"-tagname-");
        tags[0]=pc_addtag(ptr);
        numtags=1;
        break;
      case '{':
        if (numtags>0)
          error(1,sc_tokens[tSYMBOL-tFIRST],"-tagname-");
        numtags=0;
        while (numtags<MAXTAGS) {
          if (!matchtoken('_') && !needtoken(tSYMBOL))
            break;
          tokeninfo(&val,&ptr);
          tags[numtags++]=pc_addtag(ptr);
          if (matchtoken('}'))
            break;
          needtoken(',');
        } /* while */
        needtoken(':');
        tok=tLABEL;     /* for outer loop: flag that we have seen a tagname */
        break;
      case tSYMBOL:
        if (argcnt>=sMAXARGS)
          error(45);                    /* too many function arguments */
        strcpy(name,ptr);               /* save symbol name */
        if (name[0]==PUBLIC_CHAR)
          error(56,name);               /* function arguments cannot be public */
        if (numtags==0)
          tags[numtags++]=0;            /* default tag */
        /* Stack layout:
         *   base + 0*sizeof(cell)  == previous "base"
         *   base + 1*sizeof(cell)  == function return address
         *   base + 2*sizeof(cell)  == number of arguments
         *   base + 3*sizeof(cell)  == first argument of the function
         * So the offset of each argument is "(argcnt+3) * sizeof(cell)".
         */
        doarg(name,ident,(argcnt+3)*sizeof(cell),tags,numtags,fpublic,fconst,!!(sym->dim.arglist[argcnt].usage & uWRITTEN),chkshadow,&arg);
        if (fpublic && arg.hasdefault)
          error(59,name);       /* arguments of a public function may not have a default value */
        if ((sym->usage & uPROTOTYPED)==0) {
          /* redimension the argument list, add the entry */
          arginfo* new_arglist=(arginfo*)realloc(sym->dim.arglist,(argcnt+2)*sizeof(arginfo));
          if (new_arglist==NULL)
            error(103);                 /* insufficient memory */
          sym->dim.arglist=new_arglist;
          memset(&sym->dim.arglist[argcnt+1],0,sizeof(arginfo));  /* keep the list terminated */
          sym->dim.arglist[argcnt]=arg;
        } else {
          /* check the argument with the earlier definition */
          if (argcnt>oldargcnt || !argcompare(&sym->dim.arglist[argcnt],&arg))
            error(25);          /* function definition does not match prototype */
          /* may need to free default array argument and the tag list */
          if (arg.ident==iREFARRAY && arg.hasdefault)
            free(arg.defvalue.array.data);
          else if (arg.ident==iVARIABLE
                   && ((arg.hasdefault & uSIZEOF)!=0 || (arg.hasdefault & uTAGOF)!=0))
            free(arg.defvalue.size.symname);
          free(arg.tags);
        } /* if */
        argcnt++;
        ident=iVARIABLE;
        numtags=0;
        fconst=fpragma=FALSE;
        break;
      case tELLIPS:
        if (ident!=iVARIABLE || fpragma)
          error(10);                    /* illegal function or declaration */
        if (fconst)
          error(238, "const variable arguments"); /* meaningless combination of class specifiers */
        if (numtags==0)
          tags[numtags++]=0;            /* default tag */
        if ((sym->usage & uPROTOTYPED)==0) {
          /* redimension the argument list, add the entry iVARARGS */
          arginfo* new_arglist=(arginfo*)realloc(sym->dim.arglist,(argcnt+2)*sizeof(arginfo));
          if (new_arglist==NULL)
            error(103);                 /* insufficient memory */
          sym->dim.arglist=new_arglist;
          memset(&sym->dim.arglist[argcnt+1],0,sizeof(arginfo));  /* keep the list terminated */
          sym->dim.arglist[argcnt].ident=iVARARGS;
          sym->dim.arglist[argcnt].hasdefault=FALSE;
          sym->dim.arglist[argcnt].defvalue.val=0;
          sym->dim.arglist[argcnt].defvalue_tag=0;
          sym->dim.arglist[argcnt].numtags=numtags;
          sym->dim.arglist[argcnt].tags=(int*)malloc(numtags*sizeof tags[0]);
          if (sym->dim.arglist[argcnt].tags==NULL)
            error(103);                 /* insufficient memory */
          memcpy(sym->dim.arglist[argcnt].tags,tags,numtags*sizeof tags[0]);
        } else {
          if (argcnt>oldargcnt || sym->dim.arglist[argcnt].ident!=iVARARGS)
            error(25);          /* function definition does not match prototype */
        } /* if */
        argcnt++;
        break;
      default:
        error(10);                      /* illegal function or declaration */
      } /* switch */
    } while (tok=='&' || tok==tLABEL || tok==tCONST || tok==t__PRAGMA
             || (tok!=tELLIPS && matchtoken(','))); /* more? */
    /* if the next token is not ",", it should be ")" */
    needtoken(')');
  } /* if */
  /* resolve any "sizeof" arguments (now that all arguments are known) */
  assert(sym->dim.arglist!=NULL);
  arglist=sym->dim.arglist;
  for (idx=0; idx<argcnt && arglist[idx].ident!=0; idx++) {
    if ((arglist[idx].hasdefault & uSIZEOF)!=0 || (arglist[idx].hasdefault & uTAGOF)!=0) {
      int altidx;
      /* Find the argument with the name mentioned after the "sizeof". Note
       * that we cannot use findloc here because we need the arginfo struct,
       * not the symbol.
       */
      ptr=arglist[idx].defvalue.size.symname;
      assert(ptr!=NULL);
      for (altidx=0; altidx<argcnt && strcmp(ptr,arglist[altidx].name)!=0; altidx++)
        /* nothing */;
      if (altidx>=argcnt) {
        error(17,ptr);                  /* undefined symbol */
      } else {
        assert(arglist[idx].defvalue.size.symname!=NULL);
        /* check the level against the number of dimensions */
        if (arglist[idx].defvalue.size.level>0
            && arglist[idx].defvalue.size.level>=arglist[altidx].numdim)
          error(28,arglist[idx].name);  /* invalid subscript */
        /* check the type of the argument whose size to take; for a iVARIABLE
         * or a iREFERENCE, this is always 1 (so the code is redundant)
         */
        assert(arglist[altidx].ident!=iVARARGS);
        if (arglist[altidx].ident!=iREFARRAY && (arglist[idx].hasdefault & uSIZEOF)!=0) {
          if ((arglist[idx].hasdefault & uTAGOF)!=0) {
            error(81,arglist[idx].name);  /* cannot take "tagof" an indexed array */
          } else {
            assert(arglist[altidx].ident==iVARIABLE || arglist[altidx].ident==iREFERENCE);
            error(223,ptr);             /* redundant sizeof */
          } /* if */
        } /* if */
      } /* if */
    } /* if */
  } /* for */

  sym->usage|=uPROTOTYPED;
  errorset(sRESET,0);           /* reset error flag (clear the "panic mode")*/
  return argcnt;
}

/*  doarg       - declare one argument type
 *
 *  this routine is called from "declargs()" and adds an entry in the local
 *  symbol table for one argument.
 *
 *  "fpublic" indicates whether the function for this argument list is public.
 *  The arguments themselves are never public.
 */
static void doarg(char *name,int ident,int offset,int tags[],int numtags,
                  int fpublic,int fconst,int written,int chkshadow,arginfo *arg)
{
  symbol *argsym;
  constvalue_root *enumroot=NULL;
  cell size;

  strcpy(arg->name,name);
  arg->hasdefault=FALSE;        /* preset (most common case) */
  arg->defvalue.val=0;          /* clear */
  arg->defvalue_tag=0;
  arg->numdim=0;
  if (matchtoken('[')) {
    if (ident==iREFERENCE)
      error(67,name);           /* illegal declaration ("&name[]" is unsupported) */
    do {
      if (arg->numdim == sDIMEN_MAX) {
        error(53);              /* exceeding maximum number of dimensions */
        return;
      } /* if */
      size=needsub(&arg->idxtag[arg->numdim],&enumroot);/* may be zero here, it is a pointer anyway */
      #if INT_MAX < LONG_MAX
        if (size > INT_MAX)
          error(105);           /* overflow, exceeding capacity */
      #endif
      arg->dim[arg->numdim]=(int)size;
      arg->numdim+=1;
    } while (matchtoken('['));
    ident=iREFARRAY;            /* "reference to array" (is a pointer) */
    if (matchtoken('=')) {
      lexpush();                /* initials() needs the "=" token again */
      assert(litidx==0);        /* at the start of a function, this is reset */
      assert(numtags>0);
      initials(ident,tags[0],&size,arg->dim,arg->numdim,enumroot,NULL);
      assert(size>=litidx);
      /* allocate memory to hold the initial values */
      arg->defvalue.array.data=(cell *)malloc(litidx*sizeof(cell));
      if (arg->defvalue.array.data!=NULL) {
        int i;
        memcpy(arg->defvalue.array.data,litq,litidx*sizeof(cell));
        arg->hasdefault=TRUE;   /* argument has default value */
        arg->defvalue.array.size=litidx;
        arg->defvalue.array.addr=-1;
        /* calculate size to reserve on the heap */
        arg->defvalue.array.arraysize=1;
        for (i=0; i<arg->numdim; i++)
          arg->defvalue.array.arraysize*=arg->dim[i];
        if (arg->defvalue.array.arraysize < arg->defvalue.array.size)
          arg->defvalue.array.arraysize = arg->defvalue.array.size;
      } /* if */
      litidx=0;                 /* reset */
    } /* if */
  } else {
    if (matchtoken('=')) {
      unsigned char size_tag_token;
      assert(ident==iVARIABLE || ident==iREFERENCE);
      arg->hasdefault=TRUE;     /* argument has a default value */
      size_tag_token=(unsigned char)(matchtoken(tSIZEOF) ? uSIZEOF : 0);
      if (size_tag_token==0)
        size_tag_token=(unsigned char)(matchtoken(tTAGOF) ? uTAGOF : 0);
      if (size_tag_token!=0) {
        char* symname;
        cell val;
        int parentheses;
        if (ident==iREFERENCE)
          error(66,name);       /* argument may not be a reference */
        parentheses=0;
        while (matchtoken('('))
          parentheses++;
        if (size_tag_token==uTAGOF && matchtoken(tLABEL)) {
          constvalue *tagsym;
          tokeninfo(&val,&symname);
          tagsym=find_constval(&tagname_tab,symname,0);
          arg->defvalue.val=(tagsym!=NULL) ? tagsym->value : (cell)0;
          arg->hasdefault |= uTAGOF_TAG;
        } else if (needtoken(tSYMBOL)) {
          /* save the name of the argument whose size id to take */
          tokeninfo(&val,&symname);
          if ((arg->defvalue.size.symname=duplicatestring(symname)) == NULL)
            error(103);         /* insufficient memory */
          arg->defvalue.size.level=0;
          if (size_tag_token==uSIZEOF) {
            while (matchtoken('[')) {
              arg->defvalue.size.level+=(short)1;
              needtoken(']');
            } /* while */
          } /* if */
          if (ident==iVARIABLE) /* make sure we set this only if not a reference */
            arg->hasdefault |= size_tag_token;  /* uSIZEOF or uTAGOF */
        } else {
          /* ignore the argument, otherwise it would cause more error messages, until it
           * will trigger a fatal error because of too many error messages on one line */
          lexclr(FALSE);
        } /* if */
        while (parentheses--)
          needtoken(')');
      } else {
        constexpr(&arg->defvalue.val,&arg->defvalue_tag,NULL);
        assert(numtags>0);
        check_tagmismatch(tags[0],arg->defvalue_tag,TRUE,-1);
      } /* if */
    } /* if */
  } /* if */
  arg->ident=(char)ident;
  arg->usage=(char)(fconst ? uCONST : 0);
  arg->usage|=(char)(written ? uWRITTEN : 0);
  arg->numtags=numtags;
  arg->tags=(int*)malloc(numtags*sizeof tags[0]);
  if (arg->tags==NULL)
    error(103);                 /* insufficient memory */
  memcpy(arg->tags,tags,numtags*sizeof tags[0]);
  argsym=findloc(name);
  if (argsym!=NULL) {
    error(21,name);             /* symbol already defined */
  } else {
    if (chkshadow && (argsym=findglb(name,sSTATEVAR))!=NULL && argsym->ident!=iFUNCTN)
      error(219,name);          /* variable shadows another symbol */
    /* add details of type and address */
    assert(numtags>0);
    argsym=addvariable(name,offset,ident,sLOCAL,tags[0],
                       arg->dim,arg->numdim,arg->idxtag,0);
    if (fpublic) {
      argsym->usage|=uREAD;     /* arguments of public functions are always "used" */
      if(argsym->ident==iREFARRAY || argsym->ident==iREFERENCE)
        argsym->usage|=uWRITTEN;
    } else if (argsym->ident==iVARIABLE) {
      argsym->usage|=uASSIGNED;
      argsym->assignlevel=1;
    } /* if */

    if (fconst)
      argsym->usage|=uCONST;
  } /* if */
  if (matchtoken(t__PRAGMA))
    dopragma();
  pragma_apply(argsym);
}

static int has_referrers(symbol *entry)
{
  int i;
  for (i=0; i<entry->numrefers; i++)
    if (entry->refer[i]!=NULL)
      return TRUE;
  return ((entry->usage & uGLOBALREF)!=0);
}

#if !defined SC_LIGHT
static int find_xmltag(char *source,char *xmltag,char *xmlparam,char *xmlvalue,
                       char **outer_start,int *outer_length,
                       char **inner_start,int *inner_length)
{
  char *ptr,*inner_end;
  int xmltag_len,xmlparam_len,xmlvalue_len;
  int match;

  assert(source!=NULL);
  assert(xmltag!=NULL);
  assert(outer_start!=NULL);
  assert(outer_length!=NULL);
  assert(inner_start!=NULL);
  assert(inner_length!=NULL);

  /* both NULL or both non-NULL */
  assert(xmlvalue!=NULL && xmlparam!=NULL || xmlvalue==NULL && xmlparam==NULL);

  xmltag_len=strlen(xmltag);
  xmlparam_len= (xmlparam!=NULL) ? strlen(xmlparam) : 0;
  xmlvalue_len= (xmlvalue!=NULL) ? strlen(xmlvalue) : 0;
  ptr=source;
  /* find an opening '<' */
  while ((ptr=strchr(ptr,'<'))!=NULL) {
    *outer_start=ptr;           /* be optimistic... */
    match=FALSE;                /* ...and pessimistic at the same time */
    ptr++;                      /* skip '<' */
    while (*ptr!='\0' && *ptr<=' ')
      ptr++;                    /* skip white space */
    if (strncmp(ptr,xmltag,xmltag_len)==0 && (*(ptr+xmltag_len)<=' ' || *(ptr+xmltag_len)=='>')) {
      /* xml tag found, optionally check the parameter */
      ptr+=xmltag_len;
      while (*ptr!='\0' && *ptr<=' ')
        ptr++;                  /* skip white space */
      if (xmlparam!=NULL) {
        if (strncmp(ptr,xmlparam,xmlparam_len)==0 && (*(ptr+xmlparam_len)<=' ' || *(ptr+xmlparam_len)=='=')) {
          ptr+=xmlparam_len;
          while (*ptr!='\0' && *ptr<=' ')
            ptr++;              /* skip white space */
          if (*ptr=='=') {
            ptr++;              /* skip '=' */
            while (*ptr!='\0' && *ptr<=' ')
              ptr++;            /* skip white space */
            if (*ptr=='"' || *ptr=='\'')
              ptr++;            /* skip " or ' */
            assert(xmlvalue!=NULL);
            if (strncmp(ptr,xmlvalue,xmlvalue_len)==0
                && (*(ptr+xmlvalue_len)<=' '
                    || *(ptr+xmlvalue_len)=='>'
                    || *(ptr+xmlvalue_len)=='"'
                    || *(ptr+xmlvalue_len)=='\''))
              match=TRUE;       /* found it */
          } /* if */
        } /* if */
      } else {
        match=TRUE;             /* don't check the parameter */
      } /* if */
    } /* if */
    if (match) {
      /* now find the end of the opening tag */
      while (*ptr!='\0' && *ptr!='>')
        ptr++;
      if (*ptr=='>')
        ptr++;
      while (*ptr!='\0' && *ptr<=' ')
        ptr++;                  /* skip white space */
      *inner_start=ptr;
      /* find the start of the closing tag (assume no nesting) */
      while ((ptr=strchr(ptr,'<'))!=NULL) {
        inner_end=ptr;
        ptr++;                  /* skip '<' */
        while (*ptr!='\0' && *ptr<=' ')
          ptr++;                /* skip white space */
        if (*ptr=='/') {
          ptr++;                /* skip / */
          while (*ptr!='\0' && *ptr<=' ')
            ptr++;              /* skip white space */
          if (strncmp(ptr,xmltag,xmltag_len)==0 && (*(ptr+xmltag_len)<=' ' || *(ptr+xmltag_len)=='>')) {
            /* find the end of the closing tag */
            while (*ptr!='\0' && *ptr!='>')
              ptr++;
            if (*ptr=='>')
              ptr++;
            /* set the lengths of the inner and outer segment */
            assert(*inner_start!=NULL);
            *inner_length=(int)(inner_end-*inner_start);
            assert(*outer_start!=NULL);
            *outer_length=(int)(ptr-*outer_start);
            break;              /* break out of the loop */
          } /* if */
        } /* if */
      } /* while */
      return TRUE;
    } /* if */
  } /* while */
  return FALSE; /* not found */
}

static char *xmlencode(char *dest,char *source)
{
  char temp[2*sNAMEMAX+20],*ptr;

  /* replace < by &lt; and such; normally, such a symbol occurs at most once in
   * a symbol name (e.g. "operator<")
   */
  ptr=temp;
  while (*source!='\0') {
    switch (*source) {
    case '<':
      strcpy(ptr,"&lt;");
      ptr+=4;
      break;
    case '>':
      strcpy(ptr,"&gt;");
      ptr+=4;
      break;
    case '&':
      strcpy(ptr,"&amp;");
      ptr+=5;
      break;
    default:
      *ptr++=*source;
    } /* switch */
    source++;
  } /* while */
  *ptr='\0';
  strcpy(dest,temp);
  return dest;
}

static void make_report(symbol *root,FILE *log,char *sourcefile)
{
  char symname[_MAX_PATH];
  int i,arg;
  symbol *sym,*ref;
  constvalue *tagsym;
  constvalue_root *enumroot;
  char *ptr;

  /* adapt the installation directory */
  strcpy(symname,sc_rootpath);
  #if DIRSEP_CHAR=='\\'
    while ((ptr=strchr(symname,':'))!=NULL)
      *ptr='|';
    while ((ptr=strchr(symname,DIRSEP_CHAR))!=NULL)
      *ptr='/';
  #endif

  /* the XML header */
  fprintf(log,"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n");
  fprintf(log,"<?xml-stylesheet href=\"file:///%s/xml/pawndoc.xsl\" type=\"text/xsl\"?>\n",symname);
  fprintf(log,"<doc source=\"%s\">\n",sourcefile);
  ptr=strrchr(sourcefile,DIRSEP_CHAR);
  if (ptr!=NULL)
    ptr++;
  else
    ptr=sourcefile;
  fprintf(log,"\t<assembly>\n\t\t<name>%s</name>\n\t</assembly>\n",ptr);

  /* attach the global documentation, if any */
  if (sc_documentation!=NULL) {
    fprintf(log,"\n\t<!-- general -->\n");
    fprintf(log,"\t<general>\n\t\t");
    fputs(sc_documentation,log);
    fprintf(log,"\n\t</general>\n\n");
  } /* if */

  /* use multiple passes to print constants variables and functions in
   * separate sections
   */
  fprintf(log,"\t<members>\n");

  fprintf(log,"\n\t\t<!-- enumerations -->\n");
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->parent!=NULL)
      continue;                 /* hierarchical data type */
    assert(sym->ident==iCONSTEXPR || sym->ident==iVARIABLE
           || sym->ident==iARRAY || sym->ident==iFUNCTN);
    if (sym->ident!=iCONSTEXPR || (sym->usage & uENUMROOT)==0)
      continue;
    if ((sym->usage & uREAD)==0)
      continue;
    fprintf(log,"\t\t<member name=\"T:%s\" value=\"%"PRIdC"\">\n",funcdisplayname(symname,sym->name),sym->addr);
    if (sym->tag!=0) {
      tagsym=find_tag_byval(sym->tag);
      assert(tagsym!=NULL);
      fprintf(log,"\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
    } /* if */
    /* browse through all fields */
    if ((enumroot=sym->dim.enumlist)!=NULL) {
      constvalue *cur=enumroot->first;  /* skip root */
      while (cur!=NULL) {
        fprintf(log,"\t\t\t<member name=\"C:%s\" value=\"%"PRIdC"\">\n",funcdisplayname(symname,cur->name),cur->value);
        /* find the constant with this name and get the tag */
        ref=findglb(cur->name,sGLOBAL);
        if (ref!=NULL) {
          if (ref->x.tags.index!=0) {
            tagsym=find_tag_byval(ref->x.tags.index);
            assert(tagsym!=NULL);
            fprintf(log,"\t\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
          } /* if */
          if (ref->dim.array.length!=1)
            fprintf(log,"\t\t\t\t<size value=\"%ld\"/>\n",(long)ref->dim.array.length);
        } /* if */
        fprintf(log,"\t\t\t</member>\n");
        cur=cur->next;
      } /* while */
    } /* if */
    assert(sym->refer!=NULL);
    for (i=0; i<sym->numrefers; i++) {
      if ((ref=sym->refer[i])!=NULL)
        fprintf(log,"\t\t\t<referrer name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    } /* for */
    if (sym->documentation!=NULL)
      fprintf(log,"\t\t\t%s\n",sym->documentation);
    fprintf(log,"\t\t</member>\n");
  } /* for */

  fprintf(log,"\n\t\t<!-- constants -->\n");
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->parent!=NULL)
      continue;                 /* hierarchical data type */
    assert(sym->ident==iCONSTEXPR || sym->ident==iVARIABLE
           || sym->ident==iARRAY || sym->ident==iFUNCTN);
    if (sym->ident!=iCONSTEXPR)
      continue;
    if ((sym->usage & uREAD)==0 || (sym->usage & (uENUMFIELD | uENUMROOT))!=0)
      continue;
    fprintf(log,"\t\t<member name=\"C:%s\" value=\"%"PRIdC"\">\n",funcdisplayname(symname,sym->name),sym->addr);
    if (sym->tag!=0) {
      tagsym=find_tag_byval(sym->tag);
      assert(tagsym!=NULL);
      fprintf(log,"\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
    } /* if */
    assert(sym->refer!=NULL);
    for (i=0; i<sym->numrefers; i++) {
      if ((ref=sym->refer[i])!=NULL)
        fprintf(log,"\t\t\t<referrer name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    } /* for */
    if (sym->documentation!=NULL)
      fprintf(log,"\t\t\t%s\n",sym->documentation);
    fprintf(log,"\t\t</member>\n");
  } /* for */

  fprintf(log,"\n\t\t<!-- variables -->\n");
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->parent!=NULL)
      continue;                 /* hierarchical data type */
    if (sym->ident!=iVARIABLE && sym->ident!=iARRAY)
      continue;
    fprintf(log,"\t\t<member name=\"F:%s\">\n",funcdisplayname(symname,sym->name));
    if (sym->tag!=0) {
      tagsym=find_tag_byval(sym->tag);
      assert(tagsym!=NULL);
      fprintf(log,"\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
    } /* if */
    assert(sym->refer!=NULL);
    if ((sym->usage & uPUBLIC)!=0)
      fprintf(log,"\t\t\t<attribute name=\"public\"/>\n");
    for (i=0; i<sym->numrefers; i++) {
      if ((ref=sym->refer[i])!=NULL)
        fprintf(log,"\t\t\t<referrer name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    } /* for */
    if (sym->documentation!=NULL)
      fprintf(log,"\t\t\t%s\n",sym->documentation);
    fprintf(log,"\t\t</member>\n");
  } /* for */

  fprintf(log,"\n\t\t<!-- functions -->\n");
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->parent!=NULL)
      continue;                 /* hierarchical data type */
    if (sym->ident!=iFUNCTN)
      continue;
    if ((sym->usage & (uREAD | uNATIVE))==uNATIVE)
      continue;                 /* unused native function */
    funcdisplayname(symname,sym->name);
    xmlencode(symname,symname);
    fprintf(log,"\t\t<member name=\"M:%s\" syntax=\"%s(",symname,symname);
    /* print only the names of the parameters between the parentheses */
    assert(sym->dim.arglist!=NULL);
    for (arg=0; sym->dim.arglist[arg].ident!=0; arg++) {
      int dim;
      if (arg>0)
        fprintf(log,", ");
      switch (sym->dim.arglist[arg].ident) {
      case iVARIABLE:
        fprintf(log,"%s",sym->dim.arglist[arg].name);
        break;
      case iREFERENCE:
        fprintf(log,"&amp;%s",sym->dim.arglist[arg].name);
        break;
      case iREFARRAY:
        fprintf(log,"%s",sym->dim.arglist[arg].name);
        for (dim=0; dim<sym->dim.arglist[arg].numdim;dim++)
          fprintf(log,"[]");
        break;
      case iVARARGS:
        fprintf(log,"...");
        break;
      } /* switch */
    } /* for */
    /* ??? should also print an "array return" size */
    fprintf(log,")\">\n");
    if (sym->tag!=0) {
      tagsym=find_tag_byval(sym->tag);
      assert(tagsym!=NULL);
      fprintf(log,"\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
    } /* if */
    /* check whether this function is called from the outside */
    if ((sym->usage & uNATIVE)!=0)
      fprintf(log,"\t\t\t<attribute name=\"native\"/>\n");
    if ((sym->usage & uPUBLIC)!=0)
      fprintf(log,"\t\t\t<attribute name=\"public\"/>\n");
    if (strcmp(sym->name,uMAINFUNC)==0 || strcmp(sym->name,uENTRYFUNC)==0)
      fprintf(log,"\t\t\t<attribute name=\"entry\"/>\n");
    if ((sym->usage & uNATIVE)==0)
      fprintf(log,"\t\t\t<stacksize value=\"%ld\"/>\n",(long)sym->x.stacksize);
    if (sym->states!=NULL) {
      constvalue *stlist=sym->states->first;
      assert(stlist!=NULL);     /* there should be at least one state item */
      while (stlist!=NULL && stlist->index==-1)
        stlist=stlist->next;
      assert(stlist!=NULL);     /* state id should be found */
      i=state_getfsa(stlist->index);
      assert(i>=0);             /* automaton 0 exists */
      stlist=automaton_findid(i);
      assert(stlist!=NULL);     /* automaton should be found */
      fprintf(log,"\t\t\t<automaton name=\"%s\"/>\n", !strempty(stlist->name) ? stlist->name : "(anonymous)");
      //??? dump state decision table
    } /* if */
    assert(sym->refer!=NULL);
    for (i=0; i<sym->numrefers; i++)
      if ((ref=sym->refer[i])!=NULL)
        fprintf(log,"\t\t\t<referrer name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    /* print all symbols that are required for this function to compile */
    for (ref=root->next; ref!=NULL; ref=ref->next) {
      if (ref==sym)
        continue;
      for (i=0; i<ref->numrefers; i++)
        if (ref->refer[i]==sym)
          fprintf(log,"\t\t\t<dependency name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    } /* for */
    /* print parameter list, with tag & const information, plus descriptions */
    assert(sym->dim.arglist!=NULL);
    for (arg=0; sym->dim.arglist[arg].ident!=0; arg++) {
      int dim,paraminfo;
      char *outer_start,*inner_start;
      int outer_length,inner_length;
      if (sym->dim.arglist[arg].ident==iVARARGS)
        fprintf(log,"\t\t\t<param name=\"...\">\n");
      else
        fprintf(log,"\t\t\t<param name=\"%s\">\n",sym->dim.arglist[arg].name);
      /* print the tag name(s) for each parameter */
      assert(sym->dim.arglist[arg].numtags>0);
      assert(sym->dim.arglist[arg].tags!=NULL);
      paraminfo=(sym->dim.arglist[arg].numtags>1 || sym->dim.arglist[arg].tags[0]!=0)
                || sym->dim.arglist[arg].ident==iREFERENCE
                || sym->dim.arglist[arg].ident==iREFARRAY;
      if (paraminfo)
        fprintf(log,"\t\t\t\t<paraminfo>");
      if (sym->dim.arglist[arg].numtags>1 || sym->dim.arglist[arg].tags[0]!=0) {
        assert(paraminfo);
        if (sym->dim.arglist[arg].numtags>1)
          fprintf(log," {");
        for (i=0; i<sym->dim.arglist[arg].numtags; i++) {
          if (i>0)
            fprintf(log,",");
          tagsym=find_tag_byval(sym->dim.arglist[arg].tags[i]);
          assert(tagsym!=NULL);
          fprintf(log,"%s",tagsym->name);
        } /* for */
        if (sym->dim.arglist[arg].numtags>1)
          fprintf(log,"}");
      } /* if */
      switch (sym->dim.arglist[arg].ident) {
      case iREFERENCE:
        fprintf(log," &amp;");
        break;
      case iREFARRAY:
        fprintf(log," ");
        for (dim=0; dim<sym->dim.arglist[arg].numdim; dim++) {
          if (sym->dim.arglist[arg].dim[dim]==0) {
            fprintf(log,"[]");
          } else {
            //??? find index tag
            fprintf(log,"[%d]",sym->dim.arglist[arg].dim[dim]);
          } /* if */
        } /* for */
        break;
      } /* switch */
      if (paraminfo)
        fprintf(log," </paraminfo>\n");
      /* print the user description of the parameter (parse through
       * sym->documentation)
       */
      if (sym->documentation!=NULL
          && find_xmltag(sym->documentation, "param", "name", sym->dim.arglist[arg].name,
                         &outer_start, &outer_length, &inner_start, &inner_length))
      {
        char *tail;
        fprintf(log,"\t\t\t\t%.*s\n",inner_length,inner_start);
        /* delete from documentation string */
        tail=outer_start+outer_length;
        memmove(outer_start,tail,strlen(tail)+1);
      } /* if */
      fprintf(log,"\t\t\t</param>\n");
    } /* for */
    if (sym->documentation!=NULL)
      fprintf(log,"\t\t\t%s\n",sym->documentation);
    fprintf(log,"\t\t</member>\n");
  } /* for */

  fprintf(log,"\n\t</members>\n");
  fprintf(log,"</doc>\n");
}
#endif

/* Every symbol has a referrer list, that contains the functions that use
 * the symbol. Now, if function "apple" is accessed by functions "banana" and
 * "citron", but neither function "banana" nor "citron" are used by anyone
 * else, then, by inference, function "apple" is not used either.
 */
static void reduce_referrers(symbol *root)
{
  int i,restart;
  symbol *sym,*ref;

  do {
    restart=0;
    for (sym=root->next; sym!=NULL; sym=sym->next) {
      if (sym->parent!=NULL)
        continue;                 /* hierarchical data type */
      if (sym->ident==iFUNCTN
          && (sym->usage & uNATIVE)==0
          && (sym->usage & uPUBLIC)==0 && strcmp(sym->name,uMAINFUNC)!=0 && strcmp(sym->name,uENTRYFUNC)!=0
          && !has_referrers(sym))
      {
        sym->usage&=~(uREAD | uWRITTEN);  /* erase usage bits if there is no referrer */
        /* find all symbols that are referred by this symbol */
        for (ref=root->next; ref!=NULL; ref=ref->next) {
          if (ref->parent!=NULL)
            continue;             /* hierarchical data type */
          assert(ref->refer!=NULL);
          for (i=0; i<ref->numrefers && ref->refer[i]!=sym; i++)
            /* nothing */;
          if (i<ref->numrefers) {
            assert(ref->refer[i]==sym);
            ref->refer[i]=NULL;
            restart++;
          } /* if */
        } /* for */
      } else if ((sym->ident==iVARIABLE || sym->ident==iARRAY)
                 && (sym->usage & uPUBLIC)==0
                 && sym->parent==NULL
                 && !has_referrers(sym))
      {
        sym->usage&=~(uREAD | uWRITTEN);  /* erase usage bits if there is no referrer */
      } /* if */
    } /* for */
    /* after removing a symbol, check whether more can be removed */
  } while (restart>0);
}

#if !defined SC_LIGHT
static long max_stacksize_recurse(symbol **sourcesym,symbol *sym,symbol **rsourcesym,long basesize,int *pubfuncparams,int *recursion)
{
  long size,maxsize;
  int i,stkpos;

  assert(sourcesym!=NULL);
  assert(sym!=NULL);
  assert(sym->ident==iFUNCTN);
  assert((sym->usage & uNATIVE)==0);
  assert(recursion!=NULL);

  maxsize=sym->x.stacksize;
  for (i=0; i<sym->numrefers; i++) {
    if (sym->refer[i]!=NULL) {
      assert(sym->refer[i]->ident==iFUNCTN);
      assert((sym->refer[i]->usage & uNATIVE)==0); /* a native function cannot refer to a user-function */
      *(rsourcesym)=sym;
      *(rsourcesym+1)=NULL;
      for (stkpos=0; sourcesym[stkpos]!=NULL; stkpos++) {
        if (sym->refer[i]==sourcesym[stkpos]) {   /* recursion detection */
          *recursion=TRUE;
          goto break_recursion;         /* recursion was detected, quit loop */
        } /* if */
      } /* for */
      /* add this symbol to the stack */
      sourcesym[stkpos]=sym;
      sourcesym[stkpos+1]=NULL;
      /* check size of callee */
      size=max_stacksize_recurse(sourcesym,sym->refer[i],rsourcesym+1,sym->x.stacksize,pubfuncparams,recursion);
      if (maxsize<size)
        maxsize=size;
      /* remove this symbol from the stack */
      sourcesym[stkpos]=NULL;
    } /* if */
  } /* for */
  break_recursion:

  if ((sym->usage & uPUBLIC)!=0) {
    /* Find out how many parameters a public function has, then see if this
     * is bigger than some maximum
     */
    arginfo *arg=sym->dim.arglist;
    int count=0;
    assert(arg!=0);
    while (arg->ident!=0) {
      count++;
      arg++;
    } /* while */
    assert(pubfuncparams!=0);
    if (count>*pubfuncparams)
      *pubfuncparams=count;
  } /* if */

  return maxsize+basesize;
}

static long max_stacksize(symbol *root,int *recursion)
{
  /* Loop over all non-native functions. For each function, loop
   * over all of its referrers, accumulating the stack requirements.
   * Detect (indirect) recursion with a "mark-and-sweep" algorithm.
   * I (mis-)use the "compound" field of the symbol structure for
   * the marker, as this field is unused for functions.
   *
   * Note that the stack is shared with the heap. A host application
   * may "eat" cells from the heap as well, through amx_Allot(). The
   * stack requirements are thus only an estimate.
   */
  long size,maxsize;
  int maxparams,numfunctions;
  symbol *sym;
  symbol **symstack,**rsymstack;

  assert(root!=NULL);
  assert(recursion!=NULL);
  /* count number of functions (for allocating the stack for recursion detection) */
  numfunctions=0;
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->ident==iFUNCTN) {
      assert(sym->compound==0);
      if ((sym->usage & uNATIVE)==0)
        numfunctions++;
    } /* if */
  } /* for */
  /* allocate function symbol stack */
  symstack=(symbol **)malloc((numfunctions+1)*sizeof(symbol*));
  rsymstack=(symbol **)malloc((numfunctions+1)*sizeof(symbol*));
  if (symstack==NULL || rsymstack==NULL)
    error(103);         /* insufficient memory (fatal error) */
  memset(symstack,0,(numfunctions+1)*sizeof(symbol*));
  memset(rsymstack,0,(numfunctions+1)*sizeof(symbol*));

  maxsize=0;
  maxparams=0;
  *recursion=FALSE;     /* assume no recursion */
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    int recursion_detected;
    /* drop out if this is not a user-implemented function */
    if (sym->ident!=iFUNCTN || (sym->usage & uNATIVE)!=0)
      continue;
    /* accumulate stack size for this symbol */
    symstack[0]=sym;
    assert(symstack[1]==NULL);
    recursion_detected=FALSE;
    size=max_stacksize_recurse(symstack,sym,rsymstack,0L,&maxparams,&recursion_detected);
    if (recursion_detected && pc_recursion) {
      if (rsymstack[1]==NULL) {
        pc_printf("recursion detected: function %s directly calls itself\n", sym->name);
      } else {
        int i;
        pc_printf("recursion detected: function %s indirectly calls itself:\n", sym->name);
        pc_printf("%s ", sym->name);
        for (i=1; rsymstack[i]!=NULL; i++) {
          pc_printf("<- %s ", rsymstack[i]->name);
        }
        pc_printf("<- %s\n", sym->name);
      }
      *recursion=recursion_detected;
    }
    assert(size>=0);
    if (maxsize<size)
      maxsize=size;
  } /* for */

  free((void*)symstack);
  free((void*)rsymstack);
  maxsize++;                  /* +1 because a zero cell is always pushed on top
                               * of the stack to catch stack overwrites */
  return maxsize+(maxparams+1);/* +1 because # of parameters is always pushed on entry */
}
#endif

/*  testsymbols - test for unused local or global variables
 *
 *  "Public" functions are excluded from the check, since these
 *  may be exported to other object modules.
 *  Labels are excluded from the check if the argument 'testlabs'
 *  is 0. Thus, labels are not tested until the end of the function.
 *  Constants may also be excluded (convenient for global constants).
 *
 *  When the nesting level drops below "level", the check stops.
 *
 *  The function returns whether there is an "entry" point for the file.
 *  This flag will only be 1 when browsing the global symbol table.
 */
static int testsymbols(symbol *root,int level,int testlabs,int testconst)
{
  char symname[2*sNAMEMAX+16];
  int entry=FALSE;

  symbol *sym=root->next;
  while (sym!=NULL && sym->compound>=level) {
    switch (sym->ident) {
    case iLABEL:
      if (testlabs) {
        if ((sym->usage & uDEFINE)==0) {
          error_suggest(19,sym->name,NULL,estSYMBOL,esfLABEL);  /* not a label: ... */
        } else if ((sym->usage & uREAD)==0) {
          errorset(sSETPOS,sym->lnumber);
          error(203,sym->name);     /* symbol isn't used: ... */
          errorset(sSETPOS,-1);
        } /* if */
      } /* if */
      break;
    case iFUNCTN:
      if ((sym->usage & (uDEFINE | uREAD | uNATIVE | uSTOCK | uPUBLIC))==uDEFINE) {
        funcdisplayname(symname,sym->name);
        if (!strempty(symname))
          error(203,symname);       /* symbol isn't used ... (and not public/native/stock) */
      } /* if */
      if (((sym->usage & uPUBLIC)!=0 || strcmp(sym->name,uMAINFUNC)==0)
          && (sym->usage & uDEFINE)!=0)
        entry=TRUE;                 /* there is an entry point */
      /* also mark the function to the debug information */
      if (((sym->usage & uREAD)!=0 || (sym->usage & uPUBLIC)!=0) && (sym->usage & uNATIVE)==0)
        insert_dbgsymbol(sym);
      break;
    case iCONSTEXPR:
      if (testconst && (sym->usage & uREAD)==0) {
        errorset(sSETPOS,sym->lnumber);
        error(203,sym->name);       /* symbol isn't used: ... */
        errorset(sSETPOS,-1);
      } /* if */
      break;
    default:
      /* a variable */
      if (sym->parent!=NULL)
        break;                      /* hierarchical data type */
      if ((sym->usage & (uWRITTEN | uREAD | uSTOCK | uPUBLIC))==0) {
        errorset(sSETPOS,sym->lnumber);
        error(203,sym->name,sym->lnumber);  /* symbol isn't used (and not stock) */
        errorset(sSETPOS,-1);
      } else if ((sym->usage & (uREAD | uSTOCK | uPUBLIC))==0 && sym->ident!=iREFERENCE) {
        errorset(sSETPOS,sym->lnumber);
        error(204,sym->name);       /* value assigned to symbol is never used */
        errorset(sSETPOS,-1);
      } else if ((sym->usage & (uWRITTEN | uPUBLIC | uCONST))==0 && sym->ident==iREFARRAY) {
        int warn = TRUE;
        symbol *depend;
        for (depend=sym->child; depend!=NULL; depend=depend->child) {
          if ((depend->usage & (uWRITTEN | uPUBLIC | uCONST))!=0) {
            warn=FALSE;
            break;
          } /* if */
        } /* for */
        if (warn) {
          errorset(sSETPOS, sym->lnumber);
          error(214, sym->name);       /* make array argument "const" */
          errorset(sSETPOS, -1);
        } /* if */
      } /* if */
      /* also mark the variable (local or global) to the debug information */
      if ((sym->usage & (uWRITTEN | uREAD))!=0 && (sym->usage & uNATIVE)==0)
        insert_dbgsymbol(sym);
    } /* switch */
    sym=sym->next;
  } /* while */

  return entry;
}

static void scanloopvariables(symstate **loopvars,int dowhile)
{
  symbol *start,*sym;
  int num;

  /* error messages are only printed on the "writing" pass,
   * so if we are not writing yet, then we have a quick exit */
  if (sc_status!=statWRITE)
    return;

  /* if there's no enclosing loop (only one active loop entry, which is the
   * current loop), and the current loop is not 'do-while', then we don't need
   * to memoize usage flags for local variables, so we have an early exit */
  if (wqptr-wqSIZE==wq && !dowhile)
    return;

  /* skip labels */
  start=&loctab;
  while ((start=start->next)!=NULL && start->ident==iLABEL)
    /* nothing */;
  /* if there are no other local symbols, we have an early exit */
  if (start==NULL)
    return;

  /* count the number of local symbols */
  for (num=0,sym=start; sym!=NULL; num++,sym=sym->next)
    /* nothing */;

  assert(*loopvars==NULL);
  assert(num!=0);
  *loopvars=(symstate *)calloc((size_t)num,sizeof(symstate));
  if (*loopvars==NULL)
    error(103); /* insufficient memory */

  for (num=0,sym=start; sym!=NULL; num++,sym=sym->next) {
    /* If the variable already has the uLOOPVAR flag set (from being used
     * in an enclosing loop), we have to set the uNOLOOPVAR to exclude it
     * from checks in the current loop, ... */
    if ((sym->ident==iVARIABLE || sym->ident==iREFERENCE)
        && (dowhile || (sym->usage & uLOOPVAR)!=0)) {
      /* ... but it might be already set from an enclosing loop as well, so we
       * have to temporarily store it in "loopvars[num]" first. Also, if this is
       * a 'do-while' loop, we need to memoize and unset the 'uWRITTEN' flag, so
       * later when analyzing the loop condition (which comes after the loop
       * body) we'll be able to determine if the variable was modified inside
       * the loop body by checking if the 'uWRITTEN' flag is set. */
      (*loopvars)[num].usage |= (sym->usage & (uNOLOOPVAR | uWRITTEN));
      sym->usage &= ~uWRITTEN;
      if (wqptr-wqSIZE!=wq)
        sym->usage |= uNOLOOPVAR;
    } /* if */
  } /* if */
}

static void testloopvariables(symstate *loopvars,int dowhile,int line)
{
  symbol *start,*sym;
  int num,warnnum=0;

  /* the error messages are only printed on the "writing" pass,
   * so if we are not writing yet, then we have a quick exit */
  if (sc_status!=statWRITE)
    return;

  /* skip labels */
  start=&loctab;
  while ((start=start->next)!=NULL && start->ident==iLABEL)
    /* nothing */;

  /* decrement pc_numloopvars by 1 for each variable that wasn't modified
   * inside the loop body; if pc_numloopvars gets zeroed after this, it would
   * mean none of the variables used inside the loop condition were modified */
  if (pc_numloopvars!=0) {
    warnnum=(pc_numloopvars==1) ? 250 : 251;
    for (sym=start; sym!=NULL; sym=sym->next)
      if ((sym->ident==iVARIABLE || sym->ident==iREFERENCE)
          && (sym->usage & (uLOOPVAR | uNOLOOPVAR))==uLOOPVAR
          && (!dowhile || (sym->usage & uWRITTEN)==0))
        pc_numloopvars--;
    if (pc_numloopvars==0 && warnnum==251) {
      errorset(sSETPOS,line);
      error(251); /* none of the variables used in loop condition are modified in loop body */
      errorset(sSETPOS,-1);
    } /* if */
  } /* if */

  for (num=0,sym=start; sym!=NULL; num++,sym=sym->next) {
    if (sym->ident==iVARIABLE || sym->ident==iREFERENCE) {
      if ((sym->usage & (uLOOPVAR | uNOLOOPVAR))==uLOOPVAR) {
        sym->usage &= ~uLOOPVAR;
        /* warn only if none of the variables used inside the loop condition
         * were modified inside the loop body */
        if (pc_numloopvars==0 && warnnum==250) {
          errorset(sSETPOS,line);
          error(250,sym->name); /* variable used in loop condition not modified in loop body */
          errorset(sSETPOS,-1);
        } /* if */
      } /* if */
      sym->usage &= ~uNOLOOPVAR;
      if (loopvars!=NULL)
        sym->usage |= loopvars[num].usage;
    } /* if */
  } /* for */
  free(loopvars);
}

static cell calc_array_datasize(symbol *sym, cell *offset)
{
  cell length;

  assert(sym!=NULL);
  assert(sym->ident==iARRAY || sym->ident==iREFARRAY);
  length=sym->dim.array.length;
  if (sym->dim.array.level > 0) {
    cell sublength=calc_array_datasize(sym->child,offset);
    if (offset!=NULL)
      *offset=length*(*offset+sizeof(cell));
    if (sublength>0)
      length*=sublength;
    else
      length=0;
  } else {
    if (offset!=NULL)
      *offset=0;
  } /* if */
  return length;
}

static void destructsymbols(symbol *root,int level)
{
  cell offset=0;
  int savepri=FALSE;
  symbol *sym=root->next;
  while (sym!=NULL && sym->compound>=level) {
    if ((sym->ident==iVARIABLE || sym->ident==iARRAY) && !(sym->vclass==sSTATIC && sym->fnumber==-1) && !(sym->usage&uNODESTRUCT)) {
      char symbolname[16];
      symbol *opsym;
      cell elements;
      /* check that the '~' operator is defined for this tag */
      operator_symname(symbolname,"~",sym->tag,0,1,0);
      if ((opsym=findglb(symbolname,sGLOBAL))!=NULL) {
        if ((opsym->usage & uMISSING)!=0 || (opsym->usage & uPROTOTYPED)==0) {
          char symname[2*sNAMEMAX+16];  /* allow space for user defined operators */
          char *ptr= (sym->documentation!=NULL) ? sym->documentation : "";
          funcdisplayname(symname,opsym->name);
          if ((opsym->usage & uMISSING)!=0)
            error(4,symname,ptr);       /* function not defined */
          if ((opsym->usage & uPROTOTYPED)==0)
            error(71,symname);          /* operator must be declared before use */
        } /* if */
        /* save PRI, in case of a return statement */
        if (!savepri) {
          pushreg(sPRI);        /* right-hand operand is in PRI */
          savepri=TRUE;
        } /* if */
        /* if the variable is an array, get the number of elements */
        if (sym->ident==iARRAY) {
          /* according to the PAWN Implementer Guide, the destructor
           * should be triggered for the data of the array only; hence
           * if the array is a part of a larger array, it must be ignored
           * as it's parent would(or has already) trigger(ed) the destructor
           */
          if (sym->parent!=NULL) {
            sym=sym->next;
            continue;
          } /* if */
          elements=calc_array_datasize(sym,&offset);
          /* "elements" can be zero when the variable is declared like
           *    new mytag: myvar[2][] = { {1, 2}, {3, 4} }
           * one should declare all dimensions!
           */
          if (elements==0)
            error(46,sym->name);        /* array size is unknown */
        } else {
          elements=1;
          offset=0;
        } /* if */
        pushval(elements);
        /* call the '~' operator */
        address(sym,sPRI);
        addconst(offset);       /* add offset to array data to the address */
        pushreg(sPRI);
        pushval(2*sizeof(cell));/* 2 parameters */
        assert(opsym->ident==iFUNCTN);
        ffcall(opsym,NULL,1);
        if (sc_status!=statSKIP)
          markusage(opsym,uREAD);   /* do not mark as "used" when this call itself is skipped */
        if ((opsym->usage & uNATIVE)!=0 && opsym->x.lib!=NULL)
          opsym->x.lib->value += 1; /* increment "usage count" of the library */
      } /* if */
    } else if (level==0 && sym->ident==iREFERENCE) {
      sym->usage &= ~uASSIGNED;
    } /* if */
    sym=sym->next;
  } /* while */
  /* restore PRI, if it was saved */
  if (savepri)
    popreg(sPRI);
}

static constvalue *insert_constval(constvalue *prev,constvalue *next,
                                   const char *name,cell val,int index)
{
  constvalue *cur;

  if ((cur=(constvalue*)malloc(sizeof(constvalue)))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  memset(cur,0,sizeof(constvalue));
  if (name!=NULL) {
    assert(strlen(name)<sNAMEMAX+1);
    strcpy(cur->name,name);
  } /* if */
  cur->value=val;
  cur->index=index;
  cur->next=next;
  if (prev!=NULL)
    prev->next=cur;
  return cur;
}

SC_FUNC constvalue *append_constval(constvalue_root *table,const char *name,
                                    cell val,int index)
{
  constvalue *newvalue;

  if (table->last!=NULL) {
    newvalue=insert_constval(table->last,NULL,name,val,index);
  } else {
    newvalue=insert_constval(NULL,NULL,name,val,index);
    table->first=newvalue;
  } /* if */
  table->last=newvalue;
  return newvalue;
}

SC_FUNC constvalue *find_constval(constvalue_root *table,char *name,int index)
{
  constvalue *ptr = table->first;

  while (ptr!=NULL) {
    if (strcmp(name,ptr->name)==0 && ptr->index==index)
      return ptr;
    ptr=ptr->next;
  } /* while */
  return NULL;
}

static constvalue *find_constval_byval(constvalue_root *table,cell val)
{
  constvalue *ptr = table->first;

  while (ptr!=NULL) {
    if (ptr->value==val)
      return ptr;
    ptr=ptr->next;
  } /* while */
  return NULL;
}

#if 0   /* never used */
static int delete_constval(constvalue_root *table,char *name)
{
  constvalue *prev=NULL;
  constvalue *cur=table->first;

  while (cur!=NULL) {
    if (strcmp(name,cur->name)==0) {
      if (prev!=NULL)
        prev->next=cur->next;
      else
        table->first=cur->next;
      if (table->last==cur)
        table->last=prev;
      free(cur);
      return TRUE;
    } /* if */
    prev=cur;
    cur=cur->next;
  } /* while */
  return FALSE;
}
#endif

SC_FUNC void delete_consttable(constvalue_root *table)
{
  constvalue *cur=table->first, *next;

  while (cur!=NULL) {
    next=cur->next;
    free(cur);
    cur=next;
  } /* while */
  memset(table,0,sizeof(constvalue_root));
}

/*  add_constant
 *
 *  Adds a symbol to the symbol table. Returns NULL on failure.
 */
SC_FUNC symbol *add_constant(char *name,cell val,int vclass,int tag)
{
  symbol *sym;

  /* Test whether a global or local symbol with the same name exists. Since
   * constants are stored in the symbols table, this also finds previously
   * defined constants. */
  sym=findglb(name,sSTATEVAR);
  if (sym==NULL)
    sym=findloc(name);
  if (sym!=NULL) {
    int redef=FALSE;
    if (sym->ident!=iCONSTEXPR)
      redef=TRUE;               /* redefinition of a function/variable to a constant is not allowed */
    if ((sym->usage & uENUMFIELD)!=0) {
      /* enum field, special case if it has a different tag and the new symbol is also an enum field */
      constvalue *tagid;
      symbol *tagsym;
      if (sym->tag==tag)
        redef=TRUE;             /* enumeration field is redefined (same tag) */
      tagid=find_tag_byval(tag);
      if (tagid==NULL) {
        redef=TRUE;             /* new constant does not have a tag */
      } else {
        tagsym=findconst(tagid->name,NULL);
        if (tagsym==NULL || (tagsym->usage & uENUMROOT)==0)
          redef=TRUE;           /* new constant is not an enumeration field */
      } /* if */
      /* in this particular case (enumeration field that is part of a different
       * enum, and non-conflicting with plain constants) we want to be able to
       * redefine it
       */
      if (!redef)
        goto redef_enumfield;
    } else if (sym->tag!=tag) {
      redef=TRUE;               /* redefinition of a constant (non-enum) to a different tag is not allowed */
    } /* if */
    if (redef) {
      error(21,name);           /* symbol already defined */
      return NULL;
    } else if (sym->addr!=val || (sym->usage & uENUMROOT)!=0) {
      error(201,name);          /* redefinition of constant (different value) */
      sym->addr=val;            /* set new value */
    } /* if */
    /* silently ignore redefinitions of constants with the same value & tag */
    return sym;
  } /* if */

  /* constant doesn't exist yet (or is allowed to be redefined) */
redef_enumfield:
  sym=addsym(name,val,iCONSTEXPR,vclass,tag,uDEFINE);
  assert(sym!=NULL);            /* fatal error 103 must be given on error */
  if (vclass==sLOCAL)
    sym->compound=pc_nestlevel;
  return sym;
}

/* add_builtin_constant
 *
 * Adds a predefined constant to the symbol table.
 */
SC_FUNC symbol *add_builtin_constant(char *name,cell val,int vclass,int tag)
{
  symbol *sym;

  sym=add_constant(name,val,vclass,tag);
  sym->flags|=flagPREDEF;
  return sym;
}

/*  add_builtin_string_constant
 *
 *  Adds a predefined string constant to the symbol table.
 */
SC_FUNC symbol *add_builtin_string_constant(char *name,const char *val,
                                            int vclass)
{
  symbol *sym;

  /* Test whether a global or local symbol with the same name exists. Since
   * constants are stored in the symbols table, this also finds previously
   * defined constants. */
  sym=findglb(name,sSTATEVAR);
  if (sym==NULL)
    sym=findloc(name);
  if (sym!=NULL) {
    if (sym->ident!=iARRAY) {
      error(21,name);           /* symbol already defined */
      return NULL;
    } /* if */
  } else {
    sym=addsym(name,0,iARRAY,vclass,0,uDEFINE | uSTOCK);
  } /* if */
  sym->addr=(litidx+glb_declared)*sizeof(cell);
  /* Store this constant only if it's used somewhere. This can be detected
   * in the second stage. */
  if (sc_status==statIDLE
      || (sc_status==statWRITE && (sym->usage & uREAD)!=0)) {
    assert(litidx==0);
    begdseg();
    while (*val!='\0')
      litadd(*val++);
    litadd(0);
    glb_declared+=litidx;
    dumplits();
    litidx=0;
  }
  sym->usage|=uDEFINE;
  sym->flags|=flagPREDEF;
  return sym;
}

/*  statement           - The Statement Parser
 *
 *  This routine is called whenever the parser needs to know what statement
 *  it encounters (i.e. whenever program syntax requires a statement).
 */
static void statement(int *lastindent,int allow_decl)
{
  int tok,save;
  cell val;
  char *st;

  if (!freading) {
    error(36);                  /* empty statement */
    return;
  } /* if */
  errorset(sRESET,0);
  pc_exprtemp=0;                /* new statement: no live operand-stack temporaries yet */
  pc_await_composed=0;               /* ...and no "await" suspends emitted yet this statement */

  tok=lex(&val,&st);
  if ((emit_flags & efBLOCK)!=0) {
    emit_parse_line();
    if (matchtoken('}'))
      emit_flags &= ~efBLOCK;
    return;
  } /* if */
  if (tok!='{') {
    insert_dbgline(fline);
    setline(TRUE);
  } /* if */
  /* lex() has set stmtindent */
  if (lastindent!=NULL && tok!=tLABEL) {
    if (*lastindent>=0 && *lastindent!=stmtindent && !indent_nowarn && sc_tabsize>0)
      error(217);               /* loose indentation */
    *lastindent=stmtindent;
    indent_nowarn=FALSE;        /* if warning was blocked, re-enable it */
  } /* if */
  switch (tok) {
  case 0:
    /* nothing */
    break;
  case tSTOCK:
    error(10);                  /* invalid function or declaration */
    /* fallthrough */
  case tNEW:
    if (allow_decl) {
      declloc(FALSE);
      lastst=tNEW;
    } else {
      error(3);                 /* declaration only valid in a block */
    } /* if */
    break;
  case tSTATIC:
    if (matchtoken(tENUM))
      decl_enum(sLOCAL,FALSE);
    else if (allow_decl) {
      declloc(TRUE);
      lastst=tNEW;
    } else {
      error(3);                 /* declaration only valid in a block */
    } /* if */
    break;
  case '{':
  case tBEGIN:
    save=fline;
    if (!matchtoken('}'))       /* {} is the empty statement */
      compound(save==fline,tok);
    /* lastst (for "last statement") does not change */
    break;
  case ';':
    error(36);                  /* empty statement */
    break;
  case tIF:
    lastst=doif();
    break;
  case tWHILE:
    lastst=dowhile();
    break;
  case tDO:
    lastst=dodo();
    break;
  case tFOR:
    lastst=dofor();
    break;
  case tFOREACH:
    lastst=doforeach();
    break;
  case tSWITCH:
    lastst=doswitch();
    break;
  case tCASE:
  case tDEFAULT:
    docase(tok==tDEFAULT);
    break;
  case tGOTO:
    lastst=dogoto();
    break;
  case tLABEL:
    dolabel();
    lastst=tLABEL;
    break;
  case tRETURN:
    doreturn();
    lastst=tRETURN;
    break;
  case tYIELD:
    doyield();
    /* the generator is suspended, not finished: the statement after the
     * "yield" is reached when the generator is resumed */
    lastst=tYIELD;
    break;
  case tINLINE:
    if (allow_decl) {
      doinline();
      lastst=tINLINE;
    } else {
      error(3);                 /* declaration only valid in a block */
    } /* if */
    break;
  case tBREAK:
    dobreak();
    lastst=tBREAK;
    break;
  case tCONTINUE:
    docont();           /* sets lastst itself (tCONTINUE for loop-continue,
                         * tEXPR for the "continue(...)" call-hook intrinsic) */
    break;
  case tEXIT:
    doexit();
    lastst=tEXIT;
    break;
  case tASSERT:
    doassert();
    lastst=tASSERT;
    break;
  case tSLEEP:
    dosleep();
    lastst=tSLEEP;
    break;
  case tSTATE:
    dostate();
    lastst=tSTATE;
    break;
  case tCONST:
    decl_const(sLOCAL);
    break;
  case tENUM:
    matchtoken(tSTATIC);
    decl_enum(sLOCAL,FALSE);
    break;
  case t__PRAGMA:
    dopragma();
    needtoken(tTERM);
    pragma_apply(curfunc);
    break;
  case t__EMIT: {
    const unsigned char *bck_lptr=lptr-strlen(sc_tokens[tok-tFIRST]);
    if (matchtoken('{')) {
      emit_flags |= efBLOCK;
      lastst=t__EMIT;
      break;
    } /* if */
    lptr=bck_lptr;
    lexclr(FALSE);
    tok=lex(&val,&st);
  } /* case */
  /* fallthrough */
  default:          /* non-empty expression */
    sc_allowproccall=optproccall;
    if (!allow_decl)
      pc_nestlevel++;
    lexpush();      /* analyze token later */
    doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE,NULL);
    needtoken(tTERM);
    lastst=tEXPR;
    if (!allow_decl)
      pc_nestlevel--;
    sc_allowproccall=FALSE;
  } /* switch */
}

static void compound(int stmt_sameline,int starttok)
{
  int indent=-1;
  cell save_decl=declared;
  int count_stmt=0;
  int block_start=fline;  /* save line where the compound block started */
  int endtok;

  /* if there is more text on this line, we should adjust the statement indent */
  if (stmt_sameline) {
    int i;
    const unsigned char *p=lptr;
    /* go back to the opening brace */
    while (*p!=starttok) {
      assert(p>pline);
      p--;
    } /* while */
    assert(*p==starttok);  /* it should be found */
    /* go forward, skipping white-space */
    p++;
    while (*p<=' ' && *p!='\0')
      p++;
    assert(*p!='\0'); /* a token should be found */
    stmtindent=0;
    for (i=0; i<(int)(p-pline); i++)
      if (pline[i]=='\t' && sc_tabsize>0)
        stmtindent += (int)(sc_tabsize - (stmtindent+sc_tabsize) % sc_tabsize);
      else
        stmtindent++;
  } /* if */

  endtok=(starttok=='{') ? '}' : tEND;
  pc_nestlevel+=1;              /* increase compound statement level */
  while (matchtoken(endtok)==0){/* repeat until compound statement is closed */
    if (!freading){
      error(30,block_start);    /* compound block not closed at end of file */
      break;
    } else {
      if (count_stmt>0 && isterminal(lastst)) {
        if (matchtoken(tLABEL)) {
          cell val;
          char *name;
          symbol *sym;
          tokeninfo(&val,&name);
          lexpush();            /* push the token so it can be analyzed later */
          sym=findloc(name);
          /* before issuing a warning, check if the label was previously used (via 'goto') */
          if (sym!=NULL && sym->ident==iLABEL && (sym->usage & uREAD)==0)
            error(225);         /* unreachable code */
        } else if (lastst==tTERMSWITCH && matchtoken(tRETURN)) {
          lexpush();            /* push the token so it can be analyzed later */
        } else {
          error(225);           /* unreachable code */
        } /* if */
      } /* if */
      statement(&indent,TRUE);  /* do a statement */
      count_stmt++;
    } /* if */
  } /* while */
  if (lastst!=tRETURN)
    if (pc_nestlevel >= 1 || (curfunc->flags & flagNAKED)==0)
      destructsymbols(&loctab,pc_nestlevel);
  if (!isterminal(lastst) && (pc_nestlevel>=1 || (curfunc->flags & flagNAKED)==0))
      modstk((int)(declared-save_decl)*sizeof(cell)); /* delete local variable space */
  testsymbols(&loctab,pc_nestlevel,FALSE,TRUE);     /* look for unused block locals */
  declared=save_decl;
  delete_symbols(&loctab,pc_nestlevel,FALSE,TRUE);  /* erase local symbols, but
                                                     * retain block local labels
                                                     * (within the function) */
  pc_nestlevel-=1;              /* decrease compound statement level */
}

/*  doexpr
 *
 *  Global references: stgidx   (referred to only)
 */
static int doexpr(int comma,int chkeffect,int allowarray,int mark_endexpr,
                  int *tag,symbol **symptr,int chkfuncresult,cell *val)
{
  int index,ident;
  int localstaging=FALSE;

  if (!staging) {
    stgset(TRUE);               /* start stage-buffering */
    localstaging=TRUE;
    assert(stgidx==0);
  } /* if */
  index=stgidx;
  errorset(sEXPRMARK,0);
  do {
    /* on second round through, mark the end of the previous expression */
    if (index!=stgidx) {
      markexpr(sEXPR,NULL,0);
      /* also, if this is not the first expression and we are inside a "return"
       * statement, we need to manually free the heap space allocated for the
       * array returned by the function called in the previous expression */
      if (pc_retexpr) {
        modheap(pc_retheap);
        pc_retheap=0;
      } /* if */
    } /* if */
    pc_sideeffect=FALSE;
    pc_ovlassignment=FALSE;
    pc_await_composed=0;              /* each comma-clause is an independent operand-stack
                                 * expression: an "await" here does not share a live
                                 * temp with one in a sibling clause (or an earlier
                                 * for-header clause), so start its await count fresh */
    ident=expression(val,tag,symptr,chkfuncresult);
    if (!allowarray && (ident==iARRAY || ident==iREFARRAY))
      error(33,"-unknown-");    /* array must be indexed */
    if (chkeffect && !pc_sideeffect)
      error(215);               /* expression has no effect */
    sc_allowproccall=FALSE;     /* cannot use "procedure call" syntax anymore */
  } while (comma && matchtoken(',')); /* more? */
  if (mark_endexpr)
    markexpr(sEXPR,NULL,0);     /* optionally, mark the end of the expression */
  errorset(sEXPRRELEASE,0);
  if (localstaging) {
    stgout(index);
    stgset(FALSE);              /* stop staging */
  } /* if */
  return ident;
}

/*  constexpr
 */
SC_FUNC int constexpr(cell *val,int *tag,symbol **symptr)
{
  int ident,index;
  cell cidx;

  stgset(TRUE);         /* start stage-buffering */
  stgget(&index,&cidx); /* mark position in code generator */
  errorset(sEXPRMARK,0);
  ident=expression(val,tag,symptr,FALSE);
  stgdel(index,cidx);   /* scratch generated code */
  stgset(FALSE);        /* stop stage-buffering */
  if (ident!=iCONSTEXPR) {
    error(8);           /* must be constant expression */
    if (val!=NULL)
      *val=0;
    if (tag!=NULL)
      *tag=0;
    if (symptr!=NULL)
      *symptr=NULL;
  } /* if */
  errorset(sEXPRRELEASE,0);
  return (ident==iCONSTEXPR);
}

/*  test
 *
 *  In the case a "simple assignment" operator ("=") is used within a test,
 *  the warning "possibly unintended assignment" is displayed. This routine
 *  sets the global variable "sc_intest" to true, it is restored upon termination.
 *  In the case the assignment was intended, use parentheses around the
 *  expression to avoid the warning; primary() sets "sc_intest" to 0.
 *
 *  Global references: sc_intest (altered, but restored upon termination)
 */
static int test(int label,int parens,int invert)
{
  int index,tok;
  cell cidx;
  int ident,tag;
  int endtok;
  cell constval;
  symbol *sym;
  int localstaging=FALSE;

  if (!staging) {
    stgset(TRUE);               /* start staging */
    localstaging=TRUE;
    #if !defined NDEBUG
      stgget(&index,&cidx);     /* should start at zero if started locally */
      assert(index==0);
    #endif
  } /* if */

  PUSHSTK_I(sc_intest);
  sc_intest=TRUE;
  endtok=0;
  pc_await_composed=0;                /* a test condition (if/while/for-cond/do) is its own
                                 * operand-stack expression; count its awaits fresh so
                                 * a for-header "cond" and "incr" await are independent */
  if (parens!=TEST_PLAIN) {
    if (matchtoken('('))
      endtok=')';
    else if (parens==TEST_THEN)
      endtok=tTHEN;
    else if (parens==TEST_DO)
      endtok=tDO;
  } /* if */
  do {
    stgget(&index,&cidx);       /* mark position (of last expression) in
                                 * code generator */
    pc_sideeffect=FALSE;
    ident=expression(&constval,&tag,&sym,TRUE);
    tok=matchtoken(',');
    if (tok) {
      if (!pc_sideeffect)
        error(248);
      markexpr(sEXPR,NULL,0);
    } /* if */
  } while (tok); /* do */
  if (endtok!=0)
    needtoken(endtok);
  if (ident==iARRAY || ident==iREFARRAY) {
    char *ptr=(sym!=NULL) ? sym->name : "-unknown-";
    error(33,ptr);              /* array must be indexed */
  } /* if */
  if (ident==iCONSTEXPR) {      /* constant expression */
    int testtype=0;
    sc_intest=(short)POPSTK_I();/* restore stack */
    stgdel(index,cidx);
    if (constval) {             /* code always executed */
      error(206);               /* redundant test: always non-zero */
      testtype=tENDLESS;
    } else {
      error(205);               /* redundant code: never executed */
      jumplabel(label);
    } /* if */
    if (localstaging) {
      stgout(0);                /* write "jumplabel" code */
      stgset(FALSE);            /* stop staging */
    } /* if */
    return testtype;
  } /* if */
  if (tag!=0 && tag!=BOOLTAG)
    if (check_userop(lneg,tag,0,1,NULL,&tag))
      invert= !invert;          /* user-defined ! operator inverted result */
  if (invert)
    jmp_ne0(label);             /* jump to label if true (different from 0) */
  else
    jmp_eq0(label);             /* jump to label if false (equal to 0) */
  markexpr(sEXPR,NULL,0);       /* end expression (give optimizer a chance) */
  sc_intest=(short)POPSTK_I();  /* double typecast to avoid warning with Microsoft C */
  if (localstaging) {
    stgout(0);                  /* output queue from the very beginning (see
                                 * assert() when localstaging is set to TRUE) */
    stgset(FALSE);              /* stop staging */
  } /* if */
  return 0;
}

static int doif(void)
{
  int flab1,flab2;
  int ifindent;
  int lastst_true;
  int returnst=tIF;
  symstate *assignments=NULL;

  lastst=0;                     /* reset the last statement */
  ifindent=stmtindent;          /* save the indent of the "if" instruction */
  flab1=getlabel();             /* get label number for false branch */
  test(flab1,TEST_THEN,FALSE);  /* get expression, branch to flab1 if false */
  statement(NULL,FALSE);        /* if true, do a statement */
  if (!matchtoken(tELSE)) {     /* if...else ? */
    setlabel(flab1);            /* no, simple if..., print false label */
  } else {
    lastst_true=lastst;         /* save last statement of the "true" branch */
    lastst=0;                   /* reset the last statement */
    /* to avoid the "dangling else" error, we want a warning if the "else"
     * has a lower indent than the matching "if" */
    if (stmtindent<ifindent && sc_tabsize>0)
      error(217);               /* loose indentation */
    memoizeassignments(pc_nestlevel+1,&assignments);
    flab2=getlabel();
    if (!isterminal(lastst))
      jumplabel(flab2);         /* "true" branch jumps around "else" clause, unless the "true" branch statement already jumped */
    setlabel(flab1);            /* print false label */
    statement(NULL,FALSE);      /* do "else" clause */
    setlabel(flab2);            /* print true label */
    /* if both the "true" branch and the "false" branch ended with the same
     * kind of statement, set the last statement id to that kind, rather than
     * to the generic tIF; this allows for better "unreachable code" checking
     */
    if (lastst==lastst_true && lastst!=0)
      returnst=lastst;
    /* otherwise, if both branches end with terminal statements (not necessary
     * of the same kind), set the last statement ID to tTERMINAL */
    else if (isterminal(lastst_true) && isterminal(lastst))
      returnst=tTERMINAL;
  } /* if */
  restoreassignments(pc_nestlevel+1,assignments);
  return returnst;
}

static int dowhile(void)
{
  int wq[wqSIZE];               /* allocate local queue */
  int save_endlessloop,save_numloopvars,retcode;
  int loopline=fline;
  symstate *loopvars=NULL;

  save_endlessloop=endlessloop;
  save_numloopvars=pc_numloopvars;
  pc_numloopvars=0;
  addwhile(wq);                 /* add entry to queue for "break" */
  setlabel(wq[wqLOOP]);         /* loop label */
  /* The debugger uses the "break" opcode to be able to "break" out of
   * a loop. To make sure that each loop has a break opcode, even for the
   * tiniest loop, set it below the top of the loop
   */
  setline(TRUE);
  scanloopvariables(&loopvars,FALSE);
  pc_nestlevel++; /* temporarily increase the "compound statement" nesting level,
                   * so any assignments made inside the loop control expression
                   * could be cleaned up later */
  pc_loopcond=tWHILE;
  endlessloop=test(wq[wqEXIT],TEST_DO,FALSE);/* branch to wq[wqEXIT] if false */
  pc_loopcond=0;
  statement(NULL,FALSE);        /* if so, do a statement */
  pc_nestlevel--;
  clearassignments(pc_nestlevel+1);
  testloopvariables(loopvars,FALSE,loopline);
  jumplabel(wq[wqLOOP]);        /* and loop to "while" start */
  setlabel(wq[wqEXIT]);         /* exit label */
  delwhile();                   /* delete queue entry */

  retcode=endlessloop ? tENDLESS : tWHILE;
  pc_numloopvars=save_numloopvars;
  endlessloop=save_endlessloop;
  return retcode;
}

/*
 *  Note that "continue" will in this case not jump to the top of the loop, but
 *  to the end: just before the TRUE-or-FALSE testing code.
 */
static int dodo(void)
{
  int wq[wqSIZE],top;
  int save_endlessloop,save_numloopvars,retcode;
  int loopline=fline;
  symstate *loopvars=NULL;

  save_endlessloop=endlessloop;
  save_numloopvars=pc_numloopvars;
  pc_numloopvars=0;
  addwhile(wq);           /* see "dowhile" for more info */
  top=getlabel();         /* make a label first */
  setlabel(top);          /* loop label */
  scanloopvariables(&loopvars,TRUE);
  statement(NULL,FALSE);
  needtoken(tWHILE);
  setlabel(wq[wqLOOP]);   /* "continue" always jumps to WQLOOP. */
  setline(TRUE);
  pc_nestlevel++; /* temporarily increase the "compound statement" nesting level,
                   * so any assignments made inside the loop control expression
                   * could be cleaned up later */
  pc_loopcond=tDO;
  endlessloop=test(wq[wqEXIT],TEST_OPT,FALSE);
  pc_loopcond=0;
  pc_nestlevel--;
  clearassignments(pc_nestlevel+1);
  testloopvariables(loopvars,TRUE,loopline);
  jumplabel(top);
  setlabel(wq[wqEXIT]);
  delwhile();
  needtoken(tTERM);

  retcode=endlessloop ? tENDLESS : tDO;
  pc_numloopvars=save_numloopvars;
  endlessloop=save_endlessloop;
  return retcode;
}

static int dofor(void)
{
  int wq[wqSIZE],skiplab;
  cell save_decl;
  int save_nestlevel,save_endlessloop,save_numloopvars;
  int index,endtok;
  int *ptr;
  int loopline=fline;
  symstate *loopvars=NULL;

  save_decl=declared;
  save_nestlevel=pc_nestlevel;
  save_endlessloop=endlessloop;
  save_numloopvars=pc_numloopvars;
  pc_numloopvars=0;

  addwhile(wq);
  skiplab=getlabel();
  endtok= matchtoken('(') ? ')' : tDO;
  pc_nestlevel++; /* temporarily increase the "compound statement" nesting level,
                   * so any assignments made inside the loop initialization, control
                   * expression and increment blocks could be cleaned up later */
  if (matchtoken(';')==0) {
    /* new variable declarations are allowed here */
    if (matchtoken(tNEW)) {
      /* The variable in expr1 of the for loop is at a
       * 'compound statement' level of it own.
       */
      declloc(FALSE); /* declare local variable */
    } else {
      doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE,NULL); /* expression 1 */
      needtoken(';');
    } /* if */
  } /* if */
  /* Adjust the "declared" field in the "while queue", in case that
   * local variables were declared in the first expression of the
   * "for" loop. These are deleted in separately, so a "break" or a "continue"
   * must ignore these fields.
   */
  ptr=readwhile();
  assert(ptr!=NULL);
  ptr[wqBRK]=(int)declared;
  ptr[wqCONT]=(int)declared;
  ptr[wqLVL]=pc_nestlevel+1;
  jumplabel(skiplab);               /* skip expression 3 1st time */
  setlabel(wq[wqLOOP]);             /* "continue" goes to this label: expr3 */
  setline(TRUE);
  scanloopvariables(&loopvars,FALSE);
  /* Expressions 2 and 3 are reversed in the generated code: expression 3
   * precedes expression 2. When parsing, the code is buffered and marks for
   * the start of each expression are inserted in the buffer.
   */
  assert(!staging);
  stgset(TRUE);                     /* start staging */
  assert(stgidx==0);
  index=stgidx;
  stgmark(sSTARTREORDER);
  stgmark((char)(sEXPRSTART+0));    /* mark start of 2nd expression in stage */
  setlabel(skiplab);                /* jump to this point after 1st expression */
  if (matchtoken(';')) {
    endlessloop=1;
  } else {
    pc_loopcond=tFOR;
    endlessloop=test(wq[wqEXIT],TEST_PLAIN,FALSE);/* expression 2 (jump to wq[wqEXIT] if false) */
    pc_loopcond=0;
    needtoken(';');
  } /* if */
  stgmark((char)(sEXPRSTART+1));    /* mark start of 3th expression in stage */
  if (!matchtoken(endtok)) {
    doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE,NULL);   /* expression 3 */
    needtoken(endtok);
  } /* if */
  stgmark(sENDREORDER);             /* mark end of reversed evaluation */
  stgout(index);
  stgset(FALSE);                    /* stop staging */
  statement(NULL,FALSE);
  clearassignments(save_nestlevel+1);
  testloopvariables(loopvars,FALSE,loopline);
  jumplabel(wq[wqLOOP]);
  setlabel(wq[wqEXIT]);
  delwhile();

  assert(pc_nestlevel>save_nestlevel);
  if (declared>save_decl) {
    /* Clean up the space and the symbol table for the local
     * variable in "expr1".
     */
    destructsymbols(&loctab,pc_nestlevel);
    modstk((int)(declared-save_decl)*sizeof(cell));
    testsymbols(&loctab,pc_nestlevel,FALSE,TRUE);   /* look for unused block locals */
    declared=save_decl;
    delete_symbols(&loctab,pc_nestlevel,FALSE,TRUE);
  } /* if */
  pc_nestlevel=save_nestlevel;    /* reset 'compound statement' nesting level */

  index=endlessloop ? tENDLESS : tFOR;
  pc_numloopvars=save_numloopvars;
  endlessloop=save_endlessloop;
  return index;
}

/*  foreach_gen_store / foreach_gen_load - raw access to a coroutine state-block
 *  slot by BYTE OFFSET (no symbol). The block base B lives in the hidden
 *  "localsbase" frame cell (pc_genlocalsbase); the slot's absolute data address
 *  is B+off. These mirror the lifted-local access in sc4.c but take an offset
 *  directly, so "doforeach" can park its own hidden walk state (the set base and
 *  the cursor value) in the block. Living in the block, that state survives a
 *  "yield" in the loop body -- which is exactly what lets a plain-set "foreach"
 *  appear inside a generator (the err-099 sugar).
 */
static void foreach_gen_store(cell off)
{
  stgwrite("\tpush.pri\n");            /* save the value to store */
  code_idx+=opcodes(1);
  stgwrite("\tload.s.pri ");           /* PRI = B (state-block base) */
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");                /* PRI = B + off (slot address) */
  outval(off,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tmove.alt\n");            /* ALT = B + off */
  code_idx+=opcodes(1);
  stgwrite("\tpop.pri\n");             /* PRI = the value again */
  code_idx+=opcodes(1);
  stgwrite("\tstor.i\n");              /* [B + off] = PRI */
  code_idx+=opcodes(1);
}

static void foreach_gen_load(cell off)
{
  stgwrite("\tload.s.pri ");           /* PRI = B (state-block base) */
  outval(pc_genlocalsbase,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tadd.c ");                /* PRI = B + off (slot address) */
  outval(off,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  stgwrite("\tload.i\n");              /* PRI = [B + off] */
  code_idx+=opcodes(1);
}

/*  foreach_store_loopvar - bind the loop variable to the value in PRI, choosing
 *  the addressing mode from the symbol: a lifted/captured var stores through the
 *  state block (survives a yield), a plain local through "stor.s.pri", a global
 *  through "stor.pri". PRI holds the value on entry.
 */
static void foreach_store_loopvar(symbol *loopsym,cell iaddr)
{
  if ((loopsym->usage & (uLIFTED|uCAPTURED))!=0) {
    foreach_gen_store(loopsym->addr);
  } else {
    if (loopsym->vclass==sLOCAL)
      stgwrite("\tstor.s.pri ");
    else
      stgwrite("\tstor.pri ");
    outval(iaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);
  } /* if */
}

/*  doforeach
 *
 *  Native compact-set iteration statement:
 *      foreach ( [ new ] <var> : <array> ) <body>
 *  <array> is an ordinary script array used as a sorted set (see the
 *  <foreach> header): array[0] holds the count of in-use values, and
 *  array[1..count] the distinct values in ascending order. The generated
 *  code walks an indexed loop k=1..count, binding the loop variable to
 *  array[k] on each pass (the value, not the index).
 *
 *  Only existing AMX opcodes are emitted, so the output runs on an
 *  unmodified host. "break"/"continue" reuse the loop machinery
 *  (addwhile/readwhile) exactly as "for" does.
 *
 *  The operand may also be a call to an "iterfunc", in which case there is no
 *  backing array and the two generator sub-paths below are taken instead: the
 *  re-entrant call-loop for an "iterfunc" that is handed the running state, and
 *  the coroutine protocol for one that declares "yield" (or no parameters).
 */
static int doforeach(void)
{
  int wq[wqSIZE];
  cell save_decl;
  int save_nestlevel,save_endlessloop;
  int *ptr;
  int isnew,hascolon,validarray,reverse;
  int lbl_cond;
  int tok,oident;
  cell val;
  char *str;
  char varname[sNAMEMAX+1];
  symbol *loopsym;
  cell iaddr,kaddr,cntaddr,baseaddr,operand_heap,paddr,fastaddr;
  int dim[sDIMEN_MAX],idxtag[sDIMEN_MAX];
  symbol *gensym,*cand;         /* generator (iterfunc) support */
  cell curaddr,argaddr[sMAXARGS],iterstop,stateaddr;
  int nuser,ai,argident,statebyref;
  cell genaddr;                 /* coroutine generator support */
  int blkcells;

  save_decl=declared;
  save_nestlevel=pc_nestlevel;
  save_endlessloop=endlessloop;
  endlessloop=0;

  addwhile(wq);
  needtoken('(');
  pc_nestlevel++;       /* the loop variable is at a nesting level of its own */

  /* --- the loop variable: "[new] <var>" --- */
  isnew=matchtoken(tNEW);
  hascolon=FALSE;
  varname[0]='\0';
  tok=lex(&val,&str);
  if (tok==tLABEL) {
    /* "<var>:" with no space before the colon lexes as a single token,
     * and the colon is already consumed */
    assert(strlen(str)<=sNAMEMAX);
    strcpy(varname,str);
    hascolon=TRUE;
  } else if (tok==tSYMBOL) {
    assert(strlen(str)<=sNAMEMAX);
    strcpy(varname,str);
  } else {
    error(255,"\"foreach\" syntax requires \": <array>\" after the loop variable");
  } /* if */
  if (!hascolon && !matchtoken(':'))
    error(255,"\"foreach\" syntax requires \": <array>\" after the loop variable");

  /* bind or declare the loop variable (always a scalar) */
  loopsym=NULL;
  if (isnew) {
    if (findloc(varname)!=NULL && findloc(varname)->compound==pc_nestlevel)
      error(21,varname);        /* symbol already defined */
    if (pc_generator) {
      /* inside a coroutine generator the loop variable is LIFTED into the state
       * block, exactly like any other generator scalar local: its value then
       * survives a "yield" in the loop body, and "declared"/the stack stay at
       * the generator baseline so the err-099 guard is satisfied. Access is
       * emitted against "localsbase" (see foreach_store_loopvar / rvalue()). */
      int slot=curfunc->genlocals;
      iaddr=(cell)(slot+gen_reserved(curfunc))*sizeof(cell);
      loopsym=addvariable(varname,iaddr,iVARIABLE,sLOCAL,0,dim,0,idxtag,pc_nestlevel);
      loopsym->usage|=uLIFTED;
      curfunc->genlocals=slot+1;
      assert(curfunc!=NULL);
    } else {
      declared+=1;                /* the variable is put on the stack */
      iaddr=-declared*(cell)sizeof(cell);
      loopsym=addvariable(varname,iaddr,iVARIABLE,sLOCAL,0,dim,0,idxtag,pc_nestlevel);
      modstk(-(int)sizeof(cell));
      assert(curfunc!=NULL);
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=declared+1;
    } /* if */
  } else {
    loopsym=findloc(varname);
    if (loopsym==NULL)
      loopsym=findglb(varname,sGLOBAL);
    if (loopsym==NULL || (loopsym->ident!=iVARIABLE && loopsym->ident!=iREFERENCE)) {
      error(17,varname[0]!='\0' ? varname : "-");   /* undefined symbol */
      /* fabricate a local so the body still resolves and codegen stays valid */
      declared+=1;
      iaddr=-declared*(cell)sizeof(cell);
      loopsym=addvariable(varname[0]!='\0' ? varname : "-foreach",iaddr,iVARIABLE,sLOCAL,0,dim,0,idxtag,pc_nestlevel);
      modstk(-(int)sizeof(cell));
      assert(curfunc!=NULL);
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=declared+1;
    } else {
      iaddr=loopsym->addr;
    } /* if */
  } /* if */
  loopsym->usage|=uDEFINE|uWRITTEN|uREAD;

  /* --- the array to iterate over: any expression that resolves to an array
   * reference (a bare symbol, a subscripted row of a multi-dimensional array,
   * a reference-array parameter, ...). It is evaluated exactly ONCE here, and
   * its address (currently in PRI) is cached in "baseaddr" below, so a computed
   * or side-effecting operand is not re-evaluated per iteration. --- */
  validarray=FALSE;
  operand_heap=0;
  /* optional "Reverse(<operand>)" wrapper: iterate the set descending. This is
   * also the safe way to remove the current element mid-walk, since remove
   * shifts the tail (higher values) left -- already-visited in a reverse walk. */
  reverse=FALSE;
  if (matchtoken(tSYMBOL)) {
    tokeninfo(&val,&str);
    if (strcmp(str,"Reverse")==0 && matchtoken('('))
      reverse=TRUE;
    else
      lexpush();                /* not "Reverse(" -- hand the symbol to the operand parser */
  } /* if */

  /* generator detection: is the operand a call to an "iterfunc" symbol? If so,
   * emit a call-loop instead of an array walk. The two paths stay fully
   * separate -- the generator path never touches parse_foreach_operand (there
   * is no backing array to snapshot or pointer-walk). */
  gensym=NULL;
  if (matchtoken(tSYMBOL)) {
    tokeninfo(&val,&str);
    cand=findloc(str);
    if (cand==NULL)
      cand=findglb(str,sGLOBAL);
    if (cand!=NULL && cand->ident==iFUNCTN && (cand->usage & uITERFUNC)!=0 && matchtoken('('))
      gensym=cand;              /* "Name(" of a generator: '(' consumed */
    else
      lexpush();                /* not a generator call -- hand the symbol to the operand parser */
  } /* if */

  if (gensym!=NULL && (gensym->usage & uGENERATOR)!=0) {
    /* An "async" function is also tagged uITERFUNC|uGENERATOR (its suspend point
     * is "await"), but it is NOT a foreach generator: it reserves gen_reserved()
     * ==4 head cells (not 1) and is driven by a scheduler through a different
     * resume protocol. Iterating it here would under-allocate B (blkcells below
     * hardcodes the yield generator's single reserved cell) and drive it wrongly,
     * so reject it cleanly rather than miscompile. */
    if ((gensym->usage & uASYNC)!=0)
      error(267);               /* "foreach" cannot iterate an "async" function */
    /* --- COROUTINE PATH: foreach (new i : Gen(args...)) ---
     * Gen is a generator that uses "yield": instead of being called fresh with
     * the running state, it is RESUMED where it last suspended, until it runs
     * off its end (or "return"s), which yields ITER_STOP (== cellmin) and ends
     * the loop.
     *
     * The whole persistent state of the running generator lives in one heap
     * block, the "state block" (see the "yield" support above), which this loop
     * owns: it is allocated and zero-filled here, and freed at loop exit -- the
     * label "break" jumps to. Its base is passed BY VALUE as the generator's
     * hidden first argument on every step, so the generator and the
     * "@yield.emit" helper both address the state through it.
     *
     * The block holds the continuation in B[0] and the generator's L lifted
     * slots in B[1..L] (L == gensym->genlocals) -- the user parameters take the
     * first slots and the body's scalar locals the rest, both counted while the
     * generator body was parsed -- so its size is L+1 cells. The user args are
     * evaluated once here and pushed after B on every step; the prologue copies
     * them into their slots on the fresh call. */
    if (reverse)
      error(255,"\"foreach\" cannot reverse a generator (iterfunc); a generator defines its own order");
    if (sc_status!=statSKIP)
      markusage(gensym,uREAD);

    /* the call's user args (Gen(a,b,...)): evaluate each ONCE here (they are
     * loop-invariant) and cache in a hidden loop cell, exactly as the re-entrant
     * generator path does below. On every resume step they are pushed unchanged
     * after B; the generator prologue copies them into their block slots on the
     * FRESH call so a parameter is mutable and survives across a "yield". */
    nuser=0;
    if (!matchtoken(')')) {
      do {
        if (nuser>=sMAXARGS-1) {
          error(45);            /* too many function arguments */
          break;
        } /* if */
        argident=expression(&val,NULL,NULL,FALSE);   /* leaves the value in PRI */
        if (argident==iCONSTEXPR)
          ldconst(val,sPRI);    /* a constant is not auto-loaded -- force it into PRI */
        /* iARRAY/iREFARRAY (and a Callback: from "using inline"): expression()
         * left the array/record ADDRESS in PRI. Cache and push it like a scalar
         * cell, so the generator receives it as a by-reference array / Callback
         * parameter. This lets an iterfunc consume a set or take a predicate. */
        declared+=1;
        argaddr[nuser]=-declared*(cell)sizeof(cell);
        modstk(-(int)sizeof(cell));
        if (curfunc->x.stacksize<declared+1)
          curfunc->x.stacksize=declared+1;
        stgwrite("\tstor.s.pri ");
        outval(argaddr[nuser],TRUE);
        code_idx+=opcodes(1)+opargs(1);
        nuser++;
      } while (matchtoken(','));
      needtoken(')');           /* close "Gen(" after its user args */
    } /* if */
    needtoken(')');             /* close "foreach(" */

    /* hidden cell holding the state-block base, plus the block itself on the
     * AMX heap. "modheap" leaves the OLD heap top (== the block base) in ALT,
     * so the fill and the store of B need no address arithmetic. */
    blkcells=1+gensym->genlocals;   /* B[0] continuation + L lifted params & locals */
    declared+=1;
    genaddr=-declared*(cell)sizeof(cell);
    modstk(-(int)sizeof(cell));
    assert(curfunc!=NULL);
    if (curfunc->x.stacksize<declared+1)
      curfunc->x.stacksize=declared+1;
    modheap((int)blkcells*(int)sizeof(cell));   /* ALT = B */
    ldconst(0,sPRI);
    stgwrite("\tfill ");
    outval((cell)blkcells*sizeof(cell),TRUE);
    code_idx+=opcodes(1)+opargs(1);
    moveto1();                  /* PRI = ALT = B (the block base) */
    stgwrite("\tstor.s.pri ");
    outval(genaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);

    /* "break"/"continue" clean the body's own locals down to here; the loop
     * variable and the hidden base cell are the loop-scoped cells. */
    ptr=readwhile();
    assert(ptr!=NULL);
    ptr[wqBRK]=(int)declared;
    ptr[wqCONT]=(int)declared;
    ptr[wqLVL]=pc_nestlevel+1;

    lbl_cond=getlabel();
    setline(TRUE);
    setlabel(lbl_cond);
    /* call Gen(B, arg0, ..., argN-1): B is the hidden arg0, so the user args are
     * pushed first in reverse order (last param first) and B is pushed LAST, to
     * land arg0==B at FRM+12 and the user args just above it. "retn" pops them
     * all, so STK is restored on return and PRI holds the next value (or
     * ITER_STOP). */
    for (ai=nuser-1; ai>=0; ai--) {
      stgwrite("\tpush.s ");
      outval(argaddr[ai],TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* for */
    stgwrite("\tpush.s ");       /* B is arg0 -- pushed last */
    outval(genaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    pushval((cell)(nuser+1)*sizeof(cell));   /* B + nuser user args, in bytes */
    ffcall(gensym,NULL,nuser+1);

    /* if (PRI == ITER_STOP) goto exit */
    stgwrite("\tconst.alt ");
    outval(generator_iterstop,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tjeq ");
    outval(wq[wqEXIT],TRUE);
    code_idx+=opcodes(1)+opargs(1);

    /* bind the loop variable to the returned value */
    foreach_store_loopvar(loopsym,iaddr);

    statement(NULL,FALSE);      /* the loop body; "i" is live here */

    setlabel(wq[wqLOOP]);       /* "continue" lands here: call the generator again */
    jumplabel(lbl_cond);
    setlabel(wq[wqEXIT]);       /* "break" lands here too: free the state block */
    delwhile();

    /* clean up the loop variable and the hidden base cell */
    if (declared>save_decl) {
      destructsymbols(&loctab,pc_nestlevel);
      modstk((int)(declared-save_decl)*sizeof(cell));
      testsymbols(&loctab,pc_nestlevel,FALSE,TRUE);   /* look for unused block locals */
      declared=save_decl;
      delete_symbols(&loctab,pc_nestlevel,FALSE,TRUE);
    } /* if */
    /* release the state block. It is the last heap allocation made, so this is
     * a strict LIFO release, even when this loop is nested in another one. */
    modheap(-(int)blkcells*(int)sizeof(cell));
    pc_nestlevel=save_nestlevel;
    endlessloop=save_endlessloop;
    return tFOREACH;
  } /* if coroutine generator */

  if (gensym!=NULL) {
    /* --- GENERATOR PATH: foreach (new i : Gen(a,b,...)) ---
     * Gen is "iterfunc Gen(cur, x, y)": called with the running state "cur" it
     * returns the next value, or ITER_STOP (== cellmin) to end. The first call
     * receives cur == ITER_STOP (the seed). The extra args are evaluated once
     * and cached; only "cur" varies per call. Only existing AMX opcodes are
     * emitted (call/push/const.alt/jeq/...), so the output runs on an
     * unmodified host, exactly like the array walk. */
    iterstop=(cell)((ucell)1 << (PAWN_CELL_SIZE-1));  /* ITER_STOP == cellmin */

    if (reverse)
      error(255,"\"foreach\" cannot reverse a generator (iterfunc); a generator defines its own order");
    /* register the call so the generator is not stripped and, when it is defined
     * before this loop, is still emitted in the writing pass (builds the same
     * caller->callee reference an ordinary call would) */
    if (sc_status!=statSKIP)
      markusage(gensym,uREAD);

    /* hidden "cur" cell (the running state), initialised to ITER_STOP so the
     * first call receives the seed. modstk only moves STK, so PRI stays free. */
    declared+=1;
    curaddr=-declared*(cell)sizeof(cell);
    modstk(-(int)sizeof(cell));
    assert(curfunc!=NULL);
    if (curfunc->x.stacksize<declared+1)
      curfunc->x.stacksize=declared+1;
    ldconst(iterstop,sPRI);
    stgwrite("\tstor.s.pri ");
    outval(curaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);

    /* stateful generator: if the generator's FIRST parameter is a reference
     * (iterfunc Name(&state, cur, ...)), allocate a persistent hidden "state"
     * cell (init 0) and pass its address by reference on every call. This lets
     * a generator carry more than the last emitted value (e.g. Fibonacci, an
     * internal counter) across iterations. YSI's "&iterstate" analogue. */
    statebyref= (gensym->dim.arglist[0].ident==iREFERENCE);
    stateaddr=0;
    if (statebyref) {
      declared+=1;
      stateaddr=-declared*(cell)sizeof(cell);
      modstk(-(int)sizeof(cell));
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=declared+1;
      ldconst(0,sPRI);
      stgwrite("\tstor.s.pri ");
      outval(stateaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */

    /* evaluate the extra args ONCE (they are loop-invariant) and cache each in
     * a hidden cell; the cached values are passed unchanged on every call. */
    nuser=0;
    if (!matchtoken(')')) {
      do {
        if (nuser>=sMAXARGS-1) {
          error(45);            /* too many function arguments */
          break;
        } /* if */
        argident=expression(&val,NULL,NULL,FALSE);   /* leaves the value in PRI */
        if (argident==iCONSTEXPR)
          ldconst(val,sPRI);    /* a constant is not auto-loaded -- force it into PRI */
        /* iARRAY/iREFARRAY (and a Callback: from "using inline"): expression()
         * left the array/record ADDRESS in PRI. Cache and push it like a scalar
         * cell, so the generator receives it as a by-reference array / Callback
         * parameter. This lets an iterfunc consume a set or take a predicate. */
        declared+=1;
        argaddr[nuser]=-declared*(cell)sizeof(cell);
        modstk(-(int)sizeof(cell));
        if (curfunc->x.stacksize<declared+1)
          curfunc->x.stacksize=declared+1;
        stgwrite("\tstor.s.pri ");
        outval(argaddr[nuser],TRUE);
        code_idx+=opcodes(1)+opargs(1);
        nuser++;
      } while (matchtoken(','));
      needtoken(')');           /* close "Gen(" */
    } /* if */
    if (reverse)
      needtoken(')');           /* close the (rejected) "Reverse(" wrapper */
    needtoken(')');             /* close "foreach(" */

    /* "break"/"continue" clean the body's own locals down to here; the loop
     * variable, "cur" and the cached args are the loop-scoped hidden cells
     * (mirrors dofor's / the array path's adjustment). */
    ptr=readwhile();
    assert(ptr!=NULL);
    ptr[wqBRK]=(int)declared;
    ptr[wqCONT]=(int)declared;
    ptr[wqLVL]=pc_nestlevel+1;

    lbl_cond=getlabel();
    setline(TRUE);
    setlabel(lbl_cond);
    /* call Gen(cur, arg1, ..., argN): push args in reversed parameter order
     * (last param first), "cur" last, then the byte count, then call. The
     * callee's "retn" pops the arguments, so STK is restored on return and PRI
     * holds the next value. */
    for (ai=nuser-1; ai>=0; ai--) {
      stgwrite("\tpush.s ");
      outval(argaddr[ai],TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* for */
    stgwrite("\tpush.s ");      /* "cur": first param (or 2nd, after &state) */
    outval(curaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    if (statebyref) {
      stgwrite("\tpush.adr ");   /* &state is parameter 0 -- pushed last */
      outval(stateaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */
    pushval((cell)(nuser+1+(statebyref?1:0))*sizeof(cell));
    ffcall(gensym,NULL,nuser+1+(statebyref?1:0));

    /* if (PRI == ITER_STOP) goto exit */
    stgwrite("\tconst.alt ");
    outval(iterstop,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tjeq ");
    outval(wq[wqEXIT],TRUE);
    code_idx+=opcodes(1)+opargs(1);

    /* cur = the returned next value (still in PRI) */
    stgwrite("\tstor.s.pri ");
    outval(curaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    /* bind the loop variable to the returned value */
    foreach_store_loopvar(loopsym,iaddr);

    statement(NULL,FALSE);      /* the loop body; "i" is live here */

    setlabel(wq[wqLOOP]);       /* "continue" lands here: cur is already updated */
    jumplabel(lbl_cond);        /* re-call the generator for the next value */
    setlabel(wq[wqEXIT]);
    delwhile();

    /* clean up the loop variable and the hidden cur/arg cells */
    if (declared>save_decl) {
      destructsymbols(&loctab,pc_nestlevel);
      modstk((int)(declared-save_decl)*sizeof(cell));
      testsymbols(&loctab,pc_nestlevel,FALSE,TRUE);   /* look for unused block locals */
      declared=save_decl;
      delete_symbols(&loctab,pc_nestlevel,FALSE,TRUE);
    } /* if */
    pc_nestlevel=save_nestlevel;
    endlessloop=save_endlessloop;
    return tFOREACH;
  } /* if generator */

  oident=parse_foreach_operand(NULL,&operand_heap);
  if (oident==iARRAY || oident==iREFARRAY) {
    validarray=TRUE;            /* the row's base address is now in PRI */
  } else {
    error(255,"\"foreach\" iterates over an array or iterator, not a value");
  } /* if */
  if (reverse)
    needtoken(')');             /* close "Reverse(" */
  needtoken(')');               /* close "foreach(" */

  if (pc_generator && validarray) {
    /* --- GENERATOR + PLAIN-SET PATH: foreach (new i : someSet) inside a
     * coroutine generator (the err-099 sugar). ---
     * The stack-based fast-walk below parks its cursor cells (index, count,
     * position) on the stack, which a "yield" in the loop body would discard
     * on suspend -- hence the old error 099. Here the whole walk state is
     * value-based and LIFTED into the generator's state block instead: two
     * hidden block slots hold the set base address and the current member
     * value, and the loop variable is already lifted (above). Nothing is put
     * on the stack ("declared" stays at the generator baseline), so a "yield"
     * in the body is legal, and the lifted base/cursor survive the suspend.
     *
     * The advance is purely value-based -- cur = setnext(base, cur) (setprev
     * for Reverse) -- so it is removal-safe (setnext of the last-visited value
     * still returns the correct successor) without the O(1) fast path the
     * on-stack walk uses. A generator trades that micro-optimisation for the
     * ability to suspend mid-walk. */
    symbol *n_get=findglb("setget",sGLOBAL);
    symbol *n_len=findglb("setlen",sGLOBAL);
    symbol *n_step=findglb(reverse ? "setprev" : "setnext",sGLOBAL);
    cell basegenoff,curgenoff;
    int bslot,cslot;
    if (n_get==NULL || n_len==NULL || n_step==NULL) {
      error(255,"the \"foreach\" keyword needs #include <foreach> (it lowers to the set* natives)");
    } else {
      /* two hidden lifted slots: the set base and the cursor value */
      bslot=curfunc->genlocals;
      basegenoff=(cell)(bslot+gen_reserved(curfunc))*sizeof(cell);
      curfunc->genlocals=bslot+1;
      cslot=curfunc->genlocals;
      curgenoff=(cell)(cslot+gen_reserved(curfunc))*sizeof(cell);
      curfunc->genlocals=cslot+1;

      /* base = the operand address (still in PRI from parse_foreach_operand) */
      foreach_gen_store(basegenoff);

      /* cur = setget(base, reverse ? setlen(base)-1 : 0): the first member in
       * walk order, or -1 when the set is empty (setget clamps out-of-range). */
      if (reverse) {
        foreach_gen_load(basegenoff);
        pushreg(sPRI);
        pushval(1*(cell)sizeof(cell));
        ffcall(n_len,NULL,1);            /* PRI = count */
        markusage(n_len,uREAD);
        addconst(-1);                    /* index = count-1 (-1 if empty) */
        pushreg(sPRI);                   /* arg2 = index */
      } else {
        ldconst(0,sPRI);
        pushreg(sPRI);                   /* arg2 = index 0 */
      } /* if */
      foreach_gen_load(basegenoff);
      pushreg(sPRI);                     /* arg1 = set */
      pushval(2*(cell)sizeof(cell));
      ffcall(n_get,NULL,2);              /* PRI = first member value, or -1 */
      markusage(n_get,uREAD);
      foreach_gen_store(curgenoff);      /* cur = PRI */

      /* "break"/"continue" clean only the body's own locals: this loop adds
       * NO stack cells, so both unwind to the current "declared". */
      ptr=readwhile();
      assert(ptr!=NULL);
      ptr[wqBRK]=(int)declared;
      ptr[wqCONT]=(int)declared;
      ptr[wqLVL]=pc_nestlevel+1;

      lbl_cond=getlabel();
      setline(TRUE);
      setlabel(lbl_cond);
      /* terminate when cur == -1 (empty / walked off a reverse start) or
       * cur == cellmin (setnext/setprev signalled end). PRI keeps cur across
       * both tests (const.alt/jeq touch neither PRI). */
      foreach_gen_load(curgenoff);       /* PRI = cur */
      stgwrite("\tconst.alt ");
      outval(-1,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tjeq ");
      outval(wq[wqEXIT],TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tconst.alt ");
      outval(generator_iterstop,TRUE);   /* == cellmin */
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tjeq ");
      outval(wq[wqEXIT],TRUE);
      code_idx+=opcodes(1)+opargs(1);
      /* bind the loop variable to cur (still in PRI) */
      foreach_store_loopvar(loopsym,iaddr);

      statement(NULL,FALSE);             /* the loop body; "i" is live, may yield */

      setlabel(wq[wqLOOP]);              /* "continue" lands here: advance by value */
      foreach_gen_load(curgenoff);       /* PRI = cur */
      pushreg(sPRI);                     /* arg2 = value */
      foreach_gen_load(basegenoff);      /* PRI = base */
      pushreg(sPRI);                     /* arg1 = set */
      pushval(2*(cell)sizeof(cell));
      ffcall(n_step,NULL,2);             /* PRI = next/prev value, or cellmin */
      markusage(n_step,uREAD);
      foreach_gen_store(curgenoff);      /* cur = PRI */
      jumplabel(lbl_cond);
      setlabel(wq[wqEXIT]);
      delwhile();
    } /* if natives present */

    /* cleanup: no stack cells were pushed (state is lifted), so "declared" is
     * unchanged; just remove the loop-variable symbol and free any operand
     * heap. Guard the modstk defensively in case the body left the stack high
     * (it should not; body blocks clean their own locals). */
    if (declared>save_decl) {
      destructsymbols(&loctab,pc_nestlevel);
      modstk((int)(declared-save_decl)*sizeof(cell));
      declared=save_decl;
    } /* if */
    testsymbols(&loctab,pc_nestlevel,FALSE,TRUE);   /* look for unused block locals */
    delete_symbols(&loctab,pc_nestlevel,FALSE,TRUE);
    if (operand_heap>0)
      modheap(-(int)operand_heap*(int)sizeof(cell));
    pc_nestlevel=save_nestlevel;
    endlessloop=save_endlessloop;
    return tFOREACH;
  } /* if generator + plain set */

  /* hidden loop-scoped cells: the cached operand address (row base), the
   * running index k and the snapshot count. "modstk" only adjusts STK, so the
   * operand address computed above stays intact in PRI. */
  declared+=1;
  baseaddr=-declared*(cell)sizeof(cell);
  modstk(-(int)sizeof(cell));
  declared+=1;
  kaddr=-declared*(cell)sizeof(cell);
  modstk(-(int)sizeof(cell));
  declared+=1;
  cntaddr=-declared*(cell)sizeof(cell);
  modstk(-(int)sizeof(cell));
  declared+=1;                          /* p: 1-based position of the current value (fast-walk) */
  paddr=-declared*(cell)sizeof(cell);
  modstk(-(int)sizeof(cell));
  declared+=1;                          /* usefast: 1 while the inline O(1) walk is still valid */
  fastaddr=-declared*(cell)sizeof(cell);
  modstk(-(int)sizeof(cell));
  assert(curfunc!=NULL);
  if (curfunc->x.stacksize<declared+1)
    curfunc->x.stacksize=declared+1;

  /* cache the operand address (still in PRI) into "baseaddr"; every count and
   * value read below loads it back into ALT rather than recomputing it */
  if (validarray) {
    stgwrite("\tstor.s.pri ");
    outval(baseaddr,TRUE);
    code_idx+=opcodes(1)+opargs(1);
  } /* if */

  /* "break"/"continue" must skip only the body's own locals, not the loop
   * variable or the hidden index/count cells (mirrors dofor's adjustment) */
  ptr=readwhile();
  assert(ptr!=NULL);
  ptr[wqBRK]=(int)declared;
  ptr[wqCONT]=(int)declared;
  ptr[wqLVL]=pc_nestlevel+1;

  lbl_cond=getlabel();
  setline(TRUE);

  /* Value-based, removal-safe set walk (Y-Less issue #1). The set is compact
   * (array[0]=count, values in array[1..count] ascending); a raw pointer walk
   * desyncs when setremove compacts it mid-loop. Instead: cnt = setlen(base);
   * empty -> skip to exit; first member = setget(base, 0) ascending /
   * setget(base, cnt-1) descending. kaddr holds the current value; the advance
   * block (after the body) moves to the next/previous member by value. */
  if (validarray) {
    symbol *n_len=findglb("setlen",sGLOBAL);
    symbol *n_get=findglb("setget",sGLOBAL);
    if (n_len==NULL || n_get==NULL) {
      error(255,"the \"foreach\" keyword needs #include <foreach> (it lowers to the set* natives)");
    } else {
      stgwrite("\tload.s.pri ");       /* PRI = base address */
      outval(baseaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      pushreg(sPRI);
      pushval(1*(cell)sizeof(cell));
      ffcall(n_len,NULL,1);            /* PRI = count */
      markusage(n_len,uREAD);
      stgwrite("\tjzer ");             /* empty set -> skip the whole loop */
      outval(wq[wqEXIT],TRUE);
      code_idx+=opcodes(1)+opargs(1);
      if (reverse)
        addconst(-1);                  /* index = count-1 (PRI still holds count) */
      else
        ldconst(0,sPRI);               /* index = 0 */
      pushreg(sPRI);                   /* arg2 = index */
      stgwrite("\tload.s.pri ");       /* PRI = base address */
      outval(baseaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      pushreg(sPRI);                   /* arg1 = set */
      pushval(2*(cell)sizeof(cell));
      ffcall(n_get,NULL,2);            /* PRI = first member value */
      markusage(n_get,uREAD);
      stgwrite("\tstor.s.pri ");       /* curval = PRI */
      outval(kaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      /* init the inline fast-walk state: p = 1 (forward) or count (reverse),
       * and usefast = 1. While usefast holds, the advance is an O(1) inline
       * step (no set* call); a mid-loop shift downgrades it to setnext/setprev. */
      if (reverse) {
        stgwrite("\tload.s.pri ");
        outval(baseaddr,TRUE);
        code_idx+=opcodes(1)+opargs(1);
        stgwrite("\tload.i\n");         /* PRI = array[0] = count */
        code_idx+=opcodes(1);
      } else {
        ldconst(1,sPRI);
      } /* if */
      stgwrite("\tstor.s.pri ");
      outval(paddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      ldconst(1,sPRI);
      stgwrite("\tstor.s.pri ");
      outval(fastaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */
  } else {
    /* degenerate (operand not an array; already errored): skip the loop */
    jumplabel(wq[wqEXIT]);
  } /* if */

  setlabel(lbl_cond);
  /* value-based walk: kaddr holds the current member value (set by the
   * initialiser and by the advance block below). Bind the loop variable to it.
   * The empty-set case was already skipped to wqEXIT by the initialiser, and
   * termination (curval == cellmin) is tested after the advance. */
  stgwrite("\tload.s.pri ");
  outval(kaddr,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  if (loopsym->vclass==sLOCAL)
    stgwrite("\tstor.s.pri ");
  else
    stgwrite("\tstor.pri ");
  outval(iaddr,TRUE);
  code_idx+=opcodes(1)+opargs(1);

  statement(NULL,FALSE);        /* the loop body; "i" is live here */

  setlabel(wq[wqLOOP]);         /* "continue" lands here: advance to next member */
  /* advance BY VALUE (setnext, or setprev for Reverse): the removal-safe step.
   * setnext/setprev of the last-visited value still returns the correct
   * successor even if that value or others were removed by the body. kaddr
   * holds the current value and is overwritten with the result. */
  {
    symbol *n_step=findglb(reverse ? "setprev" : "setnext",sGLOBAL);
    if (n_step==NULL) {
      error(255,"the \"foreach\" keyword needs #include <foreach> (it lowers to the set* natives)");
    } else {
      int lbl_slow=getlabel();
      int lbl_down=getlabel();
      /* inline O(1) fast path (no set* call), valid while usefast != 0: if
       * array[p] still holds the current value v then nothing at or before p
       * was removed, so the next member is array[p +/- 1]. A mid-loop shift
       * (array[p] != v: v or an earlier value removed) downgrades to
       * setnext/setprev for the rest of the loop (still correct). */
      stgwrite("\tload.s.pri ");
      outval(fastaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tjzer ");
      outval(lbl_slow,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tload.s.alt ");        /* ALT = base */
      outval(baseaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tload.s.pri ");        /* PRI = p */
      outval(paddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tlidx\n");             /* PRI = array[p] */
      code_idx+=opcodes(1);
      stgwrite("\tload.s.alt ");        /* ALT = v */
      outval(kaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tjneq ");              /* array[p] != v -> downgrade */
      outval(lbl_down,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tload.s.pri ");        /* p += reverse ? -1 : +1 */
      outval(paddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tadd.c ");
      outval(reverse ? -1 : 1,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tstor.s.pri ");
      outval(paddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      if (reverse) {
        ldconst(1,sALT);               /* ALT = 1 */
        stgwrite("\tload.s.pri ");
        outval(paddr,TRUE);
        code_idx+=opcodes(1)+opargs(1);
        stgwrite("\tjsless ");          /* p < 1 -> exit */
        outval(wq[wqEXIT],TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } else {
        stgwrite("\tload.s.pri ");      /* PRI = array[0] = count */
        outval(baseaddr,TRUE);
        code_idx+=opcodes(1)+opargs(1);
        stgwrite("\tload.i\n");
        code_idx+=opcodes(1);
        stgwrite("\tmove.alt\n");        /* ALT = count */
        code_idx+=opcodes(1);
        stgwrite("\tload.s.pri ");       /* PRI = p */
        outval(paddr,TRUE);
        code_idx+=opcodes(1)+opargs(1);
        stgwrite("\tjsgrtr ");           /* p > count -> exit */
        outval(wq[wqEXIT],TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* if */
      stgwrite("\tload.s.alt ");         /* v = array[p] */
      outval(baseaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tload.s.pri ");
      outval(paddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tlidx\n");
      code_idx+=opcodes(1);
      stgwrite("\tstor.s.pri ");
      outval(kaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      stgwrite("\tjump ");
      outval(lbl_cond,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      setlabel(lbl_down);                /* shift seen: stop using the fast path */
      stgwrite("\tzero.pri\n");
      code_idx+=opcodes(1);
      stgwrite("\tstor.s.pri ");
      outval(fastaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      setlabel(lbl_slow);                /* slow path: setnext/setprev(base, v) */
      stgwrite("\tload.s.pri ");
      outval(kaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      pushreg(sPRI);
      stgwrite("\tload.s.pri ");
      outval(baseaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      pushreg(sPRI);
      pushval(2*(cell)sizeof(cell));
      ffcall(n_step,NULL,2);             /* PRI = next/prev value, or cellmin */
      markusage(n_step,uREAD);
      stgwrite("\tstor.s.pri ");
      outval(kaddr,TRUE);
      code_idx+=opcodes(1)+opargs(1);
      ldconst((cell)((ucell)1 << (PAWN_CELL_SIZE-1)),sALT);  /* ALT = cellmin */
      stgwrite("\tjneq ");               /* loop again while curval != cellmin */
      outval(lbl_cond,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */
  }
  setlabel(wq[wqEXIT]);
  delwhile();

  /* clean up the loop variable and the hidden index/count cells */
  if (declared>save_decl) {
    destructsymbols(&loctab,pc_nestlevel);
    modstk((int)(declared-save_decl)*sizeof(cell));
    testsymbols(&loctab,pc_nestlevel,FALSE,TRUE); /* look for unused block locals */
    declared=save_decl;
    delete_symbols(&loctab,pc_nestlevel,FALSE,TRUE);
  } /* if */
  /* release any heap the operand left behind. The temporary (e.g. an array
   * returned by a function used as the operand) was kept alive for the whole
   * loop, so it is freed here at loop exit -- where "break" also lands. */
  if (operand_heap>0)
    modheap(-(int)operand_heap*(int)sizeof(cell));
  pc_nestlevel=save_nestlevel;
  endlessloop=save_endlessloop;
  return tFOREACH;
}

/* The switch statement is incompatible with its C sibling:
 * 1. the cases are not drop through
 * 2. only one instruction may appear below each case, use a compound
 *    instruction to execute multiple instructions
 * 3. the "case" keyword accepts a comma separated list of values to
 *    match, it also accepts a range using the syntax "1 .. 4"
 *
 * SWITCH param
 *   PRI = expression result
 *   param = table offset (code segment)
 *
 */
static int doswitch(void)
{
  int lbl_table,lbl_exit,lbl_case,lbl_dispatch;
  int swdefault,casecount;
  int tok,endtok;
  int swtag,csetag;
  int allterminal;
  int enumsymcount;
  int save_fline;
  symbol *enumsym,*csesym;
  int ident;
  cell val;
  char *str;
  constvalue_root caselist = { NULL, NULL};   /* case list starts empty */
  constvalue *cse,*csp,*newval;
  char labelname[sNAMEMAX+1];
  symstate *assignments=NULL;

  endtok= matchtoken('(') ? ')' : tDO;
  ident=doexpr(TRUE,FALSE,FALSE,FALSE,&swtag,NULL,TRUE,NULL);   /* evaluate switch expression */
  if (ident==iCONSTEXPR)
    error(243);                 /* redundant code: switch control expression is constant */
  needtoken(endtok);
  /* generate the code for the switch statement, the label is the address
   * of the case table (to be generated later).
   */
  lbl_table=getlabel();
  lbl_dispatch=getlabel();       /* dispatch block (range checks + OP_SWITCH), emitted after the bodies */
  lbl_case=0;                   /* just to avoid a compiler warning */
  /* Jump over the case bodies to the dispatch block. The switch value stays
   * in PRI across an unconditional jump, so the inline range bounds-checks
   * and the OP_SWITCH there still see it. */
  jumplabel(lbl_dispatch);

  save_fline=fline;
  enumsym=NULL;
  if (swtag!=0) {
    constvalue *tagsym=find_tag_byval(swtag);
    assert(tagsym->name!=NULL);
    enumsymcount=0;
    enumsym=findconst(tagsym->name,NULL);
    if (enumsym!=NULL && (enumsym->tag!=swtag || enumsym->dim.enumlist==NULL))
      enumsym=NULL;
  } /* if */

  if (matchtoken(tBEGIN)) {
    endtok=tEND;
  } else {
    endtok='}';
    needtoken('{');
  } /* if */
  lbl_exit=getlabel();          /* get label number for jumping out of switch */
  swdefault=FALSE;
  allterminal=TRUE;             /* assume that all cases end with terminal statements */
  casecount=0;
  do {
    tok=lex(&val,&str);         /* read in (new) token */
    switch (tok) {
    case tCASE:
      lastst=0;
      if (casecount!=0)
        memoizeassignments(pc_nestlevel+1,&assignments);
      if (swdefault!=FALSE)
        error(15);        /* "default" case must be last in switch statement */
      lbl_case=getlabel();
      PUSHSTK_I(sc_allowtags);
      sc_allowtags=FALSE; /* do not allow tagnames here */
      do {
        casecount++;

        /* ??? enforce/document that, in a switch, a statement cannot start
         *     with a label. Then, you can search for:
         *     * the first semicolon (marks the end of a statement)
         *     * an opening brace (marks the start of a compound statement)
         *     and search for the right-most colon before that statement
         *     Now, by replacing the ':' by a special COLON token, you can
         *     parse all expressions until that special token.
         */

        constexpr(&val,&csetag,&csesym);
        check_tagmismatch(swtag,csetag,TRUE,-1);
        if (enumsym!=NULL) {
          if (csesym!=NULL && csesym->parent==enumsym)
            enumsymcount++;
          else
            enumsym=NULL;
        } /* if */
        /* Search the insertion point (the table is kept in sorted order, so
         * that advanced abstract machines can sift the case table with a
         * binary search). Check for duplicate case values at the same time.
         */
        for (csp=NULL, cse=caselist.first;
             cse!=NULL && cse->value<val;
             csp=cse, cse=cse->next)
          /* nothing */;
        if (cse!=NULL && cse->value==val)
          error(40,val);                /* duplicate "case" label */
        /* Since the label is stored as a string in the "constvalue", the
         * size of an identifier must be at least 8, as there are 8
         * hexadecimal digits in a 32-bit number.
         */
        #if sNAMEMAX < 8
          #error Length of identifier (sNAMEMAX) too small.
        #endif
        assert(csp==NULL || csp->next==cse);
        newval=insert_constval(csp,cse,itoh(lbl_case),val,0);
        if (csp==NULL)
          caselist.first=newval;
        if (matchtoken(tDBLDOT)) {
          cell end;
          constexpr(&end,&csetag,NULL);
          if (end<=val)
            error(50);                  /* invalid range */
          check_tagmismatch(swtag,csetag,TRUE,-1);
          enumsym=NULL; /* stop counting the number of covered enum elements */
          while (++val<=end) {
            casecount++;
            /* find the new insertion point */
            for (csp=NULL, cse=caselist.first;
                 cse!=NULL && cse->value<val;
                 csp=cse, cse=cse->next)
              /* nothing */;
            if (cse!=NULL && cse->value==val)
              error(40,val);            /* duplicate "case" label */
            assert(csp==NULL || csp->next==cse);
            insert_constval(csp,cse,itoh(lbl_case),val,0);
          } /* while */
        } /* if */
      } while (matchtoken(','));
      needtoken(':');                   /* ':' ends the case */
      sc_allowtags=(short)POPSTK_I();   /* reset */
      setlabel(lbl_case);
      statement(NULL,FALSE);
      jumplabel(lbl_exit);
      allterminal &= isterminal(lastst);
      break;
    case tDEFAULT:
      lastst=0;
      if (casecount!=0)
        memoizeassignments(pc_nestlevel+1,&assignments);
      if (swdefault!=FALSE)
        error(16);         /* multiple defaults in switch */
      lbl_case=getlabel();
      setlabel(lbl_case);
      needtoken(':');
      swdefault=TRUE;
      statement(NULL,FALSE);
      /* Jump to lbl_exit, even thouh this is the last clause in the
       * switch, because the jump table is generated between the last
       * clause of the switch and the exit label.
       */
      jumplabel(lbl_exit);
      allterminal &= isterminal(lastst);
      break;
    default:
      if (tok!=endtok) {
        error(2);
        indent_nowarn=TRUE; /* disable this check */
        tok=endtok;     /* break out of the loop after an error */
      } /* if */
    } /* switch */
  } while (tok!=endtok);
  restoreassignments(pc_nestlevel+1,assignments);

  if (enumsym!=NULL && swdefault==FALSE && enumsym->x.tags.unique-enumsymcount<=2) {
    constvalue_root *enumlist=enumsym->dim.enumlist;
    constvalue *cur,*found,*prev=NULL,*save_next=NULL;
    for (cur=enumlist->first; cur!=NULL; prev=cur,cur=cur->next) {
      /* if multiple enum elements share the same value, we only want to count the first one */
      if (prev!=NULL) {
        /* see if there's another constvalue before the current one that has the same value */
        save_next=prev->next;
        prev->next=NULL;
        found=find_constval_byval(enumlist,cur->value);
        prev->next=save_next;
        if (found!=NULL)
          continue;
      } /* if */
      /* check if the value of this constant is handled in switch, if so - continue */
      if (find_constval_byval(&caselist,cur->value)!=NULL)
        continue;
      errorset(sSETPOS,save_fline);
      error(244,cur->name); /* enum element not handled in switch */
      errorset(sSETPOS,-1);
    } /* for */
  } /* if */

  #if !defined NDEBUG
    /* verify that the case table is sorted (unfortunately, duplicates can
     * occur; there really shouldn't be duplicate cases, but the compiler
     * may not crash or drop into an assertion for a user error). */
    for (cse=caselist.first; cse!=NULL && cse->next!=NULL; cse=cse->next)
      assert(cse->value <= cse->next->value);
  #endif
  /* Determine the "none-matched" (default) label. */
  if (swdefault==FALSE) {
    /* store lbl_exit as the "none-matched" label in the switch table */
    strcpy(labelname,itoh(lbl_exit));
  } else {
    /* lbl_case holds the label of the "default" clause */
    strcpy(labelname,itoh(lbl_case));
  } /* if */

  /* Emit the dispatch block (reached only via the jump above, so PRI still
   * holds the switch value).
   *
   * The case list is sorted and fully expanded (one entry per value, ranges
   * included). Coalesce it into maximal runs of consecutive values that share
   * the same body label: a run wider than one value came from a "case lo..hi:"
   * range (or a "case a,b,c:" list of adjacent values) and is emitted as a
   * single inline bounds-check instead of one OP_SWITCH record per value --
   * this is what stops "case 0..9999:" from ballooning the .amx. The leftover
   * single values still go through the lean, binary-searchable OP_SWITCH
   * table. */
  setlabel(lbl_dispatch);
  {
    constvalue *runstart;
    int singlecount=0;
    /* pass 1: emit a bounds-check per range run; tally the single values */
    for (cse=caselist.first; cse!=NULL; cse=cse->next) {
      runstart=cse;
      while (cse->next!=NULL
             && cse->next->value==cse->value+1
             && strcmp(cse->next->name,cse->name)==0)
        cse=cse->next;
      if (cse!=runstart) {
        int skip=getlabel();
        ffcaserange(runstart->value,cse->value,runstart->name,skip);
        setlabel(skip);
      } else {
        singlecount++;
      } /* if */
    } /* for */
    /* pass 2: the single values go through OP_SWITCH; if there are none, jump
     * straight to the default/none-matched label. */
    if (singlecount>0) {
      ffswitch(lbl_table);
      setlabel(lbl_table);
      ffcase(singlecount,labelname,TRUE);
      for (cse=caselist.first; cse!=NULL; cse=cse->next) {
        runstart=cse;
        while (cse->next!=NULL
               && cse->next->value==cse->value+1
               && strcmp(cse->next->name,cse->name)==0)
          cse=cse->next;
        if (cse==runstart)
          ffcase(runstart->value,runstart->name,FALSE);
      } /* for */
    } else {
      jumplabel(swdefault ? lbl_case : lbl_exit);
    } /* if */
  }

  setlabel(lbl_exit);
  delete_consttable(&caselist); /* clear list of case labels */

  return (swdefault && allterminal) ? tTERMSWITCH : tSWITCH;
}

/* docase() is only called in erroneous situations when there's a case
 * outside of switch.
 */
static void docase(int isdefault)
{
  error(14);                        /* invalid statement; not in switch */
  if (!isdefault) {
    /* try to skim through the case values, so they won't be
     * misinterpreted as a separate statement later */
    PUSHSTK_I(sc_allowtags);
    sc_allowtags=FALSE;             /* do not allow tagnames here */
    do {
      /* no need to verify the values, as the error output is blocked
       * for the rest of the statement anyway (by "error(14)" above);
       * simply eat the values by calling constexpr() */
      constexpr(NULL,NULL,NULL);
      if (matchtoken(tDBLDOT))
        constexpr(NULL,NULL,NULL);
    } while (matchtoken(','));
    sc_allowtags=(short)POPSTK_I(); /* reset */
  } /* if */
  needtoken(':');                   /* ':' ends the case */
}

static void doassert(void)
{
  int flab1,index;
  cell cidx;

  if ((sc_debug & sCHKBOUNDS)!=0) {
    flab1=getlabel();           /* get label number for "OK" branch */
    test(flab1,TEST_PLAIN,TRUE);/* get expression and branch to flab1 if true */
    insert_dbgline(fline);      /* make sure we can find the correct line number */
    ffabort(xASSERTION);
    setlabel(flab1);
  } else {
    stgset(TRUE);               /* start staging */
    stgget(&index,&cidx);       /* mark position in code generator */
    do {
      expression(NULL,NULL,NULL,FALSE);
      stgdel(index,cidx);       /* just scrap the code */
    } while (matchtoken(','));
    stgset(FALSE);              /* stop staging */
  } /* if */
  needtoken(tTERM);
}

static int dogoto(void)
{
  char *st;
  cell val;
  symbol *sym;
  int returnst=tGOTO;

  /* if we were inside an endless loop, assume that we jump out of it */
  endlessloop=0;

  if (lex(&val,&st)==tSYMBOL) {
    sym=fetchlab(st);
    if ((sym->usage & uDEFINE)!=0) {
      clearassignments(1);
    } else if (wqptr>wq) {
      /* The label is not defined yet, which means it must be defined after the
       * 'goto'. If we're inside of a loop, the target label may or may not be
       * defined inside of the same loop - we can't know that ahead of time
       * due to how the compiler works, so the best we can do for now is clear
       * all the assignments at the current compound statement nesting level
       * in order to avoid false-positives of warning 240. */
      clearassignments(pc_nestlevel);
    } /* if */
    jumplabel((int)sym->addr);
    sym->usage|=uREAD;  /* set "uREAD" bit */
    if ((sym->usage & uDEFINE)!=0) {
      /* if there are no unimplemented labels, then the subsequent code is unreachable */
      symbol *cur;
      for (cur=&loctab; (cur=cur->next)!=NULL; )
        if (cur->ident==iLABEL && (cur->usage & uDEFINE)==0)
          break;
      if (cur==NULL)
        returnst=tTERMINAL;
    } /* if */
    // ??? if the label is defined (check sym->usage & uDEFINE), check
    //     sym->compound (nesting level of the label) against pc_nestlevel;
    //     if sym->compound < pc_nestlevel, call the destructor operator
  } else {
    error_suggest(20,st,NULL,estSYMBOL,esfLABEL);   /* illegal symbol name */
  } /* if */
  needtoken(tTERM);
  return returnst;
}

static void dolabel(void)
{
  char *st;
  cell val;
  symbol *sym;

  tokeninfo(&val,&st);  /* retrieve label name again */
  if (find_constval(&tagname_tab,st,0)!=NULL)
    error(221,st);      /* label name shadows tagname */
  sym=fetchlab(st);
  if ((sym->usage & uDEFINE)!=0)
    error(21,st);       /* symbol already defined */
  setlabel((int)sym->addr);
  /* since one can jump around variable declarations or out of compound
   * blocks, the stack must be manually adjusted
   */
  setstk(-declared*sizeof(cell));
  sym->usage|=uDEFINE;  /* label is now defined */
}

/*  fetchlab
 *
 *  Finds a label from the (local) symbol table or adds one to it.
 *  Labels are local in scope.
 *
 *  Note: The "_usage" bit is set to zero. The routines that call "fetchlab()"
 *        must set this bit accordingly.
 */
static symbol *fetchlab(char *name)
{
  symbol *sym;

  sym=findloc(name);            /* labels are local in scope */
  if (sym) {
    if (sym->ident!=iLABEL)
      error_suggest(19,sym->name,NULL,estSYMBOL,esfLABEL);  /* not a label: ... */
  } else {
    sym=addsym(name,getlabel(),iLABEL,sLOCAL,0,0);
    assert(sym!=NULL);          /* fatal error 103 must be given on error */
    sym->x.declared=(int)declared;
    sym->compound=pc_nestlevel;
  } /* if */
  return sym;
}

static void emit_invalid_token(int expected_token,int found_token)
{
  char s[2];

  assert(expected_token>=tFIRST);
  if (found_token<tFIRST) {
    sprintf(s,"%c",(char)found_token);
    error(1,sc_tokens[expected_token-tFIRST],s);
  } else {
    error(1,sc_tokens[expected_token-tFIRST],sc_tokens[found_token-tFIRST]);
  } /* if */
}

static regid emit_findreg(char *opname)
{
  const char *regname=strrchr(opname,'.');
  assert(regname!=NULL);
  regname+=1;
  assert(strcmp(regname,"pri")==0 || strcmp(regname,"alt")==0);
  return (strcmp(regname,"pri")==0) ? sPRI : sALT;
}

/* emit_getlval
 *
 * Looks for an lvalue and generates code to get cell address in PRI
 * if the lvalue is an array element (iARRAYCELL or iARRAYCHAR).
 */
static int emit_getlval(int *identptr,emit_outval *p,int *islocal,
                        regid reg,int allow_char,int allow_const,
                        int store_pri,int store_alt,int *ispushed)
{
  int tok,index,ident,close;
  cell cidx,val,length;
  char *str;
  symbol *sym;

  assert(identptr!=NULL);
  assert(p!=NULL);
  assert((!store_pri && !store_alt) || ((store_pri ^ store_alt) && (reg==sALT && ispushed!=NULL)));

  if (staging) {
    assert((emit_flags & efEXPR)!=0);
    stgget(&index,&cidx);
  } /* if */

  tok=lex(&val,&str);
  if (tok!=tSYMBOL) {
invalid_lvalue:
    error(22);          /* must be lvalue */
    return FALSE;
  } /* if */

  sym=findloc(str);
  if (sym==NULL)
    sym=findglb(str,sSTATEVAR);
  if (sym==NULL || (sym->ident!=iFUNCTN && sym->ident!=iREFFUNC && (sym->usage & uDEFINE)==0)) {
    error(17,str);      /* undefined symbol */
    return FALSE;
  } /* if */
  markusage(sym,uREAD | uWRITTEN);
  if (!allow_const && (sym->usage & uCONST)!=0)
    goto invalid_lvalue;

  p->type=eotNUMBER;
  switch (sym->ident)
  {
  case iVARIABLE:
  case iREFERENCE:
    *identptr=sym->ident;
    *islocal=((sym->vclass & sLOCAL)!=0);
    p->value.ucell=*(ucell *)&sym->addr;
    break;
  case iARRAY:
  case iREFARRAY:
    /* get the index */
    if (matchtoken('[')) {
      *identptr=iARRAYCELL;
      close=']';
    } else if (matchtoken('{')) {
      *identptr=iARRAYCHAR;
      close='}';
    } else {
      error(33,sym->name);      /* array must be indexed */
      return FALSE;
    } /* if */
    if (store_alt || store_pri) {
      pushreg(store_pri ? sPRI : sALT);
      *ispushed=TRUE;
    } /* if */
    ident=expression(&val,NULL,NULL,TRUE);
    needtoken(close);

    /* check if the index isn't out of bounds */
    length=sym->dim.array.length;
    if (close=='}')
      length *= (8*sizeof(cell))/sCHARBITS;
    if (ident==iCONSTEXPR) {    /* if the index is a constant value, check it at compile time */
      if (val<0 || (length!=0 && val>=length)) {
        error(32,sym->name);    /* array index out of bounds */
        return FALSE;
      } /* if */
    } else if (length!=0) {     /* otherwise generate code for a run-time boundary check */
      ffbounds(length-1);
    } /* if */

    /* calculate cell address */
    if (ident==iCONSTEXPR) {
      if (staging)
        stgdel(index,cidx);     /* erase generated code */
      if (store_alt || store_pri)
        *ispushed=FALSE;
      p->value.ucell= *(ucell *)&sym->addr;
      if (close==']')
        val *= (cell)sizeof(cell);
      else
        val *= (cell)(sCHARBITS/8);
      if (sym->ident==iARRAY) {
        p->value.ucell += (ucell)val;
        if (close==']') {
          /* If we are accessing an array cell and its address is known at
           * compile time, we can return it as 'iVARIABLE',
           * so the calling function could generate more optimal code.
           */
          *islocal=((sym->vclass & sLOCAL)!=0);
          *identptr=iVARIABLE;
          break;
        } /* if */
        if (reg==sPRI) {
          outinstr(((sym->vclass & sLOCAL)!=0) ? "addr.pri" : "const.pri",p,1);
        } else {
          if (store_alt)
            moveto1();
          outinstr(((sym->vclass & sLOCAL)!=0) ? "addr.alt" : "const.alt",p,1);
        } /* if */
      } else {  /* sym->ident==iREFARRAY */
        if (close==']' && val==0) {
          *identptr=iREFERENCE;
          break;
        } /* if */
        if (reg==sPRI) {
          outinstr("load.s.pri",p,1);
          if (val==1)
            outinstr("inc.pri",NULL,0);
          else
            addconst(val);
        } else {
          if (val==0 || val==1) {
            if (store_alt)
              outinstr("move.pri",NULL,0);
            outinstr("load.s.alt",p,1);
            if (val==1)
              outinstr("inc.alt",NULL,0);
          } else {
            if (store_pri)
              outinstr("move.alt",NULL,0);
            outinstr("load.s.pri",p,1);
            addconst(val);
            if (store_pri || store_alt)
              outinstr("xchg",NULL,0);
            else
              outinstr("move.alt",NULL,0);
          } /* if */
        } /* if */
      } /* if */
      if (close=='}') {
        p->value.ucell=(ucell)(sCHARBITS/8);
        outinstr((reg==sPRI) ? "align.pri" : "align.alt",p,1);
      } /* if */
    } else {    /* ident!=iCONSTEXPR */
      if (close=='}' && sym->ident==iARRAY && (sym->vclass & sLOCAL)==0) {
        char2addr();
        addconst(sym->addr);
        charalign();
        if (reg==sALT)
          outinstr("move.alt",NULL,0);
        break;
      } /* if */
      p->value.ucell= *(ucell *)&sym->addr;
      if (sym->ident==iARRAY)
        outinstr(((sym->vclass & sLOCAL)!=0) ? "addr.alt" : "const.alt",p,1);
      else      /* sym->ident==iREFARRAY */
        outinstr("load.s.alt",p,1);
      if (close==']') {
        outinstr("idxaddr",NULL,0);
      } else {
        char2addr();
        ob_add();
        charalign();
      } /* if */
      if (reg==sALT)
        outinstr("move.alt",NULL,0);
    } /* if */
    break;
  default:
    goto invalid_lvalue;
  } /* switch */

  if (!staging) {   /* issue an error if a pseudo-opcode is used outside of function body */
    error(10);      /* invalid function or declaration */
    return FALSE;
  } /* if */
  if ((sym->ident==iARRAY || sym->ident==iREFARRAY) && close=='}' && !allow_char) {
    /* issue an error if array character access isn't allowed
     * (currently it's only in 'push.u.adr')
     */
    error(35,1);    /* argument type mismatch (argument 1) */
    return FALSE;
  } /* if */
  return TRUE;
}

/* emit_getrval
 *
 * Looks for an rvalue and generates code to handle expressions.
 */
static int emit_getrval(int *identptr,cell *val)
{
  int index,result=TRUE;
  cell cidx;
  symbol *sym;

  assert(identptr!=NULL);
  assert(val!=NULL);

  if (staging) {
    assert((emit_flags & efEXPR)!=0);
    stgget(&index,&cidx);
  } else {
    error(10);          /* invalid function or declaration */
    result=FALSE;
  } /* if */

  *identptr=expression(val,NULL,&sym,TRUE);
  switch (*identptr) {
  case iVARIABLE:
  case iREFERENCE:
    *val=sym->addr;
    break;
  case iCONSTEXPR:
    /* If the expression result is a constant value or a variable - erase the code
     * for this expression so the caller would be able to generate more optimal
     * code for it, without unnecessary register clobbering.
     */
    if (staging)
      stgdel(index,cidx);
    break;
  case iARRAY:
  case iREFARRAY:
    error(33,(sym!=NULL) ? sym->name : "-unknown-");    /* array must be indexed */
    result=FALSE;
    break;
  } /* switch */

  return result;
}

static int emit_param_any_internal(emit_outval *p,int expected_tok,
                                   int allow_nonint,int allow_expr)
{
  char *str;
  cell val,cidx;
  symbol *sym;
  int tok,negate,ident,index;

  negate=FALSE;
  p->type=eotNUMBER;
fetchtok:
  tok=lex(&val,&str);
  switch (tok) {
  case tNUMBER:
    p->value.ucell=(ucell)(negate ? -val : val);
    break;
  case tRATIONAL:
    if (!allow_nonint)
      goto invalid_token;
    p->value.ucell=(negate ? ((ucell)val|((ucell)1 << (PAWN_CELL_SIZE-1))) : (ucell)val);
    break;
  case tSYMBOL:
    sym=findloc(str);
    if (sym==NULL)
      sym=findglb(str,sSTATEVAR);
    if (sym==NULL || (sym->ident!=iFUNCTN && sym->ident!=iREFFUNC && (sym->usage & uDEFINE)==0)) {
      error(17,str);    /* undefined symbol */
      return FALSE;
    } /* if */
    if (sym->ident==iLABEL) {
      sym->usage|=uREAD;
      if (negate) {
        tok=tLABEL;
        goto invalid_token_neg;
      } /* if */
      if (!allow_nonint) {
        tok=tLABEL;
        goto invalid_token;
      } /* if */
      p->type=eotLABEL;
      p->value.ucell=(ucell)sym->addr;
    } else if (sym->ident==iFUNCTN || sym->ident==iREFFUNC) {
      const int ntvref=sym->usage & uREAD;
      markusage(sym,uREAD);
      if (negate) {
        tok=teFUNCTN;
        goto invalid_token_neg;
      } /* if */
      if (!allow_nonint) {
        tok=(sym->usage & uNATIVE) ? teNATIVE : teFUNCTN;
        goto invalid_token;
      } /* if */
      if ((sym->usage & uNATIVE)!=0 && ntvref==0 && sym->addr>=0)
        sym->addr=ntv_funcid++;
      p->type=eotFUNCTION;
      p->value.string=str;
    } else {
      markusage(sym,uREAD | uWRITTEN);
      if (!allow_nonint && sym->ident!=iCONSTEXPR) {
        if (sym->vclass==sLOCAL)
          tok=(sym->ident==iREFERENCE || sym->ident==iREFARRAY) ? teREFERENCE : teLOCAL;
        else
          tok=teDATA;
        goto invalid_token;
      } /* if */
      p->value.ucell=(ucell)(negate ? -sym->addr : sym->addr);
    } /* if */
    break;
  case '(':
    if (!allow_expr)
      goto invalid_token;
    if ((emit_flags & efEXPR)==0)
      stgset(TRUE);
    stgget(&index,&cidx);
    ident=expression(&val,NULL,NULL,FALSE);
    stgdel(index,cidx);
    if ((emit_flags & efEXPR)==0)
      stgset(FALSE);
    needtoken(')');
    p->value.ucell=(ucell)(negate ? -val : val);
    if (ident!=iCONSTEXPR) {
      error(8);         /* must be constant expression */
      return FALSE;
    } /* if */
    break;
  case ':':
    if (negate)
      goto invalid_token_neg;
    tok=lex(&val,&str);
    if (tok!=tSYMBOL) {
      emit_invalid_token(tSYMBOL,tok);
      return FALSE;
    } /* if */
    sym=fetchlab(str);
    sym->usage|=uREAD;
    p->type=eotLABEL;
    p->value.ucell=(ucell)sym->addr;
    break;
  case '-':
    if (!negate) {
      negate=TRUE;
      goto fetchtok;
    } else {
      char ival[sNAMEMAX+2];
    invalid_token_neg:
      if (tok<tFIRST)
        sprintf(ival,"-%c",tok);
      else
        sprintf(ival,"-(%s)",sc_tokens[tok-tFIRST]);
      error(1,sc_tokens[expected_tok-tFIRST],ival);
      return FALSE;
    } /* if */
  default:
  invalid_token:
    emit_invalid_token(expected_tok,tok);
    return FALSE;
  } /* switch */
  return TRUE;
}

static void emit_param_any(emit_outval *p)
{
  emit_param_any_internal(p,teANY,TRUE,TRUE);
}

static void emit_param_integer(emit_outval *p)
{
  emit_param_any_internal(p,tNUMBER,FALSE,TRUE);
}

static void emit_param_index(emit_outval *p,int isrange,
                             const cell *valid_values,int numvalues)
{
  int i;
  cell val;

  assert(isrange ? (numvalues==2) : (numvalues>0));
  if (!emit_param_any_internal(p,tNUMBER,FALSE,FALSE))
    return;
  val=(cell)p->value.ucell;
  if (isrange) {
    if (valid_values[0]<=val && val<=valid_values[1])
      return;
  } else {
    for (i=0; i<numvalues; i++)
      if (val==valid_values[i])
        return;
  } /* if */
  error(241);    /* negative or too big shift count */
}

static void emit_param_nonneg(emit_outval *p)
{
  if (!emit_param_any_internal(p,teNONNEG,FALSE,TRUE))
    return;
  if ((cell)p->value.ucell<(cell)0) {
#if PAWN_CELL_SIZE==16
    char ival[7];
#elif PAWN_CELL_SIZE==32
    char ival[12];
#elif PAWN_CELL_SIZE==64
    char ival[21];
#else
  #error Unsupported cell size
#endif
    sprintf(ival,"%"PRIdC,(cell)p->value.ucell);
    error(1,sc_tokens[teNONNEG-tFIRST],ival);
  } /* if */
}

static void emit_param_shift(emit_outval *p)
{
  if (emit_param_any_internal(p,tNUMBER,FALSE,TRUE))
    if (p->value.ucell>=(sizeof(cell)*8))
      error(50);    /* invalid range */
}

static void emit_param_data(emit_outval *p)
{
  cell val;
  char *str;
  symbol *sym;
  int tok;

  p->type=eotNUMBER;
  tok=lex(&val,&str);
  switch (tok) {
  case tSYMBOL:
    sym=findloc(str);
    if (sym!=NULL) {
      markusage(sym,uREAD | uWRITTEN);
      if (sym->ident==iLABEL) {
        tok=tLABEL;
        goto invalid_token;
      } /* if */
      if (sym->vclass!=sSTATIC) {
        if (sym->ident==iCONSTEXPR)
          tok=teNUMERIC;
        else
          tok=(sym->ident==iREFERENCE || sym->ident==iREFARRAY) ? teREFERENCE : teLOCAL;
        goto invalid_token;
      } /* if */
    } else {
      sym=findglb(str,sSTATEVAR);
      if (sym==NULL) {
        error(17,str);  /* undefined symbol */
        return;
      } /* if */
      markusage(sym,(sym->ident==iFUNCTN || sym->ident==iREFFUNC) ? uREAD : (uREAD | uWRITTEN));
      if (sym->ident==iFUNCTN || sym->ident==iREFFUNC) {
        tok=((sym->usage & uNATIVE)!=0) ? teNATIVE : teFUNCTN;
        goto invalid_token;
      } /* if */
      if (sym->ident==iCONSTEXPR) {
        tok=teNUMERIC;
        goto invalid_token;
      } /* if */
    } /* if */
    val=sym->addr;
    break;
  default:
  invalid_token:
    emit_invalid_token(teDATA,tok);
    return;
  } /* switch */
  if ((val % sizeof(cell))==0)
    p->value.ucell=(ucell)val;
  else
    error(11);  /* must be a multiple of cell size */
}

static void emit_param_local(emit_outval *p,int allow_ref)
{
  cell val;
  char *str;
  symbol *sym;
  int tok,negate;

  negate=FALSE;
  p->type=eotNUMBER;
fetchtok:
  tok=lex(&val,&str);
  switch (tok) {
  case tNUMBER:
    break;
  case tSYMBOL:
    sym=findloc(str);
    if (sym!=NULL) {
      markusage(sym,uREAD | uWRITTEN);
      if (sym->ident==iLABEL) {
        tok=tLABEL;
        goto invalid_token;
      } /* if */
      if (sym->vclass==sSTATIC) {
        tok=teDATA;
        goto invalid_token;
      } /* if */
      if (allow_ref==FALSE && (sym->ident==iREFERENCE || sym->ident==iREFARRAY)) {
        tok=teREFERENCE;
        goto invalid_token;
      } /* if */
      if (negate && sym->ident!=iCONSTEXPR) {
        tok=(sym->ident==iREFERENCE || sym->ident==iREFARRAY) ? teREFERENCE : teLOCAL;
        goto invalid_token_neg;
      } /* if */
    } else {
      sym=findglb(str,sSTATEVAR);
      if (sym==NULL) {
        error(17,str);  /* undefined symbol */
        return;
      } /* if */
      markusage(sym,(sym->ident==iFUNCTN || sym->ident==iREFFUNC) ? uREAD : (uREAD | uWRITTEN));
      if (sym->ident!=iCONSTEXPR) {
        if (sym->ident==iFUNCTN || sym->ident==iREFFUNC)
          tok=((sym->usage & uNATIVE)!=0) ? teNATIVE : teFUNCTN;
        else
          tok=teDATA;
        goto invalid_token;
      } /* if */
    } /* if */
    val=sym->addr;
    break;
  case '-':
    if (!negate) {
      negate=TRUE;
      goto fetchtok;
    } else {
      char ival[sNAMEMAX+2];
    invalid_token_neg:
      if (tok<tFIRST)
        sprintf(ival,"-%c",tok);
      else
        sprintf(ival,"-(%s)",sc_tokens[tok-tFIRST]);
      error(1,sc_tokens[teLOCAL-tFIRST],ival);
      return;
    }
  default:
  invalid_token:
    if (negate)
      goto invalid_token_neg;
    emit_invalid_token(teLOCAL,tok);
    return;
  } /* switch */
  if ((val % sizeof(cell))!=0) {
    error(11);  /* must be a multiple of cell size */
    return;
  }
  p->value.ucell=(ucell)(negate ? -val : val);
}

static void emit_param_label(emit_outval *p)
{
  cell val;
  char *str;
  symbol *sym;
  int tok;

  p->type=eotNUMBER;
  tok=lex(&val,&str);
  switch (tok)
  {
  case ':':
    tok=lex(&val,&str);
    if (tok!=tSYMBOL)
      goto invalid_token;
    /* fallthrough */
  case tSYMBOL:
    sym=findloc(str);
    if (sym==NULL)
      sym=findglb(str,sSTATEVAR);
    if (sym!=NULL) {
      markusage(sym,(sym->ident==iFUNCTN || sym->ident==iREFFUNC) ? uREAD : (uREAD | uWRITTEN));
      if (sym->ident!=iLABEL) {
        if (sym->ident==iFUNCTN || sym->ident==iREFFUNC) {
          tok=((sym->usage & uNATIVE)!=0) ? teNATIVE : teFUNCTN;
        } else if (sym->ident==iCONSTEXPR) {
          tok=teNUMERIC;
        } else {
          if (sym->vclass==sLOCAL)
            tok=(sym->ident==iREFERENCE || sym->ident==iREFARRAY) ? teREFERENCE : teLOCAL;
          else
            tok=teDATA;
        } /* if */
        goto invalid_token;
      } /* if */
    } else {
      sym=fetchlab(str);
    } /* if */
    sym->usage|=uREAD;
    p->value.ucell=(ucell)sym->addr;
    break;
  default:
  invalid_token:
    emit_invalid_token(tLABEL,tok);
  }
}

static void emit_param_function(emit_outval *p,int isnative)
{
  cell val;
  char *str;
  symbol *sym;
  int tok,ntvref;

  p->type=eotNUMBER;
  tok=lex(&val,&str);
  switch (tok)
  {
  case tSYMBOL:
    sym=findloc(str);
    if (sym==NULL)
      sym=findglb(str,sSTATEVAR);
    if (sym==NULL) {
      error(17,str);    /* undefined symbol */
      return;
    } /* if */
    if (sym->ident==iFUNCTN || sym->ident==iREFFUNC) {
      ntvref=sym->usage & uREAD;
      markusage(sym,uREAD);
      if (!!(sym->usage & uNATIVE)==isnative)
        break;
      tok=(isnative!=FALSE) ? teFUNCTN : teNATIVE;
    } else {
      markusage(sym,uREAD | uWRITTEN);
      if (sym->ident==iLABEL) {
        tok=tLABEL;
      } else if (sym->ident==iCONSTEXPR) {
        tok=teNUMERIC;
      } else {
        if (sym->vclass==sLOCAL)
          tok=(sym->ident==iREFERENCE || sym->ident==iREFARRAY) ? teREFERENCE : teLOCAL;
        else
          tok=teDATA;
      } /* if */
    } /* if */
    /* fallthrough */
  default:
    emit_invalid_token((isnative!=FALSE) ? teNATIVE : teFUNCTN,tok);
    return;
  } /* switch */

  if (isnative!=FALSE) {
    if (ntvref==0 && sym->addr>=0)
      sym->addr=ntv_funcid++;
    p->value.ucell=(ucell)sym->addr;
  } else {
    p->type=eotFUNCTION;
    p->value.string=str;
  } /* if */
}

static void emit_noop(char *name)
{
  (void)name;
}

static void emit_parm0(char *name)
{
  outinstr(name,NULL,0);
}

static void emit_parm1_any(char *name)
{
  emit_outval p[1];

  emit_param_any(&p[0]);
  outinstr(name,p,arraysize(p));
}

static void emit_parm1_integer(char *name)
{
  emit_outval p[1];

  emit_param_integer(&p[0]);
  outinstr(name,p,arraysize(p));
}

static void emit_parm1_nonneg(char *name)
{
  emit_outval p[1];

  emit_param_nonneg(&p[0]);
  outinstr(name,p,arraysize(p));
}

static void emit_parm1_shift(char *name)
{
  emit_outval p[1];

  emit_param_shift(&p[0]);
  outinstr(name,p,arraysize(p));
}

static void emit_parm1_data(char *name)
{
  emit_outval p[1];

  emit_param_data(&p[0]);
  outinstr(name,p,arraysize(p));
}

static void emit_parm1_local(char *name)
{
  emit_outval p[1];

  emit_param_local(&p[0],TRUE);
  outinstr(name,p,arraysize(p));
}

static void emit_parm1_local_noref(char *name)
{
  emit_outval p[1];

  emit_param_local(&p[0],FALSE);
  outinstr(name,p,arraysize(p));
}

static void emit_parm1_label(char *name)
{
  emit_outval p[1];

  emit_param_label(&p[0]);
  outinstr(name,p,arraysize(p));
}

static void emit_do_casetbl(char *name)
{
  emit_outval p[2];

  (void)name;
  emit_param_nonneg(&p[0]);
  emit_param_label(&p[1]);
  stgwrite("\tcasetbl\n");
  outinstr("case",p,arraysize(p));
}

static void emit_do_case(char *name)
{
  emit_outval p[2];

  emit_param_any(&p[0]);
  emit_param_label(&p[1]);
  outinstr("case",p,arraysize(p));
  code_idx-=opcodes(1);
}

static void emit_do_lodb_strb(char *name)
{
  static const cell valid_values[] = { 1,2,4 };
  emit_outval p[1];

  emit_param_index(&p[0],FALSE,valid_values,arraysize(valid_values));
  outinstr(name,p,arraysize(p));
}

static void emit_do_align(char *name)
{
  static const cell valid_values[] = { 0,sizeof(cell)-1 };
  emit_outval p[1];

  emit_param_index(&p[0],TRUE,valid_values,arraysize(valid_values));
  outinstr(name,p,arraysize(p));
}

static void emit_do_call(char *name)
{
  emit_outval p[1];

  emit_param_function(&p[0],FALSE);
  outinstr(name,p,arraysize(p));
}

static void emit_do_sysreq_c(char *name)
{
  emit_outval p[1];

  emit_param_function(&p[0],TRUE);

  /* if macro optimisations aren't enabled, output a 'sysreq.c' instruction,
   * otherwise generate the following sequence:
   *   const.pri <funcid>
   *   sysreq.pri
   */
  if (pc_optimize<=sOPTIMIZE_NOMACRO) {
    outinstr(name,p,1);
  } else {
    outinstr("const.pri",&p[0],1);
    outinstr("sysreq.pri",NULL,0);
  } /* if */
}

static void emit_do_sysreq_n(char *name)
{
  emit_outval p[2];

  emit_param_function(&p[0],TRUE);
  emit_param_any(&p[1]);

  /* if macro optimisations are enabled, output a 'sysreq.n' instruction,
   * otherwise generate the following sequence:
   *   push <argsize>
   *   sysreq.c <funcid>
   *   stack <argsize> + <cellsize>
   */
  if (pc_optimize>sOPTIMIZE_NOMACRO) {
    outinstr(name,p,2);
  } else {
    outinstr("push.c",&p[1],1);
    outinstr("sysreq.c",&p[0],1);
    p[1].value.ucell+=sizeof(cell);
    outinstr("stack",&p[1],1);
  } /* if */
}

static void emit_do_const(char *name)
{
  emit_outval p[2];

  emit_param_data(&p[0]);
  emit_param_any(&p[1]);

  /* if macro optimisations are enabled, output a 'const' instruction,
   * otherwise generate the following sequence:
   *   push.pri
   *   const.pri <val>
   *   stor.pri <addr>
   *   pop.pri
   */
  if (pc_optimize>sOPTIMIZE_NOMACRO) {
    outinstr(name,p,2);
  } else {
    outinstr("push.pri",NULL,0);
    outinstr("const.pri",&p[1],1);
    outinstr("stor.pri",&p[0],1);
    outinstr("pop.pri",NULL,0);
  } /* if */
}

static void emit_do_const_s(char *name)
{
  emit_outval p[2];

  emit_param_local(&p[0],FALSE);
  emit_param_any(&p[1]);

  /* if macro optimisations are enabled, output a 'const.s' instruction,
   * otherwise generate the following sequence:
   *   push.pri
   *   const.pri <val>
   *   stor.s.pri <addr>
   *   pop.pri
   */
  if (pc_optimize>sOPTIMIZE_NOMACRO) {
    outinstr(name,p,2);
  } else {
    outinstr("push.pri",NULL,0);
    outinstr("const.pri",&p[1],1);
    outinstr("stor.s.pri",&p[0],1);
    outinstr("pop.pri",NULL,0);
  } /* if */
}

static void emit_do_load_both(char *name)
{
  emit_outval p[2];

  emit_param_data(&p[0]);
  emit_param_data(&p[1]);

  /* if macro optimisations are enabled, output a 'load.both' instruction,
   * otherwise generate the following sequence:
   *   load.pri <val1>
   *   load.alt <val2>
   */
  if (pc_optimize>sOPTIMIZE_NOMACRO) {
    outinstr(name,p,2);
  } else {
    outinstr("load.pri",&p[0],1);
    outinstr("load.alt",&p[1],1);
  } /* if */
}

static void emit_do_load_s_both(char *name)
{
  emit_outval p[2];

  emit_param_local(&p[0],TRUE);
  emit_param_local(&p[1],TRUE);

  /* if macro optimisations are enabled, output a 'load.s.both' instruction,
   * otherwise generate the following sequence:
   *   load.s.pri <val1>
   *   load.s.alt <val2>
   */
  if (pc_optimize>sOPTIMIZE_NOMACRO) {
    outinstr(name,p,2);
  } else {
    outinstr("load.s.pri",&p[0],1);
    outinstr("load.s.alt",&p[1],1);
  } /* if */
}

static void emit_do_pushn_c(char *name)
{
  emit_outval p[5];
  int i,numargs;

  assert(name[0]=='p' && name[1]=='u' && name[2]=='s'
         && name[3]=='h' && '2'<=name[4] && name[4]<='5');
  numargs=name[4]-'0';
  for (i=0; i<numargs; i++)
    emit_param_any(&p[i]);

  /* if macro optimisations are enabled, output a 'push<N>.c' instruction,
   * otherwise generate a sequence of <N> 'push.c' instructions
   */
  if (pc_optimize>sOPTIMIZE_NOMACRO) {
    outinstr(name,p,numargs);
  } else {
    for (i=0; i<numargs; i++)
      outinstr("push.c",&p[i],1);
  } /* if */
}

static void emit_do_pushn(char *name)
{
  emit_outval p[5];
  int i,numargs;

  assert(name[0]=='p' && name[1]=='u' && name[2]=='s'
         && name[3]=='h' && '2'<=name[4] && name[4]<='5');
  numargs=name[4]-'0';
  for (i=0; i<numargs; i++)
    emit_param_data(&p[i]);

  /* if macro optimisations are enabled, output a 'push<N>' instruction,
   * otherwise generate a sequence of <N> 'push' instructions
   */
  if (pc_optimize>sOPTIMIZE_NOMACRO) {
    outinstr(name,p,numargs);
  } else {
    for (i=0; i<numargs; i++)
      outinstr("push",&p[i],1);
  } /* if */
}

static void emit_do_pushn_s_adr(char *name)
{
  emit_outval p[5];
  int i,numargs;

  assert(name[0]=='p' && name[1]=='u' && name[2]=='s'
         && name[3]=='h' && '2'<=name[4] && name[4]<='5' && name[5]=='.');
  numargs=name[4]-'0';
  for (i=0; i<numargs; i++)
    emit_param_local(&p[i],TRUE);

  /* if macro optimisations are enabled, output a 'push<N>.s/.adr' instruction,
   * otherwise generate a sequence of <N> 'push.s/.adr' instructions
   */
  if (pc_optimize>sOPTIMIZE_NOMACRO) {
    outinstr(name,p,numargs);
  } else {
    name=(name[6]=='s') ? "push.s" : "push.adr";
    for (i=0; i<numargs; i++)
      outinstr(name,&p[i],1);
  } /* if */
}

static void emit_do_load_u_pri_alt(char *name)
{
  cell val;
  regid reg;
  int ident;

  if (!emit_getrval(&ident,&val))
    return;
  reg=emit_findreg(name);
  if (ident==iCONSTEXPR)
    ldconst(val,reg);
  else if (reg==sALT)
    outinstr("move.alt",NULL,0);
}

static void emit_do_stor_u_pri_alt(char *name)
{
  emit_outval p[1];
  regid reg;
  int ident,islocal,ispushed;

  reg=emit_findreg(name);
  if (!emit_getlval(&ident,&p[0],&islocal,sALT,TRUE,FALSE,(reg==sPRI),(reg==sALT),&ispushed))
    return;
  switch (ident) {
  case iVARIABLE:
    if (islocal)
      outinstr((reg==sPRI) ? "stor.s.pri" : "stor.s.alt",p,1);
    else
      outinstr((reg==sPRI) ? "stor.pri" : "stor.alt",p,1);
    break;
  case iREFERENCE:
    outinstr((reg==sPRI) ? "sref.s.pri" : "sref.s.alt",p,1);
    break;
  case iARRAYCELL:
  case iARRAYCHAR:
    if (ispushed)
      popreg(sPRI);
    if (ident==iARRAYCELL) {
      outinstr("stor.i",NULL,0);
    } else {
      p[0].value.ucell=sCHARBITS/8;
      outinstr("strb.i",p,1);
    } /* if */
    break;
  default:
    assert(0);
    break;
  } /* switch */
}

static void emit_do_addr_u_pri_alt(char *name)
{
  emit_outval p[1];
  regid reg;
  int ident,islocal;

  reg=emit_findreg(name);
  if (!emit_getlval(&ident,&p[0],&islocal,reg,TRUE,TRUE,FALSE,FALSE,NULL))
    return;
  switch (ident) {
  case iVARIABLE:
    if (islocal)
      outinstr((reg==sPRI) ? "addr.pri" : "addr.alt",p,1);
    else if (p[0].value.ucell==(ucell)0)
      outinstr((reg==sPRI) ? "zero.pri" : "zero.alt",NULL,0);
    else
      outinstr((reg==sPRI) ? "const.pri" : "const.alt",p,1);
    break;
  case iREFERENCE:
    outinstr((reg==sPRI) ? "load.s.pri" : "load.s.alt",p,1);
    break;
  case iARRAYCELL:
  case iARRAYCHAR:
    break;
  default:
    assert(0);
    break;
  } /* switch */
}

static void emit_do_push_u(char *name)
{
  cell val;
  int ident;

  if (!emit_getrval(&ident,&val))
    return;
  if (ident==iCONSTEXPR)
    pushval(val);
  else
    outinstr("push.pri",NULL,0);
}

static void emit_do_push_u_adr(char *name)
{
  emit_outval p[1];
  int ident,islocal;

  if (!emit_getlval(&ident,&p[0],&islocal,sPRI,FALSE,TRUE,FALSE,FALSE,NULL))
    return;
  switch (ident) {
  case iVARIABLE:
    outinstr(islocal ? "push.adr" : "push.c",p,1);
    break;
  case iREFERENCE:
    outinstr("push.s",p,1);
    break;
  case iARRAYCELL:
  case iARRAYCHAR:
    pushreg(sPRI);
    break;
  default:
    assert(0);
    break;
  } /* switch */
}

static void emit_do_zero_u(char *name)
{
  emit_outval p[1];
  int ident,islocal;

  if (!emit_getlval(&ident,&p[0],&islocal,sALT,TRUE,FALSE,FALSE,FALSE,NULL))
    return;
  switch (ident) {
  case iVARIABLE:
    outinstr(islocal ? "zero.s" : "zero",p,1);
    break;
  case iREFERENCE:
    outinstr("zero.pri",NULL,0);
    outinstr("sref.s.pri",&p[0],1);
    break;
  case iARRAYCELL:
    outinstr("zero.pri",NULL,0);
    outinstr("stor.i",NULL,0);
    break;
  case iARRAYCHAR:
    outinstr("zero.pri",NULL,0);
    p[0].value.ucell=(ucell)(sCHARBITS/8);
    outinstr("strb.i",p,1);
    break;
  default:
    assert(0);
    break;
  } /* switch */
}

static void emit_do_inc_dec_u(char *name)
{
  emit_outval p[1];
  int ident,islocal;

  assert(strcmp(name,"inc.u")==0 || strcmp(name,"dec.u")==0);

  if (!emit_getlval(&ident,&p[0],&islocal,sPRI,TRUE,FALSE,FALSE,FALSE,NULL))
    return;
  switch (ident) {
  case iVARIABLE:
    if (islocal)
      outinstr((name[0]=='i') ? "inc.s" : "dec.s",p,1);
    else
      outinstr((name[0]=='i') ? "inc" : "dec",p,1);
    break;
  case iREFERENCE:
    outinstr("load.s.pri",&p[0],1);
    /* fallthrough */
  case iARRAYCELL:
    outinstr((name[0]=='i') ? "inc.i" : "dec.i",NULL,0);
    break;
  case iARRAYCHAR:
    p[0].value.ucell=(ucell)(sCHARBITS/8);
    outinstr("move.alt",NULL,0);
    outinstr("lodb.i",p,1);
    outinstr((name[0]=='i') ? "inc.pri" : "dec.pri",NULL,0);
    outinstr("strb.i",p,1);
    break;
  default:
    assert(0);
    break;
  } /* switch */
}

static EMIT_OPCODE emit_opcodelist[] = {
  { NULL,         emit_noop },
  { "add",        emit_parm0 },
  { "add.c",      emit_parm1_any },
  { "addr.alt",   emit_parm1_local },
  { "addr.pri",   emit_parm1_local },
  { "addr.u.alt", emit_do_addr_u_pri_alt },
  { "addr.u.pri", emit_do_addr_u_pri_alt },
  { "align.alt",  emit_do_align },
  { "align.pri",  emit_do_align },
  { "and",        emit_parm0 },
  { "bounds",     emit_parm1_integer },
  { "break",      emit_parm0 },
  { "call",       emit_do_call },
  { "call.pri",   emit_parm0 },
  { "case",       emit_do_case },
  { "casetbl",    emit_do_casetbl },
  { "cmps",       emit_parm1_nonneg },
  { "const",      emit_do_const },
  { "const.alt",  emit_parm1_any },
  { "const.pri",  emit_parm1_any },
  { "const.s",    emit_do_const_s },
  { "dec",        emit_parm1_data },
  { "dec.alt",    emit_parm0 },
  { "dec.i",      emit_parm0 },
  { "dec.pri",    emit_parm0 },
  { "dec.s",      emit_parm1_local_noref },
  { "dec.u",      emit_do_inc_dec_u },
  { "eq",         emit_parm0 },
  { "eq.c.alt",   emit_parm1_any },
  { "eq.c.pri",   emit_parm1_any },
  { "fill",       emit_parm1_nonneg },
  { "geq",        emit_parm0 },
  { "grtr",       emit_parm0 },
  { "halt",       emit_parm1_nonneg },
  { "heap",       emit_parm1_integer },
  { "idxaddr",    emit_parm0 },
  { "idxaddr.b",  emit_parm1_shift },
  { "inc",        emit_parm1_data },
  { "inc.alt",    emit_parm0 },
  { "inc.i",      emit_parm0 },
  { "inc.pri",    emit_parm0 },
  { "inc.s",      emit_parm1_local_noref },
  { "inc.u",      emit_do_inc_dec_u },
  { "invert",     emit_parm0 },
  { "jeq",        emit_parm1_label },
  { "jgeq",       emit_parm1_label },
  { "jgrtr",      emit_parm1_label },
  { "jleq",       emit_parm1_label },
  { "jless",      emit_parm1_label },
  { "jneq",       emit_parm1_label },
  { "jnz",        emit_parm1_label },
  { "jrel",       emit_parm1_integer },
  { "jsgeq",      emit_parm1_label },
  { "jsgrtr",     emit_parm1_label },
  { "jsleq",      emit_parm1_label },
  { "jsless",     emit_parm1_label },
  { "jump",       emit_parm1_label },
  { "jump.pri",   emit_parm0 },
  { "jzer",       emit_parm1_label },
  { "lctrl",      emit_parm1_integer },
  { "leq",        emit_parm0 },
  { "less",       emit_parm0 },
  { "lidx",       emit_parm0 },
  { "lidx.b",     emit_parm1_shift },
  { "load.alt",   emit_parm1_data },
  { "load.both",  emit_do_load_both },
  { "load.i",     emit_parm0 },
  { "load.pri",   emit_parm1_data },
  { "load.s.alt", emit_parm1_local },
  { "load.s.both",emit_do_load_s_both },
  { "load.s.pri", emit_parm1_local },
  { "load.u.alt", emit_do_load_u_pri_alt },
  { "load.u.pri", emit_do_load_u_pri_alt },
  { "lodb.i",     emit_do_lodb_strb },
  { "lref.alt",   emit_parm1_data },
  { "lref.pri",   emit_parm1_data },
  { "lref.s.alt", emit_parm1_local },
  { "lref.s.pri", emit_parm1_local },
  { "move.alt",   emit_parm0 },
  { "move.pri",   emit_parm0 },
  { "movs",       emit_parm1_nonneg },
  { "neg",        emit_parm0 },
  { "neq",        emit_parm0 },
  { "nop",        emit_parm0 },
  { "not",        emit_parm0 },
  { "or",         emit_parm0 },
  { "pop.alt",    emit_parm0 },
  { "pop.pri",    emit_parm0 },
  { "proc",       emit_parm0 },
  { "push",       emit_parm1_data },
  { "push.adr",   emit_parm1_local },
  { "push.alt",   emit_parm0 },
  { "push.c",     emit_parm1_any },
  { "push.pri",   emit_parm0 },
  { "push.r",     emit_parm1_integer },
  { "push.s",     emit_parm1_local },
  { "push.u",     emit_do_push_u },
  { "push.u.adr", emit_do_push_u_adr },
  { "push2",      emit_do_pushn },
  { "push2.adr",  emit_do_pushn_s_adr },
  { "push2.c",    emit_do_pushn_c },
  { "push2.s",    emit_do_pushn_s_adr },
  { "push3",      emit_do_pushn },
  { "push3.adr",  emit_do_pushn_s_adr },
  { "push3.c",    emit_do_pushn_c },
  { "push3.s",    emit_do_pushn_s_adr },
  { "push4",      emit_do_pushn },
  { "push4.adr",  emit_do_pushn_s_adr },
  { "push4.c",    emit_do_pushn_c },
  { "push4.s",    emit_do_pushn_s_adr },
  { "push5",      emit_do_pushn },
  { "push5.adr",  emit_do_pushn_s_adr },
  { "push5.c",    emit_do_pushn_c },
  { "push5.s",    emit_do_pushn_s_adr },
  { "ret",        emit_parm0 },
  { "retn",       emit_parm0 },
  { "sctrl",      emit_parm1_integer },
  { "sdiv",       emit_parm0 },
  { "sdiv.alt",   emit_parm0 },
  { "sgeq",       emit_parm0 },
  { "sgrtr",      emit_parm0 },
  { "shl",        emit_parm0 },
  { "shl.c.alt",  emit_parm1_shift },
  { "shl.c.pri",  emit_parm1_shift },
  { "shr",        emit_parm0 },
  { "shr.c.alt",  emit_parm1_shift },
  { "shr.c.pri",  emit_parm1_shift },
  { "sign.alt",   emit_parm0 },
  { "sign.pri",   emit_parm0 },
  { "sleq",       emit_parm0 },
  { "sless",      emit_parm0 },
  { "smul",       emit_parm0 },
  { "smul.c",     emit_parm1_integer },
  { "sref.alt",   emit_parm1_data },
  { "sref.pri",   emit_parm1_data },
  { "sref.s.alt", emit_parm1_local },
  { "sref.s.pri", emit_parm1_local },
  { "sshr",       emit_parm0 },
  { "stack",      emit_parm1_integer },
  { "stor.alt",   emit_parm1_data },
  { "stor.i",     emit_parm0 },
  { "stor.pri",   emit_parm1_data },
  { "stor.s.alt", emit_parm1_local_noref },
  { "stor.s.pri", emit_parm1_local_noref },
  { "stor.u.alt", emit_do_stor_u_pri_alt },
  { "stor.u.pri", emit_do_stor_u_pri_alt },
  { "strb.i",     emit_do_lodb_strb },
  { "sub",        emit_parm0 },
  { "sub.alt",    emit_parm0 },
  { "swap.alt",   emit_parm0 },
  { "swap.pri",   emit_parm0 },
  { "switch",     emit_parm1_label },
  { "sysreq.c",   emit_do_sysreq_c },
  { "sysreq.n",   emit_do_sysreq_n },
  { "sysreq.pri", emit_parm0 },
  { "udiv",       emit_parm0 },
  { "udiv.alt",   emit_parm0 },
  { "umul",       emit_parm0 },
  { "xchg",       emit_parm0 },
  { "xor",        emit_parm0 },
  { "zero",       emit_parm1_data },
  { "zero.alt",   emit_parm0 },
  { "zero.pri",   emit_parm0 },
  { "zero.s",     emit_parm1_local_noref },
  { "zero.u",     emit_do_zero_u },
};

static int emit_findopcode(const char *instr)
{
  int low,high,mid,cmp;

  /* look up the instruction with a binary search */
  low=1;                /* entry 0 is reserved (for "not found") */
  high=arraysize(emit_opcodelist)-1;
  while (low<high) {
    mid=(low+high)/2;
    cmp=strcmp(instr,emit_opcodelist[mid].name);
    if (cmp>0)
      low=mid+1;
    else
      high=mid;
  } /* while */

  assert(low==high);
  if (strcmp(instr,emit_opcodelist[low].name)==0)
    return low;         /* found */
  return 0;             /* not found, return special index */
}

SC_FUNC void emit_parse_line(void)
{
  cell val;
  char* st;
  int tok,len,i;
  symbol *sym;
  char name[MAX_INSTR_LEN];

  #if !defined NDEBUG
    /* verify that the opcode list is sorted (skip entry 1; it is reserved
     * for a non-existent opcode)
     */
    { /* local */
      static int sorted=FALSE;
      if (!sorted) {
        assert(emit_opcodelist[1].name!=NULL);
        for (i=2; i<arraysize(emit_opcodelist); i++) {
          assert(emit_opcodelist[i].name!=NULL);
          assert(stricmp(emit_opcodelist[i].name,emit_opcodelist[i-1].name)>0);
        } /* for */
        sorted=TRUE;
      } /* if */
    } /* local */
  #endif

  tok=tokeninfo(&val,&st);
  if (tok==tSYMBOL || (tok>tMIDDLE && tok<=tLAST)) {
    /* get the token length */
    if (tok>tMIDDLE && tok<=tLAST)
      len=strlen(sc_tokens[tok-tFIRST]);
    else
      len=strlen(st);

    /* move back to the start of the last fetched token
     * and copy the instruction name
     */
    lptr-=len;
    for (i=0; i<arraysize(name)-1 && (isalnum(*lptr) || *lptr=='.'); ++i,++lptr)
      name[i]=(char)tolower(*lptr);
    name[i]='\0';

    /* find the corresponding argument handler and call it */
    i=emit_findopcode(name);
    if (emit_opcodelist[i].name==NULL && name[0]!='\0')
      error(104,name); /* invalid assembler instruction */
    emit_opcodelist[i].func(name);
  } else if (tok==tLABEL) {
    if ((emit_flags & (efEXPR | efGLOBAL))!=0) {
      error(29);        /* invalid expression, assumed zero */
    } else if (find_constval(&tagname_tab,st,0)!=NULL) {
      error(221,st);    /* label name shadows tagname */
    } else {
      sym=fetchlab(st);
      if ((sym->usage & uDEFINE)!=0)
        error(21,st);   /* symbol already defined */
      setlabel((int)sym->addr);
      sym->usage|=uDEFINE;
    } /* if */
  } /* if */

  if ((emit_flags & (efEXPR | efGLOBAL))==0) {
    assert((emit_flags & efBLOCK)!=0);
    /* make sure the string only contains whitespaces
     * and an optional trailing '}'
     */
    while (*lptr<=' ' && *lptr!='\0')
      lptr++;
    if (*lptr!='\0' && *lptr!='}')
      error(38);  /* extra characters on line */
  } /* if */
}

/* isvariadic
 *
 * Checks if the function is variadic.
 */
static int isvariadic(symbol *sym)
{
  int i;
  for (i=0; curfunc->dim.arglist[i].ident!=0; i++) {
    /* check whether this is a variadic function */
    if (curfunc->dim.arglist[i].ident==iVARARGS) {
      return TRUE;
    } /* if */
  } /* for */
  return FALSE;
}

/* isterminal
 *
 * Checks if the token represents one of the terminal kinds of statements.
 */
static int isterminal(int tok)
{
  return (tok==tRETURN || tok==tBREAK || tok==tCONTINUE || tok==tENDLESS
          || tok==tEXIT || tok==tTERMINAL || tok==tTERMSWITCH);
}

/*  doreturn
 *
 *  Global references: rettype  (altered)
 */
static void doreturn(void)
{
  int tag,ident;
  int level;
  symbol *sym,*sub;

  if (!matchtoken(tTERM)) {
    /* "return <value>" */
    if ((rettype & uRETNONE)!=0)
      error(78);                        /* mix "return;" and "return value;" */
    assert(pc_retexpr==FALSE);
    pc_retexpr=TRUE;
    pc_retheap=0;
    ident=doexpr(TRUE,FALSE,TRUE,FALSE,&tag,&sym,TRUE,NULL);
    pc_retexpr=FALSE;
    needtoken(tTERM);
    /* only warn about unreachable code if the return value is not constant */
    if (ident!=iCONSTEXPR && lastst==tTERMSWITCH)
      error(225); /* unreachable code */
    /* see if this function already has a sub type (an array attached) */
    assert(curfunc!=NULL);
    sub=curfunc->child;
    assert(sub==NULL || sub->ident==iREFARRAY);
    if ((rettype & uRETVALUE)!=0) {
      int retarray=(ident==iARRAY || ident==iREFARRAY);
      /* there was an earlier "return" statement in this function */
      if ((sub==NULL && retarray && sym!=NULL) || (sub!=NULL && !retarray))
        error(79);                      /* mixing "return array;" and "return value;" */
      if (retarray && (curfunc->usage & uPUBLIC)!=0)
        error(90,curfunc->name);        /* public function may not return array */
    } /* if */
    rettype|=uRETVALUE;                 /* function returns a value */
    if (ident==iARRAY || ident==iREFARRAY) {
      int dim[sDIMEN_MAX],numdim=0;
      cell arraysize;
      if (sym==NULL) {
        /* array literals cannot be returned directly */
        error(29); /* invalid expression, assumed zero */
      } else {
        if (sub!=NULL) {
          assert(sub->ident==iREFARRAY);
          /* this function has an array attached already; check that the current
           * "return" statement returns exactly the same array
           */
          level=sym->dim.array.level;
          if (sub->dim.array.level!=level) {
            error(48);                    /* array dimensions must match */
          } else {
            for (numdim=0; numdim<=level; numdim++) {
              dim[numdim]=(int)sub->dim.array.length;
              if (sym->dim.array.length!=dim[numdim])
                error(47);    /* array sizes must match */
              if (numdim<level) {
                sym=sym->child;
                sub=sub->child;
                assert(sym!=NULL && sub!=NULL);
                /* ^^^ both arrays have the same dimensions (this was checked
                 *     earlier) so the dependent should always be found
                 */
              } /* if */
            } /* for */
          } /* if */
        } else {
          int idxtag[sDIMEN_MAX];
          int argcount;
          /* this function does not yet have an array attached; clone the
           * returned symbol beneath the current function
           */
          sub=sym;
          assert(sub!=NULL);
          level=sub->dim.array.level;
          for (numdim=0; numdim<=level; numdim++) {
            dim[numdim]=(int)sub->dim.array.length;
            idxtag[numdim]=sub->x.tags.index;
            if (numdim<level) {
              sub=sub->child;
              assert(sub!=NULL);
            } /* if */
            /* check that all dimensions are known */
            if (dim[numdim]<=0)
              error(46,sym->name);
          } /* for */
          /* the address of the array is stored in a hidden parameter; the address
           * of this parameter is 1 + the number of parameters (times the size of
           * a cell) + the size of the stack frame and the return address
           *   base + 0*sizeof(cell)         == previous "base"
           *   base + 1*sizeof(cell)         == function return address
           *   base + 2*sizeof(cell)         == number of arguments
           *   base + 3*sizeof(cell)         == first argument of the function
           *   ...
           *   base + ((n-1)+3)*sizeof(cell) == last argument of the function
           *   base + (n+3)*sizeof(cell)     == hidden parameter with array address
           */
          assert(curfunc!=NULL);
          assert(curfunc->dim.arglist!=NULL);
          for (argcount=0; curfunc->dim.arglist[argcount].ident!=0; argcount++)
            /* nothing */;
          sub=addvariable(curfunc->name,(argcount+3)*sizeof(cell),iREFARRAY,sGLOBAL,
                          curfunc->tag,dim,numdim,idxtag,0);
          sub->parent=curfunc;
          curfunc->child=sub;
        } /* if */
        /* get the hidden parameter, copy the array (the array is on the heap;
         * it stays on the heap for the moment, and it is removed -usually- at
         * the end of the expression/statement, see expression() in SC3.C)
         */
        if (isvariadic(sub)) {
          pushreg(sPRI);                  /* save source address stored in PRI */
          sub->addr=2*sizeof(cell);
          address(sub,sALT);              /* get the number of arguments */
          getfrm();
          addconst(3*sizeof(cell));
          ob_add();
          dereference();
          swap1();
          popreg(sALT);                   /* ALT = destination */
        } else {
          address(sub,sALT);              /* ALT = destination */
        } /* if */
        arraysize=calc_arraysize(dim,numdim,0);
        memcopy(arraysize*sizeof(cell));  /* source already in PRI */
        /* moveto1(); is not necessary, callfunction() does a popreg() */
      } /* if */
    } /* if */
    modheap(pc_retheap);
    /* try to use "operator=" if tags don't match */
    if (!matchtag(curfunc->tag,tag,TRUE))
      check_userop(NULL,tag,curfunc->tag,2,NULL,&tag);
    /* check tagname with function tagname */
    check_tagmismatch(curfunc->tag,tag,TRUE,-1);
  } else {
    /* this return statement contains no expression */
    ldconst(0,sPRI);
    if ((rettype & uRETVALUE)!=0 && (curfunc->flags & flagNAKED)==0 && !pc_generator) {
      char symname[2*sNAMEMAX+16];      /* allow space for user defined operators */
      assert(curfunc!=NULL);
      funcdisplayname(symname,curfunc->name);
      error(209,symname);               /* function should return a value */
    } /* if */
    rettype|=uRETNONE;                  /* function does not return anything */
  } /* if */
  /* Return-to-awaiter (exp 012, Task 2): an "async" coroutine whose "return" runs
   * delivers its value straight into its awaiter and resumes it. PRI holds the
   * return value here (the 0 of a bare "return;" or the evaluated expression). If
   * B[ASYNC_AWAITER_SLOT]==-1 this is a top-level coroutine with no awaiter, so it
   * just completes as before (the sentinel is -1, not 0, because a real awaiter
   * block can sit at data address 0). This is an EXPLICIT completion (not the T1 "same
   * B[0] site" heuristic): it resumes the awaiter by its B pointer directly via
   * "call.pri", so no scheduler registry lookup is needed for chained awaits. */
  if (curfunc!=NULL && (curfunc->usage & uASYNC)!=0) {
    cell retval_cell,awaiter_cell;
    int lbl_noawaiter;
    /* hidden cell #1: the return value (call.pri below clobbers PRI/ALT) */
    declared+=1; retval_cell=-declared*(cell)sizeof(cell);
    modstk(-(int)sizeof(cell));
    if (curfunc->x.stacksize<declared+1) curfunc->x.stacksize=declared+1;
    stgwrite("\tstor.s.pri ");            /* save the return value */
    outval(retval_cell,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    /* hidden cell #2: the awaiter's state block B[ASYNC_AWAITER_SLOT] */
    declared+=1; awaiter_cell=-declared*(cell)sizeof(cell);
    modstk(-(int)sizeof(cell));
    if (curfunc->x.stacksize<declared+1) curfunc->x.stacksize=declared+1;
    stgwrite("\tload.s.pri ");            /* PRI = B */
    outval(pc_genlocalsbase,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tadd.c ");                 /* PRI = &B[awaiter] */
    outval((cell)ASYNC_AWAITER_SLOT*sizeof(cell),TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tload.i\n");               /* PRI = awaiterB */
    code_idx+=opcodes(1);
    stgwrite("\tstor.s.pri ");            /* save awaiterB */
    outval(awaiter_cell,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    lbl_noawaiter=getlabel();
    stgwrite("\tadd.c ");                 /* PRI = awaiterB + 1 (so the -1 sentinel becomes 0) */
    outval((cell)1,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    jmp_eq0(lbl_noawaiter);               /* awaiterB==-1 -> top-level, just complete */
    /* awaiterB[ASYNC_INBOX_SLOT] = return value */
    stgwrite("\tload.s.pri ");            /* PRI = awaiterB */
    outval(awaiter_cell,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tadd.c ");                 /* PRI = &awaiterB[inbox] */
    outval((cell)ASYNC_INBOX_SLOT*sizeof(cell),TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tmove.alt\n");             /* ALT = &awaiterB[inbox] (destination) */
    code_idx+=opcodes(1);
    stgwrite("\tload.s.pri ");            /* PRI = return value */
    outval(retval_cell,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tstor.i\n");               /* awaiterB[inbox] = return value */
    code_idx+=opcodes(1);
    /* Reset the fault channel before resuming the awaiter: this coroutine
     * returned NORMALLY, so its awaiter's composed "await" must see no fault --
     * otherwise a stale 1 from a leaf fault this inner already handled would
     * wrongly trip the awaiter's Async_Failed(). PRI is dead here (reloaded from
     * awaiter_cell below), so the call's clobber is safe. */
    async_emit_clearfault();
    /* resume awaiterB by its stored entry address: push arg0 = awaiterB, then
     * dispatch through awaiterB[ASYNC_ENTRY_SLOT] via "call.pri" (no new opcode) */
    stgwrite("\tpush.s ");                /* arg0 = awaiterB */
    outval(awaiter_cell,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    pushval((cell)sizeof(cell));          /* 1 argument */
    stgwrite("\tload.s.pri ");            /* PRI = awaiterB */
    outval(awaiter_cell,TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tadd.c ");                 /* PRI = &awaiterB[entry] */
    outval((cell)ASYNC_ENTRY_SLOT*sizeof(cell),TRUE);
    code_idx+=opcodes(1)+opargs(1);
    stgwrite("\tload.i\n");               /* PRI = awaiterB[entry] = code address */
    code_idx+=opcodes(1);
    stgwrite("\tcall.pri\n");             /* indirect call: resume the awaiter */
    code_idx+=opcodes(1);
    setlabel(lbl_noawaiter);
    /* this coroutine has COMPLETED via "return": hand its return value (still live in
     * retval_cell) to __async_complete, which records the result (task_keep /
     * Async_Result), fires any bound callback (task_bind), and reclaims its arena slot
     * unless kept. Covers the composed inner path too (it never returns through a
     * top-level Async_Resume). Emitted BEFORE the hidden cells are freed so retval_cell
     * is still valid; clobbers PRI, which is dead here. */
    async_emit_complete_self(retval_cell);
    /* free the two hidden cells before the function's frame teardown below */
    modstk((int)(2*sizeof(cell)));
    declared-=2;
  } /* if */
  /* in a generator a "return" ends the sequence, so whatever was returned (or
   * the 0 of a bare "return;") is replaced by the stop sentinel that "foreach"
   * watches for */
  if (pc_generator)
    ldconst(generator_iterstop,sPRI);
  destructsymbols(&loctab,0);           /* call destructor for *all* locals */
  modstk((int)declared*sizeof(cell));   /* end of function, remove *all*
                                         * local variables */
  ffret(strcmp(curfunc->name,uENTRYFUNC)!=0);
}

static void dobreak(void)
{
  int *ptr;

  endlessloop=0;      /* if we were inside an endless loop, we just jumped out */
  ptr=readwhile();      /* readwhile() gives an error if not in loop */
  needtoken(tTERM);
  if (ptr==NULL)
    return;
  destructsymbols(&loctab,ptr[wqLVL]);
  clearassignments(1);
  modstk(((int)declared-ptr[wqBRK])*sizeof(cell));
  jumplabel(ptr[wqEXIT]);
}

static void docont(void)
{
  int *ptr;

  /* Distinguish the call-hook chain-advance intrinsic "continue(...)" (which
   * takes a parenthesised argument list) from the loop statement "continue;".
   * The parenthesis peek is the sole discriminator, so a plain "continue;"
   * (no '(') is never disturbed, inside a hook body or not. */
  if (pc_callhook_dispatcher!=NULL) {
    /* Inside a call-hook body, "continue(...)" written as a bare STATEMENT is
     * the chain-advance intrinsic (e.g. calling the original for side effects).
     * lptr sits just past the "continue" keyword statement() lexed; peek the
     * raw line for a following '(' -- a same-line scan that never triggers a
     * new line read, so the "continue" text stays in the current line buffer
     * and can be safely re-lexed. When present, rewind to the keyword and route
     * the statement through the expression path so primary()'s continue(...)
     * lowering runs -- identical to expression position. A plain loop
     * "continue;" (no '(') falls through to the loop handling below untouched. */
    const unsigned char *peek=lptr;
    while (*peek==' ' || *peek=='\t')
      peek++;
    if (*peek=='(') {
      lptr-=strlen(sc_tokens[tCONTINUE-tFIRST]);  /* rewind onto the "continue" keyword */
      lexclr(FALSE);            /* force a fresh lex from the rewound position */
      doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE,NULL);
      needtoken(tTERM);
      /* the call form is an ordinary expression statement: control falls through
       * to the next statement, so mark it tEXPR (NOT tCONTINUE) -- otherwise the
       * statement following it would be wrongly reported as unreachable code. */
      lastst=tEXPR;
      return;
    } /* if */
  } else if (matchtoken('(')) {
    /* Outside a call-hook body the call form is invalid: catch it here with a
     * dedicated diagnostic instead of the misleading "out of context" error. */
    error(257);         /* "continue(...)" only valid inside a hook body */
    lexclr(TRUE);       /* skip the argument list, resync at the terminator */
    lastst=tEXPR;       /* not a flow-terminating loop-continue */
    return;
  } /* if */

  ptr=readwhile();      /* readwhile() gives an error if not in loop */
  needtoken(tTERM);
  lastst=tCONTINUE;     /* plain loop-continue: terminates the current flow */
  if (ptr==NULL)
    return;
  destructsymbols(&loctab,ptr[wqLVL]);
  clearassignments(1);
  modstk(((int)declared-ptr[wqCONT])*sizeof(cell));
  jumplabel(ptr[wqLOOP]);
}

SC_FUNC void exporttag(int tag)
{
  /* find the tag by value in the table, then set the top bit to mark it
   * "public"
   */
  if (tag!=0 && (tag & PUBLICTAG)==0) {
    constvalue *ptr;
    for (ptr=tagname_tab.first; ptr!=NULL && tag!=(int)(ptr->value & TAGMASK); ptr=ptr->next)
      /* nothing */;
    if (ptr!=NULL)
      ptr->value |= PUBLICTAG;
  } /* if */
}

static void doexit(void)
{
  int tag=0;

  if (matchtoken(tTERM)==0){
    doexpr(TRUE,FALSE,FALSE,FALSE,&tag,NULL,TRUE,NULL);
    needtoken(tTERM);
  } else {
    ldconst(0,sPRI);
  } /* if */
  ldconst(tag,sALT);
  exporttag(tag);
  destructsymbols(&loctab,0);           /* call destructor for *all* locals */
  ffabort(xEXIT);
}

static void dosleep(void)
{
  int tag=0;

  if (matchtoken(tTERM)==0){
    doexpr(TRUE,FALSE,FALSE,FALSE,&tag,NULL,TRUE,NULL);
    needtoken(tTERM);
  } else {
    ldconst(0,sPRI);
  } /* if */
  ldconst(tag,sALT);
  exporttag(tag);
  ffabort(xSLEEP);

  /* for stack usage checking, mark the use of the sleep instruction */
  pc_memflags |= suSLEEP_INSTR;
}

static void dostate(void)
{
  constvalue *automaton;
  constvalue *state;
  constvalue *stlist;
  int flabel;
  symbol *sym;
  #if !defined SC_LIGHT
    int length,index,listid,listindex,stateindex;
    char *doc;
  #endif

  /* check for an optional condition */
  if (matchtoken('(')) {
    flabel=getlabel();          /* get label number for "false" branch */
    pc_docexpr=TRUE;            /* attach expression as a documentation string */
    test(flabel,TEST_PLAIN,FALSE);/* get expression, branch to flabel if false */
    pc_docexpr=FALSE;
    needtoken(')');
  } else {
    flabel=-1;
  } /* if */

  if (!sc_getstateid(&automaton,&state)) {
    delete_autolisttable();
    return;
  } /* if */
  needtoken(tTERM);

  /* store the new state id */
  assert(state!=NULL);
  ldconst(state->value,sPRI);
  assert(automaton!=NULL);
  assert(automaton->index==0 && automaton->name[0]=='\0' || automaton->index>0);
  storereg(automaton->value,sPRI);

  /* find the optional entry() function for the state */
  sym=findglb(uENTRYFUNC,sGLOBAL);
  if (sc_status==statWRITE && sym!=NULL && sym->ident==iFUNCTN && sym->states!=NULL) {
    for (stlist=sym->states->first; stlist!=NULL; stlist=stlist->next) {
      assert(!strempty(stlist->name));
      if (state_getfsa(stlist->index)==automaton->index && state_inlist(stlist->index,(int)state->value))
        break;      /* found! */
    } /* for */
    assert(stlist==NULL || state_inlist(stlist->index,state->value));
    if (stlist!=NULL) {
      /* the label to jump to is in stlist->name */
      ffcall(sym,stlist->name,0);
    } /* if */
  } /* if */

  if (flabel>=0)
    setlabel(flabel);           /* condition was false, jump around the state switch */

  #if !defined SC_LIGHT
    /* mark for documentation */
    if (sc_status==statFIRST) {
      char *str;
      /* get the last list id attached to the function, this contains the source states */
      assert(curfunc!=NULL);
      if (curfunc->states!=NULL) {
        stlist=curfunc->states->first;
        assert(stlist!=NULL);
        while (stlist->next!=NULL)
          stlist=stlist->next;
        listid=stlist->index;
      } else {
        listid=-1;
      } /* if */
      listindex=0;
      length=strlen(state->name)+70; /* +70 for the fixed part "<transition ... />\n" */
      /* see if there are any condition strings to attach */
      for (index=0; (str=get_autolist(index))!=NULL; index++)
        length+=strlen(str);
      if ((doc=(char*)malloc(length*sizeof(char)))!=NULL) {
        do {
          sprintf(doc,"<transition target=\"%s\"",state->name);
          if (listid>=0) {
            /* get the source state */
            stateindex=state_listitem(listid,listindex);
            state=state_findid(stateindex);
            assert(state!=NULL);
            sprintf(doc+strlen(doc)," source=\"%s\"",state->name);
          } /* if */
          if (get_autolist(0)!=NULL) {
            /* add the condition */
            strcat(doc," condition=\"");
            for (index=0; (str=get_autolist(index))!=NULL; index++) {
              /* remove the ')' token that may be appended before detecting that the expression has ended */
              if (*str!=')' || *(str+1)!='\0' || get_autolist(index+1)!=NULL)
                strcat(doc,str);
            } /* for */
            strcat(doc,"\"");
          } /* if */
          strcat(doc,"/>\n");
          insert_docstring(doc);
        } while (listid>=0 && ++listindex<state_count(listid));
        free(doc);
      } /* if */
    } /* if */
  #endif
  delete_autolisttable();
}


static void addwhile(int *ptr)
{
  int k;

  ptr[wqBRK]=(int)declared;     /* stack pointer (for "break") */
  ptr[wqCONT]=(int)declared;    /* for "continue", possibly adjusted later */
  ptr[wqLOOP]=getlabel();
  ptr[wqEXIT]=getlabel();
  ptr[wqLVL]=pc_nestlevel+1;
  if (wqptr>=(wq+wqTABSZ-wqSIZE))
    error(102,"loop table");    /* loop table overflow (too many active loops)*/
  k=0;
  while (k<wqSIZE){     /* copy "ptr" to while queue table */
    *wqptr=*ptr;
    wqptr+=1;
    ptr+=1;
    k+=1;
  } /* while */
}

static void delwhile(void)
{
  if (wqptr>wq)
    wqptr-=wqSIZE;
}

static int *readwhile(void)
{
  if (wqptr<=wq){
    error(24);          /* out of context */
    return NULL;
  } else {
    return (wqptr-wqSIZE);
  } /* if */
}

/* parsestringparam()
 *
 * Uses the standard string parsing mechanism to parse string parameters
 * for operator '__pragma'.
 */
static char *parsestringparam(int onlycheck,int *bck_litidx)
{
  int tok;
  int bck_packstr;
  cell val;
  char *str;

  assert(bck_litidx!=NULL);

  /* back up 'litidx', so we can remove the string from the literal queue later */
  *bck_litidx=litidx;
  /* force the string to be packed by default, so it would be easier to process it */
  bck_packstr=sc_packstr;
  sc_packstr=TRUE;

  /* read the string parameter */
  tok=lex(&val,&str);
  sc_packstr=bck_packstr;
  if (tok!=tSTRING || !pc_ispackedstr) {
    /* either not a string, or the user prepended "!" to the option string */
    char tokstr[2];
    if (tok==tSTRING) {
      tok='!';
      litidx=*bck_litidx;       /* remove the string from the literal queue */
    } /* if */
    if (tok<tFIRST) {
      sprintf(tokstr,"%c",tok);
      str=tokstr;
    } else {
      str=sc_tokens[tok-tFIRST];
    } /* if */
    error(1,sc_tokens[tSTRING-tFIRST],str);
    return NULL;
  } /* if */
  assert(litidx>*bck_litidx);

  if (onlycheck) {
    /* skip the byte swapping and remove the string from the literal queue,
     * as the caller only needed to check if the string was valid */
    litidx=*bck_litidx;
    return NULL;
  } /* if */

  /* swap the cell bytes if we're on a Little Endian platform */
#if BYTE_ORDER==LITTLE_ENDIAN
  { /* local */
    char *bytes;
    cell i=val;
    do {
      char t;
      bytes=(char *)&litq[i++];
      t=bytes[0], bytes[0]=bytes[sizeof(cell)-1], bytes[sizeof(cell)-1]=t;
#if PAWN_CELL_SIZE>=32
        t=bytes[1], bytes[1]=bytes[sizeof(cell)-2], bytes[sizeof(cell)-2]=t;
#if PAWN_CELL_SIZE==64
        t=bytes[2], bytes[2]=bytes[sizeof(cell)-3], bytes[sizeof(cell)-3]=t;
        t=bytes[3], bytes[3]=bytes[sizeof(cell)-4], bytes[sizeof(cell)-4]=t;
#endif // PAWN_CELL_SIZE==64
#endif // PAWN_CELL_SIZE>=32
    } while (bytes[0]!='\0' && bytes[1]!='\0'
#if PAWN_CELL_SIZE>=32
             && bytes[2]!='\0' && bytes[3]!='\0'
#if PAWN_CELL_SIZE==64
             && bytes[4]!='\0' && bytes[5]!='\0' && bytes[6]!='\0' && bytes[7]!='\0'
#endif // PAWN_CELL_SIZE==64
#endif // PAWN_CELL_SIZE>=32
    ); /* do */
  } /* local */
#endif
  return (char*)&litq[val];
}

static void dopragma(void)
{
  int bck_litidx;
  int i;
  cell val;
  char *str;

  needtoken('(');

  /* The options are specified as strings, e.g.
   *   native Func() __pragma("naked", "deprecated - use OtherFunc() instead");
   * In order to process the options, we can reuse the standard string parsing
   * mechanism. This way, as a bonus, we'll also be able to use multi-line
   * strings and the stringization operator.
   */
  do {
    /* read the option string */
    str=parsestringparam(FALSE,&bck_litidx);
    if (str==NULL)
      continue;

    /* split the option name from parameters */
    for (i=0; str[i]!='\0' && str[i]!=' '; i++)
      /* nothing */;
    if (str[i]!='\0') {
      str[i]='\0';
      while (str[++i]==' ')
        /* nothing */;
    } /* if */

    /* check the option name, set the corresponding attribute flag
     * and parse the argument(s), if needed */
    if (!strcmp(str,"deprecated")) {
      free(pc_deprecate);
      pc_deprecate=duplicatestring(&str[i]);
      if (pc_deprecate==NULL)
        error(103);     /* insufficient memory */
      pc_attributes |= (1U << attrDEPRECATED);
    } else if (!strcmp(str,"unused")) {
      pc_attributes |= (1U << attrUNUSED);
      if (str[i]!='\0') goto unknown_pragma;
    } else if (!strcmp(str,"unread")) {
      pc_attributes |= (1U << attrUNREAD);
      if (str[i]!='\0') goto unknown_pragma;
    } else if (!strcmp(str,"unwritten")) {
      pc_attributes |= (1U << attrUNWRITTEN);
      if (str[i]!='\0') goto unknown_pragma;
    } else if (!strcmp(str,"nodestruct")) {
      pc_attributes |= (1U << attrNODESTRUCT);
      if (str[i]!='\0') goto unknown_pragma;
    } else if (!strcmp(str,"naked")) {
      pc_attributes |= (1U << attrNAKED);
      if (str[i]!='\0') goto unknown_pragma;
    } else if (!strcmp(str,"warning")) {
      str += i;
      while (*str==' ') str++;
      for (i=0; str[i]!='\0' && str[i]!=' '; i++)
        /* nothing */;
      if (str[i]!='\0') {
        str[i]='\0';
        while (str[++i]==' ')
          /* nothing */;
      } /* if */
      if (strcmp(str,"enable")==0 || strcmp(str,"disable")==0) {
        int len=number(&val,(unsigned char *)&str[i]);
        if (len==0)
          goto unknown_pragma;
        pc_enablewarning((int)val,(str[0]=='e') ? warnENABLE : warnDISABLE);
        /* warn if there are extra characters after the warning number */
        for (i += len; str[i]==' '; i++)
          /* nothing */;
        if (str[i]!='\0')
          goto unknown_pragma;
      } else if (strcmp(str,"push")==0 && str[i]=='\0') {
        pc_pushwarnings();
      } else if (strcmp(str,"pop")==0 && str[i]=='\0') {
        pc_popwarnings();
      } else {
        goto unknown_pragma;
      } /* if */
    } else {
unknown_pragma:
      error(207);       /* unknown #pragma */
    } /* if */

    /* remove the string from the literal queue */
    litidx=bck_litidx;
  } while (matchtoken(','));

  needtoken(')');
}

static void pragma_apply(symbol *sym)
{
  int attr;

  /* make sure we have enough space for all attribute flags */
  assert_static((int)NUM_ATTRS<=sizeof(pc_attributes)*8);

  /* if no attributes are set, then we have a quick exit */
  if (pc_attributes==0)
    return;

  assert(sym!=NULL);

  for (attr=0; attr<NUM_ATTRS; attr++) {
    if ((pc_attributes & (1U << attr))==0)
      continue;
    switch (attr) {
    case attrDEPRECATED:
      pragma_deprecated(sym);
      break;
    case attrUNREAD:
    case attrUNWRITTEN:
    case attrUNUSED:
      pragma_unused(sym,(attr==attrUNREAD),(attr==attrUNWRITTEN));
      break;
    case attrNODESTRUCT:
      pragma_nodestruct(sym);
      break;
    case attrNAKED:
      if (sym->ident==iFUNCTN)
        sym->flags |= flagNAKED;
      break;
    default:
      assert(0);
    } /* switch */
  } /* for */

  pc_attributes=0;
}

SC_FUNC void pragma_deprecated(symbol *sym)
{
  if (pc_deprecate!=NULL) {
    if (sym->ident==iFUNCTN) {
      sym->flags |= flagDEPRECATED;
      if (sc_status==statWRITE) {
        if (sym->documentation!=NULL)
          free(sym->documentation);
        sym->documentation=pc_deprecate;
        pc_deprecate=NULL;
      } /* if */
    } /* if */
    free(pc_deprecate);
    pc_deprecate=NULL;
  } /* if */
}

SC_FUNC void pragma_unused(symbol *sym, int unread, int unwritten)
{
  assert(!unread || !unwritten);
  /* mark as read if the pragma wasn't "unwritten" */
  if (!unwritten) {
    sym->usage |= uREAD;
    sym->usage &= ~uASSIGNED;
  } /* if */
  /* mark as written if the pragma wasn't "unread" */
  if (sym->ident == iVARIABLE || sym->ident == iREFERENCE
      || sym->ident == iARRAY || sym->ident == iREFARRAY)
    sym->usage |= unread ? 0 : uWRITTEN;
}

SC_FUNC void pragma_nodestruct(symbol *sym)
{
  if (sym->ident==iVARIABLE || sym->ident==iARRAY)
    sym->usage |= uNODESTRUCT;
}

/* do_static_check()
 * Checks compile-time assertions and triggers an error/warning.
 *
 * The 'use_warning' parameter is set to TRUE if warnings are to be
 * used instead of errors to notify assertion failures.
 */
SC_FUNC cell do_static_check(int use_warning)
{
  int already_staging,already_recording,optmsg;
  int ident,index;
  int bck_litidx,recstartpos;
  cell cidx,val;
  char *str;

  optmsg=FALSE;
  index=0;
  cidx=0;

  needtoken('(');
  already_staging=stgget(&index,&cidx);
  if (!already_staging) {
    stgset(TRUE);       /* start stage-buffering */
    errorset(sEXPRMARK,0);
  } /* if */
  already_recording=pc_isrecording;
  if (!already_recording) {
    recstart();
    recstartpos=0;
  } else {
    recstop();  /* trim out the part of the current line that hasn't been read by lex() yet */
    recstartpos=strlen(pc_recstr);
    recstart(); /* restart recording */
  } /* if */
  ident=expression(&val,NULL,NULL,FALSE);
  if (!already_recording || val==0)
    recstop();
  str=&pc_recstr[recstartpos];
  if (recstartpos!=0 && pc_recstr[recstartpos]==' ')
    str++;      /* skip leading whitespace */
  if (ident!=iCONSTEXPR)
    error(8);           /* must be constant expression */
  stgdel(index,cidx);   /* scratch generated code */
  if (!already_staging) {
    errorset(sEXPRRELEASE,0);
    stgset(FALSE);      /* stop stage-buffering */
  } /* if */

  /* read the optional message */
  if (matchtoken(',')) {
    if (!already_recording) {
      free(pc_recstr);
      pc_recstr=NULL;
    } /* if */
    optmsg=TRUE;
    str=parsestringparam(val!=0,&bck_litidx);
  } /* if */

  if (val==0) {
    int errnum=use_warning ? 249    /* check failed */
                           : 110;   /* assertion failed */
    error(errnum,(str!=NULL) ? str : "");
    if (optmsg)
      litidx=bck_litidx;        /* remove the string from the literal queue */
    if (already_recording)
      recstart();               /* restart recording */
  } /* if */
  if (!optmsg && !already_recording) {
    free(pc_recstr);
    pc_recstr=NULL;
  } /* if */
  needtoken(')');
  return !!val;
}
