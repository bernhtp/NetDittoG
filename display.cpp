/*
===============================================================================

  Program    - Display
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/12/93
  Description- Error message display and handler.  Full screen management
               functions and audit log.

  Updates -
  94-01-30 TPB Change file sizes from ULONG to __int64 (32-bit to 64-bit)
===============================================================================
*/

#include <conio.h>

#include "netditto.hpp"
#include "util32.hpp"
#include "viosupp.hpp"

//------------------ Full screen attribute defines ----------------------------
#define A_IBKG               0x1f         // Interior background attr
#define A_LBKG               0x19         // light v-line Interior background attr
#define A_BBKG               0x1e         // Box background attr
#define A_IBKGH              0x71         // Highlight interior background attr
#define A_IBKGT              0x1e         // Title
#define A_MSG                0x4f         // message area

// http://ascii-table.com/ascii-extended-pc-list.php
struct BoxDraw 
{
    WCHAR                   hl;           // horizontal line
    WCHAR                   vl;           // vertical line
    WCHAR                   tl;           // top left
    WCHAR                   tr;           // top right
    WCHAR                   ti;           // top interior
    WCHAR                   bl;           // bot left
    WCHAR                   br;           // bot right
    WCHAR                   bi;           // bot interior
    WCHAR                   ci;           // cross interior
    WCHAR                   li;           // left interior
    WCHAR                   ri;           // right interior
};

static const BoxDraw box1 =               // single-line box graphic chars
{
    0x2500, 0x2502, 0x250c, 0x2510, 0x252c, 0x2514, 0x2518, 0x2534, 0x253c, 0x251c, 0x2524
};

static const BoxDraw box2 =               // double-line box graphic chars
{
    0x2550, 0x2551, 0x2554, 0x2557, 0x2556, 0x255a, 0x255d, 0x2569, 0x256c, 0x2560, 0x2563
};



//--------------------------------GLOBALS--------------------------------------
enum vmode { Line, FullScreen };
static vmode gVideoMode = Line;
static FILE                * gFileLog = NULL;
wchar_t                    * gLogName = NULL;
TErrorScreen                 err(-1, -1, NULL);
TError                     & errCommon = err;

//-----------------------------------------------------------------------------

// override for the StrWrite virtual function for full screen support
void TErrorScreen::StrWrite(int level, wchar_t const * str) const
{
   if ( gVideoMode == FullScreen )
   {
      VtxtDispStrN(str, (wchar_t)(level > 1 ? 0x4f : A_IBKG),
                   VtxtModeInfo.dwMaximumWindowSize.X,
                   VtxtModeInfo.dwMaximumWindowSize.Y-1-(level>1), 0);
   }
   else
      TError::StrWrite(level, str);
}

//-----------------------------------------------------------------------------
// Statistics formatting constants
//-----------------------------------------------------------------------------
#define SCOL         8                     // source start col
#define TCOL         (SCOL+40)             // target start col
#define SROW         5                     // starting row for dir scan info
#define CROW         (SROW+6)              // starting row change info
#define CWIDTH       7                     // count cell width
#define BWIDTH       9                     // bytes cell width
#define AWIDTH       (CWIDTH + BWIDTH + 1) // count/byte cell width with separater
#define PATHROW      2                     // path start row
#define PATHCOL      9                     // path start col
#define PATHWIDTH    35                    // path width
#define CHEADWIDTH   AWIDTH                // col header width
#define RHEADWIDTH   7                     // row header width

