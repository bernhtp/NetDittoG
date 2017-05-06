const int MAX_PATHX = 32764;	// max path char length exluding \\?\ long-path prefix	
enum PathExistsResult { NotExist, YesDir, ErrorNo, YesButFile };	// return states for PathDirExists

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

/// DirEntryMax accommodates the largest possible size DirEntry
struct DirEntryMax
{
	DirEntry		de;
	wchar_t			rest[_MAX_PATH];

	DirEntryMax(WIN32_FIND_DATA const * find);
	DirEntry *		GetDirEntry() { return (DirEntry *)this; }
};

struct VolInfo
{
	__int64					cbVolTotal;				// total bytes on volume
	__int64					cbVolFree;				// total bytes free on volume
	DWORD					cbCluster;				// cluster size of volume
	DWORD					fsFlags;				// overall process/action/status flags
	DWORD					volser;					// unique volume serial number
	UINT					driveType;				// drive type from GetDriveType()
	WCHAR					fsName[16];				// file system name
	WCHAR					volName[MAX_PATH];		// volume name (drive or UNC)
	bool					bUNC;					// UNC form name? UNC\server\share 
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
(both files and directores) in the active nested path for a recursive merge-match process to detect 
differences.  It is necessary to get all items of a directory before processing any because the sort/merge
requires the items in the source and target to be ordered in the same way (the same compare function).
For example, take the following directory structure:
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
	static const size_t		BUFFER_GROW = (1 << 14) * sizeof(size_t);	// buffer size gowth increment
	BYTE				  * m_index;			// MOVEABLE lifo buffer for nested headers, namespaces and indexes
	size_t					m_LevelHeaderOffset;// Transient current nested LevelHeader object offset
	size_t					m_cbBuffer;			// current buffer size in bytes
	size_t					m_cbUsed;			// bytes currently used in buffer
	struct {									// anonymous struct to ensure m_apipath prefix and m_path are contiguous
		wchar_t const		m_apipath[4];		// \\?\ prefix for m_path used by long-form unicode file APIs
		wchar_t				m_path[MAX_PATHX];	// current path being processed
	};
	size_t					m_pathlen;			// char length of the current m_path value (not including m_apipath prefix)
	bool					m_isUNC;			// is m_path in UNC (\\server\share) form?
	VolInfo					m_volinfo;			// volume information for the m_path base

	void			CreateIndex();							// Creates DirEntry offset index for sort
	void			DirEntryAdd(WIN32_FIND_DATA const * p_find);	// Adds a DirEntry to the current level
	size_t			DirPush();								// Pushes a new LevelHeader on top of the lifo buffer stack and return its offset
	wchar_t const * GetFilename(size_t p_offset) const		// returns filename from a DirEntry offset
						{ return ((DirEntry *)(m_index + p_offset))->cFileName; }
	void			GrowBuffer();							// Expand the existing m_buffer size
public:
					DirList();
					~DirList() { free(m_index); }
	wchar_t const * ApiPath() const { return m_apipath; }	// returns the api path string for \\?\-prefixed operations
	size_t			DirEntryLength(size_t p_offset) const	// returns DirEntry byte length for the offset provided
						{ return ((DirEntry *)(m_index + p_offset))->ByteLength(); }
	void			DirPop(size_t p_levelHeaderOffset);		// Pops off the top LevelHeader from the lifo buffer stack
	DirEntry const * GetDirEntry(size_t p_offset) const		// returns the DirEntry for the offset provided
						{ return (DirEntry *)(m_index + p_offset); } 
	DWORD			GetFSFlags() const						// returns the volume file system flags
						{ return m_volinfo.fsFlags; }
	bool			IsUNC() const { return m_isUNC; }		// is the path a UNC?
	DWORD			NormalizeAndSetPath(wchar_t const * p_path, bool bMakeDir);	// Tests and Normalizes p_path before setting m_path.  Returns 0 or rc
	int				PathAppend(wchar_t const * p_dir);		// append a dir element to m_path and return the result length
	wchar_t const * Path() const { return m_path; }			// returns the path string 
	PathExistsResult PathDirExists();						// Tests existance of m_path.  ret-0=not exist, 1=exists, 2=error, 3=file
	size_t			PathLength() const { return m_pathlen; }// returns current path length (not including apipath prefix)
	void			PathTrunc(int p_len)					// truncates m_path to to p_len length
						{ m_path[p_len] = L'\0'; m_pathlen = p_len; } 
	DWORD			ProcessCurrentPath();					// enumerate the current path into the buffer
	size_t			SetPath(wchar_t const * p_path)			// sets the path string 
						{ wcscpy_s(m_path, p_path); m_pathlen = wcslen(m_path); return m_pathlen; }
};


