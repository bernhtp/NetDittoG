/*
===============================================================================

Module     - DIrList.cpp
Class      - NetDitto Utility
Author     - Tom Bernhardt
Created    - 05/01/17
Description- Methods and functions related to class DirEntry, DirList, DirBlock
	DirBlockCollection, and DirListEnum.  See DirList.h for definitions.
===============================================================================
*/
#include "NetDitto.hpp"


/// copies the \\server\share portion of a UNC path to tgtPath and returns the length
static int									 // ret-length copied or -1 if error
ServerShareCopy(
	wchar_t				  * p_tgtPath		,// out-target \\server\share path
	wchar_t const		  * p_uncpath		 // in -UNC path after \\?\ prefix
	)
{
	wchar_t const		  * c;
	int						nSlash = 0;
	wchar_t				  * t = p_tgtPath;

	// iterate through the string stopping at the fourth backslash or '\0'
	for ( c = p_uncpath;  *c;  c++ )
	{
		if ( *c == L'\\' )
		{
			if ( ++nSlash > 3 )				// stop at three '\' chars - \\server\share
				break;
		}
		*t++ = *c;
	}
	*t = L'\0';								// terminate at \\server\share
	// If UNC but not at least sharename separater blackslash, error
	if ( nSlash < 3 )
		return -1;
	return (int)(c - p_uncpath);
}


/// Creates root DirEntry dir
DirEntry * __stdcall DirEntryCreate(WIN32_FIND_DATAW const * p_find)
{
	DirEntry                * direntry;
	size_t                    len;

	len = sizeof *direntry + WcsByteLen(p_find->cFileName);

	direntry = (DirEntry *)malloc(len);
	memset(direntry, 0, len);
	wcscpy(direntry->cFileName, p_find->cFileName);
	direntry->attrFile = p_find->dwFileAttributes;
	direntry->ftimeLastWrite = p_find->ftLastWriteTime;

	return direntry;
}

///? Test apparent compiler bug in not accepting array literal initializer
class Path
{
	wchar_t	const	m_apiprefix[4] = { L'\\', L'\\', L'?', L'\\' };
	wchar_t			m_path[32767];

	Path(wchar_t const p_path)  {}
	wchar_t * GetPath() { return m_path; }
	wchar_t const * GetApipath() const { return m_apiprefix; }
};

/// Initializes IndexHeader
void IndexLevel::Initialize()
{
	marker1 = -1;
	marker2 = 0xeeeeeeee;
	m_nEntries = 0;
}


DirList::DirList(StatsCommon * p_stats)
	: m_path(L""), m_pathlen(0), m_currindex(INDEXLEVEL_NULL_OFFSET), m_stats(p_stats)
{
	m_indexsize = INDEX_INCREMENT;
	m_index = (byte *)malloc(m_indexsize);
}

DirEntry * DirList::DirEntryAdd(WIN32_FIND_DATA const * p_find)	// Creates a DirEntry at the current level
{
	DirEntry * direntry = m_dirblocks.DirEntryCreate(p_find); 
	IndexEntryAdd(direntry); 
	return direntry;
}


/// returns the indicated index entry value, NULL if invalid
DirEntry * DirList::GetIndexDirEntry(int p_n) 
{
	IndexLevel * i = GetCurrIndexLevel();
	return p_n >= i->m_nEntries ? NULL : i->m_direntry[p_n];
}


/// expand the index size when needed
void DirList::IndexGrow()
{
	m_indexsize += INDEX_INCREMENT;
	m_index = (byte *)realloc(m_index, m_indexsize);
}


/// Appends new name to end of m_path
int DirList::PathAppend(wchar_t const * p_name)
{
	m_path[m_pathlen] = L'\\';
	wcscpy_s(m_path + m_pathlen + 1, MAX_PATHX - m_pathlen - 3, p_name);
	m_pathlen += wcslen(p_name) + 1;
	return m_pathlen;
}


/// Push new IndexHeader on top of m_index stack
void DirList::Push()
{
	IndexLevel		  * currheader = NULL;
	DirEntryBlock	  * prevblock = NULL;
	size_t				previndex = INDEXLEVEL_NULL_OFFSET;
	short				prevdepth = -1;

	if ( m_currindex == INDEXLEVEL_NULL_OFFSET )
	{
		// special case for first/root IndexLevel
		currheader = (IndexLevel *)m_index;
		m_currindex = 0;
	}
	else
	{
		// if this isn't the first/root
		currheader = (IndexLevel *)(m_index + m_currindex);
		previndex = m_currindex;
		prevdepth = currheader->m_depth;
		if ( IndexUsed() + sizeof(IndexLevel) >= m_indexsize )
			IndexGrow();
		// create new IndexLevel on stack
		m_currindex = IndexUsed();
		currheader = GetCurrIndexLevel();
	}

	currheader->Initialize();
	currheader->m_initblock = m_dirblocks.GetCurrBlock();
	currheader->m_initoffset = m_dirblocks.GetNextAvail();
	currheader->m_prevheader = previndex;
	currheader->m_depth = prevdepth + 1;
	currheader->m_pathlen = (short)m_pathlen;
}

