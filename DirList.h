#pragma warning(disable : 4200)	// use zero-length array extension for dynamic arrays

const int MAX_PATHX = 32764;	// max path char length exluding \\?\ long-path prefix	
enum PathExistsResult { NotExist, YesDir, ErrorNo, YesButFile };	// return states for PathDirExists

/// DirEntry objects are packed consecutively in the buffer.  Note the variable length
struct DirEntry					// directory entry summary
{
	FILETIME				ftimeLastWrite;			// last written
	__int64					cbFile;					// size of file in bytes
	DWORD					attrFile;				// file/dir attribute
	WCHAR					cFileName[1];			// file/dir name (varying length at least 1 for null)

	size_t		ByteLength() const					// returns byte length of this DirEntry 
	{
		return sizeof *this + (wcslen(cFileName) * sizeof cFileName[0]);
	}
	static size_t ByteLength(WIN32_FIND_DATA const * p_find) // returns byte length of a DirEntry created with this p_find 
	{
		return sizeof (DirEntry) + (wcslen(p_find->cFileName) * sizeof cFileName[0]);
	}
};

DirEntry * _stdcall DirEntryCreate(WIN32_FIND_DATAW const * p_find);

/// DirEntryMax accommodates the largest possible size DirEntry
struct DirEntryMax
{
	DirEntry		de;								// last element is filename - followed by enough space for the max size
	wchar_t			rest[_MAX_PATH];

	DirEntryMax(WIN32_FIND_DATA const * find);
	DirEntry *		GetDirEntry() { return &de; }
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
	
	VolInfo() { memset(this, 0, sizeof *this); }
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

	DirEntry *		DirEntryCreate(WIN32_FIND_DATA const * P_find);	// adds DirEntry to next available block space
	DirEntryBlock *	GetCurrBlock() { return m_currblock; }			// returns curr DirEntryBlock buffer of DirEntry objects
	size_t			GetNextAvail() { return m_nextavail; }			// Returns the next available offset for a new DirEntry
	void			Popto(DirEntryBlock * p_block, size_t p_nextavail)	// Pops the curr DirBlock and its avail offset to previous velues
						{ m_currblock = p_block; m_nextavail = p_nextavail; }
};


/*
DirList performs directory enumeration for a path.  It holds the DirEntry objects (names, attributes)
(both files and directores) in the active nested path for a recursive merge-match process to detect 
differences.  It is necessary to get all items of a directory before processing any because the sort/merge
requires the items in the source and target to be ordered in the same way (the same sort compare function).
To do this very efficiently (memory and CPU), custom, low-level data structures are created to implement
the LIFO stack of both indexes and data.  For example, take the following directory structure:
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

There are two major internal data structures that operate in a LIFO manner:

	m_index		Buffer that stores the nested blocks of pointers to DirEntry objects
				Each nest level starts with an IndexLevel structure followed by
				an array of zero or more pointers to DirEntry objects.
				The IndexLevel struct also contains information to restore the LIFO
				stack to the previous level as part of a Pop operation.
	m_dirblocks	Linked list of large buffers that store contiguous DirEntry objects.  
				Storing DirEntrys in large buffers radically reduces malloc heap overhead 
				and fragmentation.

Each level will contain:
- A LevelHeader object
- Zero or more DirEntry objects (LevelHeder.nDirEntry)
- Zero or more DirEntryOffset index objects (buffer offset of each DirEntry for sort/merge)

NOTE: because the index address can change when it grows, great care should be taken in computing
pointers from offsets to buffer.  They generally must be recomputed from the current m_index base
each time unless you know it won't grow in between and thus won't possibly change.
*/
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
	DirEntry *				m_direntry[0];		// zero of more (m_nEntries) DirEntry * pointers for level index
	
	void			Initialize();				// lays in and initialzies the IndexHeader within the DirEntryIndex
	size_t			ByteSize() const			// Returns size in bytes of this index level including variable m_direntry[x]
						{ return sizeof *this + m_nEntries * sizeof m_direntry[0]; }
	DirEntry *		GetDirEntry(int p_n)		// Returns the DirEntry at p_n
						{ return m_direntry[p_n]; }
};

static const size_t INDEXLEVEL_NULL_OFFSET = -1;

