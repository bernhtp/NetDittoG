/*
================================================================================

  Program    - Match
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 06/13/92
  Description- Generates the source and target lists at each level by calling
               DirGet and matches the resultant dirBuffers.  Based upon the
               results of the match and the command line parms, it will take
               the approproate actions.

  Updates -
  95/08/14 RED Change save/restore of DirBuffer and DirIndex.

================================================================================
*/

#include "netditto.hpp"

// end-of-list macros for both source and target
#define SrcEOL ( !srcIndex || srcNbr >= gOptions.source.dirBuffer.currIndex->usedSlots )
#define TgtEOL ( !tgtIndex || tgtNbr >= gOptions.target.dirBuffer.currIndex->usedSlots )

struct HiddenSemanticAction
{
   char                      srcSet;      // 0=no action, 1=set srcEntry to NULL
   char                      tgtSet;      // 0=no action, 1=set tgtEntry to NULL
};


void _stdcall                              // ret-0=success
   HiddenSemanticsSet(
      DirEntry            ** srcEntry     ,// i/o-source dir entry addr
      DirEntry            ** tgtEntry      // i/o-target dir entry addr
   )
{
   // This table describes the actions to be taken on all combinations of missing, hidden
   // and not hidden on source and target when processing with /-h -- don't process
   // hidden attribute files/dirs.  The action for both source and target consists of
   // whether to null out the entry (make believe it never existed).
   static const HiddenSemanticAction action[3][3] =
   //                            Target
   // Source                   M     H    NH
   /* missing      */     {{ {0,0},{0,1},{0,0} },
   /* hidden       */      { {1,0},{1,1},{1,0} },
   /* not-hidden   */      { {0,0},{1,1},{0,0} }};

   int                       s, t;

   if ( *srcEntry )
      if ( (*srcEntry)->attrFile & FILE_ATTRIBUTE_HIDDEN )
         s = 1;
      else
         s = 2;
   else
      s = 0;

   if ( *tgtEntry )
      if ( (*tgtEntry)->attrFile & FILE_ATTRIBUTE_HIDDEN )
         t = 1;
      else
         t = 2;
   else
      t = 0;

   if ( action[s][t].srcSet )
      *srcEntry = NULL;
   if ( action[s][t].tgtSet )
      *tgtEntry = NULL;
}