/// Pop to the previous index, dirblock, diroffset and path
void DirList::Pop()
{
	IndexLevel * currheader = (IndexLevel *)(m_index + m_currindex);
	if ( currheader->m_prevheader != INDEXLEVEL_NULL_OFFSET )
	{
		IndexLevel * prevheader = (IndexLevel *)(m_index + currheader->m_prevheader);
		m_path[prevheader->m_pathlen] = L'\0';
	}
	m_currindex = currheader->m_prevheader;
	m_dirblocks.Popto(currheader->m_initblock, currheader->m_initoffset);
}


/// Adds an entry into the current index level for a DirEntry *
void DirList::IndexEntryAdd(DirEntry * p_direntry)
{
	if ( IndexUsed() + sizeof p_direntry >= m_indexsize )
		IndexGrow();
	IndexLevel * indexlevel = GetCurrIndexLevel();
	indexlevel->m_direntry[indexlevel->m_nEntries++] = p_direntry;
}


/// compare function for qsort_s to esnure that file lists or ordered consistently for match/merge
static int QSortCompare(const void * p_a, const void * p_b)
{
	return _wcsicmp(((DirEntry *)p_a)->cFileName, ((DirEntry *)p_b)->cFileName);
}

/// Sorts the index for the current level by DirEntry.cFilename
void DirList::IndexSort()
{
	IndexLevel * currindex = GetCurrIndexLevel();
	qsort(currindex->m_direntry, currindex->m_nEntries, sizeof (DirEntry *), QSortCompare);
}


/// Normalizes p_path before setting m_path.  Returns 0 or rc.  Sets volume information 
/* Converts p_path to a canonical path form to m_path:
	- Absolute paths, i.e., drive letter with full path or UNC with full path
	- Network-mapped drive letters are converted to UNC form 
	- UNC paths (\\server\share\other) are converted into long-name API format: UNC\Server\share\other
	- No trailing backslash.  
	- Roots (e.g., c:\ and \\server\share) are represented without the trailing backslash.  They require special treatment since they don't exist as a directory
*/
DWORD DirList::SetNormalizedRootPath(wchar_t const * p_path) 
{
	union
	{
		REMOTE_NAME_INFO    i;
		WCHAR               x[_MAX_PATH + 1];
	}                       info;
	DWORD                   rc,
							sizeBuffer = sizeof info,
							maxCompLen,
							sectorsPerCluster,
							bytesPerSector,
							nFreeClusters,
							nTotalClusters;
	WCHAR                   volRoot[_MAX_PATH],
							fullpath[_MAX_PATH];
	int						len;
	UINT                    driveType;

	m_pathlen = 0;			// zero pathlen if early return from error
	if ( _wfullpath(fullpath, p_path, DIM(fullpath)) == NULL )
		return ERROR_BAD_PATHNAME;
	if ( wcsncmp(fullpath, L"\\\\", 2) )
	{
		// not a UNC, yet
		wcsncpy(volRoot, fullpath, 3);		// get first three chars, e.g., "C:\"
		volRoot[3] = L'\0';
		driveType = GetDriveType(volRoot);
		switch (driveType)
		{
		case DRIVE_REMOTE:					// network mapped drive
			m_isUNC = true;
			rc = WNetGetUniversalName(fullpath,
									REMOTE_NAME_INFO_LEVEL,
									(PVOID)&info,
									&sizeBuffer);
			switch (rc)
			{
			case 0:
				wcscpy(volRoot, info.i.lpConnectionName);
				// the \\server\share form is copied as UNC\server\share
				wcscpy(m_path, L"UNC");
				wcscpy(m_path + 3, info.i.lpUniversalName + 1);
				break;
			case ERROR_NOT_CONNECTED:
				wcscpy(m_path, fullpath);
				break;
			default:
				err.SysMsgWrite(34021, rc, L"WNetGetUniversalName(%s)=%ld ",
					fullpath, rc);
				m_path[0] = L'\0';
				return rc;
			}
			break;
		case DRIVE_UNKNOWN:                 // unknown drive
		case DRIVE_NO_ROOT_DIR:             // invalid root directory
			m_path[0] = L'\0';
			m_pathlen = 0;
			return ERROR_INVALID_DRIVE;
		default:
			wcscpy_s(m_path, fullpath);
		}
	}
	else
	{
		m_isUNC = true;
		driveType = DRIVE_REMOTE;
		wcscpy(m_path, L"UNC");         // Unicode form is \\?\UNC\server\share\...
		wcscpy(m_path + 3, fullpath + 1);
		len = ServerShareCopy(volRoot, fullpath);	// get \\server\share part of UNC
		if ( len < 0 )
			return ERROR_BAD_NETPATH;
	}

	len = (int)wcslen(volRoot);
	if ( volRoot[len - 1] != L'\\' )
		wcscpy(volRoot + len, L"\\");

	if ( !GetVolumeInformation( volRoot,
								m_volinfo.volName,
								DIM(m_volinfo.volName),
								&m_volinfo.volser,
								&maxCompLen,
								&m_volinfo.fsFlags,
								m_volinfo.fsName,
								DIM(m_volinfo.fsName)))
	{
		rc = GetLastError();
		err.SysMsgWrite(34022, rc, L"GetVolumeInformation(%s)=%ld ", volRoot, rc);
		return rc;
	}

	if ( !GetDiskFreeSpace( volRoot,
							&sectorsPerCluster,
							&bytesPerSector,
							&nFreeClusters,
							&nTotalClusters))
	{
		rc = GetLastError();
		err.SysMsgWrite(34023, rc, L"GetDiskFreeSpace(%s)=%ld ", volRoot, rc);
		return rc;
	}

	m_volinfo.cbCluster = bytesPerSector * sectorsPerCluster;
	m_volinfo.cbVolTotal = (__int64)nTotalClusters * m_volinfo.cbCluster;
	m_volinfo.cbVolFree = (__int64)nFreeClusters  * m_volinfo.cbCluster;
	m_pathlen = wcslen(m_path);
	if ( m_path[m_pathlen - 1] == L'\\' )
		m_path[--m_pathlen] = L'\0';		// strip off any trailing backslash
	return 0;
}


