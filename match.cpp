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

struct HiddenSemanticAction
{
   char                      srcSet;      // 0=no action, 1=set srcEntry to NULL
   char                      tgtSet;      // 0=no action, 1=set tgtEntry to NULL
};

/// Handles file propagation according to the -h option when files/dirs are hidden
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

   DirEntry                * srcEntry,    // current source entry
                           * tgtEntry;    // current target entry


   if ( srcDirEntry )
   {
      if ( srcDirEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY )
         if ( rc = gOptions.source.dirList.PathProcess(srcEntry->cFileName) )
            return rc;
         else;
      else
         MismatchSourceNotDir(srcDirEntry, tgtDirEntry);
   }
   if ( tgtDirEntry )
   {
      if ( tgtDirEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY )
         if ( rc = gOptions.target.dirList.PathProcess(tgtEntry->cFileName) )
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

	DirListEnum				srcEnum(&gOptions.source.dirList);
	DirListEnum				tgtEnum(&gOptions.target.dirList);

	// match source and target entry lists and process differences
	for ( srcEntry = srcEnum.GetNext(), tgtEntry = tgtEnum.GetNext();  srcEntry || tgtEntry; )
	{
		DirEntry * srcTemp = srcEntry, 
				 * tgtTemp = tgtEntry;
		if ( tgtEntry && srcTemp)		// compare them if both exist
		{
			comp = _wcsicmp(srcTemp->cFileName, tgtTemp->cFileName);
			if ( comp < 0)
				tgtTemp = NULL;
			else if ( comp > 0 )
				srcTemp = NULL;
		}

		if ( tgtTemp )
		{
			if ( !srcTemp )
				gOptions.source.dirList.PathAppend(tgtTemp->cFileName);
			gOptions.target.dirList.PathAppend(tgtTemp->cFileName);
			tgtEntry = tgtEnum.GetNext();
		}
		if ( srcTemp )
		{
			gOptions.source.dirList.PathAppend(srcTemp->cFileName);
			if ( !tgtTemp )
				gOptions.target.dirList.PathAppend(srcTemp->cFileName);
			srcEntry = srcEnum.GetNext();
		}

		// resolve hidden file/directory semantics when /-h specified
		if ( !(gOptions.global & OPT_GlobalHidden) )
			HiddenSemanticsSet(&srcTemp, &tgtTemp);

		// recursive call for subdirectories
		if ( (srcTemp  &&  srcTemp->attrFile & FILE_ATTRIBUTE_DIRECTORY )
  		  || (tgtEntry  &&  tgtEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY) )
		{
			if ( (level+1) <= gOptions.maxLevel )
				MatchEntries(level + 1, srcTemp, tgtTemp);
		}
		else if (srcTemp || tgtTemp)
		{
			MatchedFileProcess(srcEntry, tgtTemp);
		}
	}

	DisplayPathOffset(gOptions.target.path);

	if ( tgtDirEntry )
		// do target dirs on way up in case of deletion
		MatchedDirTgtExists(srcDirEntry, tgtDirEntry);
	else
		// Takes care of dir attributes that can't be set at dir creation time
		MatchedDirNoTgtExit(srcDirEntry);

	return 0;
}
