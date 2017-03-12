/*
================================================================================

  Program    - VioSupp
  Class      - Vio Support functions
  Author     - Tom Bernhardt
  Created    - 08/08/92
  Description- Vio support functions
  Updates    -

================================================================================
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

#include "viosupp.hpp"

CONSOLE_SCREEN_BUFFER_INFO   VtxtModeInfo;
HANDLE                       hConsole;


//------------------------------------------------------------------------------
//  Displays the "str" character string at position row/col with the "attr"
//  attribute.  "lenWrt" chars are always written.  If wcslen(str) exceeds
//  "lenWrt", "str" is display truncated to this length.  If "lenWrt" is
//  longer, blanks are written for the remainder of "lenWrt".
//------------------------------------------------------------------------------
void _stdcall
   VtxtDispStrN(
      wchar_t const        * str        ,// in -character to display
      wchar_t                attr       ,// in -display attribute or 0xff
      short                  lenWrt     ,// in -length of display write
      short                  row        ,// in -row pos to start display
      short                  col         // in -col pos to start display
   )
{
   size_t                    lenStr = wcslen(str),
                             minLen = min(lenStr, (size_t)lenWrt);
   COORD                     c = {col, row};
   DWORD                     nWrite;

   FillConsoleOutputAttribute(hConsole, attr, lenWrt, c, &nWrite);
   WriteConsoleOutputCharacter(hConsole, str, minLen, c, &nWrite);

   if ( minLen < (size_t)lenWrt )
   {
      COORD                  c2 = {(short)(col + minLen), row};

      c.Y += (short)minLen;
      FillConsoleOutputCharacter(hConsole, L' ', lenWrt - minLen, c2, &nWrite);
   }
}

#define UL figStr[0]         // char code box Upper Left
#define UR figStr[1]         // char code box Upper Right
#define LL figStr[2]         // char code box Lower Left
#define LR figStr[3]         // char code box Lower Right
#define HL figStr[4]         // char code box Horiz Line
#define VL figStr[5]         // char code box Vert  Line
//------------------------------------------------------------------------------
//  Displays a box bounded by it upper-left and lower-right corners row1/col1
//  and row2/col2, respectively.  The attribute char of the box chars is
//  specified by the "attrib" argument.  If "figStr" is not NULL, this
//  parameter specifies the characters used to make up the box (the default is
//  the double-line chars) in the orders of upper-left, upper-right,
//  lower-left, lower-right, horizontal line and vertical line.  If "fill"
//  is not NULL, it represent a pointer to a cell containing the char and
//  attribute pair used to fill the box interior.  If NULL, no filling is
//  performed.
//------------------------------------------------------------------------------
void _stdcall
   VtxtDispBox(
      short                  row1        ,// in- upper left corner row
      short                  col1        ,// in- upper left corner col
      short                  row2        ,// in- lower right corner row
      short                  col2        ,// in- lower right corner col
      WCHAR                  attrib      ,// in- display attribute of box lines
      WCHAR const          * figStr      ,// in- char values to write
      WCHAR const          * fill         // in- fill cell or NULL
   )
{
    WCHAR static const        defaultFig[6] = { 0x2554,0x2557,0x255a,0x255d,0x2550,0x2551 };
    short                     r,
                             hLen;                    // horizonal length
   DWORD                     nWrite;
   COORD                     coordUL = {col1, row1},
                             coordLR = {col2, row2},
                             coordUR = {col2, row1},
                             coordLL = {col1, row2};

   if ( !figStr )
      figStr = defaultFig;

   if ( UL )
   {
      FillConsoleOutputAttribute(hConsole, attrib, 1, coordUL, &nWrite);
      FillConsoleOutputCharacter(hConsole, UL, 1, coordUL, &nWrite);
   }
   if ( LR )
   {
      FillConsoleOutputAttribute(hConsole, attrib, 1, coordLR, &nWrite);
      FillConsoleOutputCharacter(hConsole, LR, 1, coordLR, &nWrite);
   }
   if ( UR )
   {
      FillConsoleOutputAttribute(hConsole, attrib, 1, coordUR, &nWrite);
      FillConsoleOutputCharacter(hConsole, UR, 1, coordUR, &nWrite);
   }
   if ( LL )
   {
      FillConsoleOutputAttribute(hConsole, attrib, 1, coordLL, &nWrite);
      FillConsoleOutputCharacter(hConsole, LL, 1, coordLL, &nWrite);
   }
   hLen = col2 - col1 - 1;
   if ( HL )                 // draw horizontal lines
   {
      COORD                  c1 = {col1+1, row1}, c2 = {col1+1, row2};

      FillConsoleOutputAttribute(hConsole, attrib, hLen, c1, &nWrite);
      FillConsoleOutputAttribute(hConsole, attrib, hLen, c2, &nWrite);
      FillConsoleOutputCharacter(hConsole, HL, hLen, c1, &nWrite);
      FillConsoleOutputCharacter(hConsole, HL, hLen, c2, &nWrite);
   }
   for ( r = row1 + 1;  r < row2;  r++ )
   {                                // draw vertical lines
      if ( VL )
      {
         COORD               c1 = {col1,r}, c2 = {col2,r};

         FillConsoleOutputAttribute(hConsole, attrib, 1, c1, &nWrite);
         FillConsoleOutputAttribute(hConsole, attrib, 1, c2, &nWrite);
         FillConsoleOutputCharacter(hConsole, VL,     1, c1, &nWrite);
         FillConsoleOutputCharacter(hConsole, VL,     1, c2, &nWrite);
      }
      if ( fill )                           // fill box if specified
      {
         COORD               c = {col1+1,r};

         FillConsoleOutputAttribute(hConsole, fill[1], hLen, c, &nWrite);
         FillConsoleOutputCharacter(hConsole, fill[0], hLen, c, &nWrite);
      }
   }
   return;
}


//------------------------------------------------------------------------------
//  Puts message with optional beep at bottom of screen
//------------------------------------------------------------------------------
void _stdcall
   VtxtDispMsg(
      short const            num         ,// in -error number/level code
      wchar_t const          msg[]       ,// in -error message to display
      ...                                 // in -printf args to msg pattern
   )
{
   static wchar_t const      prefLetter[] = L"IWESVUXXXXX";
   short                     level;
   wchar_t                   suffix[200],
                             fullmsg[100];
   va_list                   argPtr;

   level = abs(num / 1000);          // 1000's position of error number
   va_start(argPtr, msg);
   _vswprintf(suffix, msg, argPtr);
   va_end(argPtr);
   if ( num == 0 )
      wcsncpy(fullmsg, suffix, 80);
   else
      _swprintf(fullmsg, L"%c%4d: %-.74s", prefLetter[level], num, suffix);
   VtxtDispStrN(fullmsg, 0x4f, VtxtModeInfo.dwMaximumWindowSize.Y,
                (short)(VtxtModeInfo.dwMaximumWindowSize.X-1-(level>1)), 0);
   if ( level > 1 )
   {
      Beep(1000, 500);
      Beep( 500, 500);
      Beep(1000, 500);
   }
   if ( level > 4 )
      exit(level);
}


//------------------------------------------------------------------------------
//  Initalizes display area and sets VtxtModeInfo structure
//------------------------------------------------------------------------------
void _stdcall
   VtxtInitialize(
      WCHAR const           * cell         // in-cell to clear screen with
   )
{
   hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
   if ( hConsole == INVALID_HANDLE_VALUE )
   {
      fwprintf(stderr, L"GetStdHandle()=%ld\n", GetLastError());
      exit(9);
   }

   if ( !GetConsoleScreenBufferInfo(hConsole, &VtxtModeInfo) )
   {
      fwprintf(stderr, L"GetConsoleScreenBufferInfo()=%ld\n", GetLastError());
      exit(9);
   }

   if ( cell )
   {
      COORD                  c = {0, 0};
      int                    len = VtxtModeInfo.dwMaximumWindowSize.X
                                 * VtxtModeInfo.dwMaximumWindowSize.Y;
      DWORD                  nWrite;

      FillConsoleOutputAttribute(hConsole, cell[1], len, c, &nWrite);
      FillConsoleOutputCharacter(hConsole, cell[0], len, c, &nWrite);
   }
}


//------------------------------------------------------------------------------
//  Displays a vertical line starting at row/col and extending downwards for
//  a vertical length of "len".  If cell is not NULL, it specifies the char
//  and attribute to use for the line.
//------------------------------------------------------------------------------
void _stdcall
   VtxtDispVertLine(
      WCHAR const          * cell        ,// in- cell to used to draw line
      short                  len         ,// in- height of vert line
      short                  row         ,// in- row to start vert line
      short                  col          // in- col of vert line
   )
{
   short                     r;
   wchar_t                   fill,
                             attr;
   DWORD                     nWrite;

   if ( cell == NULL )
   {
      fill = 0xba;
      attr = 0x1f;
   }
   else
   {
      fill = cell[0];
      attr = cell[1];
   }

   for ( r = row;  r < row + len;  r++ )
   {
      COORD                  c = {col, r};

      FillConsoleOutputAttribute(hConsole, attr, 1, c, &nWrite);
      FillConsoleOutputCharacter(hConsole, fill, 1, c, &nWrite);
   }
}


//------------------------------------------------------------------------------
//  Displays a horizontal line starting at row/col and extending right for
//  a length of "len".  If cell is not NULL, it specifies the char
//  and attribute to use for the line.
//------------------------------------------------------------------------------
void _stdcall
   VtxtDispHorzLine(
      WCHAR const          * cell        ,// in- cell to used to draw line
      short                  len         ,// in- width of line
      short                  row         ,// in- row to start vert line
      short                  col          // in- col of vert line
   )
{
   wchar_t                   fill,
                             attr;
   COORD                     c = {col, row};
   DWORD                     nWrite;

   if ( cell == NULL )
   {
      fill = (BYTE)'\xba';
      attr = (BYTE)'\x1f';
   }
   else
   {
      fill = cell[0];
      attr = cell[1];
   }

   FillConsoleOutputAttribute(hConsole, attr, len, c, &nWrite);
   FillConsoleOutputCharacter(hConsole, fill, len, c, &nWrite);
}


//------------------------------------------------------------------------------
//  Displays a horizontal line starting at row/col and extending right for
//  a length of "len".  If cell is not NULL, it specifies the char
//  and attribute to use for the line.
//------------------------------------------------------------------------------
void _stdcall
   VtxtDispTitleLine(
      WCHAR const          * cell        ,// in- cell to used to draw line
      wchar_t const        * title       ,// in -title to center in line
      short                  len         ,// in- width of line
      short                  row         ,// in- row to start vert line
      short                  col          // in- col of vert line
   )
{
   wchar_t                   fill,
                             attr;
   COORD                     c = {col, row},
                             cTitle = {(short)(col + (len - wcslen(title)) / 2), row};
   DWORD                     nWrite;

   if ( cell == NULL )
   {
      fill = (BYTE)'\xba';
      attr = (BYTE)'\x1f';
   }
   else
   {
      fill = cell[0];
      attr = cell[1];
   }

   FillConsoleOutputAttribute(hConsole, attr, len, c, &nWrite);
   FillConsoleOutputCharacter(hConsole, fill, len, c, &nWrite);
   WriteConsoleOutputCharacter(hConsole, title, wcslen(title),
                               cTitle, &nWrite);
}
