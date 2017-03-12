/*
===============================================================================
Module      -  TSync.cpp
System      -  Common
Author      -  Rich Denham
Created     -  96/11/08
Description -  Common synchronization classes
               This includes TCriticalSection, TEvent, TMutex, and TSharedSync.
Updates     -
===============================================================================
*/

#include <stdio.h>
#include <windows.h>
#include "Common.hpp"
#include "Err.hpp"
#include "TSync.hpp"

TEvent::TEvent(BOOL initialSignalState, BOOL manualSet)
{
   handle = CreateEvent(NULL, manualSet, initialSignalState, NULL);
   if ( handle == INVALID_HANDLE_VALUE )
   {
      DWORD                  rc = GetLastError();

      errCommon.SysMsgWrite(ErrU, rc, L"CreateEvent()=%d, ", rc );
   }
}

// constructor for simple unnamed and unsecured mutex
TMutex::TMutex(
      BOOL                   initialOwn
   )
{
   handle = CreateMutex(NULL, initialOwn, NULL);
   if ( handle == INVALID_HANDLE_VALUE )
   {
      // this really can't happen unless the system is hosed
      DWORD                  rc = GetLastError();

      errCommon.SysMsgWrite(ErrU, rc, L"CreateMutex()=%ld, ", rc );
   }
}

// TSync.cpp - end of file


