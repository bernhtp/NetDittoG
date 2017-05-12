/*
===============================================================================

  Module     - NetDitto
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 06/13/92
  Description- main program
               NetDitto detects, and optionally replicates, differences between
               the source and target paths in a variety of domains including
               directory and file contents, attributes and permissions.  What
               components are examined for differences (and hence replicated)
               are completely specifyable on the command line as well as other
               options that resolve conflicts and special situations.

  Updates -

===============================================================================
*/

#include "netditto.hpp"

// globals
Options                     gOptions;   // global options
DirList						gSource;	// global source state
DirList						gTarget;	// global target state

int
   wmain(
      int                    argn        ,// in -number of arguments
      WCHAR const         ** argv         // in -argument values
   )
{
	WCHAR const *x[] = { L"NetDittoG", L"\\temp\\x", L"\\temp\\xxx", L"/-u", L"/m", L"/l\\temp\\x.log", NULL };
	DWORD                     rc;
   DirEntry                * srcEntry,
                           * tgtEntry;
   time_t                    t;

   if ( rc = ParmParse(x) )
      return rc;
   if ( gLogName )
      err.LogOpen(gLogName, 0, -1);
   OptionsConstruct();
   if ( gOptions.global & OPT_GlobalBackup )
      BackupPriviledgeSet();

   PathExistsResult ret;
   if ( !wcscmp(gSource.Path(), L"-") )
      srcEntry = NULL;
   else
   {
	   ret = gSource.PathDirExists(&srcEntry);
	   if ( ret != PathExistsResult::YesDir )
         err.MsgWrite(50001, L"Source directory base, %s, does not exist (%lu)",
                             gSource.Path(), ret);
   }
   ret = gTarget.PathDirExists(&tgtEntry);
   if ( rc != PathExistsResult::YesDir )
      if ( !(gOptions.global & OPT_GlobalMakeTgt) || srcEntry == NULL )
         err.MsgWrite(50004, L"Target directory(%s) missing (%ld): "
                             L"/m not specified or '-' specified as source",
                             gTarget.Path(), ret);

   if ( !_wcsicmp(gSource.Path(), gTarget.Path()) )
      err.MsgWrite(50005, L"Source path (%s) same as target",
                          gSource.Path());

   OptionsResolve();
   DisplayInit(1);
   time(&t);
   if ( gOptions.global & OPT_GlobalSilent )
      err.LevelBeepSet(9);
   err.MsgWrite(0, L"%-.24s S=%s T=%s O=%X", _wctime(&t), gSource.Path(),
                   gTarget.Path(), gOptions.global);
   DisplayOptions();
   DisplayPaths();
   DisplayPathOffset(gTarget.Path());

   StatsTimerCreate();
   if ( gOptions.spaceMinFree  ||  gOptions.spaceInterval )
      SpaceCheckStart();

   MatchEntries(0, srcEntry, tgtEntry);

   gOptions.fState |= FLAG_Shutdown;
   StatsTimerTerminate();
   if ( gOptions.spaceMinFree  ||  gOptions.spaceInterval )
      SpaceCheckTerminate();
   DisplayTime();
   time(&t);
   err.MsgWrite(0, L"End time=%-.24s", _wctime(&t));
   DisplayInit(0);
   return 0;
}


void _stdcall OptionsConstruct()
{
	gOptions.findAttr = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY
					| FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY
					| FILE_ATTRIBUTE_ARCHIVE;

	gOptions.copyBuffer = (PBYTE)VirtualAlloc(NULL,
											gOptions.sizeBuffer = COPYBUFFSIZE,
											MEM_COMMIT,
											PAGE_READWRITE);
	if ( !gOptions.copyBuffer )
		err.SysMsgWrite(50998, GetLastError(), L"CopyBuffer VirtualAlloc(%u)=%ld ",
			gOptions.sizeBuffer, GetLastError());
}

