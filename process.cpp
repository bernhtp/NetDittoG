/*
===============================================================================

  Module     - Process
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/23/93
  Description- Functions in this module process replication once the source
               and target entries have been "matched".  This may include
               file create/copy/deletion, ACL and attribute modification, etc.
  Updates -

===============================================================================
*/
#include <stdlib.h>
#include <string.h>
#include <direct.h>

#include "netditto.hpp"
#include <winioctl.h>


//-----------------------------------------------------------------------------
// Propagates the compression attribute state
//-----------------------------------------------------------------------------
BOOL
   CompressionSet(
      HANDLE                 hSrc         ,// in -source dir/file handle
      HANDLE                 hTgt         ,// in -target dir/file handle (if open)
      DWORD                  attr          // in -target file/dir attribute
   )
{
   DWORD                     rc,
                             nBytes,
                             openMode = 0;
   HANDLE                    handle = INVALID_HANDLE_VALUE;
   USHORT                    compressType; // compression type/attr

   // if we need to set to compressed, get compression type
   if ( !(attr & FILE_ATTRIBUTE_COMPRESSED) )
      compressType = COMPRESSION_FORMAT_NONE;
   else
   {
      if ( !(gOptions.global & OPT_GlobalReadComp) )
         compressType = COMPRESSION_FORMAT_DEFAULT;
      else
      {
         // first see if we need to open source file or dir
         if ( hSrc == INVALID_HANDLE_VALUE )
         {
            handle = CreateFile(gSource.ApiPath(),
                                GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS,
                                NULL);
            if ( handle == INVALID_HANDLE_VALUE )
            {
               rc = GetLastError();
               err.SysMsgWrite(ErrS, rc, L"CreateFileGC(%s)=%ld ",
                                     gSource.Path(), rc);
               return FALSE;
            }
         }
         else
            handle = hSrc;

         if ( !DeviceIoControl(handle,
                               FSCTL_GET_COMPRESSION,
                               NULL,
                               0,
                               (LPVOID)&compressType,
                               sizeof compressType,
                               &nBytes,
                               NULL) )
         {
            rc = GetLastError();
            err.SysMsgWrite(ErrS, rc, L"Get compression(%hx,%s)=%ld ",
                                  compressType, gSource.Path(), rc);
            compressType = COMPRESSION_FORMAT_DEFAULT;
         }

         // If no parm handle, we opened it here and must close it
         if ( hSrc == INVALID_HANDLE_VALUE )
            CloseHandle(handle);
      }
   }

   // See if we need to open target file or dir
   if ( hTgt == INVALID_HANDLE_VALUE )
   {
      if ( attr & FILE_ATTRIBUTE_DIRECTORY
        || gOptions.global & (OPT_GlobalBackup | OPT_GlobalBackupForce) )
         openMode = FILE_FLAG_BACKUP_SEMANTICS;
      if ( attr & FILE_ATTRIBUTE_READONLY )
         SetFileAttributes(gTarget.ApiPath(), FILE_ATTRIBUTE_NORMAL);
      handle = CreateFile(gTarget.ApiPath(),
                        FILE_READ_DATA | FILE_WRITE_DATA,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        openMode,
                        NULL);
      if ( handle == INVALID_HANDLE_VALUE )
      {
         rc = GetLastError();
         err.SysMsgWrite(ErrS, rc, L"CreateFileSC(%hx,%s)=%ld ",
                               compressType, gTarget.Path(), rc);
         return FALSE;
      }
   }
   else
      handle = hTgt;

   if ( !DeviceIoControl(handle,
                         FSCTL_SET_COMPRESSION,
                         &compressType,
                         sizeof compressType,
                         NULL,
                         0,
                         &nBytes,
                         NULL) )
   {
      rc = GetLastError();
      err.SysMsgWrite(ErrS, rc, L"Set compression(%hd,%s)=%ld ",
                                compressType, gTarget.Path(), rc);
   }

   // only close if we had to open it for this operation, else leave it open
   if ( hTgt == INVALID_HANDLE_VALUE  &&  handle != INVALID_HANDLE_VALUE )
      CloseHandle(handle);

   if ( attr & FILE_ATTRIBUTE_READONLY )
      SetFileAttributes(gTarget.ApiPath(), attr);

   return TRUE;
}

