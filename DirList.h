/// DirEntry objects are packed consecutively in the buffer.  Note the variable length
struct DirEntry              // directory entry in lifo buffer
{
	FILETIME				ftimeLastWrite;			// last written
	__int64					cbFile;					// size of file in bytes
	DWORD					attrFile;				// file/dir attribute
	WCHAR					cFileName[1];			// file/dir name (varying length at least 1)
	size_t		ByteLength() const					// returns byte length of this DirEntry 
					{ return sizeof *this + (wcslen(cFileName) * sizeof cFileName[0]); }
};

/// LevelHeader marks the start of each nested directory level.  Provides backchain info to pop off.
struct LevelHeader
{
	size_t					parentLevelOffset;		// offset within buffer or previous level or zero for first
	size_t					cbDirEntries;			// byte length of all DirEntry objects at this level
	size_t					indexOffset;			// buffer offset of DirEntry offset index
	int						nDirEntry;				// count of DirEntry objects within this level
};

/*
DirList performs directory enumeration for a path.  It holds the DirEntry objects (names, attributes)
(both files and directores) in the active nested path for a recursive
merge-match process to detect differences.  For example, take the following directory structure:
A
  AA
    AAA
      AAAA
      AAAB
    AAB
  AB
  AC
B
  BA
  BB
  BC
C
  CA
  CB
This is what have in the buffer when the active path is A/AA/AAA
A B C - AA AB AC - AAA AAB - AAAA AAAB

Each level will contain:
- A LevelHeader object
- Zero or more DirEntry objects (LevelHeder.nDirEntry)
- Zero or more DirEntryOffset index objects (buffer offset of each DirEntry for sort/merge)

NOTE: because the buffer address can change when it grows, great care should be taken in computing
pointers from offsets to buffer.  They generally must be recomputed from the current m_buffer base
each time unless you know it won't grow in between and thus won't possibly change.
*/
class DirList
{
	friend class DirListEnum;					// used for enumeration of DirEntry objects at a dir level
	static const size_t			BUFFER_GROW = (1 << 14) * sizeof(size_t);	// buffer size increment
	BYTE					  * m_buffer;		// MOVEABLE lifo buffer for nested headers, namespaces and indexes
	size_t						m_LevelHeaderOffset;	// Transient current recursive level of LevelHeader object
	size_t						m_cbBuffer;		// current buffer size in bytes
	size_t						m_cbUsed;		// bytes currently used in buffer
	wchar_t					  * m_path;			// current path being processed

	void	GrowBuffer();						// Expand the existing m_buffer size
	bool	PathProcess(size_t p_parentLevel, int p_pathlen);
	size_t	DirPush(size_t p_levelOffset);		// Pushes a new LevelHeader on top of the lifo buffer stack
	void	DirPop(size_t p_levelHeaderOffset);	// Pops off the top LevelHeader from the lifo buffer stack
	void	DirEntryAdd(WIN32_FIND_DATA const * p_find);	// Adds a DirEntry to the current level
	void	CreateIndex();						// Creates DirEntry pointer index for sort
	wchar_t const * GetFilename(size_t p_dirEntryOffset) const // returns filename from a DirEntryOffset
				{ return ((DirEntry *)(m_buffer + p_dirEntryOffset))->cFileName; }
	~DirList() { free(m_buffer); }
public:
	DirList(wchar_t * p_path);
	size_t	DirEntryLength(size_t p_offset) const { return ((DirEntry *)(m_buffer + p_offset))->ByteLength(); }
	DirEntry const * GetDirEntry(size_t p_offset) const { return (DirEntry *)(m_buffer + p_offset); }
};


/// Enumerator for DirEntry objects in DirList
class DirListEnum
{
	DirList		  * m_dirList;
	size_t const	m_levelOffset;
	int				m_nCurrIndex;
public:
	DirListEnum(DirList * p_dirList, size_t p_levelOffset)
		: m_dirList(p_dirList), m_levelOffset(p_levelOffset), m_nCurrIndex(0) {};
	DirEntry const * GetNext();
};

/// returns the next DirEntry in this level or nullptr when past end
DirEntry const * DirListEnum::GetNext()
{
	// recompute the address each time because the buffer address can change as it grows
	LevelHeader const * levelHeader = (LevelHeader *)(m_dirList->m_buffer + m_dirList->m_LevelHeaderOffset);
	if ( m_nCurrIndex > levelHeader->nDirEntry )
		return nullptr;
	size_t const * index = (size_t *)(m_dirList->m_buffer + levelHeader->indexOffset);
	size_t entryOffset = index[m_nCurrIndex++];
	return (DirEntry *)(m_dirList->m_buffer + entryOffset);
}
