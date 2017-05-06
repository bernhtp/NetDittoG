#include <windows.h>

#include "NetDitto.hpp"


/// copies the \\server\share portion of a UNC path to tgtPath and returns the length
static int                               // ret-length copied or -1 if error
ServerShareCopy(
	WCHAR                * tgtPath		,// out-target \\server\share path
	WCHAR const          * uncPath       // in -UNC path after \\?\ prefix
)
{
	WCHAR const             * c;
	int                       nSlash = 0;
	WCHAR                   * t = tgtPath;

	// iterate through the string stopping at the fourth backslash or '\0'
	for ( c = uncPath;  *c;  c++ )
	{
		if ( *c == L'\\' )
		{
			nSlash++;
			if ( nSlash > 3 )
				break;
		}
		*t++ = *c;
	}

	*t = L'\0';

	// If UNC but not at least sharename separater blackslash, error
	if ( nSlash < 3 )
		return -1;

	return (int)(c - uncPath);
}


DirList::DirList()
	: m_apipath({ L'\\',L'\\',L'?',L'\\'}), m_path(L"")
{
	m_cbBuffer = BUFFER_GROW;
	m_index = (BYTE *)malloc(m_cbBuffer);
	m_cbUsed = 0;
	m_pathlen = 0;
}

/// Pushes a new LevelHeader on top of the lifo buffer stack and returns its offset
size_t DirList::DirPush()
{
	LevelHeader * levelHeader = (LevelHeader *)(m_index + m_cbUsed);
	m_cbUsed += sizeof *levelHeader;
	levelHeader->parentLevelOffset = m_LevelHeaderOffset;
	m_LevelHeaderOffset = m_cbUsed;
	levelHeader->cbDirEntries = 0;
	levelHeader->indexOffset = 0;
	levelHeader->nDirEntry = 0;
	return m_LevelHeaderOffset;
}

/// Pops off the top LevelHeader from the lifo buffer stack
void DirList::DirPop(size_t p_levelHeaderOffset)
{
	LevelHeader * levelHeader = (LevelHeader *)(m_index + p_levelHeaderOffset);

	m_cbUsed = p_levelHeaderOffset;
	m_LevelHeaderOffset = levelHeader->parentLevelOffset;
}

/// Expand the existing buffer
void DirList::GrowBuffer()
{
	m_cbBuffer += BUFFER_GROW;
	BYTE * newbuffer = (BYTE *)realloc(m_index, m_cbBuffer);
	// if the realloc gets a new address, copy the old contents to the new block
	if ( newbuffer != m_index)
	{
		memcpy(newbuffer, m_index, m_cbUsed);
		m_index = newbuffer;
	}
}

/// compare function for qsort_s to esnure that file lists or ordered consistently for match/merge
static int SortCompare(void * p_context, const void * p_a, const void * p_b)
{
	BYTE		* buffer = (BYTE *)p_context;
	DirEntry	* a = (DirEntry *)(buffer + *(size_t *)p_a);
	DirEntry	* b = (DirEntry *)(buffer + *(size_t *)p_b);
	return _wcsicmp(a->cFileName, b->cFileName);
}

/// Adds FILE_FIND_DATA entry to the buffer as a DirEntry
void DirList::DirEntryAdd(WIN32_FIND_DATA const * p_find)
{
	size_t			cbFilename = wcslen(p_find->cFileName) * sizeof p_find->cFileName[0];
	DirEntry	  * dirEntry = (DirEntry *)(m_index + m_cbUsed);

	if ( m_cbBuffer < m_cbUsed + sizeof *dirEntry + cbFilename + sizeof (LevelHeader))
		GrowBuffer();
	m_cbUsed += m_cbBuffer + sizeof *dirEntry + cbFilename;
	dirEntry->attrFile = p_find->dwFileAttributes;
	dirEntry->cbFile = (__int64)p_find->nFileSizeHigh << 32 + p_find->nFileSizeLow;
	dirEntry->ftimeLastWrite = p_find->ftLastWriteTime;
	memcpy(dirEntry->cFileName, p_find->cFileName, cbFilename + sizeof dirEntry->cFileName);
	LevelHeader * levelHeader = (LevelHeader *)(m_index + m_LevelHeaderOffset);
	levelHeader->cbDirEntries++;
}

