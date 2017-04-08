#include <windows.h>

#include "DirList.h"

DirList::DirList(wchar_t * p_path)
{
	m_cbBuffer = BUFFER_GROW;
	m_buffer = (BYTE *)malloc(m_cbBuffer);
	m_cbUsed = 0;
}

size_t DirList::DirPush(size_t p_parentLevelOffset)
{
	m_LevelHeaderOffset = m_cbUsed;
	LevelHeader * levelHeader = (LevelHeader *)(m_buffer + m_LevelHeaderOffset);
	m_cbUsed += sizeof *levelHeader;
	levelHeader->parentLevelOffset = p_parentLevelOffset;
	levelHeader->cbDirEntries = 0;
	levelHeader->indexOffset = 0;
	levelHeader->nDirEntry = 0;
	return m_LevelHeaderOffset;
}

/// Pop up one directory level
void DirList::DirPop(size_t p_levelHeaderOffset)
{
	LevelHeader * levelHeader = (LevelHeader *)(m_buffer + p_levelHeaderOffset);

	m_cbUsed = p_levelHeaderOffset;
	m_LevelHeaderOffset = levelHeader->parentLevelOffset;
}

/// Expand the existing buffer
void DirList::GrowBuffer()
{
	m_cbBuffer += BUFFER_GROW;
	BYTE * newbuffer = (BYTE *)realloc(m_buffer, m_cbBuffer);
	// if the realloc gets a new address, copy the old contents to the new block
	if ( newbuffer != m_buffer)
	{
		memcpy(newbuffer, m_buffer, m_cbUsed);
		m_buffer = newbuffer;
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
	DirEntry	  * dirEntry = (DirEntry *)(m_buffer + m_cbUsed);

	if ( m_cbBuffer < m_cbUsed + sizeof *dirEntry + cbFilename + sizeof (LevelHeader))
		GrowBuffer();
	m_cbUsed += m_cbBuffer + sizeof *dirEntry + cbFilename;
	dirEntry->attrFile = p_find->dwFileAttributes;
	dirEntry->cbFile = (__int64)p_find->nFileSizeHigh << 32 + p_find->nFileSizeLow;
	dirEntry->ftimeLastWrite = p_find->ftLastWriteTime;
	memcpy(dirEntry->cFileName, p_find->cFileName, cbFilename + sizeof dirEntry->cFileName);
	LevelHeader * levelHeader = (LevelHeader *)(m_buffer + m_LevelHeaderOffset);
	levelHeader->cbDirEntries++;
}

/// Creates DirEntry pointer index for sort
void DirList::CreateIndex()
{
	LevelHeader * levelHeader = (LevelHeader *)(m_buffer + m_LevelHeaderOffset);
	size_t	indexOffset = (m_cbUsed + 7) & 7;	// align index on 8-byte offset multiple after used buffer
	size_t	new_cbUsed = m_cbUsed + levelHeader->nDirEntry * sizeof indexOffset;
	bool	sorted = true;

	if ( m_cbBuffer < new_cbUsed + sizeof *levelHeader )
	{
		GrowBuffer();
		levelHeader = (LevelHeader *)(m_buffer + m_LevelHeaderOffset);	// may have changed
	}
	m_cbUsed = new_cbUsed;

	levelHeader->indexOffset = indexOffset;
	size_t entryOffset = m_LevelHeaderOffset + sizeof(LevelHeader);
	size_t * index = (size_t *)(m_buffer + indexOffset);
	for ( size_t * i = index ;  i++;  i < index + levelHeader->nDirEntry )
	{
		*i = entryOffset;
		entryOffset += DirEntryLength(entryOffset);
		if ( sorted  &&  i < index + levelHeader->nDirEntry - 1)
			sorted = _wcsicmp(GetFilename(*i), GetFilename(entryOffset)) == 0;
	}
	if ( !sorted )
	{
		qsort_s(index, levelHeader->nDirEntry, sizeof *index, SortCompare, m_buffer);
	}
}

/// fills the current level of the DirList with FileEntry objects enumerated from the file system path
bool DirList::PathProcess(size_t p_parentLevel, int p_pathlen)
{
	HANDLE					hDir;
	WIN32_FIND_DATA			findEntry;			// result of Find*File API
	DWORD					rc = 0;
	BOOL					bRc;
	size_t					levelOffset = DirPush(p_parentLevel);

	wcscpy(m_path + p_pathlen, L"\\*");			// set path to include wildcard for Find*File
												// iterate through directory entries and stuff them in DirBuffer
	for ( bRc = ((hDir = FindFirstFile(m_path, &findEntry)) != INVALID_HANDLE_VALUE),
				m_path[p_pathlen] = L'\0';		// restore path -- remove \* -
		bRc;
		bRc = FindNextFile(hDir, &findEntry) )
	{
		if ( wcscmp(findEntry.cFileName, L".") || (findEntry.cFileName, L"..") )
			continue;							// ignore directory names '.' and '..'
		// add filtering code
		DirEntryAdd(&findEntry);
	}
	rc = GetLastError();
	return rc == ERROR_NO_MORE_FILES;
}
