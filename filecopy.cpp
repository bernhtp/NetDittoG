/*
===============================================================================

  Program    - FileCopy
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/17/93
  Description- Functions to replicate file contents and set some attributes.

  Updates -

===============================================================================
*/

#include "netditto.hpp"
#include "util32.hpp"

#define INT64LOW(x)  ( *((DWORD *)&x)     )
#define INT64HIGH(x) ( *((LONG  *)&x + 1) )
#define LARGE_FILE_SIZE (256*1024)

// Converts binary attribute mask to string
WCHAR * _stdcall
   AttrStr(
      DWORD                  attr        ,// in -file/dir attribute
      WCHAR                * retStr       // out-return attribute string
   )
{
   WCHAR const             * i;
   WCHAR                   * o;

   for ( i = L"RHS3DA6NT9aCcdef", o = retStr;  *i;  i++, attr >>= 1 )
      if ( attr & 1 )        // if current attr bit on
         *o++ = *i;          //    set output string to corresponding char
   *o = L'\0';               // null terminate string
   return retStr;
}

// copies the contents of the source file to the target given open file handles
static DWORD _stdcall
   FileCopyContents(
      HANDLE                 hSrc        ,// in -input file handle
      HANDLE                 hTgt         // in -output file handle
   )
{
   DWORD                     rc = 0,
                             nSrc,
                             nTgt;                             ;
   int                     * i;
   BOOL                      b;

   while ( b = ReadFile(hSrc, gOptions.copyBuffer, gOptions.sizeBuffer, &nSrc, NULL) )
   {
      if ( nSrc == 0 )                    // if end-of-file, break while loop
         break;
      if ( gOptions.global & OPT_GlobalCopyXOR )   // complement contents option
         for ( i = (int *)gOptions.copyBuffer;  (BYTE *)i < gOptions.copyBuffer + nSrc;  i++ )
            *i = ~*i;                              // one's complement buffer

      if ( !WriteFile(hTgt, gOptions.copyBuffer, nSrc, &nTgt, NULL) )
      {
         rc = GetLastError();
         err.SysMsgWrite(30103, rc, L"WriteFile(%ld,%ld)=%ld, ", nSrc, nTgt, rc);
         return rc;
      }
      gOptions.bWritten += nTgt;
      if ( nSrc < gOptions.sizeBuffer )   // check EOF again to avoid unnecessary read
         break;
   }
   if ( !b )
      if ( rc = GetLastError() )
         err.SysMsgWrite(40104, rc, L"ReadFile(%s)=%ld ", gOptions.source.path, rc);

   return rc;
}