// Renames file or directory when case is different
DWORD
   FileDirRename(
      DirEntry const       * srcEntry
   )
{
   DWORD                     rc = 0;
   WCHAR                     newName[_MAX_PATH];
   size_t                    len;

   len = wcslen(gTarget.Path()) - wcslen(srcEntry->cFileName);
   wcsncpy(newName, gTarget.Path(), len);
   wcscpy(newName+len, srcEntry->cFileName);
   if ( !MoveFile(gTarget.ApiPath(), newName) )
   {
      rc = GetLastError();
      err.SysMsgWrite(ErrE, L"Rename(%s,%s)=%ld ",
                            gTarget.Path(), newName, rc);
   }
   return rc;
}


//-----------------------------------------------------------------------------
// Removes the target directory (if not CompareOnly)
//-----------------------------------------------------------------------------
static
DWORD _stdcall
   DirRemove(
      DirEntry const       * tgtEntry    ,// in -current target entry processed
      LogActions           * log          // out-log action codes
   )
{
   DWORD                     rc = 0;

   if ( tgtEntry->attrFile & FILE_ATTRIBUTE_READONLY
    && !( gOptions.global & OPT_GlobalReadOnly ) )
      return 0;

   gOptions.stats.change.dirRemoved++;
   log->contents = L'R';
   if ( gOptions.global & OPT_GlobalChange )
   {
      if ( tgtEntry->attrFile & FILE_ATTRIBUTE_READONLY )
         // make R/W if R/O
         if ( !SetFileAttributes(gTarget.ApiPath(), FILE_ATTRIBUTE_NORMAL) )
         {
            rc = GetLastError();
            err.SysMsgWrite(20628, rc, L"SetDirAttributes(%s)=%ld ",
                                       gTarget.Path(), rc);
            return rc;
         }
      if ( !RemoveDirectory(gTarget.ApiPath()) )
      {
         rc = GetLastError();
         if ( rc == ERROR_ACCESS_DENIED
          &&  gOptions.global & OPT_GlobalBackup )
         {
            if ( UnsecureForDelete(tgtEntry) )
            {
               if ( !RemoveDirectory(gTarget.ApiPath()) )
                  rc = GetLastError();
               else
                  rc = 0;
            }
            else
               rc = 0;
         }
         if ( rc )
            err.SysMsgWrite(30204, rc, L"RemoveDirectory(%s)=%ld ",
                                       gTarget.Path(), rc);
         return rc;
      }
   }
   return rc;
}


//-----------------------------------------------------------------------------
// Creates a directory (if not CompareOnly) and corresponding subobject such
// as permissions and attribute taken from the source.
//-----------------------------------------------------------------------------
static
DWORD _stdcall
   DirCreate(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      LogActions           * log          // out-log action codes
   )
{
   DWORD                     rc = 0;

   gOptions.stats.change.dirCreated++;
   log->contents = L'M';
   if ( gOptions.global & OPT_GlobalChange )
   {
      if ( !CreateDirectory(gTarget.ApiPath(), NULL) )
      {
         rc = GetLastError();
         err.SysMsgWrite(30201, rc, L"CreateDirectory(%s)=%ld ",
                                    gTarget.Path(), rc);
         return rc;
      }
   }
   return rc;
}


