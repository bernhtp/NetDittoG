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
DirList						gTarget;	// blobal target state

int
   wmain(
      int                    argn        ,// in -number of arguments
      WCHAR const         ** argv         // in -argument values
   )
{
   DWORD                     rc;
   DirEntry                * srcEntry,
                           * tgtEntry;
   time_t                    t;

   if ( rc = ParmParse(argv) )
      return rc;
   if ( gLogName )
      err.LogOpen(gLogName, 0, -1);
   OptionsConstruct();
   if ( gOptions.global & OPT_GlobalBackup )
      BackupPriviledgeSet();

   if ( !wcscmp(gSource.Path(), L"-") )
      srcEntry = NULL;
   else
   {
	   rc = gSource.PathDirExists();
//      rc = PathDirExists(gSource.ApiPath(), &srcEntry);
      if ( rc != 1 )
         err.MsgWrite(50001, L"Source directory base, %s, does not exist (%lu)",
                             gSource.Path(), rc);
   }

   rc = gTarget.PathDirExists();
//   rc = PathDirExists(gTarget.ApiPath(), &tgtEntry);
   if ( rc != 1 )
      if ( !(gOptions.global & OPT_GlobalMakeTgt) || srcEntry == NULL )
         err.MsgWrite(50004, L"Target directory(%s) missing (%ld): "
                             L"/m not specified or '-' specified as source",
                             gTarget.Path(), rc);

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
