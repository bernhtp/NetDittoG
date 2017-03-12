/*
================================================================================

  Program    - MTsupp
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/28/93
  Description- These functions provide the OS/2-specific multi-threaded (MT)
               support.  They must be compiled and linked with MS C 6.
               Currently the only thread is for statistics display asynchronously
               with the actual replication/comparison thread.  However, other
               threads may be added for enhancement such as for periodically
               checking for free disk space on the target.

  Updates -

================================================================================
*/

#include <process.h>

#include "netditto.hpp"
#include "util32.hpp"

static TEvent                 eventStats(FALSE, FALSE); // unsignalled auto-reset event
static TEvent                 eventSpace(FALSE, FALSE); // unsignalled auto-reset event
#define ThreadFunc void (__cdecl *)(void *)

//-----------------------------------------------------------------------------
// Thread function that displays statistics at each gOptions.statsInterval.
// It runs until a RAM semaphore (hStatsMutex) is cleared by the
// StatsDisplayTerminate function called my the main procedure.  It re-sets the
// semaphore until it is complete at which point it clears it to allow other
// processing to resume and the main program to end.
//-----------------------------------------------------------------------------
static void _cdecl
   StatsDisplayThread()
{
   DWORD                     rc;
   short                     done = 0;

   while ( !done )
   {
      switch ( rc = eventStats.WaitSingle(gOptions.statsInterval) )
      {
         case WAIT_OBJECT_0:
         case WAIT_ABANDONED:
            done = 1;
            break;
         case WAIT_TIMEOUT:
            break;
         case WAIT_FAILED:
         default:
            err.SysMsgWrite(50112, rc, L"WaitForSingleObject(eventStats)=%d ", rc);
      }
      DisplayStatsCommon(&gOptions.stats.source, &gOptions.stats.target);
      DisplayStatsChange(&gOptions.stats.change);
      DisplayStatsMatch(&gOptions.stats.match);
      DisplayTime();
   }

   eventStats.Set();
   _endthread();
}


//-----------------------------------------------------------------------------
// Creates the statistics display thread for periodic statistics update for
// the duration of the program
//-----------------------------------------------------------------------------
short _stdcall
   StatsTimerCreate()
{
   ULONG                    hStatsThread;

   if ( gOptions.statsInterval == 0 )
      gOptions.statsInterval = 1000;      // default to 1000ms (1 sec)

   hStatsThread = _beginthread((ThreadFunc)StatsDisplayThread,
                               10000, NULL);
   if ( hStatsThread == -1 )
      err.SysMsgWrite(50503, GetLastError(), L"_beginthread(StatsDisplayThread) ");

   return 0;
}

//-----------------------------------------------------------------------------
// Terminates the statistics display thread by clearing its RAM semaphore and
// then waiting until it is clear again to let the thread finish processing
// before returning to the caller.
//-----------------------------------------------------------------------------
void _stdcall
   StatsTimerTerminate()
{
   eventStats.Set();         // Releases the wait in the stats display thread
   eventStats.WaitSingle(gOptions.statsInterval);
}


//-----------------------------------------------------------------------------
// checks disk space periodically
//-----------------------------------------------------------------------------
static void _cdecl
   SpaceCheckThread()
{
   short                     done = 0;
   __int64                   spaceFree;
   WCHAR                     path[_MAX_PATH];
   DWORD                     rc,
                             sectorsPerCluster,
                             bytesPerSector,
                             freeClusters,
                             totalClusters;

   while ( !done )
   {
      wcscpy(path, gOptions.target.path);
      if ( path[1] == L'\\' )             // UNCs must be backslash-terminated
         wcscat(path, L"\\");             // for some Win32 APIs
      if ( !GetDiskFreeSpace(path, &sectorsPerCluster,
                             &bytesPerSector, &freeClusters, &totalClusters) )
      {
         err.MsgWrite(50113, L"GetDiskFreeSpace(%s)=%ld", path, GetLastError());
      }
      else
      {
         spaceFree = (__int64)sectorsPerCluster * bytesPerSector * freeClusters;
         if ( spaceFree <= gOptions.spaceMinFree )
            err.MsgWrite(50114, L"Space avail < %ld", gOptions.spaceMinFree);
         else
            err.MsgWrite(0, L"Target space free=%luk", (DWORD)(spaceFree / 1000));
      }

      switch ( rc = eventSpace.WaitSingle(gOptions.spaceInterval) )
      {
         case WAIT_OBJECT_0:
         case WAIT_ABANDONED:
            done = 1;
            break;
         case WAIT_TIMEOUT:
            break;
         case WAIT_FAILED:
         default:
            rc = GetLastError();
            err.SysMsgWrite(50112, rc, L"WaitForSingleObject(eventSpace)=%ld ", rc);
      }
   }

   eventSpace.Set();
   _endthread();
}


//-----------------------------------------------------------------------------
// Starts a thread to periodically check free disk space and terminates the
// main process if it is below a defined threshhold.
//-----------------------------------------------------------------------------
void _stdcall
   SpaceCheckStart()
{
   ULONG                     hSpaceThread;

   if ( gOptions.spaceInterval == 0 )
      gOptions.spaceInterval = 60000l;     // default to 60 secs
   if ( gOptions.spaceMinFree == 0 )
      gOptions.spaceMinFree = 1000000l;    // default to 1MB

   hSpaceThread = _beginthread((ThreadFunc)SpaceCheckThread,
                               10000, NULL);
   if ( hSpaceThread == -1 )
      err.SysMsgWrite(50503, GetLastError(), L"_beginthread(StatsDisplayThread) ");
}


//-----------------------------------------------------------------------------
// Terminates the space check/display thread by clearing its RAM semaphore and
// then waiting until it is clear again to let the thread finish processing
// before returning to the caller.
//-----------------------------------------------------------------------------
void _stdcall
   SpaceCheckTerminate(
   )
{
   eventSpace.Set();
   eventSpace.WaitSingle(gOptions.spaceInterval);
}

/*
//-----------------------------------------------------------------------------
// Thread that does overlapped DirGet -- directory scanning
//-----------------------------------------------------------------------------
struct OverlappedScanInfo
{
   DirOptions              * dir;
   StatsCommon             * stats;
   DWORD                     rc;
};


static void _cdecl
   DirGetThread(
      OverlappedScanInfo   * info          // i/o-data to pass to DirGet()
   )
{
   DWORD                     rc;

   while ( 1 )
   {
      switch ( rc = gOptions.evDirGetStart->WaitSingle() )
      {
         case WAIT_OBJECT_0:
            if ( gOptions.fState & FLAG_Shutdown )
               return;
            info->rc = DirGet(info->dir, info->stats,NULL);
            gOptions.evDirGetComplete->Set();
            break;
         default:
            if ( gOptions.fState & FLAG_Shutdown )
               return;
            err.SysMsgWrite(ErrS, rc, L"OverlappedScanWait=%ld ", rc);
      }
   }
   _endthread();
}
*/