/// Tests for the existance of the (last) directory in m_path, set *p_direntry if there or NULL
PathResult DirList::PathExists(DirEntry ** p_direntry)	// ret-0=not exist, 1=exists, 2=error, 3=file
{
	DWORD						rc;
	wchar_t					  * p;
	HANDLE						hFind;
	WIN32_FIND_DATA				findData;		// result of Find*File API
	PathResult					ret = PathResult::PathNotExist;
	bool						bIsRoot = false;

	if ( m_pathlen == 2 && m_path[1] == L':' )		// is this a root path, e.g., C: or \\server\share
		bIsRoot = true;
	else if (m_isUNC)
	{
		int						slashcount;		// count of \ path separators
		m_isUNC = true;
		// UNC\server\share form.  server starts at apipath+4, assuming \\ beginning
		for (p = m_path + 4, slashcount = 2; *p; p++)
			if (*p == L'\\')
				slashcount++;
		if (slashcount == 3)
			bIsRoot = true;
	}

	if ( bIsRoot )							// roots are containers, but dont exist as directories
		wcscpy(m_path + m_pathlen, L"\\*");	// thus, need to suffix path with \* for FileFind

	hFind = FindFirstFile(m_apipath, &findData);
	if ( hFind == INVALID_HANDLE_VALUE )
		rc = GetLastError();
	else
	{
		rc = 0;
		FindClose(hFind);
	}

	m_path[m_pathlen] = L'\0';  // remove appended chars for root check

	if ( bIsRoot && (rc == 0 || rc == ERROR_NO_MORE_FILES) )	// the root can be empty
	{
		memset(&findData, 0, sizeof findData);
		findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		ret = PathYesDir;
	}
	else if ( rc == 0 )
	{
		if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			ret = PathYesDir;
		else
			ret = PathYesButFile;
	}

	Push();			// Push a new index level regardless so an invalid dir will enum 0 children
	if ( ret == PathYesDir )
		*p_direntry = DirEntryAdd(&findData);
	else
		*p_direntry = NULL;

	return ret;
}


