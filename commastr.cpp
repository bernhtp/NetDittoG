/*
================================================================================
=                                                                              =
= Program    - comma_str                                                       =
= Class      - UTIL general system subroutine                                  =
= Author     - Tom Bernhardt                                                   =
= Created    - 06/25/90                                                        =
= Description- Returns the result buffer which is the input numeric            =
=              str value with commas inserted before every three digits        =
=              after significance and before the decimal point                 =
=                                                                              =
= Updates -                                                                    =
=                                                                              =
=                                                                              =
================================================================================
*/
#include <string.h>
#include <windows.h>

WCHAR * _stdcall                          // ret-result buffer conversion of str
   CommaStr(
      WCHAR                  result[]    ,// out-result buffer
      WCHAR const            str[]        // in -string to be converted
   )
{
   WCHAR const             * s,
                           * dec_point;
   WCHAR                   * r;
   enum {pre_signif, signif, post_dec}
                             state = pre_signif;

   if ( (dec_point = wcschr(str, L'.')) == NULL )
      dec_point = str + wcslen(str);
   for ( s = str, r = result;  *s; s++, r++ )
   {
      switch ( state )
      {
         case pre_signif:
            if ( *s >= L'0'  &&  *s <= L'9' )
               state = signif;
            break;
         case signif:
            if ( *s == L'.' )
               state = post_dec;
            else
               if ( !((s - dec_point) % 3) )
                  *r++ = L',';
            break;
         default:
            break;
      }
      *r = *s;
   }
   *r = L'\0';        // terminate result
   return result;
}
