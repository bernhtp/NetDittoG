/*
===============================================================================
Module      -  TSync.hpp
System      -  Common
Author      -  Rich Denham
Created     -  96/11/08
Description -  Common synchronization classes header file
               This includes TCriticalSection, TEvent, TMutex, and TSharedSync.
Updates     -
===============================================================================
*/

class TCriticalSection
{
   CRITICAL_SECTION          cs;
public:
                        TCriticalSection() { InitializeCriticalSection(&cs); }
                        ~TCriticalSection() { DeleteCriticalSection(&cs); }
   void                 Enter() { EnterCriticalSection(&cs); }
   void                 Leave() { LeaveCriticalSection(&cs); }
};

class TSynchObject
{
public:
   HANDLE                     handle;
                        ~TSynchObject() { CloseHandle(handle); }
   DWORD                WaitSingle(DWORD msec=INFINITE) const { return WaitForSingleObject(handle, msec); }
   HANDLE               Handle() { return handle; }
};

class TEvent : public TSynchObject
{
public:
                        TEvent(BOOL initialSignalState = FALSE, BOOL manualSet = TRUE);
   BOOL                 Set()   const { return SetEvent(handle); }
   BOOL                 Reset() const { return ResetEvent(handle); }
};

// The following class is being phased out and replaced by the more generic TEvent,
// hence, the extremely "thin" re-implementation.
class TAutoEvent : public TEvent
{
public:
                        TAutoEvent(BOOL initialSignalState = FALSE)
                        : TEvent(initialSignalState, FALSE) {}
};

class TMutex : public TSynchObject
{
public:
                        TMutex(BOOL initialOwn = TRUE);
   BOOL                 Release()   const { return ReleaseMutex(handle); }
};

// TSharedSync class

// An instantiation of this class represents a resource object that can
// support MULTIPLE READERS or ONE WRITER.

// Code executed between EnterExclusive() and LeaveExclusive() represent a writer.
//    Execution is blocked within EnterExclusive() until there is no other writer
//    and no reader.
//    Execution of LeaveExclusive() may cause another thread waiting in EnterExclusive() or
//    EnterShared() to unblock.

// Code executed between EnterShared() and LeaveShared() represent a reader.
//    Execution is blocked within EnterShared() until there is no writer.
//    Execution of EnterShared() may cause another thread waiting in EnterExclusive() or
//    EnterShared() to unblock.

// Member 'csExc' represents the exclusive enqueue resource.  It is owned by
//    a writer thread.  It is also owned during the execution of EnterShared().

// Member 'nShr' contains the number of concurrent reader threads.

// Member 'eventShr' represents 'nShr' in a manner that can be waited upon.
//    When 'nShr' is zero, 'eventShr' is signaled.
//    When 'nShr' is non-zero, 'eventShr' is nonsignaled.

// Member 'csShr' represents permission to modify 'nShr' or 'eventShr'.

class TSharedSync
{
private:
   TCriticalSection          csExc;        // Exclusive enqueue control
   TCriticalSection          csShr;        // Shared enqueue control
   TEvent                    eventShr;     // Shared enqueue event
   long                      nShr;         // Shared enqueue count
public:
   TSharedSync()
   {
      nShr = 0;
      eventShr.Set();
   }
   ~TSharedSync()
   {
   }
   void EnterExclusive()
   {
      csExc.Enter();
      eventShr.WaitSingle();
   }   
   void LeaveExclusive()
   {
      csExc.Leave();
   }
   void EnterShared()
   {
      csExc.Enter();
      csShr.Enter();
      if ( ++nShr == 1 )
         eventShr.Reset();
      csShr.Leave();
      csExc.Leave();
   }
   void LeaveShared()
   {
      csShr.Enter();
      if ( --nShr == 0 )
         eventShr.Set();
      csShr.Leave();
   }
};

// TSync.hpp - end of file