/// Enumerator for DirEntry objects in DirList
class DirListEnum
{
	DirList				  * m_dirList;				// DirList being enumerated
	size_t					m_levelOffset;			// LevelOffset of this level
	int						m_nCurrIndex;			// current index number in the enumeration
public:
					DirListEnum(DirList * p_dirList)	// give the DirList to be enumerated for the current path
						: m_dirList(p_dirList), m_levelOffset(p_dirList->m_LevelHeaderOffset), m_nCurrIndex(0) 
						{ m_dirList->ProcessCurrentPath(); m_levelOffset = m_dirList->m_LevelHeaderOffset; }
					~DirListEnum() { m_dirList->DirPop(m_levelOffset); }
	DirEntry *		GetNext();
};


//=========================================================================================================================
/// Header for LIFO index nested block followed by m_nEntries DirEntry* entries
class IndexLevel
{
public:
	int						marker1;			// value marker 0xffffffff for memory inspection
	int						m_nEntries;			// DirEntry* count at this nesting level
	DirEntryBlock		  * m_initblock;		// first DirEntryBlock
	size_t					m_initoffset;		// first offset within m_initblock
	size_t					m_prevheader;		// previous IndexHeader offset within m_prevblock
	short					m_pathlen;			// path length after path element has been pushed
	short					m_depth;			// nesting depth - nest number of this header
	int						marker2;			// 0xeeeeeeeeeeeeeeee marker at end of header
	(DirEntry *				m_direntry)[0];		// zero of more (m_nEntries) DirEntry * pointers for level index
	
	IndexLevel *	Initialize();				// lays in and initialzies the IndexHeader within the DirEntryIndex
	size_t			ByteSize() const { return sizeof *this + m_nEntries * sizeof m_direntry[0]; }
};


static int const		DirBlocksize = 1 << 18;	// fixed size of DirEntryBlock m_buffer for all DirEntry objects
/// DirEntryBlock has a buffer that holds any number of contiguous DirEntry objects
class DirEntryBlock : public TList
{
	byte					m_buffer[DirBlocksize];	// DirEntry buffer
public:
	DirEntry *		GetDirEntry(size_t p_offset) { return (DirEntry *)(m_buffer + p_offset); }
};

/// DirEntry objects are stored in any number of DirEntryBlocks.  They operate as a LIFO set of buffers
/// When there isn't room in the current block for a DirEntry, it advances to the next - creating one if necessary
class DirBlockCollection : public TListCollection
{
	DirEntryBlock		  *	m_currblock;		// current block to add DirEntry to if fits
	size_t					m_nextavail;		// next available offset within m_currblock

public:
	DirBlockCollection() { m_currblock = new DirEntryBlock(); Insert((TList*)m_currblock); m_nextavail = 0; }

	DirEntry *		DirEntryAdd(WIN32_FIND_DATA const * P_find);	// adds DirEntry to next available block space
	DirEntryBlock *	GetCurrBlock() { return m_currblock; }
	size_t			GetNextAvail() { return m_nextavail; }
	void			Popto(DirEntryBlock * p_block, size_t p_nextavail)
						{ m_currblock = p_block; m_nextavail = p_nextavail; }
};