// Copies file contents
DWORD _stdcall
   FileCopy(
      DirEntry const       * srcEntry    ,// in -source directory entry
      DirEntry const       * tgtEntry     // in -target directory entry
   )
{
   HANDLE                    hSrc,
                             hTgt;
   DWORD                     rc = 0,
                             overlapped;
   WCHAR                     temp[2][10];
   BOOL                      compressChange;

   // if file R/O and write R/O option, change to R/W
   if ( tgtEntry )
   {
      if ( tgtEntry->attrFile & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN) )
      {
         if ( (tgtEntry->attrFile & FILE_ATTRIBUTE_READONLY  &&  gOptions.global & OPT_GlobalReadOnly)
           || (tgtEntry->attrFile & FILE_ATTRIBUTE_HIDDEN    &&  gOptions.global & OPT_GlobalHidden  ) )
         {
            if ( !SetFileAttributes(gOptions.target.apipath, FILE_ATTRIBUTE_NORMAL) )
            {
               rc = GetLastError();
               err.SysMsgWrite(20103, rc, L"SetFileAttributes(%s,N)=%ld, ",
                                          gOptions.target.path, rc);
               return rc;
            }
         }
         else
         {
            // The open will fail because we have not given permission to change attributes, so let's quit it
            return 0;
         }
      }
   }

   // if the file is big and the target not on a network, we'll do unbuffered
   // overlapped I/O via I/O completion ports, so set the open attribute accordingly.
   if ( srcEntry->cbFile >= LARGE_FILE_SIZE )
   {
      overlapped = FILE_FLAG_OVERLAPPED;
      if ( gOptions.target.bUNC )
         overlapped |= FILE_FLAG_NO_BUFFERING;
   }
   else
      overlapped = 0;
   hSrc = CreateFile(gOptions.source.apipath,
                     GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING | overlapped,
                     0);
   if ( hSrc == INVALID_HANDLE_VALUE )
   {
      rc = GetLastError();
      if ( rc == ERROR_SHARING_VIOLATION )
         err.MsgWrite(20101, L"Source file in use %s", gOptions.source.path );
      else
         err.SysMsgWrite(40101, rc, L"OpenR(%s)=%ld, ", gOptions.source.apipath, rc);
      return rc;
   }

   hTgt = CreateFile(gOptions.target.apipath,
                     GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ,
                     NULL,
                     CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | overlapped,
                     0);
   if ( hTgt == INVALID_HANDLE_VALUE )
   {
      rc = GetLastError();
      if ( rc == ERROR_SHARING_VIOLATION )
         err.MsgWrite(20101, L"Target file in use %s", gOptions.target.path );
      else
      {
         err.SysMsgWrite(40102, rc, L"OpenW(%s)=%ld (attr S/T=%s/%s,%x/%x), ",
                                    gOptions.target.path,
                                    rc,
                                    srcEntry ? AttrStr(srcEntry->attrFile, temp[0]) : L"-",
                                    tgtEntry ? AttrStr(tgtEntry->attrFile, temp[1]) : L"-",
                                    srcEntry ? srcEntry->attrFile : 0,
                                    tgtEntry ? tgtEntry->attrFile : 0);
      }
      CloseHandle(hSrc);
      return rc;
   }

   // if the source and target compression attribute is different and significant
   if ( tgtEntry )
      if ( (srcEntry->attrFile ^ tgtEntry->attrFile) & FILE_ATTRIBUTE_COMPRESSED & gOptions.attrSignif )
         compressChange = TRUE;
      else
         compressChange = FALSE;
   else
      if ( srcEntry->attrFile & FILE_ATTRIBUTE_COMPRESSED & gOptions.attrSignif )
         compressChange = TRUE;
      else
         compressChange = FALSE;
   if ( compressChange )
   {
      CompressionSet(hSrc, hTgt, srcEntry->attrFile);
   }

   if ( overlapped )
      rc = FileCopyContentsOverlapped(hSrc, &hTgt);
   else
      rc = FileCopyContents(hSrc, hTgt);
   if ( rc )
      err.SysMsgWrite(104, rc, L"FileCopyContents%s(%s), ",
                               (overlapped ? L"Overlapped" : L""),
                               gOptions.target.path);
   CloseHandle(hSrc);

   if ( !SetFileTime(hTgt, NULL, NULL, &srcEntry->ftimeLastWrite) )
   {
      rc = GetLastError();
      err.SysMsgWrite(40110, rc, L"SetFileTime(%s,%02lX)=%ld ",
                             gOptions.target.path, srcEntry->attrFile, rc);
      rc = 0;
   }

   CloseHandle(hTgt);

   if ( gOptions.file.attr & OPT_PropActionUpdate )
      if ( !SetFileAttributes(gOptions.target.apipath, srcEntry->attrFile) )
      {
         rc = GetLastError();
         err.SysMsgWrite(20109, rc, L"SetFileAttributes(%s)=%d ",
                                     gOptions.target.path, rc);
         return rc;
      }

   return rc;
}


