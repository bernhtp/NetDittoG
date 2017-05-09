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


DirList::DirList()
	: m_path(L""), m_pathlen(0), m_currindex(-1)
{
	m_indexsize = INDEX_INCREMENT;
	m_index = (byte *)malloc(m_indexsize);
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
	m_pathlen += wcslen(p_name) + 1;
	wcscpy_s(m_path + 1, MAX_PATHX - m_pathlen - 3, p_name);
	return m_pathlen;
}


/// Push new IndexHeader on top of m_index stack
void DirList::Push()
{
	IndexLevel		  * currheader = NULL;
	DirEntryBlock	  * prevblock = NULL;
	size_t				prevoffset = -1;
	short				prevdepth = -1;

	if ( m_currindex >= 0 )
	{
		// if this isn't the first/root
		currheader = (IndexLevel *)(m_index + m_currindex);
		prevoffset = currheader->m_prevheader;
		prevdepth = currheader->m_depth;
	}
	if ( IndexUsed() + sizeof(IndexLevel) >= m_indexsize )
		IndexGrow();
	// create new IndexLevel on stack
	m_currindex = IndexUsed();
	currheader = GetCurrIndexLevel();
	currheader->Initialize();
	currheader->m_initblock = m_dirblocks.GetCurrBlock();
	currheader->m_initoffset = m_dirblocks.GetNextAvail();
	currheader->m_prevheader = prevoffset;
	currheader->m_depth = prevdepth + 1;
}

/// Pop to the previous index, dirblock, diroffset and path
void DirList::Pop()
{
	IndexLevel * currheader = (IndexLevel *)(m_index + m_currindex);
	IndexLevel * prevheader = (IndexLevel *)(m_index + currheader->m_prevheader);
	m_path[prevheader->m_pathlen] = L'\0';
	m_currindex = currheader->m_prevheader;
	m_dirblocks.Popto(currheader->m_initblock, currheader->m_initoffset);
}


