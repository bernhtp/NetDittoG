/*
===============================================================================

  Program    - Filter
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 06/12/92
  Description- Filters the input name against the provided include and exclude
               lists.  The return value is as follows:
                0 Passed filter
                1 Failed filter because not in include list
                2 Failed filter because in exlude list

        Note - The filter comparison uses the wildcmp function that handles
               a superset of DOS wildcard comparison handling but is case
               sensitive.  If the include list is empty, this implies that
               the name is automatically included and will only fail if it is
               specifically excluded.

  Updates -

===============================================================================
*/

#include "netditto.hpp"

short _stdcall                            // ret-0=accept 1=notInclude 2=Exclude
FilterReject(
	WCHAR const			  * name			,// in -name to filter
	FileList const		  * include		,// in -include list
	FileList const		  * exclude		 // in -exclude list
)
{
	FileList const		  * curr;
	WCHAR					lcName[MAX_PATH + 1];

	wcscpy_s(lcName, name);
	_wcslwr(lcName);       // convert to lc because wildcmp() is case sensitive

	if (include)
	{
		for (curr = include; curr && WildMatch(lcName, curr->name); curr = curr->next);
		if (curr == NULL)
			return 1;
	}

	for (curr = exclude; curr; curr = curr->next)
		if (!WildMatch(lcName, curr->name))
			return 2;
	return 0;
}


/// Returns true if p_name matches any of the wildcard filters in the direxlude list
bool DirFilterReject(wchar_t const * p_name)
{
	FileList const		  * excl;
	wchar_t					lcName[MAX_PATH + 1];

	wcscpy_s(lcName, p_name);
	_wcslwr(lcName);       // convert to lc because wildcmp() is case sensitive

	for ( excl = gOptions.direxclude;  excl;  excl = excl->next)
		if ( !WildMatch(lcName, excl->name) )
			return true;
	return false;
}