//-----------------------------------------------------------------------------
// Displays the constant data for a column that contains both count and kBytes
// data with the column and row headings and the vertical line between the two
// types.
//-----------------------------------------------------------------------------
static
void _stdcall
   DisplayStatColDefine(
      short                  row         ,// in -screen row pos
      short                  colNum      ,// in -logical data column number
      WCHAR const          * rowHeader   ,// in -major column header
      WCHAR const          * colHeader   ,// in -array of row headers
      WCHAR const          * tVline       // in -vertical line trailing col
   )
{
   short                     colStart = SCOL + colNum * (AWIDTH+1),
                             nRow,        // header data row number
                             l = (short)wcslen(colHeader);
   WCHAR static const        lVline[] = {0x2502, A_LBKG};
   wchar_t const           * hRow;        // current row header

   VtxtDispStrN(colHeader, A_IBKGT, l, row, colStart + (AWIDTH-l)/2);

   VtxtDispStrN(L"Count" , A_IBKGT, 5, row+1, colStart + CWIDTH-5);
   VtxtDispStrN(L"kBytes", A_IBKGT, 6, row+1, colStart + BWIDTH-6 + CWIDTH+1);

   for ( hRow = rowHeader, nRow = row+2;  *hRow;  hRow += RHEADWIDTH, nRow++ )
   {
      COORD                  c = {colStart + CWIDTH, nRow};
      DWORD                  nWrite;

      VtxtDispStrN(hRow, A_IBKGT, RHEADWIDTH, nRow, 1);  // cell row header
      FillConsoleOutputAttribute(hConsole, lVline[1], 1, c, &nWrite);
      FillConsoleOutputCharacter(hConsole, lVline[0], 1, c, &nWrite);
   }

   if ( tVline )
      VtxtDispVertLine(tVline, nRow-row, row, colStart + AWIDTH);
}

//-----------------------------------------------------------------------------
// Defines multiple columns of statistics with the parameter row and column
// header arrays of labels.
//-----------------------------------------------------------------------------
static
void _stdcall
   DisplayStatColDefineAll(
      short                  row         ,// in -starting screen row pos
      WCHAR const          * rowHeader   ,// in -array of major column headers
      WCHAR const          * colHeader    // in -array of row headers
   )
{
   short                     c;
   WCHAR static const        lVline[] = {0x2502, A_IBKGT   };
   WCHAR const             * ch;

   for ( ch = colHeader, c = 0;  *ch;  ch += CHEADWIDTH, c++ )
   {
      DisplayStatColDefine(row, c, rowHeader, ch,
                           ch[CHEADWIDTH] ? lVline : NULL );
   }
}

//-----------------------------------------------------------------------------
// Initializes the video system and displays the constant information for the
// full screen status reporting when the argument is 1.  When 0, it terminates
// full screen and restores the mode and cursor position leaving the on-screen
// information intact and un-scrolled.
//-----------------------------------------------------------------------------
short _stdcall
   DisplayInit(
      short                  mode         // in -1=fullscreen, 0=text
   )
{
   wchar_t static const      tMain[]   = L"NetDitto, (c) Tom Bernhardt 1991-2016",
                             tChange[] = L" Differences ";
   wchar_t static const      fBox[]    = {0x250C,0x2510,0x2514,0x2518,0x2500,0x2502},
                             cFill[]   = {L' ' , A_IBKG   },
                             lVline1[] = {0x2502, A_LBKG   },
                             lVline2[] = {0x2502, A_IBKG   },
                             lHline1[] = {0x2500, A_IBKGT  },
                             lHline2[] = {0x2550, A_IBKGT  };
   wchar_t static const      rowHeader1[][RHEADWIDTH] =
                                 {L"Dir", L" Scrty", L"File", L" Secty", L""},
                             rowHeader2[][RHEADWIDTH] =
                                 {L"Dir", L" Scrty", L" Attr", L"File", L" Scrty",L" Attr",L""},
                             colHeader1[][CHEADWIDTH] =
                                 {L"Scanned", L"Filter Accepted", L"Scanned", L"Filter Accepted", L""},
                             colHeader2[][CHEADWIDTH] =
                                 {L"Matched", L"Created", L"Updated", L"Deleted", L""};
   DWORD                     nWrite;
   COORD                     coordCur1 = {0 ,23},
                             coordCur2 = {0 ,21},
                             coordMain = {0 , 0},
                             coordChange={0 ,CROW};

   if ( mode == 0 )
   {
      VtxtDispStrN(L" ", '\x07', VtxtModeInfo.dwMaximumWindowSize.X,
                                VtxtModeInfo.dwMaximumWindowSize.Y - 1, 0);
      SetConsoleCursorPosition(hConsole, coordCur1);
      return 0;
   }
   VtxtInitialize( NULL );   // initialize screen w/o clearing it.
   gVideoMode = FullScreen;
   SetConsoleCursorPosition(hConsole, coordCur2);
   VtxtDispBox(0,0, VtxtModeInfo.dwMaximumWindowSize.Y - 2,
                    VtxtModeInfo.dwMaximumWindowSize.X-1, A_BBKG, fBox, cFill);
   coordMain.X = (VtxtModeInfo.dwMaximumWindowSize.X - DIM(tMain)) / 2;
   WriteConsoleOutputCharacter(hConsole, tMain, DIM(tMain)-1,
                               coordMain, &nWrite);

   VtxtDispStrN(L"Source", A_IBKGT, 6,  SROW-2, SCOL+  (AWIDTH+1)-4);
   VtxtDispStrN(L"Target", A_IBKGT, 6,  SROW-2, SCOL+3*(AWIDTH+1)-4);
   DisplayStatColDefineAll(SROW, rowHeader1[0], colHeader1[0]);

   // draw horizontal line
   coordChange.X = (VtxtModeInfo.dwMaximumWindowSize.X-DIM(tChange) + 8) / 2;
   VtxtDispHorzLine(lHline2, VtxtModeInfo.dwMaximumWindowSize.X - 2, CROW, 1);
   WriteConsoleOutputCharacter(hConsole, tChange, DIM(tChange)-1,
                               coordChange, &nWrite);

   DisplayStatColDefineAll(CROW+1, rowHeader2[0], colHeader2[0]);
   VtxtDispHorzLine(lHline2, VtxtModeInfo.dwMaximumWindowSize.X - 2, CROW+9, 1);

   return 0;
}