/// Creates DirEntry pointer index for sort and enum
void DirList::CreateIndex()
{
	LevelHeader	  * levelHeader = (LevelHeader *)(m_index + m_LevelHeaderOffset);
	size_t			indexOffset = (m_cbUsed + 7) & 7;	// align index on 8-byte offset multiple after used buffer
	size_t			new_cbUsed = m_cbUsed + levelHeader->nDirEntry * sizeof indexOffset;
	bool			sorted = true;

	if ( m_cbBuffer < new_cbUsed + sizeof *levelHeader )
	{
		GrowBuffer();
		levelHeader = (LevelHeader *)(m_index + m_LevelHeaderOffset);	// may have changed
	}
	m_cbUsed = new_cbUsed;

	levelHeader->indexOffset = indexOffset;
	size_t		entryOffset = m_LevelHeaderOffset + sizeof *levelHeader;
	size_t	  * index = (size_t *)(m_index + indexOffset);
	for ( size_t * i = index ;  i++;  i < index + levelHeader->nDirEntry )
	{
		*i = entryOffset;
		entryOffset += DirEntryLength(entryOffset);
		if ( sorted  &&  i > index )
			sorted = _wcsicmp(GetFilename(*i), GetFilename(*(i-1))) == 0;
	}
	if ( !sorted )
	{
		qsort_s(index, levelHeader->nDirEntry, sizeof *index, SortCompare, m_index);
	}
}

/// fills the current level of the DirList with FileEntry objects enumerated from the file system path
DWORD DirList::ProcessCurrentPath()
{
	HANDLE					hDir;
	WIN32_FIND_DATA			findEntry;			// result of Find*File API
	DWORD					rc = 0;
	BOOL					bRc;
	size_t					levelOffset = DirPush();
	int						pathlen = wcslen(m_path);

	wcscpy_s(m_path + pathlen, MAX_PATHX - pathlen, L"\\*");	// suffix path with \* for FindFileFist
	// iterate through directory entries and stuff them in DirBuffer
	for ( bRc = ((hDir = FindFirstFile(m_apipath, &findEntry)) != INVALID_HANDLE_VALUE),
				m_path[pathlen] = L'\0';		// restore path -- remove \* suffix
		bRc;
		bRc = FindNextFile(hDir, &findEntry) )
	{
		if ( wcscmp(findEntry.cFileName, L".") || wcscmp(findEntry.cFileName, L"..") )
			continue;							// ignore directory names '.' and '..'
		//?? add filtering code
		DirEntryAdd(&findEntry);
	}
	rc = GetLastError();
	FindClose(hDir);
	return rc == ERROR_NO_MORE_FILES ? 0 : rc;
}

/// append an element to m_path and return the result length
int	DirList::PathAppend(wchar_t const * p_pathelement)
{
	m_path[m_pathlen] = L'\\';
	wcscpy_s(m_path+m_pathlen+1, MAX_PATHX-m_pathlen-1, p_pathelement);
	m_pathlen += wcslen(p_pathelement) + 1;
	return m_pathlen;
}

/// Tests and Normalizes p_path before setting m_path.  Returns 0 or rc.
/// Sets volume information and optionally makes the dir if not exist
DWORD DirList::NormalizeAndSetPath(
	wchar_t const	  * p_path		,// in -raw path spec to be normalized and tested 
	bool				bMakeDir	)// in -true: make the directory if it doesn't exist
{
	union
	{
		REMOTE_NAME_INFO    i;
		WCHAR               x[_MAX_PATH + 1];
	}                       info;
	wchar_t					path[_MAX_PATH];
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
		if (len < 0)
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
			DIM(m_volinfo.fsName)) )
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
	return 0;
}


