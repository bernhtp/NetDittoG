/*
===============================================================================

  Module     - Parm.cpp
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/13/93
  Description- Parses the command line parameters and sets the Options data
               structure.

  Updates -

===============================================================================
*/

#include <ctype.h>
#include <share.h>

#include "netditto.hpp"

enum ParmState {PSInvalid, PSNone, PSSourceSet, PSTargetSet, PSFIleFilterInclude, PSFileFilterExclude, PSDirFilterExclude};

//-----------------------------------------------------------------------------
// Adds the argument string to the end of the argument list and sets the head
// if it is NULL.
//-----------------------------------------------------------------------------
void _stdcall
   ListAdd(
      FileList            ** p_top         ,// i/o-head/top of list
      wchar_t const        * p_str          // in -string to add to list
   )
{
   FileList                * curr,
                           * prev = NULL;

   for ( curr = *p_top;  curr;  curr = curr->next )  // go to end of list
      prev = curr;
   curr = (FileList *)malloc(sizeof *curr + WcsByteLen(p_str));
   wcscpy(curr->name, p_str);
   curr->next = NULL;
   if ( prev )
      prev->next = curr;
   else
      *p_top = curr;
}

// This parses the command line attribute significance bitmap.  This represent
// the attribute bits which are deemed significant in attribute difference
// comparison (difference detection)
	void
	AttrSignificanceSet(
		WCHAR const          * p_opt           // in -options string to process
	)
{
	WCHAR const             * c;
	bool                      add = true;
	DWORD                     bit;

	for ( c = p_opt;  *c;  c++ )
	{
		bit = 0;
		switch ( *c )
		{
		case L'+':
			add = 1;
			break;
		case L'-':
			add = 0;
			break;
		case L'a':
			bit = FILE_ATTRIBUTE_ARCHIVE;
			break;
		case L'c':
			bit = FILE_ATTRIBUTE_COMPRESSED;
			break;
		case L'h':
			bit = FILE_ATTRIBUTE_HIDDEN;
			break;
		case L'r':
			bit = FILE_ATTRIBUTE_READONLY;
			break;
		case L's':
			bit = FILE_ATTRIBUTE_SYSTEM;
			break;
		case L't':
			bit = FILE_ATTRIBUTE_TEMPORARY;
			break;
		default:
			err.MsgWrite(ErrE, L"Invalid switch option for attribute significance='%c'", *c);
		}
		if ( bit )
			if ( add )
				gOptions.attrSignif |= bit;
			else
				gOptions.attrSignif &= ~bit;
	}
}

/// The following state table and function processes the variable grammar
/// in the option string beginning with d or f (directory or file) for the
/// specification of what file system components/subcomponents to detect
/// differences on and optionally replicare between source and target

enum SAction {aNull,aSetT,aOrT,aSetC,aOrC,aSetO,aSetX,aSetA,aOrA,aErr};
struct StateTable
{
	SAction                   action;        // state action before transition
	WCHAR                     arg;           // argument for action
	WCHAR                     newState;      // new state transition
};
#define ArgD  0x01                          // D=directory
#define ArgF  0x02                          // F=file
#define ArgC  0x01                          // C=contents
#define ArgA  0x02                          // A=attributes
#define ArgP  0x04                          // P=permissions
#define ArgM  OPT_PropActionMake            // M=make/create
#define ArgU  OPT_PropActionUpdate          // U=update/replace
#define ArgR  OPT_PropActionRemove          // R=remove/delete
#define ArgX  OPT_PropActionAll             // *=all, CAP or MUR
#define ArgE  0x08