//-----------------------------------------------------------------------------
// Formats and displays count statistics
//-----------------------------------------------------------------------------
static void _stdcall
   DisplayStatCount(
      StatCount const      * count       ,// in -count statistics
      short                  row         ,// in -row to display
      short                  colNum       // in -logical col number
   )
{
   wchar_t                   temp[3][CWIDTH+4];
   DWORD                     nWrite;
   COORD                     c = {SCOL + colNum * (AWIDTH+1), row};

   _swprintf(temp[0], L"%7.7s", CommaStr(temp[1], _ltow(*count, temp[2], 10)) );
   WriteConsoleOutputCharacter(hConsole, temp[0], 7, c, &nWrite);
}

//-----------------------------------------------------------------------------
// Formats and displays both count and kBytes stats
//-----------------------------------------------------------------------------
static void _stdcall
   DisplayStatBoth(
      StatBoth const       * both        ,// in -count/byte statistics
      short                  row         ,// in -row to display
      short                  colNum       // in -logical col number
   )
{
   wchar_t                   temp[3][CWIDTH+4];
   DWORD                     nWrite;
   COORD                     c1 = {           SCOL + colNum * (AWIDTH+1), row},
                             c2 = {CWIDTH+1 + SCOL + colNum * (AWIDTH+1), row};

   _swprintf(temp[0], L"%7.7s", CommaStr(temp[1], _ltow(both->count, temp[2], 10)) );
   WriteConsoleOutputCharacter(hConsole, temp[0], CWIDTH, c1, &nWrite);
   _swprintf(temp[0], L"%9.9s", CommaStr(temp[1], _ltow((ULONG)(both->bytes/1000), temp[2], 10)) );
   WriteConsoleOutputCharacter(hConsole, temp[0], BWIDTH, c2, &nWrite);
}

//-----------------------------------------------------------------------------
// Displays stats accumulated in DirGet for both source and target
//-----------------------------------------------------------------------------
void _stdcall
   DisplayDirStats(
      StatsDirGet const    * source      ,// in -source directory get stats
      StatsDirGet const    * target       // in -target directory get stats
   )
{
   DisplayStatCount(&source->dirFound     , SROW+2, 0);
   DisplayStatCount(&source->dirFiltered  , SROW+2, 1);
   DisplayStatCount(&target->dirFound     , SROW+2, 2);
   DisplayStatCount(&target->dirFiltered  , SROW+2, 3);

   DisplayStatBoth (&source->fileFound    , SROW+4, 0);
   DisplayStatBoth (&source->fileFiltered , SROW+4, 1);
   DisplayStatBoth (&target->fileFound    , SROW+4, 2);
   DisplayStatBoth (&target->fileFiltered , SROW+4, 3);
}