// Compares file contents on a byte by byte basis
DWORD _stdcall
   FileContentsCompare(
   )
{
   HANDLE                    hSrc,
                             hTgt;
   DWORD                     rcSrc = 0,
                             rcTgt = 0,
                             b2 = gOptions.sizeBuffer >> 1, // split buffer
                             cmp = 0,
                             nSrc,
                             nTgt;
   BYTE                    * s,        // source and target for 1's comp compare
                           * t;
   BOOL                      bSrc, bTgt;


   err.MsgWrite(0, L"Fc %s", gOptions.target.path);
   hSrc = CreateFile(gOptions.source.apipath,
                     GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL,
                     OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING,
                     0);
   if ( hSrc == INVALID_HANDLE_VALUE)
   {
      rcSrc = GetLastError();
      if ( rcSrc == ERROR_SHARING_VIOLATION )
         err.MsgWrite(20101, L"Source file in use %s", gOptions.source.path);
      else
         err.SysMsgWrite(40101, rcSrc, L"OpenRs(%s)=%d ", gOptions.source.path, rcSrc);
      return rcSrc;
   }

   hTgt = CreateFile(gOptions.target.apipath,
                     GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL,
                     OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING,
                     0);
   if ( hTgt == INVALID_HANDLE_VALUE)
   {
      rcTgt = GetLastError();
      if ( rcTgt == ERROR_SHARING_VIOLATION )
         err.MsgWrite(20101, L"Target file in use %s", gOptions.target.path);
      else
         err.SysMsgWrite(40101, rcTgt, L"OpenRt(%s)=%d, ", gOptions.target.path, rcTgt);
      return rcTgt;
   }

   while ( (bSrc = ReadFile(hSrc, gOptions.copyBuffer   , b2, &nSrc, NULL))
        && (bTgt = ReadFile(hTgt, gOptions.copyBuffer+b2, b2, &nTgt, NULL)) )
   {
      if ( nSrc != nTgt )   // this should never occur but check just in case
      {
         cmp = 1;
         break;
      }
      if ( gOptions.global & OPT_GlobalCopyXOR )   // complement contents option
      {
         for ( s = gOptions.copyBuffer, t = gOptions.copyBuffer + b2;
               s < gOptions.copyBuffer + nSrc;
               s++, t++ )
         {
            if ( *s != (byte)~*t )
            {
               cmp = 1;
               break;
            }
         }
         if ( cmp )
            break;
      }
      else if ( cmp = memcmp(gOptions.copyBuffer, gOptions.copyBuffer + b2, nSrc) )
         break;

      if ( nSrc < b2 )                 // don't issue read just to get EOF
         break;
   }

   if ( !bSrc )
      rcSrc = GetLastError();
   else
      if ( !bTgt )
         rcTgt = GetLastError();

   CloseHandle(hSrc);
   CloseHandle(hTgt);

   if ( rcSrc = max(rcSrc, rcTgt) )
      err.SysMsgWrite(40104, rcSrc, L"ReadFile(%s)=%d",
          (rcTgt ? gOptions.target.path : gOptions.source.path), rcSrc );

   return max(cmp, rcSrc);
}


// copies the contents of the source file to the target given open file handles
static DWORD _stdcall
   FileBackupContents(
      HANDLE                 hSrc        ,// in -input file handle
      HANDLE                 hTgt         // in -output file handle
   )
{
   DWORD                     rc = 0,
                             nSrc,
                             nTgt;
// int                     * i;
   BOOL                      b;
   void                    * r = NULL,    // required by the BackupRead/Write APIs
                           * w = NULL;

   while ( b = BackupRead(hSrc,
                          gOptions.copyBuffer,
                          gOptions.sizeBuffer,
                          &nSrc,
                          FALSE,
                          TRUE,
                          &r) )
   {
      if ( nSrc == 0 )                    // if end-of-file, break while loop
         break;

/*
      // need to fix so only XOR data stream
      if ( gOptions.global & OPT_GlobalCopyXOR )   // complement contents option
         for ( i = (int *)gOptions.copyBuffer;  (BYTE *)i < gOptions.copyBuffer + nSrc;  i++ )
            *i = ~*i;                              // one's complement buffer
*/
      if ( !BackupWrite(hTgt, gOptions.copyBuffer, nSrc, &nTgt, FALSE, TRUE, &w) )
      {
         rc = GetLastError();
         // The following code attempts to recover from an error where the owner SID
         // in the scurity descriptor that is being written is not valid on the target
         // system.  This happens when replicating from a workstation to a server where
         // local accounts, not known by the server, are owners, e.g., the local admin.
         // It does this by skipping over the security stream and continuing the restore.
         // ~~Eventually we want to enhace this to modify the owner SID to something like
         // the Administrators well-known group to preserve the rest of the SD.
         if ( rc == ERROR_INVALID_OWNER )
         {
            WIN32_STREAM_ID * s = (WIN32_STREAM_ID *)gOptions.copyBuffer;
            int               n;

            if ( s->dwStreamId == BACKUP_SECURITY_DATA )
            {
               n = sizeof *s + s->Size.LowPart + s->dwStreamNameSize - 4;
               if ( !BackupWrite(hTgt, gOptions.copyBuffer + n, nSrc-n, &nTgt, FALSE, TRUE, &w) )
                  rc = GetLastError();
               else
                  rc = 0;
            }
         }
         if ( rc )
         {
            err.SysMsgWrite(30103, rc, L"BackupWrite(%d)=%ld, ", nSrc, rc);
            return rc;
         }
      }
      gOptions.bWritten += nTgt;
   }
   if ( !b )
      if ( rc = GetLastError() )
         err.SysMsgWrite(40104, rc, L"BackupRead(%s)=%ld ", gOptions.source.path, rc);

   return rc;
}