class DirList2
{
	friend class DirListEnum2;					// used for enumeration of DirEntry objects at a dir level
	static const size_t		INDEX_INCREMENT = 1 << 18;	// size multiple for index
	byte				  * m_index;			// index buffer - can grow and change address
	size_t					m_indexsize;		// size of index
	size_t					m_currindex;		// offset of current indexheader

	DirBlockCollection		m_dirblocks;		// collection of DirBlocks that hold DirEntry objects
	DirEntryBlock		  * m_currblock;		// current DirEntryBlock to add DirEntry objects to
	size_t					m_blocknext;		// next offset within current block to add DirEntry
	struct {									// anonymous struct to ensure m_apipath prefix and m_path are contiguous
		wchar_t const		m_apipath[4];		// \\?\ prefix for m_path used by long-form unicode file APIs
		wchar_t				m_path[MAX_PATHX];	// current path being processed
	};
	size_t					m_pathlen;			// char length of the current m_path value (not including m_apipath prefix)
	bool					m_isUNC;			// is m_path in UNC (\\server\share) form?
	VolInfo					m_volinfo;			// volume information for the m_path base

	void			DirEntryAdd(WIN32_FIND_DATA const * p_find);	// Adds a DirEntry to the current level
	void			IndexEntryAdd(DirEntry * p_direntry);	// adds a DirEntry* index entry
	size_t			IndexUsed() const { return m_currindex + ((IndexLevel *)m_currindex)->ByteSize(); } // get index byte size
	void			IndexGrow();				// grow index when more room is needed
	void			IndexSort();				// sort DirEntry* slots in current level by cFileName
public:
	DirList2();
	~DirList2() { free(m_index); }
	void			Push();						// Add new IndexHeader to index
	void			Pop();						// Pop index, DirBlock and path off to previous position

	wchar_t const * ApiPath() const { return m_apipath; }	// returns the api path string for \\?\-prefixed operations
	IndexLevel *	GetCurrIndexLevel()						// returns the current IndexLevel
						{ return (IndexLevel *)(m_index + m_currindex);	}
	DirEntry *		GetIndexDirEntry(int p_n);
	DWORD			GetFSFlags() const						// returns the volume file system flags
						{ return m_volinfo.fsFlags; }
	bool			IsUNC() const { return m_isUNC; }		// is the path a UNC?
	DWORD			NormalizeAndSetPath(wchar_t const * p_path, bool bMakeDir);	// Tests and Normalizes p_path before setting m_path.  Returns 0 or rc
	int				PathAppend(wchar_t const * p_dir);		// append a dir element to m_path and return the result length
	wchar_t const * Path() const { return m_path; }			// returns the path string 
	PathExistsResult PathDirExists();						// Tests existance of m_path.  ret-0=not exist, 1=exists, 2=error, 3=file
	size_t			PathLength() const { return m_pathlen; }// returns current path length (not including apipath prefix)
	void			PathTrunc(int p_len)					// truncates m_path to to p_len length
						{m_path[p_len] = L'\0'; m_pathlen = p_len;}
	DWORD			ProcessCurrentPath();					// enumerate the current path into the buffer
	size_t			SetPath(wchar_t const * p_path)			// sets the path string 
						{ wcscpy_s(m_path, p_path); m_pathlen = wcslen(m_path); return m_pathlen;}
};


/// Enumerator for DirEntry objects in DirList
class DirListEnum2
{
	DirList2			  * m_dirList;				// DirList being enumerated
	int						m_nCurrIndex;			// current index number in the enumeration
public:
	DirListEnum2(DirList2 * p_dirList)	// give the DirList to be enumerated for the current path
		: m_dirList(p_dirList), m_nCurrIndex(0) {}
	~DirListEnum2() { m_dirList->Pop(); }
	DirEntry *		GetNext();
};