/// fills the current level of the DirList with DirEntry objects enumerated from the file system path
DWORD DirList::ProcessCurrentPath()
{
//? Add stats parm and code
	HANDLE					hDir;
	WIN32_FIND_DATA			find;			// result of Find*File API
	DWORD					rc = 0;
	BOOL					bSuccess;
	int						pathlen = m_pathlen;
	DirEntry			  * preventry = NULL,
						  * currentry;
	bool					bSorted = true;

	m_stats->dirFound++;
	m_stats->dirFiltered++;						// dirs currently always filtered
	wcscpy(m_path + pathlen, L"\\*");			// suffix path with \* for FindFileFist
	// iterate through directory entries and add them as DirEntry objects
	for ( bSuccess = ((hDir = FindFirstFile(m_apipath, &find)) != INVALID_HANDLE_VALUE),
				m_path[pathlen] = L'\0';		// restore path -- remove \* suffix
		  bSuccess;
		  bSuccess = FindNextFile(hDir, &find) )
	{
		if ( find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY 
		  && (!wcscmp(find.cFileName, L".") || !wcscmp(find.cFileName, L"..")) )
			continue;							// ignore directory names '.' and '..'
		if ( find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			if ( DirFilterReject(find.cFileName) )
				continue;						// skip any directory matching any DirExclude spec
		}
		else
		{
			m_stats->fileFound.count++;
			m_stats->fileFound.bytes += INT64R(find.nFileSizeLow, find.nFileSizeHigh);
		}
		if ( FilterReject(find.cFileName, gOptions.include, gOptions.exclude) )
			continue;

		if ( !(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
		{
			m_stats->fileFiltered.count++;
			m_stats->fileFiltered.bytes += INT64R(find.nFileSizeLow, find.nFileSizeHigh);
		}
		currentry = DirEntryAdd(&find);
		if ( bSorted && preventry )				// bSorted indicates whether the file system returned items in canonical order
			bSorted = _wcsicmp(currentry->cFileName, preventry->cFileName) > 0;
		preventry = currentry;
	}
	rc = GetLastError();
	FindClose(hDir);
	if ( !bSorted )								// if items out of order, sort them
		IndexSort();
	if ( rc == 0 || rc == ERROR_NO_MORE_FILES )
		return 0;
	return rc;
}


/// Creates a new DirEntry and adds it to the next available DirEntryBlock
DirEntry * DirBlockCollection::DirEntryCreate(WIN32_FIND_DATA const * p_find)
{
	size_t			cbDirEntry = DirEntry::ByteLength(p_find);

	if ( cbDirEntry + m_nextavail >= DirBlocksize )
	{
		// if no room, set to next block.  If none, create/insert it 
		DirEntryBlock * newblock = (DirEntryBlock *)m_currblock->right;
		if ( newblock == NULL )
			Insert( newblock = new DirEntryBlock() );
		m_currblock = newblock;
		m_nextavail = 0;
	}
	DirEntry * direntry = m_currblock->GetDirEntry(m_nextavail);
	direntry->attrFile = p_find->dwFileAttributes;
	direntry->cbFile = INT64R(p_find->nFileSizeLow, p_find->nFileSizeHigh);
	direntry->ftimeLastWrite = p_find->ftLastWriteTime;
	wcscpy(direntry->cFileName, p_find->cFileName);
	m_nextavail += cbDirEntry;
	return direntry;
}

/// Constructor to enumerate current index level of p_dirlist
DirListEnum::DirListEnum(DirList * p_dirList)						// give the DirList to be enumerated for the current path
	: m_dirList(p_dirList), m_nCurrIndex(0) 
{
	m_dirList->Push(); 
}


/// Advance to next DirList* item in enumeration unless alread at EOL
bool DirListEnum::Advance()
{
	IndexLevel * indlevel = m_dirList->GetCurrIndexLevel();

	if ( m_nCurrIndex > indlevel->m_nEntries )
		return false;
	m_nCurrIndex++;
	return true;
}


bool DirListEnum::EOL() const								// end of list?
{
	IndexLevel * indlevel = m_dirList->GetCurrIndexLevel();

	if ( m_nCurrIndex < indlevel->m_nEntries )
		return false;
	return true;
}


/// returns the current DirEntry* and advances, or NULL if no more
DirEntry * DirListEnum::GetNext()
{
	IndexLevel * indlevel = m_dirList->GetCurrIndexLevel();
	if ( m_nCurrIndex >= indlevel->m_nEntries )
		return NULL;
	return indlevel->m_direntry[m_nCurrIndex++];
}


/// returns the current DirEntry* without advancing, or NULL if no more
DirEntry * DirListEnum::Peek()
{
	IndexLevel * indlevel = m_dirList->GetCurrIndexLevel();
	if ( m_nCurrIndex >= indlevel->m_nEntries )
		return NULL;
	return indlevel->m_direntry[m_nCurrIndex];
}