/// Adds an entry into the current index level for a DirEntry *
void DirList::IndexEntryAdd(DirEntry * p_direntry)
{
	if ( IndexUsed() + sizeof p_direntry >= m_indexsize )
		IndexGrow();
	IndexLevel * indexlevel = GetCurrIndexLevel();
	indexlevel->m_direntry[indexlevel->m_nEntries] = p_direntry;
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
							fullPath[_MAX_PATH];
	int						len;
	UINT                    driveType;

	m_pathlen = 0;			// zero pathlen if early return from error
	if ( _wfullpath(fullPath, p_path, DIM(fullPath)) == NULL )
		return ERROR_BAD_PATHNAME;
	if ( wcsncmp(fullPath, L"\\\\", 2) )
	{
		// not a UNC
		wcsncpy(volRoot, p_path, 3);		// get first three chars, e.g., "c:\"
		volRoot[3] = L'\0';
		driveType = GetDriveType(volRoot);
		switch (driveType)
		{
		case DRIVE_REMOTE:
			m_isUNC = true;
			rc = WNetGetUniversalName(fullPath,
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
				wcscpy(m_path, fullPath);
				break;
			default:
				err.SysMsgWrite(34021, rc, L"WNetGetUniversalName(%s)=%ld ",
					fullPath, rc);
				m_path[0] = L'\0';
				return rc;
			}
			break;
		case 0:                          // unknown drive
		case 1:                          // invalid root directory
			m_path[0] = L'\0';
			return ERROR_INVALID_DRIVE;
		default:
			wcscpy_s(m_path, fullPath);
		}
	}
	else
	{
		m_isUNC = true;
		driveType = DRIVE_REMOTE;
		wcscpy(m_path, L"UNC");         // Unicode form is \\?\UNC\server\share\...
		wcscpy(m_path + 3, fullPath + 1);
		len = ServerShareCopy(volRoot, fullPath);
		if ( len < 0 )
			return ERROR_BAD_NETPATH;
	}

	// get \\server\share part of UNC
	len = (int)wcslen(volRoot);
	if ( volRoot[len - 1] != L'\\' )
		wcscpy(volRoot + len, L"\\");

	if ( !GetVolumeInformation(volRoot,
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

	if ( !GetDiskFreeSpace(volRoot,
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
	return 0;
}


/// Tests for the existance of the (last) directory in m_path
PathExistsResult DirList::PathDirExists(DirEntry ** p_direntry)	// ret-0=not exist, 1=exists, 2=error, 3=file
{
	DWORD						rc;
	wchar_t					  * p;
	HANDLE						hFind;
	WIN32_FIND_DATA				findData;		// result of Find*File API
	PathExistsResult			ret = PathExistsResult::NotExist;

	m_isUNC = false;
	if ( !wcsncmp(m_path, L"UNC\\", 4) )		// if UNC form, must append * to path
	{
		int							slashcount;		// count of \ path separators
		// UNC\server\share form.  server starts at apipath+4
		m_isUNC = true;
		for ( p = m_path + 4, slashcount = 2;  *p;  p++ )
			if ( *p == L'\\' )
				slashcount++;
		if ( p[-1] == L'\\' )		// if UNC with just share suffixed with backslash
			if ( slashcount == 4 )
			{
				m_pathlen = p - m_path - 1;
				wcscpy(p, L"*");
			}
			else
				m_pathlen = p - m_path;
		else
		{
			if ( slashcount == 3 )			// if UNC with just share without trailing backslash
				wcscpy(p, L"\\*");
			m_pathlen = p - m_path;
		}
	}
	else
	{
		m_pathlen = wcslen(m_path);
		if ( m_path[m_pathlen - 1] == L'\\' )	// fix path format by removing any trailing backslash
			m_pathlen--;
	}

	hFind = FindFirstFile(m_apipath, &findData);
	if ( hFind == INVALID_HANDLE_VALUE )
		rc = GetLastError();
	else
	{
		rc = 0;
		FindClose(hFind);
	}

	m_path[m_pathlen] = L'\0';  // remove any appended chars

	switch (rc)
	{
	case ERROR_NO_MORE_FILES:        // root with no dirs/files, not even . or ..
		findData.cFileName[0] = L'\0';
	case 0:
		if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			Push();
			*p_direntry = DirEntryAdd(&findData);
			ret = PathExistsResult::YesDir;
		}
		else
		{
			*p_direntry = NULL;
			ret = PathExistsResult::YesButFile;
		}
		break;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		*p_direntry = NULL;
		// check to see if root directory or root to UNC path because this
		// will fail without a terminating \* so we need to check it again
		if ( m_isUNC || (m_pathlen == 2 && m_path[1] == L':') )
		{
			wcscpy(m_path + m_pathlen, L"\\*");
			hFind = FindFirstFile(m_apipath, &findData);
			m_path[m_pathlen] = L'\0';
			if ( hFind == INVALID_HANDLE_VALUE )
				ret = PathExistsResult::NotExist;
			else
			{
				FindClose(hFind);
				findData.cFileName[0] = L'\0';
				findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				*p_direntry = DirEntryCreate(&findData);
				ret = PathExistsResult::YesDir;
			}
		}
		else
			ret = PathExistsResult::NotExist;
		break;
	default:
		err.SysMsgWrite(50011, rc, L"FindFirstFile(%s)=%ld, ", m_path, rc);
	}
	return ret;
}


/// fills the current level of the DirList with FileEntry objects enumerated from the file system path
DWORD DirList::ProcessCurrentPath()
{
//? Add stats parm and code
	HANDLE					hDir;
	WIN32_FIND_DATA			findEntry;			// result of Find*File API
	DWORD					rc = 0;
	BOOL					bSuccess;
	int						pathlen = m_pathlen;

	wcscpy(m_path + pathlen, L"\\*");			// suffix path with \* for FindFileFist
	// iterate through directory entries and add them as DirEntry objects
	for ( bSuccess = ((hDir = FindFirstFile(m_apipath, &findEntry)) != INVALID_HANDLE_VALUE),
				m_path[pathlen] = L'\0';		// restore path -- remove \* suffix
		  bSuccess;
		  bSuccess = FindNextFile(hDir, &findEntry) )
	{
		if ( !wcscmp(findEntry.cFileName, L".") || !wcscmp(findEntry.cFileName, L"..") )
			continue;							// ignore directory names '.' and '..'
		if ( FilterReject(findEntry.cFileName, gOptions.include, gOptions.exclude) )
			continue;
		DirEntryAdd(&findEntry);
	}
	rc = GetLastError();
	FindClose(hDir);
	return rc == ERROR_NO_MORE_FILES ? 0 : rc;
}


/// Creates a new DirEntry and adds it to the next available DirEntryBlock
DirEntry * DirBlockCollection::DirEntryAdd(WIN32_FIND_DATA const * p_find)
{
	size_t			cbFilename = WcsByteLen(p_find->cFileName);

	if ( cbFilename + m_nextavail >= DirBlocksize )
	{
		// if no room, see if set to next block.  If none, create it and end of list
		DirEntryBlock * newblock = (DirEntryBlock *)m_currblock->right;
		if ( newblock == NULL )
			Insert( newblock = new DirEntryBlock() );
		m_currblock = newblock;
		m_nextavail = 0;
	}
	DirEntry * de = m_currblock->GetDirEntry(m_nextavail);
	de->attrFile = p_find->dwFileAttributes;
	de->cbFile = INT64R(p_find->nFileSizeLow, p_find->nFileSizeHigh);
	de->ftimeLastWrite = p_find->ftLastWriteTime;
	memcpy(de->cFileName, p_find->cFileName, cbFilename);
	m_nextavail += m_nextavail;
	return de;
}


/// returns the current DirEntry* and advances, or NULL if no more
DirEntry * DirListEnum::GetNext()
{
	IndexLevel * i = m_dirList->GetCurrIndexLevel();
	return m_nCurrIndex >= i->m_nEntries ? NULL : i->m_direntry[m_nCurrIndex++];
}


/// returns the current DirEntry* without advancing, or NULL if no more
DirEntry * DirListEnum::Peek()
{
	IndexLevel * i = m_dirList->GetCurrIndexLevel();
	return m_nCurrIndex >= i->m_nEntries ? NULL : i->m_direntry[m_nCurrIndex];
}