static
short _stdcall								// ret-0=success
	ParmParseObjectAction(
		WCHAR const		  * str				// in -parameter string to parse
   )
{
   StateTable static const   sTable[][6] = {
//          0              1              2              3
// input         4              5
/* 0 d */{ {aSetT,ArgD,1},{aOrT ,ArgD,1},{aErr ,   0,6},{aErr ,   0,6},
                {aErr ,   0,6},{aErr ,   0,6}                         },
/* 1 f */{ {aSetT,ArgF,1},{aOrT ,ArgF,1},{aErr ,   0,6},{aErr ,   0,6},
                {aErr ,   0,6},{aErr ,   0,6}                         },
/* 2 c */{ {aErr ,   0,6},{aSetC,ArgC,2},{aOrC ,ArgC,2},{aErr ,   0,6},
                {aErr ,   0,6},{aErr ,   0,6}                         },
/* 3 a */{ {aErr ,   0,6},{aSetC,ArgA,2},{aOrC ,ArgA,2},{aErr ,   0,6},
                {aErr ,   0,6},{aErr ,   0,6}                         },
/* 4 p */{ {aErr ,   0,6},{aSetC,ArgP,2},{aOrC ,ArgP,2},{aErr ,   0,6},
                {aErr ,   0,6},{aErr ,   0,6}                         },
/* 5 + */{ {aErr ,   0,6},{aSetX,ArgP,3},{aSetO,ArgP,3},{aErr ,   0,6},
                {aErr ,   0,6},{aErr ,   0,6}                         },
/* 6 - */{ {aErr ,   0,6},{aSetX,ArgM,3},{aSetO,ArgM,3},{aErr ,   0,6},
                {aErr ,   0,6},{aErr ,   0,6}                         },
/* 7 = */{ {aErr ,   0,6},{aSetX,ArgE,3},{aSetO,ArgE,3},{aErr ,   0,6},
                {aErr ,   0,6},{aErr ,   0,6}                         },
/* 8 m */{ {aErr ,   0,6},{aErr ,   0,3},{aErr ,   0,3},{aSetA,ArgM,4},
                {aOrA ,ArgM,4},{aErr ,   0,6}                         },
/* 9 u */{ {aErr ,   0,6},{aErr ,   0,3},{aErr ,   0,3},{aSetA,ArgU,4},
                {aOrA ,ArgU,4},{aErr ,   0,6}                         },
/*10 r */{ {aErr ,   0,6},{aErr ,   0,3},{aErr ,   0,3},{aSetA,ArgR,4},
                {aOrA ,ArgR,4},{aErr ,   0,6}                         },
/*11 * */{ {aErr ,   0,6},{aSetC,ArgX,2},{aErr ,   0,3},{aSetA,ArgX,5},
                {aOrA ,ArgM,4},{aErr ,   0,6}                         },
/*12 \0*/{ {aErr ,   0,6},{aErr ,   0,6},{aErr ,   0,6},{aErr ,   0,6},
                {aNull,   0,5},{aErr ,   0,6}                         },
/*13   */{ {aErr ,   1,6},{aErr ,   1,6},{aErr ,   1,6},{aErr ,   1,6},
                {aErr ,   1,6},{aErr ,   1,6}                         }};
	StateTable const       * st;
	WCHAR const static	  * msg[] = {
		L"Syntax error",                                              // 0
		L"Invalid character"                                        };// 1
	WCHAR const static		inputTrans[] = L"dfcap+-=mur*";
	USHORT					state = 0,		// current state
							nError = 0,		// number of errors
							input;			// input index after translation
	WCHAR   const         * s;
	WCHAR   const         * t;				// temp pointer for strchr
	struct
	{
		byte				type;
		byte                component;
		byte                oper;
		byte                action;
	}						result = {0,0,0,0};

	for ( s = str;  state < 5;  s++ )
	{
		if ( t = wcschr(inputTrans, *s) )
			input = (USHORT)(t - inputTrans);
		else
			if ( *s )
				input = DIM(inputTrans);
			else
				input = DIM(inputTrans) - 1;

		st = &sTable[input][state];
		switch ( st->action )
		{
		case aErr:
			err.MsgWrite(30103, L"%s\n%*s %s [%d,%d]='%c'", str-1, s-str+9, L"^",
								msg[st->arg], state, input, *s);
			break;
		case aNull:         // no action
			break;
		case aSetT:         // set type of object: file, dir or both
		case aOrT:
			result.type |= st->arg;
			break;
		case aSetC:         // set component(s): contents, attr and/or perms
		case aOrC:
			result.component |= st->arg;
			break;
		case aSetX:         // set operator and missing components to all
			result.oper |= st->arg;
			result.component = ArgX;
			break;
		case aSetO:         // set operator: =+-
			result.oper |= st->arg;
			break;
		case aSetA:         // set action masks for MUR*
		case aOrA:
			result.action |= st->arg;
			break;
		default:
			err.MsgWrite(60101, L"Program error A=%d I=%c S=%d", st->action, *s, state);
		}
		state = st->newState;
	}
	if ( state != 5 )
		err.MsgWrite(50112, L"Error(s) found [%d,%d]", state, input);

	if ( result.type & ArgD )     // directory
	{
		if ( result.component & ArgC )
			switch ( result.oper )
			{
			case ArgP:
				gOptions.dir.contents |= result.action;
				break;
			case ArgM:
				gOptions.dir.contents &= ~result.action;
				break;
			case ArgE:
				gOptions.dir.contents = result.action;
				break;
			}
		if ( result.component & ArgA )
			switch ( result.oper )
			{
			case ArgP:
				gOptions.dir.attr |= result.action;
				break;
			case ArgM:
				gOptions.dir.attr &= ~result.action;
				break;
			case ArgE:
				gOptions.dir.attr = result.action;
				break;
			}
		if ( result.component & ArgP )
			switch ( result.oper )
			{
			case ArgP:
				gOptions.dir.perms |= result.action;
				break;
			case ArgM:
				gOptions.dir.perms &= ~result.action;
				break;
			case ArgE:
				gOptions.dir.perms = result.action;
				break;
			}
	}
	if ( result.type & ArgF )     // directory
	{
		if ( result.component & ArgC )
			switch ( result.oper )
			{
			case ArgP:
				gOptions.file.contents |= result.action;
				break;
			case ArgM:
				gOptions.file.contents &= ~result.action;
				break;
			case ArgE:
				gOptions.file.contents = result.action;
				break;
			}
		if ( result.component & ArgA )
			switch ( result.oper )
			{
			case ArgP:
				gOptions.file.attr |= result.action;
				break;
			case ArgM:
				gOptions.file.attr &= ~result.action;
				break;
			case ArgE:
				gOptions.file.attr = result.action;
				break;
			}
		if ( result.component & ArgP )
			switch ( result.oper )
			{
			case ArgP:
				gOptions.file.perms |= result.action;
				break;
			case ArgM:
				gOptions.file.perms &= ~result.action;
				break;
			case ArgE:
				gOptions.file.perms = result.action;
				break;
			}
	}

	return 0;
}