// Copies file contents via the backup APIs and thus handling security, streams, etc.
DWORD _stdcall
   FileBackupCopy(
      DirEntry const       * srcEntry    ,// in -source directory entry
      DirEntry const       * tgtEntry     // in -target directory entry
   )
{
   HANDLE                    hSrc,
                             hTgt;
   DWORD                     rc = 0,
                             attr;
   WCHAR                     temp[2][10];

   // if file R/O and write R/O option, change to R/W
   if ( tgtEntry  &&  tgtEntry->attrFile & FILE_ATTRIBUTE_READONLY )
      if ( gOptions.global & OPT_GlobalReadOnly )
         if ( !SetFileAttributes(gOptions.target.apipath, FILE_ATTRIBUTE_NORMAL) )
         {
            rc = GetLastError();
            err.SysMsgWrite(20103, rc, L"SetFileAttributes(%s)=%ld, ",
                                        gOptions.target.path, rc);
            return rc;
         }
   attr = (srcEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY
            ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL)
        | FILE_FLAG_SEQUENTIAL_SCAN
        | FILE_FLAG_BACKUP_SEMANTICS;

   hSrc = CreateFile(gOptions.source.apipath,
                     GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL,
                     OPEN_EXISTING,
                     attr,
                     0);
   if ( hSrc == INVALID_HANDLE_VALUE )
   {
      rc = GetLastError();
      if ( rc == ERROR_SHARING_VIOLATION )
         err.MsgWrite(20161, L"Source file in use - bypassed - %s", gOptions.source.path);
      else
         err.SysMsgWrite(40161, rc, L"OpenR(%s)=%ld, ", gOptions.source.apipath, rc);
      return rc;
   }

   hTgt = CreateFile(gOptions.target.apipath,
                     GENERIC_WRITE | GENERIC_READ | WRITE_OWNER | WRITE_DAC,
                     FILE_SHARE_READ,
                     NULL,
                     OPEN_ALWAYS | (tgtEntry && !(tgtEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY) ? TRUNCATE_EXISTING : 0),
                     attr,
                     0);
   if ( hTgt == INVALID_HANDLE_VALUE )
   {
      rc = GetLastError();
      switch ( rc )
      {
         case ERROR_SHARING_VIOLATION:
            err.MsgWrite(20161, L"Target file in use - bypassed - %s", gOptions.target.path);
            break;
         case ERROR_ACCESS_DENIED:
         default:
            err.SysMsgWrite(30162, rc, L"OpenWb(%s,%lx)=%ld (attr=%s->%s,%x/%x), ",
                                 gOptions.target.path,
                                 attr,
                                 rc,
                                 srcEntry ? AttrStr(srcEntry->attrFile, temp[0]) : L"-",
                                 tgtEntry ? AttrStr(attr, temp[1]) : L"-",
                                 srcEntry ? srcEntry->attrFile : 0,
                                 tgtEntry ? attr : 0);
      }
      CloseHandle(hSrc);
      return rc;
   }

   // if the source and target compression attribute is different and significant
   if ( tgtEntry )
      attr = FILE_ATTRIBUTE_COMPRESSED & gOptions.attrSignif & (srcEntry->attrFile ^ tgtEntry->attrFile);
   else
      attr = FILE_ATTRIBUTE_COMPRESSED & gOptions.attrSignif & srcEntry->attrFile;
   if ( attr )
   {
      CompressionSet(hSrc, hTgt, srcEntry->attrFile);
   }

   if ( rc = FileBackupContents(hSrc, hTgt) )
      err.SysMsgWrite(104, rc, L"FileCopyContents(%s), ", gOptions.target.path);

   if ( !SetFileTime(hTgt, NULL, NULL, &srcEntry->ftimeLastWrite) )
   {
      rc = GetLastError();
      err.SysMsgWrite(40110, rc, L"SetFileTime(%s,%02lX)=%ld ", gOptions.target.path,
                          srcEntry->attrFile, rc);
      rc = 0;
   }

   CloseHandle(hSrc);
   CloseHandle(hTgt);

   if ( gOptions.file.attr & OPT_PropActionUpdate )
   {
      if ( tgtEntry )
         attr = ~FILE_ATTRIBUTE_COMPRESSED
              & gOptions.attrSignif
              & (srcEntry->attrFile ^ tgtEntry->attrFile);
      else
         attr = ~FILE_ATTRIBUTE_COMPRESSED
              & gOptions.attrSignif
              & srcEntry->attrFile;
      if ( attr )
      {
         if ( !SetFileAttributes(gOptions.target.apipath, srcEntry->attrFile) )
         {
            rc = GetLastError();
            err.SysMsgWrite(20109, rc, L"SetFileAttributes(%s)=%d ", gOptions.target.path, rc);
            return rc;
         }
      }
   }
   return rc;
}
