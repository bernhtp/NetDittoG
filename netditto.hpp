/*
===============================================================================
 module     : NetDitto.h
 description:
    prototypes, macros, constants and headers for the NetDitto utility
  Module     - NetDitto.h
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 92/00/00
  Description- Prototypes, macros, constants and headers for the NetDitto
               utility.


===============================================================================
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "Err.hpp"
#include "Common.hpp"
#include "WildMatch.hpp"
#include "TList.h"

#include "stats.h"
#include "DirList.h"

//-----------------------------------------------------------------------------
// Options structures, types, macros and constants
//-----------------------------------------------------------------------------

typedef wchar_t          LogAction;     // action such as M=Make, U=Update, R=Remove
struct LogActions
{
   LogAction                 contents;
   LogAction                 attr;
   LogAction                 perms;
};

struct FileList                          // type for file lists used for include and exclude
{                                        // next file in list or NULL for end
	FileList			  * next;
	wchar_t					name[1];	// a variable length string for the wildcard filter
};

typedef byte                 Actions;    // actions that can be taken (bitmask)

struct Property                          // Actions for dir/file properties
{
   Actions                   contents;   // name, contents of file
   Actions                   attr;       // file/dir attributes
   Actions                   perms;      // file/dir permissions
};

DWORD static const GETATTRIB =	FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE
							  | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY
							  | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY;

#define OPT_PropActionMake   0x01        // create object
#define OPT_PropActionUpdate 0x02        // replace object
#define OPT_PropActionRemove 0x04        // delete object
#define OPT_PropActionAll    0x07        // replicate all
#define OPT_PropActionNone   0x00        // replicate none

#define OPT_GlobalReadOnly   0x00000001  // allow replace/delete of R/O files
#define OPT_GlobalOptimize   0x00000002  // consider same time/size as same
#define OPT_GlobalNewer      0x00000004  // only replicate newer differences
#define OPT_GlobalPermAttr   0x00000008  // consider ACL attr field significant in compare
#define OPT_GlobalDirPrec    0x00000010  // dirs have precedence not same type
#define OPT_GlobalChange     0x00000100  // change vs. compare only
#define OPT_GlobalHidden     0x00000040  // allow replace/delete of hidden/system files
#define OPT_GlobalDirTime    0x00000080  // replicate dir attr including timestamp
#define OPT_GlobalAttrArch   0x00000020  // replicate archive attr differences
#define OPT_GlobalMakeTgt    0x00000200  // make target directory if not exist
#define OPT_GlobalDispDetail 0x00000400  // display detail
#define OPT_GlobalDispMatches 0x00000800 // display detail matches
#define OPT_GlobalCopyXOR    0x00001000  // complement bits when copying
#define OPT_GlobalSilent     0x00002000  // Turn off beep
#define OPT_GlobalBackup     0x00004000  // Use backup semantics/APIs
#define OPT_GlobalBackupForce 0x00008000 // Use backup semantics/APIs and for update
#define OPT_GlobalNameCase   0x00010000  // make name case significant when different
#define OPT_GlobalReadComp   0x00020000  // read source compression type for target repl
#define OPT_DirFilter        0x00040000  // directory exclude filter set

#define FLAG_Shutdown        (1 << 0)    // Shutdown program
#define FLAG_SameVolume      (1 << 1)    // source and target on same volume name
#define FLAG_OverlappedScan  (1 << 2)    // overlapped directory scanning



static const int COPYBUFFSIZE = 1 << 18;// 256K

struct Options                          // main object of system containing processed parms and data structs
{
	__int64           bWritten;			// bytes  written for stats display
	__int64           spaceMinFree;		// space free minimum
	FileList        * include;			// list of filespecs to include
	FileList        * exclude;			// list of filespecs to exclude
	FileList		* direxclude;		// list of wildcards to exclude directories
	BYTE            * copyBuffer;		// copy buffer - file/dir contents/ACLs
	long              statsInterval;	// stats display interval (mSec) for MT version
	long              spaceInterval;	// space free check interval (mSec) for MT version
	DWORD             sizeBuffer;		// copy buffer size
	short             maxLevel;			// max directory recursion level
	Property          dir;				// actions for dir/properties
	Property          file;				// actions for file/properties
	DWORD             fState;			// status flags
	DWORD             global;			// global actions
	Stats             stats;			// statistics
	DWORD             findAttr;			// DosFind attribute
	DWORD             attrSignif;		// mask of significant attributes to compare
	WIN32_STREAM_ID * unsecure;			// backup stream to unsecure object for deletion
};


short _stdcall
   DisplayInit(
      short                  mode         // in -1=fullscreen, 0=text
   );

void _stdcall
   DisplayDirStats(
      StatsDirGet const    * source      ,// in -source directory get stats
      StatsDirGet const    * target       // in -target directory get stats
   );

void _stdcall
   DisplayStatsChange(
      StatsChange const    * stats        // in -change statistics
   );

void _stdcall
   DisplayStatsCommon(
      StatsCommon const    * source      ,// in -source directory get stats
      StatsCommon const    * target       // in -target directory get stats
   );

void _stdcall
   DisplayStatsMatch(
      StatsMatch const     * stats        // in -change statistics
   );

void _stdcall
   DisplayOptions(
   );

void _stdcall
   DisplayPathOffset(
       WCHAR const         * path         // in -target path
   );

void _stdcall
   DisplayPaths(
   );

void _stdcall
   DisplayTime(
   );

DWORD _stdcall
   FileContentsCompare(
   );

DWORD _stdcall
   FileCopy(
      DirEntry const       * srcEntry    ,// in -source directory entry
      DirEntry const       * tgtEntry     // in -target directory entry
   );
DWORD _stdcall
   FileCopyContentsOverlapped(
      HANDLE                 hSrc        ,// in -source file handle
      HANDLE               * hTgt         // i/o-target file handle
   );
DWORD _stdcall
   FileBackupCopy(
      DirEntry const       * srcEntry    ,// in -source directory entry
      DirEntry const       * tgtEntry     // in -target directory entry
   );

//WORD _stdcall
//   FileNoSource(
//      DirEntry const       * tgtEntry     // in -current source entry processed
//   );

short _stdcall                            // ret-0=accept 1=notInclude 2=Exclude
   FilterReject(
      WCHAR const          * name        ,// in -name to filter
      FileList const       * include     ,// in -include list
      FileList const       * exclude      // in -exclude list
   );

bool DirFilterReject(wchar_t const * p_name);	// returns true if p_name matches any of the wildcards in the dir exclude list

DWORD _stdcall                            // ret-0=success
   MatchEntries(
      short                  level       ,// in -current recursion/directory level
      DirEntry             * srcDirEntry ,// in -source dir entry
      DirEntry             * tgtDirEntry  // in -target dir entry
   );

DWORD _stdcall
   MatchedFileProcess(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   );

DWORD _stdcall
   MatchedDirTgtExists(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   );

DWORD _stdcall
   MatchedDirNoTgtExit(
      DirEntry const       * srcEntry     // in -current source entry processed
   );

DWORD _stdcall
   MatchedDirNoTgt(
      DirEntry const       * srcEntry     // in -current source entry processed
   );

void _stdcall
   MismatchSourceNotDir(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   );

void _stdcall
   MismatchTargetNotDir(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   );

DWORD _stdcall                            // ret-0=success
   ParmParse(
       WCHAR const         ** argv        // in -argument values
   );

void _stdcall                             // ret-0=success
   OptionsConstruct();

DWORD _stdcall                            // ret-number of warnings/fixes
   OptionsResolve();

short _stdcall                            // ret-0=perms equal
   PermCreate(
      BOOL                   isDir       ,// in -0=file, 1=dir
      LogAction            * logAction    // out-log action taken
   );

short _stdcall                            // ret-0=perms equal
   PermDelete(
      BOOL                   isDir       ,// in -0=file, 1=dir
      LogAction            * logAction    // out-log action taken
   );

short _stdcall                            // ret-0=perms equal
   PermCompare(
   );

short _stdcall                            // ret-0=perms equal
   PermReplicate(
      BOOL                   isDir       ,// in -0=file, 1=dir
      LogAction            * logAction    // out-log action taken
   );

BOOL
   CompressionSet(
      HANDLE                 hSrc         ,// in -source dir/file handle
      HANDLE                 hTgt         ,// in -target dir/file handle (if open)
      DWORD                  attr          // in -target file/dir attribute
   );

BOOL BackupPriviledgeSet();

BOOL
   UnsecureForDelete(
      DirEntry const       * tgtEntry
   );

short _stdcall
   StatsTimerCreate(
   );

void _stdcall
   StatsTimerTerminate(
   );

void _stdcall
   SpaceCheckStart(
   );

void _stdcall
   SpaceCheckTerminate(
   );

// Derivation of standard TError for full screen message handling
class TErrorScreen : public TError
{
public:
                        TErrorScreen(
      int                    displevel = 0	,// in -mimimum severity level to display
      int                    loglevel = 0	,// in -mimimum severity level to log
      WCHAR   const        * filename = L""	,// in -file name of log (NULL if none)
      int                    logmode = 0  )	 // in -0=replace, 1=append
                        : TError(displevel, loglevel, filename, logmode) { };

   virtual void __stdcall StrWrite(int level, WCHAR const * str) const;
};

// ------------------------------- Globals ------------------------------------
extern WCHAR              * gLogName;
extern Options              gOptions;
extern TErrorScreen			err;

extern DirList				gSource;
extern DirList				gTarget;