//-----------------------------------------------------------------------------
// Displays common stats accumulated in DirGet for both source and target
//-----------------------------------------------------------------------------
void _stdcall
   DisplayStatsCommon(
      StatsCommon const    * source      ,// in -source directory get stats
      StatsCommon const    * target       // in -target directory get stats
   )
{
   DisplayStatCount(&source->dirFound        , SROW+2, 0);
   DisplayStatCount(&source->dirFiltered     , SROW+2, 1);
   DisplayStatCount(&target->dirFound        , SROW+2, 2);
   DisplayStatCount(&target->dirFiltered     , SROW+2, 3);

   DisplayStatBoth (&source->dirPermFiltered , SROW+3, 1);
   DisplayStatBoth (&target->dirPermFiltered , SROW+3, 3);

   DisplayStatBoth (&source->fileFound       , SROW+4, 0);
   DisplayStatBoth (&source->fileFiltered    , SROW+4, 1);
   DisplayStatBoth (&target->fileFound       , SROW+4, 2);
   DisplayStatBoth (&target->fileFiltered    , SROW+4, 3);

   DisplayStatBoth (&source->filePermFiltered, SROW+5, 1);
   DisplayStatBoth (&target->filePermFiltered, SROW+5, 3);
}

//-----------------------------------------------------------------------------
// Displays change statistics, either actual or virtual via /compare option
//-----------------------------------------------------------------------------
void _stdcall
   DisplayStatsChange(
      StatsChange const    * stats        // in -change statistics
   )
{
   DisplayStatCount(&stats->dirCreated     , CROW+3, 1);
   DisplayStatCount(&stats->dirRemoved     , CROW+3, 3);

   DisplayStatBoth (&stats->dirPermCreated , CROW+4, 1);
   DisplayStatBoth (&stats->dirPermUpdated , CROW+4, 2);
   DisplayStatBoth (&stats->dirPermRemoved , CROW+4, 3);

   DisplayStatCount(&stats->dirAttrUpdated , CROW+5, 2);

   DisplayStatBoth (&stats->fileCreated    , CROW+6, 1);
   DisplayStatBoth (&stats->fileUpdated    , CROW+6, 2);
   DisplayStatBoth (&stats->fileRemoved    , CROW+6, 3);

   DisplayStatBoth (&stats->filePermCreated, CROW+7, 1);
   DisplayStatBoth (&stats->filePermUpdated, CROW+7, 2);
   DisplayStatBoth (&stats->filePermRemoved, CROW+7, 3);

   DisplayStatCount(&stats->fileAttrUpdated, CROW+8, 2);
}

//-----------------------------------------------------------------------------
// Displays match statistics
//-----------------------------------------------------------------------------
void _stdcall
   DisplayStatsMatch(
      StatsMatch const     * stats        // in -change statistics
   )
{
   DisplayStatCount(&stats->dirMatched     , CROW+3, 0);
   DisplayStatBoth (&stats->dirPermMatched , CROW+4, 0);
   DisplayStatBoth (&stats->fileMatched    , CROW+6, 0);
   DisplayStatBoth (&stats->filePermMatched, CROW+7, 0);
}

//-----------------------------------------------------------------------------
// Displays the current paths being processed
//-----------------------------------------------------------------------------
void _stdcall
   DisplayPaths(
   )
{
   VtxtDispStrN(L"Start", A_IBKGT, 8, PATHROW+2, 1);
   VtxtDispStrN(gOptions.source.path, A_IBKGH, PATHWIDTH-1, PATHROW+2, PATHCOL);
   VtxtDispStrN(gOptions.target.path, A_IBKGH, PATHWIDTH  , PATHROW+2, PATHCOL+PATHWIDTH);
}