//-----------------------------------------------------------------------------
// May update directory permissions based upon options and values.
//-----------------------------------------------------------------------------
static
DWORD _stdcall
   DirAttrUpdate(
      DirEntry const       * srcEntry     ,// in -source entry
      DirEntry const       * tgtEntry     ,// in -target entry
      LogAction            * logAction     // out-log action code
   )
{
   DWORD                     rc = 0;

   BOOL                      attrDiff,     // attribute difference, if any
                             timeDiff;
   HANDLE                    hDir;

   if ( gOptions.global & OPT_GlobalBackupForce )
   {
      FileBackupCopy(srcEntry, tgtEntry);
      attrDiff = timeDiff = 1;
   }
   else
   {
      // set significant attribute differences
      attrDiff = (srcEntry->attrFile ^ tgtEntry->attrFile) & gOptions.attrSignif;
      if ( attrDiff )
         timeDiff =  gOptions.global & OPT_GlobalDirTime
                  && CompareFileTime(&srcEntry->ftimeLastWrite,
                                     &tgtEntry->ftimeLastWrite);
      else
         timeDiff = 0;
   }
/*
   // Always recheck directory time and attributes at termination of its recursive
   // process level because adding/deleting files/dirs will cause it to change
   // Remove this code when level-based statistics are maintained so that this
   // check becomes unnecessary.
   if ( !timeDiff && !attrDiff )
   {
      GetFileAttributes(
   }
*/
   if ( timeDiff || attrDiff )
   {
      gOptions.stats.change.dirAttrUpdated++;
      if ( timeDiff  &&  gOptions.global & OPT_GlobalChange)
      {
         if ( tgtEntry->attrFile & FILE_ATTRIBUTE_READONLY )
         {
            if ( !SetFileAttributes(gTarget.ApiPath(), FILE_ATTRIBUTE_NORMAL) )
            {
               rc = GetLastError();
               err.SysMsgWrite(30638, rc, L"SetFileAttributesN(%s)=%ld ",
                                          gTarget.Path(), rc);
            }
         }
         hDir = CreateFile(gTarget.ApiPath(),
                           GENERIC_WRITE | GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS,
                           0);
         if ( hDir == INVALID_HANDLE_VALUE )
         {
            rc = GetLastError();
            err.SysMsgWrite(20629, rc, L"CreateFileTC(%s)=%ld ",
                                       gTarget.Path(), rc);
         }
         else
         {
            if ( !SetFileTime(hDir, NULL, NULL, &srcEntry->ftimeLastWrite) )
            {
               rc = GetLastError();
               err.SysMsgWrite(20630, rc, L"SetFileTime(%s)=%ld ",
                                          gTarget.Path(), rc);
            }
            if ( attrDiff & FILE_ATTRIBUTE_COMPRESSED )  // significant compression attribute different?
            {
               CompressionSet(INVALID_HANDLE_VALUE, hDir, srcEntry->attrFile);
               attrDiff &= ~FILE_ATTRIBUTE_COMPRESSED;
            }
            CloseHandle(hDir);
         }
         *logAction = L't';
      }
      if ( attrDiff )
      {
         if ( attrDiff & ~FILE_ATTRIBUTE_COMPRESSED )
         {
            // Don't do this if only attribute difference is compression because
            // this won't change it -- the next code block will.
            if ( !SetFileAttributes(gTarget.ApiPath(),
                                    srcEntry->attrFile & ~FILE_ATTRIBUTE_DIRECTORY) )
            {
               rc = GetLastError();
               err.SysMsgWrite(20628, rc, L"SetFileAttributes(%s)=%ld ",
                                          gTarget.Path(), rc);
            }
         }
         if ( attrDiff & FILE_ATTRIBUTE_COMPRESSED )  // significant compression attribute different?
         {
            CompressionSet(INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, srcEntry->attrFile);
         }
         *logAction = L'a';
      }
   }

   if ( gOptions.global & OPT_GlobalNameCase
     && gOptions.dir.attr & OPT_PropActionUpdate )
   {
      // check if names differ by case only
      if ( wcscmp(srcEntry->cFileName, tgtEntry->cFileName)
       && !_wcsicmp(srcEntry->cFileName, tgtEntry->cFileName) )
      {
         if ( gOptions.dir.attr & OPT_PropActionUpdate )
            FileDirRename(srcEntry);
         if ( *logAction == L' ' )
         {
            gOptions.stats.change.dirAttrUpdated++;
            *logAction = L'a';
         }
      }
   }

   return rc;
}


//-----------------------------------------------------------------------------
// Processes a matched directory entry.  The target exists but the source may
// be missing.  This is called on exit from a recursive directory process
// and may remove the directory once all dirs/files have first been deleted.
//-----------------------------------------------------------------------------
DWORD _stdcall
   MatchedDirTgtExists(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   )
{
   DWORD                     rc = 0;
   LogActions                log = {L' ', L' ', L' '};

   if ( !srcEntry )
   {
      if ( gOptions.dir.contents & OPT_PropActionRemove )
      {
         rc = DirRemove(tgtEntry, &log);
      }
      else
         if ( gOptions.dir.perms & OPT_PropActionRemove )
            rc = PermDelete(1, &log.perms);
   }
   else
   {
      if ( gOptions.dir.attr & OPT_PropActionUpdate )
         rc = DirAttrUpdate(srcEntry, tgtEntry, &log.attr);
      if ( gOptions.dir.perms )
      {
         rc = PermReplicate(1, &log.perms);
      }
   }
   if ( gOptions.global & OPT_GlobalDispDetail )
      if ( gOptions.global & OPT_GlobalDispMatches || wcsncmp((WCHAR*)&log, L"   ", 3))
         err.MsgWrite(0, L" %-3.3s %s", &log, gTarget.Path());
   return rc;
}


