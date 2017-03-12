//±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
//±                                                                            ±
//± VioSupp.h - Vio Support funtions                                           ±
//± Tom Bernhardt 08-10-92                                                     ±
//±                                                                            ±
//±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±

extern CONSOLE_SCREEN_BUFFER_INFO VtxtModeInfo;
extern HANDLE                     hConsole;

#define XYCOORD(x,y) ((COORD){x,y})

void _stdcall      
   VtxtDispStrN(
      WCHAR const          * str        ,// in -character to display
      WCHAR                  attr       ,// in -display attribute or 0xff
      short                  lenWrt     ,// in -length of display write
      short                  row        ,// in -row pos to start display
      short                  col         // in -col pos to start display
   );
void _stdcall      
   VtxtDispBox(
      short                  row1        ,// in- upper left corner row
      short                  col1        ,// in- upper left corner col
      short                  row2        ,// in- lower right corner row
      short                  col2        ,// in- lower right corner col
      WCHAR                  attrib      ,// in- display attribute of box lines
      WCHAR const          * figStr      ,// in- char values to write
      WCHAR const          * fill         // in- fill cell or NULL
   );
void _cdecl      
   VtxtDispMsg(
      short const            num         ,// in -error number/level code
      WCHAR const            msg[]       ,// in -error message to display
      ...                                 // in -printf args to msg pattern
   );
void _stdcall      
   VtxtInitialize(
      WCHAR const          * cell         // in-cell to clear screen with
   );
void _stdcall      
   VtxtDispVertLine(
      WCHAR const          * cell        ,// in- cell used to draw line
      short                  len         ,// in- height of vert line
      short                  row         ,// in- row to start vert line
      short                  col          // in- col of vert line
   );
void _stdcall      
   VtxtDispHorzLine(
      WCHAR const          * cell        ,// in- cell to used to draw line
      short                  len         ,// in- height of vert line
      short                  row         ,// in- row to start vert line
      short                  col          // in- col of vert line
   );
void _stdcall      
   VtxtDispTitleLine(
      WCHAR const          * cell        ,// in- cell to used to draw line
      WCHAR const          * title       ,// in -title to center in line
      short                  len         ,// in- width of line
      short                  row         ,// in- row to start vert line
      short                  col          // in- col of vert line
   );