class DirList
{
	friend class DirListEnum;					// used for enumeration of DirEntry objects at a dir level
	static const size_t		INDEX_INCREMENT = 1 << 18;	// size multiple for index
	byte				  * m_index;			// index buffer - can grow and change address
	size_t					m_indexsize;		// size of index
	size_t					m_currindex;		// offset of current indexheader

	DirBlockCollection		m_dirblocks;		// collection of DirBlocks that hold DirEntry objects
	DirEntryBlock		  * m_currblock;		// current DirEntryBlock to add DirEntry objects to
	size_t					m_blocknext;		// next offset within current block to add DirEntry
	struct {									// anonymous struct to ensure m_apipath prefix and m_path are contiguous
		wchar_t const		m_apipath[4] = { L'\\', L'\\', L'?', L'\\' };	// \\?\ prefix for m_path used by long-form unicode file APIs
		wchar_t				m_path[MAX_PATHX];	// current path being processed
	};
	size_t					m_pathlen;			// char length of the current m_path value (not including m_apipath prefix)
	bool					m_isUNC;			// is m_path in UNC (\\server\share) form?
	VolInfo					m_volinfo;			// volume information for the m_path base
	StatsCommon			  * m_stats;			// scan stats common to source and target

	DirEntry *		DirEntryAdd(WIN32_FIND_DATA const * p_find);// Creates a DirEntry at the current level
	void			IndexEntryAdd(DirEntry * p_direntry);	// adds a DirEntry* index entry
	size_t			IndexUsed()  { return m_currindex + GetCurrIndexLevel()->ByteSize(); } // get index used in bytes
	void			IndexGrow();							// grow index when more room is needed
	void			IndexSort();							// sort DirEntry* slots in current level by cFileName
public:
	DirList(StatsCommon * p_stats);
	~DirList() { free(m_index); }

	void			Push();									// Pushes new IndexLevel at end of m_index
//	void			Push(wchar_t const * p_dir)				// Push with new directory name
//						{ PathAppend(p_dir); Push(); ProcessCurrentPath(); }
	void			Pop();									// Pop IndexLevel, DirBlock and path off to previous position

	wchar_t const * ApiPath() const { return m_apipath; }	// returns the api path string for \\?\-prefixed operations
	IndexLevel *	GetCurrIndexLevel()						// returns the current IndexLevel
						{ return (IndexLevel *)(m_index + m_currindex);	}
	DirEntry *		GetIndexDirEntry(int p_n);
	DWORD			GetFSFlags() const						// returns the volume file system flags
						{ return m_volinfo.fsFlags; }
	bool			IsUNC() const { return m_isUNC; }		// is the path a UNC?
	DWORD			SetNormalizedRootPath(wchar_t const * p_path);	// Tests and Normalizes p_path before setting m_path.  Returns 0 or rc
	int				PathAppend(wchar_t const * p_dir);		// append a dir element to m_path and return the result length
	wchar_t const * Path() const { return m_path; }			// returns the path string 
	PathExistsResult PathDirExists(DirEntry ** p_direnty);	// Tests existance of m_path.  ret-0=not exist, 1=exists, 2=error, 3=file
	size_t			PathLength() const { return m_pathlen; }// returns current path length (not including apipath prefix)
	void			PathTrunc()								// truncates m_path to current IndexLevel->m_pathlen
	{
		m_pathlen = GetCurrIndexLevel()->m_pathlen; m_path[m_pathlen] = L'\0';
	}
	DWORD			ProcessCurrentPath();					// enumerate the current path into the buffer
	size_t			SetRootPath(wchar_t const * p_path)		// sets the path string 
						{ wcscpy_s(m_path, p_path); m_pathlen = wcslen(m_path); return m_pathlen;}
};


/// Enumerator for DirEntry objects in DirList
class DirListEnum
{
	DirList				  * m_dirList;			// DirList being enumerated
	int						m_nCurrIndex;		// current index number in the enumeration
public:
	DirListEnum(DirList * p_dirList);						// give the DirList to be enumerated for the current path
	~DirListEnum() { m_dirList->Pop(); }					// destructor ensures a Pop of the IndexLevel
	DirEntry *		GetNext();								// return next DirEntry and advance or NULL if EOL
	DirEntry *		Peek();									// Get current DirEntry without advancing - NULL if EOL
	bool			EOL() const;							// end of list?
	bool			Advance();								// Advance m_nCurrIndex to next index number
};