//-----------------------------------------------------------------------------
// Processes a matched directory entry.  The target is missing but not the
// source (both can't be missing).  This is called when recursing down to
// potentially create files and directories.
//-----------------------------------------------------------------------------
DWORD _stdcall
   MatchedDirNoTgt(
      DirEntry const       * srcEntry     // in -current source entry processed
   )
{
   DWORD                     rc = 0;
   LogActions                log = {L' ', L' ', L' '};

   if ( gOptions.dir.contents & OPT_PropActionMake )
   {
      rc = DirCreate(srcEntry, &log);
      if ( !rc )
      {
         if ( gOptions.dir.perms & OPT_PropActionMake )
         {
            rc = PermCreate(1, &log.perms);
            if ( srcEntry->attrFile & gOptions.attrSignif & FILE_ATTRIBUTE_COMPRESSED )
               if ( gOptions.global & OPT_GlobalChange )
                  CompressionSet(INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, srcEntry->attrFile );
         }
      }
   }
   if ( gOptions.global & OPT_GlobalDispDetail )
      if ( gOptions.global & OPT_GlobalDispMatches || wcsncmp((WCHAR*)&log, L"   ", 3) )
         err.MsgWrite(0, L" %-3.3s %s", &log, gTarget.Path());

   return rc;
}


//-----------------------------------------------------------------------------
// Crazy situation where names match but target is dir and source isn't
//-----------------------------------------------------------------------------
void _stdcall
   MismatchSourceNotDir(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   )
{
   err.MsgWrite(30221, L"Source not directory but target is (%s)",
                       gTarget.Path());
}


//-----------------------------------------------------------------------------
// Crazy situation where names match but source is dir and target isn't
//-----------------------------------------------------------------------------
void _stdcall
   MismatchTargetNotDir(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   )
{
   err.MsgWrite(30222, L"Target not directory but source is (%s)",
                        gTarget.Path());
}


//-----------------------------------------------------------------------------
// This function is for special processing when exiting a recursion level when
// the target does not exist.  Normally no-target processing is done on the
// way down and not on the way up.  However, there are some things that can
// only be done on the way up such as setting the date/time/attr for a
// directory because if it is set on the way down, the immediately following
// file creates, etc. will reset it.
//-----------------------------------------------------------------------------
DWORD _stdcall
   MatchedDirNoTgtExit(
      DirEntry const       * srcEntry     // in -current source entry processed
   )
{
   HANDLE                    hDir;
   DWORD                     rc = 0;

   if ( gOptions.dir.contents & OPT_PropActionMake
     && gOptions.dir.attr     & OPT_PropActionUpdate
     && gOptions.global & OPT_GlobalChange
     && gOptions.global & OPT_GlobalDirTime )
   {
      if ( gOptions.global & OPT_GlobalBackup )
      {
         FileBackupCopy(srcEntry, NULL);
      }
      else
      {
         hDir = CreateFile(gTarget.ApiPath(), GENERIC_WRITE | GENERIC_WRITE,
                           FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, 0);
         if ( hDir == INVALID_HANDLE_VALUE )
         {
            rc = GetLastError();
            err.SysMsgWrite(30630, rc, L"CreateFileDirTC(%s)=%ld ", gTarget.Path(), rc);
         }
         else
         {
            if ( !SetFileTime(hDir, NULL, NULL, &srcEntry->ftimeLastWrite) )
            {
               rc = GetLastError();
               err.SysMsgWrite(20630, rc, L"SetFileTime(%s)=%ld ", gTarget.Path(), rc);
            }
            CloseHandle(hDir);
         }

         if ( !SetFileAttributes(gTarget.ApiPath(), srcEntry->attrFile
                                                     & ~FILE_ATTRIBUTE_DIRECTORY) )
         {
            rc = GetLastError();
            err.SysMsgWrite(20631, rc, L"SetFileAttributes(%s)=%ld ",
                                       gTarget.Path(), rc);
         }
      }
   }

   return rc;
}


