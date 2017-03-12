/*
================================================================================
                                                                               
 Module     - UString                                                         
 Class      - Common general system subroutine                                  
 Author     - Tom Bernhardt                                                   
 Created    - 96/07/22                                                        
 Description- Copies multiple null-terminated strings to the specified        
              target.  The variable length parameter list is terminated       
              with a NULL pointer.                                            
 Updates -                                                                    
                                                                              
================================================================================
*/
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>

#include "UString.hpp"

char * _cdecl                             // ret-target string
   UStrJoin(
      char                 * target      ,// out-target string
      size_t                 sizeTarget  ,// in -maximum size of target in chars
      char const           * source1     ,// in -first source string or NULL
      ...                                 // in -remainder of source strings
   )
{
   va_list                   nextArg;
   char const              * s;
   char                    * t;
   size_t                    len;

   if ( source1 == NULL )
      target[0] = '\0';
   else
   {
      t = target;
      for ( va_start(nextArg, source1), s = source1;  
            s  &&  sizeTarget > 1;  
            s = va_arg(nextArg, char const *), sizeTarget -= len )
		{
         len = strlen(s);
         if ( len >= sizeTarget - 1 )
         {
            len = sizeTarget - 1;
         }
         memcpy(t, s, len);
         t += len;
      }							
      va_end(nextArg);
      if ( sizeTarget > 0 )
         t[0] = '\0';
   }
   return target;
}

WCHAR * _cdecl                            // ret-target string
   UStrJoin(
      WCHAR                * target      ,// out-target string
      size_t                 sizeTarget  ,// in -maximum size of target in chars
      WCHAR const          * source1     ,// in -first source string or NULL
      ...                                 // in -remainder of source strings
   )
{
   va_list                   nextArg;
   WCHAR const             * s;
   WCHAR                   * t;
   size_t                    len;

   if ( source1 == NULL )
      target[0] = L'\0';
   else
   {
      t = target;
      for ( va_start(nextArg, source1), s = source1;  
            s  &&  sizeTarget > 1;  
            s = va_arg(nextArg, WCHAR const *), sizeTarget -= len )
		{
         len = wcslen(s);
         if ( len >= sizeTarget - 1 )
         {
            len = sizeTarget - 1;
         }
         memcpy(t, s, len * sizeof *s);
         t += len;
      }							
      va_end(nextArg);
      if ( sizeTarget > 0 )
         t[0] = L'\0';
   }
   return target;
}