static void	Usage(
		bool					bDetail       // in -true = show detail
	)
{
	wprintf(L"Usage: NetDitto source dest [/option1 ... /optionN] [filter1 . . . filterN]\n");
	if ( bDetail )
		wprintf(L" source   source directory.  Hyphen (-) is a specially reserverd symbol for\n"
				"          a null source (used to delete destination components)\n"
				" dest     destination directory.  If this does not already exist, the /m\n"
				"          switch must be specified.\n"
				" filter   Used to specify individual files or wildcards, e.g., *.xl? will\n"
				"          match all file with .xl? extensions (for Excel).  Filters\n"
				"          following an /x option are exclude filters, which take\n"
				"          precedence over include filters.  Without any filter, all files\n"
				"          are selected.\n"
				" @filename The at sign (@) specifies that the following string is a file\n"
				"          name to open and use as additional command line specification.\n"
				"/switches preceding the letter with a hyphen turns the switch off,\n"
				"          e.g., /-u turns off update\n"
				" /a       Make the archive attribute bit significant in processing, both\n"
				"          for compare and replication.  Default is off.\n"
				" /d       Specify directory object actions.  See /fd section\n"
				" /f       Specify file object actions.  See /fd section\n"
				" /h       Process hidden and system files on target for update and delete.\n"
				"          Default is on.\n"
				" /l	      Specifies that logging will take place.  The /l may be immediately\n"
				"          (no space) followed by a space specifying the path/name of the log\n"
				"          file.  Otherwise, 'NetDitto.log' is the default name in the\n"
				"          current working drive/directory.\n"
				" /m       Make (create ala MkDir) the dest directory if it does not exist.\n"
				" /n       Newer:  Only consider target files different when source is newer\n"
				"          as determined by examing the file time/date last written.  Default\n"
				"          is on.\n"
				" /o       Optimize:  Consider files with the same timestamp and size to be\n"
				"          identical.  Default is on.\n"
				" /r       Process read-only target files (i.e., for delete/update actions).\n"
				"          With this turned off, read-only target files are not considered\n"
				"          for any processing, including comparison.  Default is on.\n"
				" /s       Display status and statistics display in real time.  When this\n"
				"          is turned off, no status and statistics are displayed.  \n"
				"          Default is on.\n"
				" /sd      Show detail status information on the bottom line of screen.\n"
				"          Default is on.\n"
				" /sm      Show detail matched information (no differences detected) on the\n"
				"          bottom of the screen and in the log.  Default is on.  Turning\n"
				"          this off is a convenient means to show only difference both on\n"
				"          the screen and in the log.\n"
				" /t       Consider directory timestamps significant and replicate them per\n"
				"          actions specified (i.e., directory attribute update).  Without\n"
				"          this, target directories retain their timestamps and get the\n"
				"          current timestamp when created.  Default is on.\n"
				" /u       Update target with specified differences.  Turning this off\n"
				"          means compare only.  Default is on.\n"
				" /x       Subsequent strings/wildcards are exclude file specifications.  Default\n"
				"          is off.\n"
				" /xd      Subsequent strings/wildcards are exclude dir specifications.  Default\n"
				"          is off.\n"
				" /#       # represents one or more decimal digits for the max directory\n"
				"          recursion depth, e.g., /2 means only process two levels down\n"
				"          from the starting source and target directory levels.  Default\n"
				"          is 255 (infinite).\n"
				" /as=     Specifies which attributes are considered significant and\n"
				"          replicated to dest.  A minus sign turns off significance for\n"
				"          the following attributes, e.g., /as=-c turns off the\n"
				"          replication of compression state.\n"
				" /backup  Uses backup/restore mode (if available) to circumvent security\n"
				"          and replicate security for new and updated files/dirs\n"
				" /backupforce  Same as /backup but forces all dirs/files to be replicated\n"
				"          so all security is always replicated.\n\n"
				" /{fd}{cap*}[+-=]{mur*}\n"
				"   fd     One or both of these must be specified representing files and\n"
				"          directories.\n"
				"   cap*   Zero or more components (contents, attributes, permissions) or\n"
				"          * for all.  None means '*'.\n"
				"   +-=    Exactly one of the operators for add, delete or set actions to\n"
				"          those specified.\n"
				"   mur*   One or more actions (make, update, remove), or * for all.\n"
			);
}