//-----------------------------------------------------------------------------
// Displays the current path difference (from the start point) being processed
//-----------------------------------------------------------------------------
void _stdcall
   DisplayPathOffset(
      WCHAR const          * path         // in -target path
   )
{
   static short              len = -1;    // persistent original target path

   if ( len < 0 )
      len = (short)wcslen(path);
   VtxtDispStrN(path + len, A_IBKG, 78, PATHROW, 1);
}

//-----------------------------------------------------------------------------
// Displays parsed options information on the screen
//-----------------------------------------------------------------------------
void _stdcall
   DisplayOptions()
{
   WCHAR                    temp[80],
                          * t;
   FileList               * f;
   short                    l;

   VtxtDispStrN(L"Include:", A_IBKGT, 8,  1,  1);
   VtxtDispStrN(L"Exclude:", A_IBKGT, 8,  1, 40);

   for ( f = gOptions.include, t = temp;  f;  f = f->next )
   {
      if ( t + (l = (short)wcslen(f->name)) < temp + DIM(temp) - 4 )
      {
         wcscpy(t, f->name);
         t += l + 1;
         t[-1] = L' ';
      }
      else
         break;
   }
   *t = L'\0';
   VtxtDispStrN(temp, A_IBKGH, 30, 1, 9);

   for ( f = gOptions.exclude, t = temp;  f;  f = f->next )
   {
      if ( t + (l = (short)wcslen(f->name)) < temp + DIM(temp) - 4 )
      {
         wcscpy(t, f->name);
         t += l + 1;
         t[-1] = L' ';
      }
      else
         break;
   }
   *t = L'\0';
   VtxtDispStrN(temp, A_IBKGH, 31, 1, 48);
}


//-----------------------------------------------------------------------------
// Displays the elapsed time and number of bytes written per second.
//-----------------------------------------------------------------------------
extern BufferOffset          bufferMax;
void _stdcall
   DisplayTime()
{
   DWORD static              tStart = 0,  // initial value
                             tLast;
   DWORD                     tNow = GetTickCount(),
                             tDiff,
                             nWrite;
   wchar_t                   temp[3][8];
   wchar_t static const      eTitle[] = L"Elapsed time",
                             rTitle[] = L" KB/sec Current";
   COORD                     c1 = { 1                , 22},
                             c2 = { 1 + DIM(eTitle)  , 22},
                             c3 = { 8 + DIM(eTitle)  , 22},
                             c4 = {70                , 22};
   __int64 static            bLastWritten = 0;
   float                     rateTot;

   if ( tStart == 0 )
   {
      tLast = tStart = GetTickCount();
      VtxtDispStrN(eTitle, A_IBKGT, DIM(eTitle) - 1, 21, 1);
      VtxtDispStrN(rTitle, A_IBKGT, DIM(rTitle) - 1, 21, 1 + DIM(eTitle));
   }

   tDiff = tNow - tStart;
   ElapsedTimeStr(tDiff / 1000, temp[0]);
   WriteConsoleOutputCharacter(hConsole, temp[0], wcslen(temp[0]), c1, &nWrite);


   if ( tDiff == 0 )
      wcscpy(temp[0], L"    N/A");
   else
   {
      if ( tNow != tLast )
      {
         rateTot = (float)gOptions.bWritten / tDiff;
         if ( rateTot >= 100  ||  rateTot == 0 )
            _swprintf(temp[0], L"%7.0f", rateTot);
         else
            _swprintf(temp[0], L"%7.3f", rateTot);
         _swprintf(temp[1], L"%7d", (DWORD)(gOptions.bWritten - bLastWritten) / (tNow - tLast));
         temp[0][DIM(temp[0])-1] = L' ';

         WriteConsoleOutputCharacter(hConsole, temp[0], DIM(temp[0]) * 2 - 1, c2, &nWrite);
         bLastWritten = gOptions.bWritten;
      }
   }

   //` sprintf(temp[0], "M=%5u", bufferMax);
   //` WriteConsoleOutputCharacter(hConsole, temp[0], wcslen(temp[0]), c4, &nWrite);
   tLast = tNow;
}
