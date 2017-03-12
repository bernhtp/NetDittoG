/*
===============================================================================

  Module     - GetInfo
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/06/95
  Description- Gets infomation about the target and source volumes.

  Updates -

===============================================================================
*/

#include "netditto.hpp"
#include "util32.hpp"

// returns the length of the \\server\share component of a UNC name
static
int                                        // ret-length or -1 if error
   ServerShareLength(
	  WCHAR const          * uncPath       // in -UNC path after \\?\ prefix
   )
{
   WCHAR const             * c;
   int                       nSlash = 0;

   // iterate through the string stopping at the fourth backslash or '\0'
   for ( c = uncPath;  *c;  c++ )
   {
      if ( *c == L'\\' )
      {
         nSlash++;
         if ( nSlash > 3 )
            break;
      }
   }

   // If UNC but not at least sharename separater blackslash, error
   if ( nSlash < 3 )
      return -1;

   return c - uncPath;
}


// returns the length of the \\server\share component of a UNC name
static
int                                        // ret-length copied or -1 if error
   ServerShareCopy(
      WCHAR                * tgtPath      ,// out-target \\server\share path
      WCHAR const          * uncPath       // in -UNC path after \\?\ prefix
   )
{
   WCHAR const             * c;
   int                       nSlash = 0;
   WCHAR                   * t = tgtPath;

   // iterate through the string stopping at the fourth backslash or '\0'
   for ( c = uncPath;  *c;  c++ )
   {
      if ( *c == L'\\' )
      {
         nSlash++;
         if ( nSlash > 3 )
            break;
      }
      *t++ = *c;
   }

   *t = L'\0';

   // If UNC but not at least sharename separater blackslash, error
   if ( nSlash < 3 )
      return -1;

   return c - uncPath;
}

DWORD
   VolumeGetInfo(
      WCHAR const          * path        ,// in -path string
      DirOptions           * dirOptions   // out-initialized dir options
   )
{
   union
   {
      REMOTE_NAME_INFO       i;
      WCHAR                  x[_MAX_PATH+1];
   }                         info;
   DWORD                     rc,
                             sizeBuffer = sizeof info,
                             maxCompLen,
                             sectorsPerCluster,
                             bytesPerSector,
                             nFreeClusters,
                             nTotalClusters;
   WCHAR                     volRoot[_MAX_PATH],
                             fullPath[_MAX_PATH];
   int                       len;
   UINT                      driveType;

   wcscpy(dirOptions->apipath, L"\\\\?\\");     // prefix with \\?\ for long paths
   _wfullpath(fullPath, path, DIM(fullPath));
   if ( wcsncmp(fullPath, L"\\\\", 2) )
   {
       // not a UNC
       _swprintf(volRoot, L"%-3.3s", fullPath);
      driveType = GetDriveType(volRoot);
      switch ( driveType )
      {
         case DRIVE_REMOTE:
             dirOptions->bUNC = true;
             rc = WNetGetUniversalName(fullPath,
                                      REMOTE_NAME_INFO_LEVEL,
                                      (PVOID)&info,
                                      &sizeBuffer);
            switch ( rc )
            {
               case 0:
                  wcscpy(volRoot, info.i.lpConnectionName);
                  // the \\server\share form is copied as UNC\server\share
                  wcscpy(dirOptions->path, L"UNC");
                  wcscpy(dirOptions->path+3, info.i.lpUniversalName+1);
                  break;
               case ERROR_NOT_CONNECTED:
                  wcscpy(dirOptions->path, fullPath);
                  break;
               default:
                  err.SysMsgWrite(34021, rc, L"WNetGetUniversalName(%s)=%ld ",
                                             fullPath, rc);
                  dirOptions->path[0] = L'\0';
                  return rc;
            }
            break;
         case 0:                          // unknown drive
         case 1:                          // invalid root directory
            dirOptions->path[0] = L'\0';
            return ERROR_INVALID_DRIVE;
         default:
            wcscpy(dirOptions->path, fullPath);
      }
   }
   else
   {
      dirOptions->bUNC = true;
      driveType = DRIVE_REMOTE;
      wcscpy(dirOptions->path, L"UNC");         // Unicode form is \\?\UNC\server\share\...
      wcscpy(dirOptions->path+3, fullPath+1);   
      len = ServerShareCopy(volRoot, fullPath);
      if ( len < 0 )
         return 1;
   }

   // get \\server\share part of UNC
   len = wcslen(volRoot);
   if ( volRoot[len - 1] != L'\\' )
      wcscpy(volRoot+len, L"\\");

   if ( !GetVolumeInformation(volRoot,
                              dirOptions->volName,
                              DIM(dirOptions->volName),
                              &dirOptions->volser,
                              &maxCompLen,
                              &dirOptions->fsFlags,
                              dirOptions->fsName,
                              DIM(dirOptions->fsName)) )
   {
      rc = GetLastError();
      err.SysMsgWrite(34022, rc, L"GetVolumeInformation(%s)=%ld ", volRoot, rc);
      return rc;
   }

   if ( !GetDiskFreeSpace(volRoot,
                          &sectorsPerCluster,
                          &bytesPerSector,
                          &nFreeClusters,
                          &nTotalClusters) )
   {
      rc = GetLastError();
      err.SysMsgWrite(34023, rc, L"GetDiskFreeSpace(%s)=%ld ", volRoot, rc);
      return rc;
   }

   dirOptions->cbCluster  = bytesPerSector * sectorsPerCluster;
   dirOptions->cbVolTotal = (__int64)nTotalClusters * dirOptions->cbCluster;
   dirOptions->cbVolFree  = (__int64)nFreeClusters  * dirOptions->cbCluster;
   return 0;
}


// Compares the volume components of two normalized paths.  A normalized path
// is a drive letter followed by the path for local drives and a full UNC path
// for network resources.  Redirected drive letters are first converted
// to a UNC path prior to invoking this function (see WNetGetUniversalName).
// The volume component is the drive letter for local paths and the
// \\server\sharename path of a UNC path.
// As with other compare functions, 0 is returned if they are equal.
// Note: the path must not include the \\?\ long path prefix
int                                        // ret-0=volume components equal
   VolCompare(
	  WCHAR const          * path1        ,// in -path 1
	  WCHAR const          * path2         // in -path 2
   )
{
   WCHAR const             * c;
   int                       len,
                             nSlash = 0;

   // If second char is a colon, path is a drive letter and we only need to
   // compare the first two chars.
   if ( path1[1] == L':' )
      return _wcsnicmp(path1, path2, 2);

   // If it's not a drive prefixed path or a UNC ('\' not second char), it's
   // not in normal form and we just compare them exactly.
   if ( path1[1] != L'\\' )
      return _wcsicmp(path1, path2);

   // Get length of UNC volume component
   for ( c = path1;  *c  &&  nSlash < 4;  c++ )
   {
      if ( *c == L'\\' )
         nSlash++;
   }

   // If UNC but not at least sharename separater blackslash, error
   if ( nSlash < 3 )
      return 1;

   len = c - path1;

   // If sharename/path separator is not null or backslash, they're not the same
   if ( path2[len] != L'\0'  &&  path2[len] != L'\\' )
      return 1;

   return _wcsnicmp(path1, path2, len-1);
}
