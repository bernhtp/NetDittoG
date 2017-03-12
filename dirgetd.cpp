/*
===============================================================================

  Module     - DirGet
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 06/12/92
  Description- Creates a directory list in the buffer by using Find*File and
               using Filter to accept/reject the entries.  The critical data
               structure here is DirBuffer that contains a reference to
               an array of offsets to a buffer pool where the directory
               entries are stuffed.

  Updates
  93/06/13 TPB Port for Win32 version
  95/02/14 TPB Conversion of the DirBuffer.index entries to be addresses
               instead of offsets into DirBuffer.start so that the sort
               compare routine no longer relied on a static, be made reentrant
               and so could be multithreaded.
  95/08/14 RED Use multiple buffers for directory entries and indexes.
===============================================================================
*/
#include "netditto.hpp"
#include "util32.hpp"

BufferOffset                 bufferMax = 0;// high water mark for lifo DirBuffer

// Sort compare function used to sort the DirBuffer index so that source and
// target filenames can be matched (sort/merged) for difference detection.
static int _cdecl                         // ret-compare result
   SortCompare(
      DirEntry const      ** i1          ,// in -index 1 address
      DirEntry const      ** i2           // in -index 2 address
   )
{
   return _wcsicmp( (*i1)->cFileName, (*i2)->cFileName );
}