//-----------------------------------------------------------------------------
// Parses/processes a single parm argument string from the command line
//-----------------------------------------------------------------------------
static
DWORD _stdcall							 // ret-0=success
	ParmArgProcess(
		WCHAR const		  * p_currArg	,// in -current argument to process
		ParmState		  * p_state		 // i/o-state for interpreting includes/excludes/paths
	)
{
	DWORD                     rc = 0;
	bool                      negative = false;
	DWORD                     globalChangeMask = 0;
	WCHAR const             * errMsg;

	switch ( p_currArg[0] )
	{
	case L'/':
		if ( p_currArg[1] == L'-' )
		{
			negative = true;
			p_currArg++;
		}
		else if ( p_currArg[1] == L'+' )
			p_currArg++;

		switch ( p_currArg[1] )      // unique letter switch testing
		{
		case 0:
			err.MsgWrite(20012, L"Missing switch option");
			break;
		case L'?':
			Usage(true);
			rc = 1;
			break;
		case L'f':
		case L'd':
			if ( negative )
			{
				err.MsgWrite(30011, L"Can't negate %c option", p_currArg[1]);
				rc = 4;
			}
			else
				rc = ParmParseObjectAction(p_currArg + 1);
			break;
		case L'l':
			if ( negative )
				if ( p_currArg[2] )
				{
					err.MsgWrite(30113, L"Trying to negate log with arguments '%s'", p_currArg - 1);
					rc = 1;
				}
				else
					gLogName = NULL;
			else
				if ( p_currArg[2] )
					gLogName = (WCHAR *)p_currArg + 2;
				else
					gLogName = L"NetDitto.log";
			break;
		default:
			if ( isdigit(p_currArg[1]) )
				gOptions.maxLevel = _wtoi(p_currArg + 1);
			else if ( !wcscmp(p_currArg + 1, L"a") )
				globalChangeMask = OPT_GlobalAttrArch;
			else if ( !wcsncmp(p_currArg + 1, L"as=", 3) )
				AttrSignificanceSet(p_currArg + 4);
			else if ( !wcscmp(p_currArg + 1, L"backup") )
				globalChangeMask = OPT_GlobalBackup;
			else if ( !wcscmp(p_currArg + 1, L"backupforce") )
				globalChangeMask = OPT_GlobalBackup | OPT_GlobalBackupForce;
			else if ( !wcscmp(p_currArg + 1, L"h") )
				globalChangeMask = OPT_GlobalHidden;
			else if ( !wcscmp(p_currArg + 1, L"m") )
				globalChangeMask = OPT_GlobalMakeTgt;
			else if ( !wcscmp(p_currArg + 1, L"n") )
				globalChangeMask = OPT_GlobalNewer;
			else if ( !wcscmp(p_currArg + 1, L"namecase") )
				globalChangeMask = OPT_GlobalNameCase;
			else if ( !wcscmp(p_currArg + 1, L"o") )
				globalChangeMask = OPT_GlobalOptimize;
			else if ( !wcscmp(p_currArg + 1, L"pa") )
				globalChangeMask = OPT_GlobalPermAttr;
			else if ( !wcscmp(p_currArg + 1, L"pd") )
				globalChangeMask = OPT_GlobalDirPrec;
			else if ( !wcscmp(p_currArg + 1, L"r") )
				globalChangeMask = OPT_GlobalReadOnly;
			else if ( !wcscmp(p_currArg + 1, L"sd") )
				globalChangeMask = OPT_GlobalDispDetail;
			else if ( !wcscmp(p_currArg + 1, L"sm") )
				globalChangeMask = OPT_GlobalDispMatches;
			else if ( !wcscmp(p_currArg + 1, L"silent") )
				globalChangeMask = OPT_GlobalSilent;
			else if ( !wcscmp(p_currArg + 1, L"t") )
				globalChangeMask = OPT_GlobalDirTime;
			else if ( !wcscmp(p_currArg + 1, L"u") )
				globalChangeMask = OPT_GlobalChange;
			else if ( !wcscmp(p_currArg + 1, L"xor") )
				globalChangeMask = OPT_GlobalCopyXOR;
			else if ( !wcscmp(p_currArg + 1, L"x") )
			{
				if ( *p_state < PSTargetSet )
				{
					rc = 2;
					err.MsgWrite(3093, L"Filter option (/x or /d) given before target specified");
				}
				*p_state = PSFileFilterExclude;		// set state so successive strings are treated as file exclude wildcards
			}
			else if ( !wcscmp(p_currArg + 1, L"xd") )
			{
				if ( *p_state < PSTargetSet )
				{
					rc = 2;
					err.MsgWrite(3094, L"Filter option (/x or /d) given before target specified");
				}
				*p_state = PSDirFilterExclude;		// set state so successive strings are treated as dir exclude wildcards
			}
			else if ( !wcsncmp(p_currArg+1, L"sf=", 3) )
			{
				gOptions.spaceMinFree = (DWORD)TextToInt64(p_currArg+3, 0,
										(__int64)500*1024*1024*1024, &errMsg);
				if ( errMsg )
				{
					err.MsgWrite(ErrE, L"%s - %s", p_currArg, errMsg);
					rc = 1;
				}
			}
			else if ( !wcsncmp(p_currArg + 1, L"si", 2) )
			{
				if ( !isdigit(p_currArg[3]) )
				{
					err.MsgWrite(20003, L"Space check interval secs (si) must be numeric");
					rc = 1;
				}
				else
					gOptions.spaceInterval = 1000l * _wtoi(p_currArg + 3);
			}
			else
			{
				err.MsgWrite(20001, L"Invalid switch '%s'", p_currArg - negative);
				rc = 1;
			}
		}
		if ( globalChangeMask )
		{
			if ( negative )
				gOptions.global &= ~globalChangeMask;  // turn off option bit
			else
				gOptions.global |= globalChangeMask;   // turn on option bit
		}
		break;
	default:
		// unswitched parms (strings without a /prefix) are source, target paths and then filter wildcards
		switch ( *p_state )
		{
		case PSNone:
			if ( wcscmp(p_currArg, L"-") )
				rc = gSource.SetNormalizedRootPath(p_currArg);
			else
				gSource.SetRootPath(L"-");
			*p_state = PSSourceSet;
			break;
		case PSSourceSet:
			rc = gTarget.SetNormalizedRootPath(p_currArg);
			*p_state = PSTargetSet;
			break;
		case PSTargetSet:		// strings are implicitly include filters after paths and no switch
		case PSFIleFilterInclude:
			ListAdd(&gOptions.include, p_currArg);
			break;
		case PSFileFilterExclude:
			ListAdd(&gOptions.exclude, p_currArg);
			break;
		case PSDirFilterExclude:
			ListAdd(&gOptions.direxclude, p_currArg);
			break;
		default:
			err.MsgWrite(20029, L"Invalid filter state=%d at %s", *p_state, p_currArg);
		}
		if ( rc )
			err.SysMsgWrite(20029, rc, L"Invalid path '%s', ", p_currArg);
	}
	return rc;
}


