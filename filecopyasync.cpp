/*
===============================================================================

  Program    - FileCopyAsynch
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/17/95
  Description- Uses overlapped I/O with completion ports to parallelize and
               optimize I/O for larger files.  Note that this function is
               only called with larger files while small ones are still
               handled via buffered and non-overlapped I/O calls.
  Updates -

===============================================================================
*/

#include <malloc.h>

#include "netditto.hpp"
#include "util32.hpp"

static DWORD const ReadKey = 0;
static DWORD const WriteKey = 1;
static DWORD const pageSize = 4096;

#define COPY_BUFFER_SIZE (32 * 1024)  // normal I/O block size
#define Buffer(n) (gOptions.copyBuffer + n * COPY_BUFFER_SIZE)

struct IOControl  // Extended overlapped struct to contain buffer index
{
   OVERLAPPED                ov;
   int                       nBuff;
};

// Copies the contents of the source file to the target given open file handles
// The target handle is by-reference because it may be closed and reopened if
// it is open unbuffered and we need to reopen it buffered to write the last
// non-page-size multiple block.
DWORD _stdcall
   FileCopyContentsOverlapped(
      HANDLE                 hSrc        ,// in -source file handle
      HANDLE               * hTgt         // i/o-target file handle
   )
{
   DWORD                     rc = 0,
                             nBytes;
   ULONG_PTR                 key;
   BOOL                      success;
   int                       nBuffer = gOptions.sizeBuffer / COPY_BUFFER_SIZE,
                             nPendingIO = 0,
                             n;
   IOControl               * ioControl = (IOControl *)_alloca(nBuffer * sizeof (IOControl));
   HANDLE                    ioPort;      // I/O completion port handle
   ULARGE_INTEGER            readPointer,
                             tgtSize;
   IOControl               * ioCompleted,
                           * lastIO = NULL;
// DWORD                     s = GetTickCount();
   ULARGE_INTEGER            cbFile;

   // Get file size again since it might have changed since directory scan
   cbFile.LowPart = GetFileSize(hSrc, &cbFile.HighPart);
   if ( cbFile.LowPart == (DWORD)-1  )
   {
      rc = GetLastError();
      if ( rc )
      {
         err.SysMsgWrite(31028, rc, L"GetFileSize=%ld ", rc);
         return rc;
      }
   }

   // Set the destination's file size to the size of the source file extended
   // to a multiple of the page size for parallelism and non-fragmentation.
   tgtSize.QuadPart = (cbFile.QuadPart) & -(long)pageSize;
   rc = SetFilePointer(*hTgt,
                       tgtSize.LowPart,
                       (long *)&tgtSize.HighPart,
                       FILE_BEGIN);
   if ( rc == 0xffffffff  )
   {
      rc = GetLastError();
      if ( rc )
      {
         err.SysMsgWrite(30208, rc, L"Extend SetFilePointer=%ld", rc);
         return rc;
      }
   }

   success = SetEndOfFile(*hTgt);
   if ( !success )
   {
      rc = GetLastError();
      err.SysMsgWrite(30209, rc, L"Extend SetEndOfFile=%ld ", rc);
      return rc;
   }

   ioPort = CreateIoCompletionPort(hSrc, NULL, ReadKey, 1);
   if ( ioPort == NULL )
   {
      rc = GetLastError();
      err.SysMsgWrite(31021, rc, L"CreateIoCompletionPort(hSrc)=%ld ", rc);
      return rc;
   }

   // Associate the destination file handle with the I/O completion port.
   ioPort = CreateIoCompletionPort(*hTgt, ioPort, WriteKey, 1);
   if ( ioPort == NULL )
   {
      rc = GetLastError();
      err.SysMsgWrite(31022, rc, L"CreateIoCompletionPort(hTgt)=%ld ", rc);
      return rc;
   }

   // kick off enough reads to fill the buffer and get things going
   for ( readPointer.QuadPart = 0, n = 0;
         n < nBuffer  &&  readPointer.QuadPart < cbFile.QuadPart;
         readPointer.QuadPart += COPY_BUFFER_SIZE, n++ )
   {
      ioControl[n].nBuff = n;
      ioControl[n].ov.Offset = readPointer.LowPart;
      ioControl[n].ov.OffsetHigh = readPointer.HighPart;
      ioControl[n].ov.hEvent = NULL; // not needed

      success = ReadFile(hSrc,
                         Buffer(n),
                         COPY_BUFFER_SIZE,
                         &nBytes,
                         &ioControl[n].ov);
      if ( !success )
      {
         rc = GetLastError();
         if ( rc != ERROR_IO_PENDING )
         {
            err.SysMsgWrite(30204, rc, L"ReadFile(%I64d)=%ld ",
                                   readPointer.QuadPart, rc);
            return rc;
         }
      }

      ++nPendingIO;
   }

   // We have started the initial async. reads, enter the main loop.
   // This simply waits until an I/O completes, then issues the next
   // I/O. When a write completes, the next read is issued. When a
   // read completes, the corresponding write is issued.
   while ( nPendingIO )
   {
      success = GetQueuedCompletionStatus(ioPort,
                                          &nBytes,
                                          &key,
                                          (LPOVERLAPPED *)&ioCompleted,
                                          (DWORD) -1 );
      if ( !success )
      {
         if ( ioCompleted == NULL )
         {
            // The call has failed.
            rc = GetLastError();
            err.SysMsgWrite(30205, rc, L"GetQueuedCompletionStatus=%ld ", rc);
            return rc;
         }
         else
         {
            // The call has succeeded, but the initial I/O operation has failed
            rc = GetLastError();
            err.SysMsgWrite(30206, rc, L"GetQueuedCompletionStatus=%ld removed a failed "
                                       "I/O packet, ", rc);
            return rc;
         }
      }

      if ( key == ReadKey )
      {
         // If the bytes read is less than that requested, it is the last block.
         // If the target of the last block is a UNC, we want to write it buffered
         // because unbuffered mode requires full block writes.
         if ( nBytes < COPY_BUFFER_SIZE  &&  !gOptions.target.bUNC )
         {
            lastIO =  ioCompleted;
            nPendingIO--;
         }
         else
         {
            success = WriteFile(*hTgt,
                                Buffer(ioCompleted->nBuff),
                                nBytes,
                                &nBytes,
                                (LPOVERLAPPED)ioCompleted);
            if ( !success  &&  GetLastError() != ERROR_IO_PENDING )
            {
               rc = GetLastError();
               err.SysMsgWrite(30207, rc, L"WriteFile(%I64d)=%ld ",
                               INT64R(ioControl->ov.Offset,ioControl->ov.OffsetHigh), rc);
               return rc;
            }
         }
      }
      else if ( key == WriteKey )
      {
         gOptions.bWritten += nBytes;
         if ( readPointer.QuadPart < cbFile.QuadPart )
         {
            // More data in the file, issue next read
            ioCompleted->ov.Offset = readPointer.LowPart;
            ioCompleted->ov.OffsetHigh = readPointer.HighPart;
            success = ReadFile(hSrc,
                               Buffer(ioCompleted->nBuff),
                               nBytes,
                               &nBytes,
                               (LPOVERLAPPED)ioCompleted);
            if ( !success  &&  GetLastError() != ERROR_IO_PENDING )
            {
               rc = GetLastError();
               err.SysMsgWrite(30208, rc, L"ReadFile(%I64d)=%ld ",
                            INT64R(ioControl->ov.Offset,ioControl->ov.OffsetHigh), rc);
               return rc;
            }
            readPointer.QuadPart += COPY_BUFFER_SIZE;
         }
         else
         {
            // No more reads left to issue, just wait for pending writes to drain
            --nPendingIO;
         }
      }
   }
   CloseHandle(ioPort);

   // A non-NULL lastIO represents the last I/O that was purposesly not written
   // when the target is unbuffered because unbuffered I/O requires sector-size
   // writes and this last I/O is smaller.  The file is opened in buffered mode,
   // positioned to the end and the last non-sector-sized block is written.
   if ( lastIO )
   {
      CloseHandle(*hTgt);
      *hTgt = CreateFile(gOptions.target.apipath,
                        GENERIC_WRITE | GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        0,
                        NULL);
      if ( *hTgt == INVALID_HANDLE_VALUE )
      {
         rc = GetLastError();
         err.SysMsgWrite(30107, rc, L"TruncateOpen=%ld ", rc);
         return rc;
      }

      // Set file position to the lastIO read position
      rc = SetFilePointer(*hTgt,
                          lastIO->ov.Offset,
                          (LONG *)&lastIO->ov.OffsetHigh,
                          FILE_BEGIN);
      if ( rc == 0xffffffff  )
      {
         rc = GetLastError();
         if ( rc )
            err.SysMsgWrite(30108, rc, L"Truncate SetFilePointer=%ld", rc);
      }

      rc = 0;
      if ( !WriteFile(*hTgt,
                      Buffer(lastIO->nBuff),
                      cbFile.LowPart & ~-(long)COPY_BUFFER_SIZE,
                      &nBytes,
                      NULL) )
      {
         rc = GetLastError();
         err.SysMsgWrite(30109, rc, L"Last Write(%s)=%ld ",
                                    gOptions.target.path, rc);
      }
      gOptions.bWritten += nBytes;
   }
// err.MsgWrite(1, "%.2fMB/sec (%s)",
//        (float)((__int64)tgtSize.QuadPart) / 1000 / (float)(GetTickCount()-s),
//        gOptions.target.path);

   return 0;
}