// matches the source and target ordered lists of DirEntry objects within a directory.
// takes action depending upon whether both names match
// Processes subdirectories recursively.
short _stdcall                            // ret-0=success
   MatchEntries(
      short                  level       ,// in -current recursion/directory level
      DirEntry             * srcDirEntry ,// in -source dir entry
      DirEntry             * tgtDirEntry  // in -target dir entry
   )
{
   int                       rc,
                             comp;        // source/target operation result

   // variables associated with popping LIFO stacks after done

   DirBlock                * srcCurrBlock  = gOptions.source.dirBuffer.currBlock,
                           * tgtCurrBlock  = gOptions.target.dirBuffer.currBlock;
   DirEntry                * srcHwm        = srcCurrBlock->hwmEntry,
                           * tgtHwm        = tgtCurrBlock->hwmEntry;
   DWORD                     srcAvail      = srcCurrBlock->avail,
                             tgtAvail      = tgtCurrBlock->avail;
   DirIndex                * srcCurrIndex  = gOptions.source.dirBuffer.currIndex,
                           * tgtCurrIndex  = gOptions.target.dirBuffer.currIndex;

   DWORD                     srcNbr        = 0;
   DWORD                     tgtNbr        = 0;
   DirEntry               ** srcIndex      = NULL,
                          ** tgtIndex      = NULL,
                           * srcEntry,    // current source entry
                           * tgtEntry;    // current target entry
   WCHAR                   * srcAppend = gOptions.source.path + wcslen(gOptions.source.path),
                           * tgtAppend = gOptions.target.path + wcslen(gOptions.target.path);

   if ( srcDirEntry )
   {
      if ( srcDirEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY )
         if ( rc = DirGet(&gOptions.source, &gOptions.stats.source, &srcIndex) )
            return rc;
         else;
      else
         MismatchSourceNotDir(srcDirEntry, tgtDirEntry);
   }
   if ( tgtDirEntry )
   {
      if ( tgtDirEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY )
         if ( rc = DirGet(&gOptions.target, &gOptions.stats.target, &tgtIndex) )
            return rc;
         else;
      else
         MismatchTargetNotDir(srcDirEntry, tgtDirEntry);
   }
   else      // process no target on way down in case of create)
      MatchedDirNoTgt(srcDirEntry);
   if ( srcDirEntry  &&  tgtDirEntry )
      gOptions.stats.match.dirMatched++;

   DisplayPathOffset(gOptions.target.path);

   // append '\\' to source and target paths. The DireEntry filename will later 
   // be appended for a full path
   *srcAppend = *tgtAppend = L'\\';       
                                          
   while ( !SrcEOL  ||  !TgtEOL )
   {
      if ( tgtIndex  &&  !TgtEOL )
         tgtEntry = *tgtIndex;
      else
         tgtEntry = NULL;
      if ( srcIndex  &&  !SrcEOL )
         srcEntry = *srcIndex;
      else
         srcEntry = NULL;

      if ( tgtEntry && srcEntry )
      {
         if ( (comp = _wcsicmp(srcEntry->cFileName, tgtEntry->cFileName)) < 0 )
            tgtEntry = NULL;
         else if ( comp > 0 )
            srcEntry = NULL;
      }

      if ( tgtEntry )
      {
         if ( !srcEntry )
            wcscpy(srcAppend+1, tgtEntry->cFileName);
         wcscpy(tgtAppend+1, tgtEntry->cFileName);
         tgtIndex++;
         tgtNbr++;
      }
      if ( srcEntry )
      {
         wcscpy(srcAppend+1, srcEntry->cFileName);
         if ( !tgtEntry )
            wcscpy(tgtAppend+1, srcEntry->cFileName);
         srcIndex++;
         srcNbr++;
      }

      // resolve hidden file/directory semantics when /-h specified
      if ( !(gOptions.global & OPT_GlobalHidden) )
         HiddenSemanticsSet(&srcEntry, &tgtEntry);

      // recursive call for subdirectories
      if ( (srcEntry  &&  srcEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY )
        || (tgtEntry  &&  tgtEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY) )
      {
         if ( (level+1) <= gOptions.maxLevel )
            MatchEntries(level + 1, srcEntry, tgtEntry);
      }
      else if ( srcEntry  ||  tgtEntry )
      {
         MatchedFileProcess(srcEntry, tgtEntry);
      }
   }

   // Pop LIFO stacks by restoring previous stack pointers
   srcAppend[0] = tgtAppend[0] = L'\0';

   gOptions.source.dirBuffer.currBlock = srcCurrBlock;
   if ( (void *) srcCurrBlock != (void *) &gOptions.source.dirBuffer.block )
   {
      srcCurrBlock->hwmEntry = srcHwm;
      srcCurrBlock->avail    = srcAvail;
   }

   gOptions.target.dirBuffer.currBlock = tgtCurrBlock;
   if ( (void *) tgtCurrBlock != (void *) &gOptions.target.dirBuffer.block )
   {
      tgtCurrBlock->hwmEntry = tgtHwm;
      tgtCurrBlock->avail    = tgtAvail;
   }

   gOptions.source.dirBuffer.currIndex = srcCurrIndex;
   gOptions.target.dirBuffer.currIndex = tgtCurrIndex;

   DisplayPathOffset(gOptions.target.path);

   if ( tgtDirEntry )
      // do target dirs on way up in case of deletion
      MatchedDirTgtExists(srcDirEntry, tgtDirEntry);
   else
      // Takes care of dir attributes that can't be set at dir creation time
      MatchedDirNoTgtExit(srcDirEntry);

   return 0;
}