//-----------------------------------------------------------------------------
// Creates a file (if not CompareOnly) and corresponding subobjects such
// as permissions and attribute taken from the source.
//-----------------------------------------------------------------------------
static
DWORD _stdcall
   FileCreate(
      DirEntry const       * srcEntry     // in -current source entry processed
   )
{
   DWORD                     rc = 0;

   gOptions.stats.change.fileCreated.count++;
   gOptions.stats.change.fileCreated.bytes += srcEntry->cbFile;
   if ( gOptions.global & OPT_GlobalChange )
   {
      if ( gOptions.global & OPT_GlobalBackup )
         rc = FileBackupCopy(srcEntry, NULL);
      else
         rc = FileCopy(srcEntry, NULL);
      if ( rc )
         err.MsgWrite(203, L"File copy bypassed %s", gTarget.Path());
   }
   return rc;
}


//-----------------------------------------------------------------------------
// Deletes a file (if not CompareOnly).
//-----------------------------------------------------------------------------
static
DWORD _stdcall
   FileRemove(
      DirEntry const       * tgtEntry     // in -current source entry processed
   )
{
   DWORD                     rc = 0;

   gOptions.stats.change.fileRemoved.count++;
   gOptions.stats.change.fileRemoved.bytes += tgtEntry->cbFile;
   if ( gOptions.global & OPT_GlobalChange )
   {
      // if file R/O, change to R/W
      if ( tgtEntry->attrFile & FILE_ATTRIBUTE_READONLY )
      {
         if ( !SetFileAttributes(gTarget.ApiPath(), FILE_ATTRIBUTE_NORMAL) )
         {
            rc = GetLastError();
            err.SysMsgWrite(30208, rc, L"SetFileAttributes(%s)=%ld ", gTarget.Path(), rc);
            return rc;
         }
      }
      if ( !DeleteFile(gTarget.ApiPath()) )
      {
         rc = GetLastError();
         if ( rc == ERROR_ACCESS_DENIED  &&  gOptions.global & OPT_GlobalBackup )
         {
            if ( UnsecureForDelete(tgtEntry) )
            {
               if ( !DeleteFile(gTarget.ApiPath()) )
                  rc = GetLastError();
               else
                  rc = 0;
            }
         }
         if ( rc )
            err.SysMsgWrite(30204, rc, L"DeleteFile(%s)=%ld ",
                                       gTarget.Path(), rc);
         return rc;
      }
   }
   return rc;
}


//-----------------------------------------------------------------------------
// Logically compares the source and target files per the global options
// and returns 1 if different.
//-----------------------------------------------------------------------------
static
DWORD _stdcall
   MatchedFileCompare(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   )
{
   __int64                   cmp;         // compare result

   cmp = *(__int64 *)&srcEntry->ftimeLastWrite - *(__int64 *)&tgtEntry->ftimeLastWrite;
   if ( cmp )
   {
      // different last-write dates
      if ( cmp < 0  &&  gOptions.global & OPT_GlobalNewer )
         return 0;      // ignore differences because target is newer

      // The following lines do a "fuzzy" compare so that file systems that
      // store timestamps with different precision levels can be compared for
      // equivalence.  20,000,000 represents the number of 100ns intervals in
      // a FAT/HPFS twosec file timestamp.
      if ( cmp < 0 )
         cmp = -cmp;
      if ( cmp >= 20000000 )
         return 1;
   }

   if ( srcEntry->cbFile != tgtEntry->cbFile )
      return 1;   // file lengths not equal

   if ( !(gOptions.global & OPT_GlobalOptimize) )
      return FileContentsCompare(); // no optimize, so compare contents

   return 0;         // they're the same
}