//-----------------------------------------------------------------------------
// Parses/processes a single parm argument string from the command line
//-----------------------------------------------------------------------------
static
DWORD _stdcall								 // ret-0=success
	ParmFileIter(
		WCHAR const			  * fileName	,// in -filename containing options
		ParmState			  * p_state		 // i/o-state for interpreting includes/excludes
	)
{
	DWORD						rc,
								rcMax = 0;
	FILE					  * fileParm;
	WCHAR						line[256],
							  * c,
							  * next;

	if ( !(fileParm = _wfsopen(fileName, L"r", SH_DENYNO)) )
	{
		err.MsgWrite(30109, L"Parameter file(%s) open failed", fileName);
		return 3;
	}

	while ( fgetws(line, DIM(line)-1, fileParm) )
	{
		for ( c = line;  *c  && *c != L'\n';  c = next)
		{
			for ( ;  *c == L' ';  c++);         // set c to next non-blank
			// set next to char following term -- blank, 0 or newline
			for ( next = c;  *next  &&  *next != L' '  &&  *next != L'\n';  next++ );
			if ( *next )
			{
				*next = L'\0';    // terminate string c
				next++;           // and advance next to next char
			}

			if ( *c )
			{
				rc = ParmArgProcess(c, p_state);
				rcMax = max(rc, rcMax);
			}
		}
	}

	fclose(fileParm);
	return rcMax;
}


