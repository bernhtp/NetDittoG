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
	DWORD _stdcall								 // ret-0=success
	MatchEntries(
		short                  p_level			,// in -current recursion/directory level
		DirEntry             * p_srcDirEntry	,// in -source dir entry
		DirEntry             * p_tgtDirEntry	 // in -target dir entry
	)
{
	int						comp;				// source/target operation result

	DirEntry			  * srcEntry = NULL,	// current source entry
                          * tgtEntry = NULL;	// current target entry
	DirListEnum				srcEnum(&gSource);
	DirListEnum				tgtEnum(&gTarget);
	DWORD					rc = 0;

	if ( p_srcDirEntry )
	{
		if ( p_srcDirEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY )
		{
			if ( rc = gSource.ProcessCurrentPath() )
				return rc;	// kick out with bad error if can't enum a directory we know we have
		}
		else
			MismatchSourceNotDir(p_srcDirEntry, p_tgtDirEntry);
	}
	if ( p_tgtDirEntry )
	{
		if ( p_tgtDirEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY )
		{
			if ( rc = gTarget.ProcessCurrentPath() )
				return rc;
		}
		else
			MismatchTargetNotDir(p_srcDirEntry, p_tgtDirEntry);
	}
	else      // process no target on way down in case of create)
		MatchedDirNoTgt(p_srcDirEntry);
	if ( p_srcDirEntry  &&  p_tgtDirEntry )
		gOptions.stats.match.dirMatched++;

   DisplayPathOffset(gTarget.Path());

	// match source and target entry lists and process differences
	while ( !srcEnum.EOL() || !tgtEnum.EOL() )
	{
		srcEntry = srcEnum.Peek(); 
		tgtEntry = tgtEnum.Peek();
		if ( tgtEntry && srcEntry)		// compare them if both exist
		{
			comp = _wcsicmp(srcEntry->cFileName, tgtEntry->cFileName);
			if ( comp < 0)
				tgtEntry = NULL;
			else if ( comp > 0 )
				srcEntry = NULL;
		}

		if ( tgtEntry )
		{
			if ( !srcEntry )
				gSource.PathAppend(tgtEntry->cFileName);
			gTarget.PathAppend(tgtEntry->cFileName);
			tgtEnum.Advance();
		}
		if ( srcEntry )
		{
			gSource.PathAppend(srcEntry->cFileName);
			if ( !tgtEntry )
				gTarget.PathAppend(srcEntry->cFileName);
			srcEnum.Advance();
		}

		// resolve hidden file/directory semantics when /-h specified by possibly NULLing entry(s)
		if ( !(gOptions.global & OPT_GlobalHidden) )
			HiddenSemanticsSet(&srcEntry, &tgtEntry);

		// recursive call for subdirectories
		if ( (srcEntry  &&  srcEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY)
  		  || (tgtEntry  &&  tgtEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY) )
		{
			if ( (p_level+1) <= gOptions.maxLevel )
				MatchEntries(p_level + 1, srcEntry, tgtEntry);
		}
		else if ( srcEntry || tgtEntry )
		{
			MatchedFileProcess(srcEntry, tgtEntry);
		}
		// clip paths to where they were at beginning of each iteration (before .PathAppend)
		gSource.PathTrunc();
		gTarget.PathTrunc();
	}

	DisplayPathOffset(gTarget.Path());

	if ( p_tgtDirEntry )
		// do target dirs on way up in case of deletion
		MatchedDirTgtExists(p_srcDirEntry, p_tgtDirEntry);
	else
		// Takes care of dir attributes that can't be set at dir creation time
		MatchedDirNoTgtExit(p_srcDirEntry);
	return 0;
}