/// Tests for the existance of the (last) directory in m_path
PathExistsResult DirList::PathDirExists()	// ret-0=not exist, 1=exists, 2=error, 3=file
{
	DWORD						rc;
	size_t						len;
	wchar_t					  * p;
	HANDLE						hFind;
	WIN32_FIND_DATA				findData;		// result of Find*File API
	PathExistsResult			ret = PathExistsResult::NotExist;

	m_isUNC = false;
	if ( !wcsncmp(m_path, L"UNC\\", 4) )	// if UNC form, must append * to path
	{
		// UNC\server\share form.  server starts at apipath+4
		m_isUNC = true;
		for ( p = m_path + 4, len = 2; *p; p++ )
			if ( *p == L'\\' )
				len++;
		if ( p[-1] == L'\\' )		// if UNC with just share suffixed with backslash
			if ( len == 4 )
			{
				len = p - m_path - 1;
				wcscpy(p, L"*");
			}
			else
				len = p - m_path;
		else
		{
			if ( len == 3 )			// if UNC with just share without trailing backslash
				wcscpy(p, L"\\*");
			len = p - m_path;
		}
	}
	else
	{
		len = wcslen(m_path);
		if ( m_path[len - 1] == L'\\' )	// fix path format by removing any trailing backslash
			len--;
	}

	hFind = FindFirstFile(m_apipath, &findData);
	if ( hFind == INVALID_HANDLE_VALUE )
		rc = GetLastError();
	else
	{
		rc = 0;
		FindClose(hFind);
	}
	m_path[len] = L'\0';  // remove any appended chars

	switch (rc)
	{
	case ERROR_NO_MORE_FILES:        // root with no dirs/files, not even . or ..
		findData.cFileName[0] = L'\0';
	case 0:
		if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			*dirEntry = DirEntryCreate(&findData);
			ret = PathExistsResult::YesDir;
		}
		else
		{
			*dirEntry = NULL;
			ret = PathExistsResult::YesButFile;
		}
		break;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		*dirEntry = NULL;
		// check to see if root directory or root to UNC path because this
		// will fail without a terminating \* so we need to check it again
		if ( m_isUNC || ( len == 2 && m_path[1] == L':') )
		{
			wcscpy(m_path + len, L"\\*");
			hFind = FindFirstFile(m_apipath, &findData);
			m_path[len] = L'\0';
			if ( hFind == INVALID_HANDLE_VALUE )
				ret = PathExistsResult::NotExist;
			else
			{
				FindClose(hFind);
				findData.cFileName[0] = L'\0';
				findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				*dirEntry = DirEntryCreate(&findData);
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


/// returns the next DirEntry in this level or nullptr when past end
DirEntry * DirListEnum::GetNext()
{
	// recompute the address each time because the buffer address can change as it grows
	LevelHeader const * levelHeader = (LevelHeader *)(m_dirList->m_index + m_dirList->m_LevelHeaderOffset);
	if ( m_nCurrIndex > levelHeader->nDirEntry )
		return nullptr;
	size_t const * index = (size_t *)(m_dirList->m_index + levelHeader->indexOffset);
	size_t entryOffset = index[m_nCurrIndex++];
	return (DirEntry *)(m_dirList->m_index + entryOffset);
}



//=========================================================================================================================

/// Initializes IndexHeader
IndexLevel * IndexLevel::Initialize()
{
	marker1 = -1;
	marker2 = 0xeeeeeeeeeeeeeeee;
	m_nEntries = 0;
}


DirList2::DirList2()
	: m_apipath({ L'\\',L'\\',L'?',L'\\' }), m_path(L""), m_pathlen(0), m_indexnext(0), m_currindex(-1)
{
	m_indexsize = INDEX_INCREMENT;
	m_index = (byte *)malloc(m_indexsize);
}


/// returns the indicated index entry value, NULL if invalid
DirEntry * DirList2::GetIndexDirEntry(int p_n) 
{
	IndexLevel * i = GetCurrIndexLevel();
	return p_n >= i->m_nEntries ? NULL : i->m_direntry[p_n];
}


/// expand the index size when needed
void DirList2::IndexGrow()
{
	m_indexsize += INDEX_INCREMENT;
	m_index = (byte *)realloc(m_index, m_indexsize);
}


/// Push new IndexHeader on top of m_index stack
void DirList2::Push()
{
	IndexLevel		  * currheader = NULL;
	DirEntryBlock	  * prevblock = NULL;
	size_t				prevoffset = -1;
	short				prevdepth = -1;

	if ( m_currindex >= -1 )
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
void DirList2::Pop()
{
	IndexLevel * currheader = (IndexLevel *)(m_index + m_currindex);
	IndexLevel * prevheader = (IndexLevel *)(m_index + currheader->m_prevheader);
	m_path[prevheader->m_pathlen] = L'\0';
	m_currindex = currheader->m_prevheader;
	m_dirblocks.Popto(currheader->m_initblock, currheader->m_initoffset);
}


/// Adds an entry into the current index level for a DirEntry *
void DirList2::IndexEntryAdd(DirEntry * p_direntry)
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
void DirList2::IndexSort()
{
	IndexLevel * currindex = GetCurrIndexLevel();
	qsort(currindex->m_direntry, currindex->m_nEntries, sizeof (DirEntry *), QSortCompare);
}


/// Adds a WIN32-FIND_DATA entry as a DirEntry
void DirList2::DirEntryAdd(WIN32_FIND_DATA const * p_find)
{
	IndexEntryAdd(m_dirblocks.DirEntryAdd(p_find));
}


/// Tests and Normalizes p_path before setting m_path.  Returns 0 or rc.
/// Sets volume information and optionally makes the dir if not exist
DWORD DirList2::NormalizeAndSetPath(
	wchar_t const	  * p_path		,// in -raw path spec to be normalized and tested 
	bool				bMakeDir	)// in -true: make the directory if it doesn't exist
{
	union
	{
		REMOTE_NAME_INFO    i;
		WCHAR               x[_MAX_PATH + 1];
	}                       info;
	wchar_t					path[_MAX_PATH];
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
	return 0;
}


/// Tests for the existance of the (last) directory in m_path
PathExistsResult DirList2::PathDirExists()	// ret-0=not exist, 1=exists, 2=error, 3=file
{
	DWORD						rc;
	size_t						len;
	wchar_t					  * p;
	HANDLE						hFind;
	WIN32_FIND_DATA				findData;		// result of Find*File API
	PathExistsResult			ret = PathExistsResult::NotExist;

	m_isUNC = false;
	if ( !wcsncmp(m_path, L"UNC\\", 4) )		// if UNC form, must append * to path
	{
		// UNC\server\share form.  server starts at apipath+4
		m_isUNC = true;
		for ( p = m_path + 4, len = 2; *p; p++ )
			if ( *p == L'\\' )
				len++;
		if ( p[-1] == L'\\' )		// if UNC with just share suffixed with backslash
			if ( len == 4 )
			{
				len = p - m_path - 1;
				wcscpy(p, L"*");
			}
			else
				len = p - m_path;
		else
		{
			if ( len == 3 )			// if UNC with just share without trailing backslash
				wcscpy(p, L"\\*");
			len = p - m_path;
		}
	}
	else
	{
		len = wcslen(m_path);
		if ( m_path[len - 1] == L'\\' )	// fix path format by removing any trailing backslash
			len--;
	}

	hFind = FindFirstFile(m_apipath, &findData);
	if ( hFind == INVALID_HANDLE_VALUE )
		rc = GetLastError();
	else
	{
		rc = 0;
		FindClose(hFind);
	}
	m_path[len] = L'\0';  // remove any appended chars

	switch (rc)
	{
	case ERROR_NO_MORE_FILES:        // root with no dirs/files, not even . or ..
		findData.cFileName[0] = L'\0';
	case 0:
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			*dirEntry = DirEntryCreate(&findData);
			ret = PathExistsResult::YesDir;
		}
		else
		{
			*dirEntry = NULL;
			ret = PathExistsResult::YesButFile;
		}
		break;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		*dirEntry = NULL;
		// check to see if root directory or root to UNC path because this
		// will fail without a terminating \* so we need to check it again
		if ( m_isUNC || (len == 2 && m_path[1] == L':') )
		{
			wcscpy(m_path + len, L"\\*");
			hFind = FindFirstFile(m_apipath, &findData);
			m_path[len] = L'\0';
			if ( hFind == INVALID_HANDLE_VALUE )
				ret = PathExistsResult::NotExist;
			else
			{
				FindClose(hFind);
				findData.cFileName[0] = L'\0';
				findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				*dirEntry = DirEntryCreate(&findData);
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
DWORD DirList2::ProcessCurrentPath()
{
	HANDLE					hDir;
	WIN32_FIND_DATA			findEntry;			// result of Find*File API
	DWORD					rc = 0;
	BOOL					bRc;
	int						pathlen = wcslen(m_path);

	Push();
	wcscpy_s(m_path + pathlen, MAX_PATHX - pathlen, L"\\*");	// suffix path with \* for FindFileFist
																// iterate through directory entries and stuff them in DirBuffer
	for ( bRc = ((hDir = FindFirstFile(m_apipath, &findEntry)) != INVALID_HANDLE_VALUE),
				m_path[pathlen] = L'\0';		// restore path -- remove \* suffix
			bRc;
			bRc = FindNextFile(hDir, &findEntry) )
	{
		if ( wcscmp(findEntry.cFileName, L".") || wcscmp(findEntry.cFileName, L"..") )
			continue;							// ignore directory names '.' and '..'
												//?? add filtering code
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
	de->cbFile = (__int64)p_find->nFileSizeHigh << 32 + p_find->nFileSizeLow;
	de->ftimeLastWrite = p_find->ftLastWriteTime;
	memcpy(de->cFileName, p_find->cFileName, cbFilename);
	m_nextavail += m_nextavail;
}


/// returns the next DirEntry* or NULL if no more
DirEntry * DirListEnum2::GetNext()
{
	IndexLevel * i = m_dirList->GetCurrIndexLevel();
	return m_nCurrIndex >= i->m_nEntries ? NULL : i->m_direntry[m_nCurrIndex++];
}