//-----------------------------------------------------------------------------
// Parses the command line parameter string
//-----------------------------------------------------------------------------
DWORD _stdcall                            // ret-0=success
	ParmParse(
		WCHAR const       ** argv         // in -argument values
	)
{
	WCHAR const			  * currArg;
	DWORD					rc,
							rcMax = 0;
	USHORT                  state = 0;
	ParmState				parmstate = PSNone;		// state for whether term is a path, file include, file exclude, or dir exclude filter

	memset(&gOptions, '\0', sizeof gOptions);		// initialize all options with defaults
	gOptions.file.contents  = OPT_PropActionAll;
	gOptions.file.attr      = OPT_PropActionAll;
	gOptions.file.perms     = OPT_PropActionNone;
	gOptions.dir.contents   = OPT_PropActionAll;
	gOptions.attrSignif     = FILE_ATTRIBUTE_HIDDEN
							| FILE_ATTRIBUTE_READONLY
							| FILE_ATTRIBUTE_SYSTEM
							| FILE_ATTRIBUTE_COMPRESSED;

	gOptions.dir.attr       = OPT_PropActionNone
							| OPT_PropActionAll
								;
	gOptions.dir.perms      = OPT_PropActionAll;

	gOptions.global         = OPT_GlobalReadOnly
							| OPT_GlobalOptimize
							| OPT_GlobalNewer
							| OPT_GlobalChange
							| OPT_GlobalHidden
							| OPT_GlobalDirTime
	//                         | OPT_GlobalDispMatches  // default changed to /-sm
							| OPT_GlobalDispDetail
							| OPT_GlobalNameCase;
	gOptions.maxLevel = 255;

	if ( !argv[1] )
		Usage(false);
	else
	{
		while ( currArg = *++argv )
		{
			if ( currArg[0] == L'@' )
				rc = ParmFileIter(currArg+1, &parmstate);
			else
				rc = ParmArgProcess(currArg, &parmstate);
			rcMax = max(rcMax, rc);
		}

		if ( !gSource.Path()[0] )
		{
			rcMax = 2;
		}
		else if ( !gTarget.Path()[0] )
		{
			rc = gTarget.SetNormalizedRootPath(L".");
			rcMax = max(rc, rcMax);
		}
	}
	return rcMax;
}


