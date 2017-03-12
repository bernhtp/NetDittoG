/*
===============================================================================

  Module     - Construct
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/23/93
  Description- Constructs and initializes the basic data structures used in
               the system.
  Updates -
  95/08/14 RED Change method of initializing the directory buffer and index.

===============================================================================
*/

#include "netditto.hpp"

#define COPYBUFFSIZE        ((unsigned)1<<18)

extern BufferOffset          bufferMax;

static short _stdcall                     // ret-0=success
   DirBufferConstruct(
      DirBuffer            * dir          // out-directory buffer
   )
{
   DirBlock                * firstBlock; // first directory block

   firstBlock = (DirBlock *) new char[gOptions.sizeDirBuff];
   if ( firstBlock )
   {
      BdQueueInit( &dir->block );        // prepare DirBlock queue
      BdQueueAddEnd( &dir->block, &firstBlock->chain );
      dir->currBlock = firstBlock;
      firstBlock->hwmEntry = &firstBlock->firstEntry;
      firstBlock->avail = gOptions.sizeDirBuff - offsetof(DirBlock,firstEntry);
      bufferMax += gOptions.sizeDirBuff;
   }
   else
   {
      err.MsgWrite( 50101, L"Buffer allocation(%u) failed.", gOptions.sizeDirBuff );
   }
   dir->currIndex = (DirIndex *) &dir->index;
   BdQueueInit( &dir->index );           // prepare DirIndex queue
   return 0;
}

void _stdcall                             // ret-0=success
   OptionsConstruct()
{
   DirBufferConstruct(&gOptions.source.dirBuffer);
   DirBufferConstruct(&gOptions.target.dirBuffer);
   gOptions.findAttr = FILE_ATTRIBUTE_NORMAL    | FILE_ATTRIBUTE_READONLY
                     | FILE_ATTRIBUTE_HIDDEN    | FILE_ATTRIBUTE_DIRECTORY
                     | FILE_ATTRIBUTE_ARCHIVE;

   gOptions.copyBuffer = (PBYTE)VirtualAlloc(NULL,
                                             gOptions.sizeBuffer = COPYBUFFSIZE,
                                             MEM_COMMIT,
                                             PAGE_READWRITE);
   if ( !gOptions.copyBuffer )
      err.SysMsgWrite(50998, GetLastError(), L"CopyBuffer LocalAlloc(%u)=%ld ",
                             gOptions.sizeBuffer, GetLastError());
}