DWORD _stdcall                            // ret-0=success -1=overflow +=error
   DirGet(
      DirOptions           * dir         ,// i/o-directory data and options
      StatsCommon          * stats       ,// i/o-dir level statistics
      DirEntry           *** dirArray     // out-array of DirEntry pointers
   )
{
   wchar_t                 * appendPath = dir->path + wcslen(dir->path);
   DWORD                     rc = 0,
                             dirCount = 0;// directory entries processed
   BOOL                      bRc,
                             sorted = 1;             // dir sorted flag
   size_t                    lenFileName,
                             cbDirEntry;             // length of buff dir entry
   HANDLE                    hDir;
   WIN32_FIND_DATA           findEntry;              // result of Find*File API
   DirEntry                * dirPrev;    // previous directory entry
   DirEntry                  dirWork;    // work directory entry
   DirBlock                * newBlock;   // allocated DirBlock
   // Starting position in DirBlock - used to build index array
   DirBlock                * orgCurrBlock = dir->dirBuffer.currBlock;
   DirEntry                * orgHwm       = orgCurrBlock->hwmEntry;
   size_t                    orgAvail     = orgCurrBlock->avail;
   // Workareas for building index array
   DirIndex                * newIndex;   // new index
   size_t                    newIndexLen;// length of new index
   size_t                    oldIndexLen;// length of old index
   DirEntry               ** ptrDirEntry;// ptr to index array element

   memset( &dirWork, '\0', sizeof dirWork);
   dirPrev = &dirWork;                    // previous directory entry for sort test

   stats->dirFiltered++;                  // dirs currently always filtered
   stats->dirFound++;                     // count current directory

   wcscpy(appendPath, L"\\*");            // set path to include wildcard for Find*File
   // iterate through directory entries and stuff them in DirBuffer
   for ( bRc = ((hDir = FindFirstFile(dir->apipath, &findEntry)) != INVALID_HANDLE_VALUE),
               appendPath[0] = L'\0';     // restore path -- remove \*.* append
         bRc;
         bRc = FindNextFile(hDir, &findEntry) )
   {
      if ( findEntry.cFileName[0] == L'.' )
         if ( findEntry.cFileName[1] == L'\0'
         ||  (findEntry.cFileName[1] == L'.' && findEntry.cFileName[2] == L'\0') )
            continue;                        // ignore names '.' and '..'

      if ( !(findEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) // if file
      {
         stats->fileFound.count++;
         stats->fileFound.bytes += INT64R(findEntry.nFileSizeLow, findEntry.nFileSizeHigh);
         if ( FilterReject(findEntry.cFileName, gOptions.include, gOptions.exclude) )
            continue;                     // filter rejected, go loop
      }

      lenFileName = wcslen(findEntry.cFileName);
      cbDirEntry = CB_DirEntry(lenFileName);
      if ( cbDirEntry > dir->dirBuffer.currBlock->avail )
      {
         // Buffer full - chain to new buffer.
         if ( (void *) dir->dirBuffer.currBlock->chain.fwd == (void *) &dir->dirBuffer.block )
         {                               // need to allocate a new buffer
            newBlock = (DirBlock *) new byte[gOptions.sizeDirBuff];
            BdQueueAddEnd( &dir->dirBuffer.block, &newBlock->chain );
            bufferMax += gOptions.sizeDirBuff;
         }
         dir->dirBuffer.currBlock = (DirBlock *) dir->dirBuffer.currBlock->chain.fwd;
         dir->dirBuffer.currBlock->hwmEntry = &dir->dirBuffer.currBlock->firstEntry;
         dir->dirBuffer.currBlock->avail = gOptions.sizeDirBuff - offsetof(DirBlock,firstEntry);
      }

      if ( !(findEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )  // if it's a file
      {
         stats->fileFiltered.count++;
         stats->fileFiltered.bytes += INT64R(findEntry.nFileSizeLow, findEntry.nFileSizeHigh);
      }

      if ( sorted )                       // check to see if the sort is broken
         // note that dirEntry points to the previous entry at this point
         if ( _wcsicmp(findEntry.cFileName, dirPrev->cFileName) < 0 )
            sorted = 0;

      dirPrev = dir->dirBuffer.currBlock->hwmEntry;
      // Create directory buffer entry by copying info from findEntry
      dirPrev->ftimeLastWrite = findEntry.ftLastWriteTime;
      dirPrev->cbFile         = INT64R(findEntry.nFileSizeLow, findEntry.nFileSizeHigh);
      dirPrev->attrFile       = findEntry.dwFileAttributes;
      wcsncpy(dirPrev->cFileName, findEntry.cFileName, lenFileName + 1);

      // Update directory block
      dir->dirBuffer.currBlock->hwmEntry = (DirEntry *) (((byte *) dir->dirBuffer.currBlock->hwmEntry)
            + cbDirEntry);
      dir->dirBuffer.currBlock->avail -= cbDirEntry;
      dirCount++;
   }

   if ( rc != (DWORD)-1 )
      rc = GetLastError();

   if ( hDir != INVALID_HANDLE_VALUE )
      FindClose(hDir);

   switch ( rc )
   {
      case ERROR_NO_MORE_FILES:
         rc = 0;
         break;
      case 0:                            // success
         err.MsgWrite(50100, L"FindFirst/Next=0, why did I get this");
         break;
      case -1:                           // buffer full
         err.MsgWrite(50101, L"Buffer full" );
         break;
      case ERROR_PATH_NOT_FOUND:
         err.MsgWrite(50102, L"Invalid path '%s'", dir->path);
         break;
      default:
         err.SysMsgWrite(50103, rc, L"DirGet(%s)=%ld ", dir->path, rc);
   }

   if ( !rc )
   {
      // if necessary, allocate a new index
      if ( (void *) dir->dirBuffer.currIndex->chain.fwd == (void *) &dir->dirBuffer.index )
      {                                   // need to allocate a new index
         newIndexLen = LEN_DirIndex + (dirCount * sizeof (DirEntry *));
         newIndexLen = max( newIndexLen, gOptions.sizeDirIndex );
         newIndex = (DirIndex *) new char[newIndexLen];
         newIndex->availSlots = (newIndexLen - LEN_DirIndex) / sizeof (DirEntry *);
         BdQueueAddEnd( &dir->dirBuffer.index, &newIndex->chain );
         bufferMax += newIndexLen;
      }
      // if next index is not big enough, allocate a bigger one
      dir->dirBuffer.currIndex = (DirIndex *) dir->dirBuffer.currIndex->chain.fwd;
      if ( dirCount > dir->dirBuffer.currIndex->availSlots )
      {
         oldIndexLen = LEN_DirIndex + dir->dirBuffer.currIndex->availSlots * sizeof (DirEntry *);
         newIndexLen = LEN_DirIndex + dirCount * sizeof (DirEntry *);
         newIndex = (DirIndex *) new char[newIndexLen];
         memcpy( newIndex, dir->dirBuffer.currIndex, oldIndexLen );
         newIndex->availSlots = (newIndexLen - LEN_DirIndex) / sizeof (DirEntry *);
         BdQueueInsAft( &dir->dirBuffer.index, &newIndex->chain, &dir->dirBuffer.currIndex->chain );
         bufferMax += newIndexLen;
         BdQueueDel( &dir->dirBuffer.index, &dir->dirBuffer.currIndex->chain );
         bufferMax -= oldIndexLen;
         delete[] dir->dirBuffer.currIndex;
         dir->dirBuffer.currIndex = newIndex;
      }
      // now build the index array
      dir->dirBuffer.currIndex->usedSlots = dirCount;
      for ( ptrDirEntry = dir->dirBuffer.currIndex->dirArray; dirCount; dirCount-- )
      {
         if ( orgHwm >= orgCurrBlock->hwmEntry )
         {
            orgCurrBlock = (DirBlock *) orgCurrBlock->chain.fwd;
            orgHwm = &orgCurrBlock->firstEntry;
         }
         *(ptrDirEntry++) = orgHwm;
         orgHwm = (DirEntry *) ((byte *) orgHwm + CB_DirEntry(wcslen(orgHwm->cFileName)));
      }
      *dirArray = dir->dirBuffer.currIndex->dirArray;

      if ( !sorted )                      // if not sorted, sort the indexes
      {
         qsort(dir->dirBuffer.currIndex->dirArray,
               dir->dirBuffer.currIndex->usedSlots,
               sizeof dir->dirBuffer.currIndex->dirArray[0],
               (int(__cdecl *)(const void*,const void*))SortCompare );
      }
   }
   return rc;
}


// initialize bi-directional queue

void _stdcall
   BdQueueInit(
      BdQueueAnchor         * anchor      // out-queue anchor to be initialized
   )
{
   anchor->head = anchor->tail = (BdQueueElement *) anchor;
   anchor->count = 0;
   return;
}

// add element at end of bi-directional queue

void _stdcall
   BdQueueAddEnd(
      BdQueueAnchor        * anchor     ,// i/o-queue anchor
      BdQueueElement       * addQel      // i/o-element to be added at end of queue
   )
{
   BdQueueElement          * lastQel    ;// last element on queue

   lastQel = anchor->tail;
   lastQel->fwd = addQel;                // insert on forward queue
   addQel->fwd = (BdQueueElement *) anchor;
   anchor->tail = addQel;                // insert on backward queue
   addQel->bwd = lastQel;
   anchor->count++;                      // one more element on queue
   return;
}

// insert element after another on bi-directional queue

void _stdcall
   BdQueueInsAft(
      BdQueueAnchor        * anchor     ,// i/o-queue anchor
      BdQueueElement       * insQel     ,// i/o-element to be inserted
      BdQueueElement       * aftQel      // i/o-insert point
   )
{
   BdQueueElement          * fwdQel     ;// queue element after insert point

   fwdQel = aftQel->fwd;
   aftQel->fwd = insQel;                 // insert on forward queue
   insQel->fwd = fwdQel;
   fwdQel->bwd = insQel;                 // insert on backward queue
   insQel->bwd = aftQel;
   anchor->count++;                      // one more element on queue
   return;
}

// remove element from bi-directional queue

void _stdcall
   BdQueueDel(
      BdQueueAnchor        * anchor     ,// i/o-queue anchor
      BdQueueElement       * delQel      // i/o-element to be deleted from queue
   )
{
   BdQueueElement          * bwdQel     ;// previous element on queue
   BdQueueElement          * fwdQel     ;// next element on queue

   bwdQel = delQel->bwd;
   fwdQel = delQel->fwd;
   bwdQel->fwd = fwdQel;                 // remove from forward queue
   fwdQel->bwd = bwdQel;                 // remove from backward queue
   anchor->count--;                      // one less element on queue
   return;
}