//-----------------------------------------------------------------------------
// Examines all options and detects/fixes semantic/environment conflicts.  For
// example, if either of the file systems does not support security, we can't
// replicate permissions meaning that /backup /backupforce /dfp? cannot be
// operative.
//-----------------------------------------------------------------------------
DWORD _stdcall                            // ret-number of warnings/fixes
	OptionsResolve()
{
	DWORD                     nFix = 0;

	// if neither source nor target support ACLs
	if ( !((gSource.GetFSFlags() | gTarget.GetFSFlags()) & FS_PERSISTENT_ACLS) )
	{
		if ( gOptions.global & OPT_GlobalBackup )
		{
			err.MsgWrite(10012, L"/backup option ignored because neither source "
								"nor target support ACLs");
			gOptions.global &= ~(OPT_GlobalBackup | OPT_GlobalBackupForce);
			nFix++;
		}

		if ( gOptions.dir.perms | gOptions.file.perms )
		{
			err.MsgWrite(10013, L"Permissions replication ignored: source/target "
								"must both support ACLs");
			gOptions.dir.perms = gOptions.file.perms = 0;
			nFix++;
		}
	}

	// Turn off compression significance if either source/target don't support compression
	if ( !(gSource.GetFSFlags() & gTarget.GetFSFlags() & FS_FILE_COMPRESSION) )
	{
		if ( gOptions.attrSignif & FILE_ATTRIBUTE_COMPRESSED )
		{
			nFix++;
			gOptions.attrSignif &= ~FILE_ATTRIBUTE_COMPRESSED;
		}
	}

	return nFix;
}
