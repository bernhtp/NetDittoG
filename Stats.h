//-----------------------------------------------------------------------------
// Statistics structures and types
//-----------------------------------------------------------------------------

typedef unsigned long StatCount;
typedef __int64      StatBytes;
struct StatBoth
{
	StatCount        count;
	StatBytes		 bytes;
};

struct StatsDirGet
{
	StatCount		 dirFound;			// n dirs scanned
	StatCount		 dirFiltered;		// n dirs made past filter
	StatBoth		 fileFound;			// n/bytes files scanned
	StatBoth		 fileFiltered;		// n/bytes files made past filter
};

struct StatsMatch
{
	StatCount		 dirMatched;		// n same-named directories
	StatBoth		 dirPermMatched;	// n same-named directories
	StatBoth		 fileMatched;		// n/bytes same named files
	StatBoth		 filePermMatched;	// n/bytes same named files
};

struct StatsCommon	 	// statistics common to both source and target directories
{
	StatCount		 dirFound;			// n dirs scanned
	StatCount		 dirFiltered;		// n dirs made past filter
	StatBoth		 fileFound;			// n/bytes files scanned
	StatBoth		 fileFiltered;		// n/bytes files made past filter
	StatBoth		 dirPermFiltered;	// n/bytes dir perms made past filter
	StatBoth		 filePermFiltered;	// n/bytes file perms made past filter
};

struct StatsChange	   // change/difference statistics for target
{
	StatCount		 dirCreated;
	StatCount		 dirRemoved;
	StatBoth		 dirPermCreated;
	StatBoth		 dirPermUpdated;
	StatBoth		 dirPermRemoved;
	StatCount		 dirAttrUpdated;
	StatBoth		 fileCreated;
	StatBoth		 fileUpdated;
	StatBoth		 fileRemoved;
	StatBoth		 filePermCreated;
	StatBoth		 filePermUpdated;
	StatBoth		 filePermRemoved;
	StatCount		 fileAttrUpdated;
};

struct Stats
{
	StatsChange		 change;
	StatsMatch		 match;
	StatsCommon		 source;
	StatsCommon		 target;
	Stats() { memset(this, 0, sizeof *this); }	// default constructor just zeroes all stats
};