//-----------------------------------------------------------------------------
// Processes a matched file entry where the file name matches on both source
// and target.  The files are not necessarily the same and this function
// determines what if any replication action need be taken.
//-----------------------------------------------------------------------------
static
DWORD _stdcall
   FileUpdate(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry    ,// in -current target entry processed
      LogActions           * log          // out-attribute action
   )
{
   DWORD                     rc,
                             attrDiff;

   if ( gOptions.global & OPT_GlobalBackupForce )
      rc = 1;
   else
      rc = MatchedFileCompare(srcEntry, tgtEntry);

   if ( rc )
   {
      log->contents = L'U';
      gOptions.stats.change.fileUpdated.count++;
      gOptions.stats.change.fileUpdated.bytes += srcEntry->cbFile;
      if ( gOptions.global & OPT_GlobalChange )
         if ( gOptions.global & OPT_GlobalBackup )
            rc = FileBackupCopy(srcEntry, tgtEntry);
         else
            rc = FileCopy(srcEntry, tgtEntry);
      else
         rc = 0;
   }
   else
   {
      gOptions.stats.match.fileMatched.count++;
      gOptions.stats.match.fileMatched.bytes += srcEntry->cbFile;
      if ( gOptions.file.attr & OPT_PropActionUpdate )
      {
         // checks to see if signficant attributes are different
         attrDiff = (srcEntry->attrFile ^ tgtEntry->attrFile) & gOptions.attrSignif;
         if ( attrDiff )
         {
            gOptions.stats.change.fileAttrUpdated++;
            log->attr = 'a';
            if ( gOptions.global & OPT_GlobalChange )
            {
               if ( attrDiff & ~FILE_ATTRIBUTE_COMPRESSED )
               {
                  if ( !SetFileAttributes(gTarget.ApiPath(), srcEntry->attrFile) )
                  {
                     rc = GetLastError();
                     err.SysMsgWrite(20101, rc, L"SetFileAttributes(%s)=%ld ",
                                                gTarget.Path(), rc);
                  }
               }
               if ( attrDiff & FILE_ATTRIBUTE_COMPRESSED )  // significant compression attribute different?
               {
                  CompressionSet(INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, srcEntry->attrFile);
               }
            }
         }
      }
   }

   if ( gOptions.global & OPT_GlobalNameCase
     && gOptions.file.attr & OPT_PropActionUpdate )
   {
      // check if names differ by case only
      if ( wcscmp(srcEntry->cFileName, tgtEntry->cFileName)
       && !_wcsicmp(srcEntry->cFileName, tgtEntry->cFileName) )
      {
         if ( gOptions.dir.attr & OPT_PropActionUpdate )
            FileDirRename(srcEntry);
         if ( log->attr == L' ' )
         {
            gOptions.stats.change.fileAttrUpdated++;
            log->attr = L'a';
         }
      }
   }
   return rc;
}


//-----------------------------------------------------------------------------
// Processes a matched file entry.  The source or the target (not both)
// may be missing (DirEntry == NULL).
//-----------------------------------------------------------------------------
DWORD _stdcall
   MatchedFileProcess(
      DirEntry const       * srcEntry    ,// in -current source entry processed
      DirEntry const       * tgtEntry     // in -current target entry processed
   )
{
   DWORD                     rc;
   LogActions                log = {L' ', L' ', L' '};

   if ( !tgtEntry )
   {
      if ( gOptions.file.contents & OPT_PropActionMake )
      {
         log.contents = L'C';
         rc = FileCreate(srcEntry);
         if ( !rc )
            if ( gOptions.file.perms & OPT_PropActionMake )
               rc = PermCreate(0, &log.perms);
      }
   }
   else if ( !srcEntry )
   {
      if ( gOptions.file.contents & OPT_PropActionRemove
       && ( !( tgtEntry->attrFile & FILE_ATTRIBUTE_READONLY )
          || gOptions.global & OPT_GlobalReadOnly   )
       && ( !( tgtEntry->attrFile & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM) )
          || gOptions.global & OPT_GlobalHidden     ) )
      {
         log.contents = L'D';
         rc = FileRemove(tgtEntry);
      }
      else
         if ( gOptions.file.perms & OPT_PropActionRemove )
            rc = PermDelete(0, &log.perms);
   }
   else     // source and target files both exist
   {
      if ( gOptions.file.contents & OPT_PropActionUpdate
       && ( !( tgtEntry->attrFile & FILE_ATTRIBUTE_READONLY )
          || gOptions.global & OPT_GlobalReadOnly   )
       && ( !( tgtEntry->attrFile & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM) )
          || gOptions.global & OPT_GlobalHidden     ) )
      {
         rc = FileUpdate(srcEntry, tgtEntry, &log);
      }
      if ( gOptions.file.perms & OPT_PropActionAll )
         rc = PermReplicate(0, &log.perms);
   }

   if ( gOptions.global & OPT_GlobalDispDetail )
      if ( gOptions.global & OPT_GlobalDispMatches || wcsncmp((WCHAR*)&log, L"   ", 3) )
         err.MsgWrite(0, L" %-3.3s %s", &log, gTarget.Path());
   return rc;
}
