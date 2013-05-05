// myscheduler.cc ^^
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "myscheduler.h"
#include "main.h"


class SchedulerRoundRobin : public CallBackObj {

    public:

        SchedulerRoundRobin(int t, char* threadName){
            timeslice = t;
            name = string(threadName);
        }
        ~SchedulerRoundRobin(){};
        int timeslice;
        string name;
        void CallBack(){
            cout << "this is callback" << endl;
            if(name = string(kernel->currentThread->getName()) )
                kernel->currentThread->Yield();
        }
};

void 
MyScheduler::SetCallback(int timeslice){ 
    callback = new SchedulerRoundRobin(timeslice, string(kernel->currentThread->getName())); 
}


void 
MyScheduler::ScheduleInterrupt(){ 
    if(callback!=NULL) 
        kernel->interrupt->Schedule(callback, timeslice, TimerInt);
}


//      returns -1 if x < y
//      returns 0 if x == y
//      returns 1 if x > y
int CompareThreads(Thread* x, Thread* y){
    if( (x->GetPriority()) < (y->GetPriority()) )
        return -1;
    if( (x->GetPriority()) == (y->GetPriority()) )
        return 0;
    if( (x->GetPriority()) > (y->GetPriority()) )
        return 1;

    return 0;
}


//----------------------------------------------------------------------
// MyScheduler::MyScheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

MyScheduler::MyScheduler()
{ 
    readyList = new SortedList<Thread *>( CompareThreads ); 
    toBeDestroyed = NULL;
    callback = NULL;
} 

//----------------------------------------------------------------------
// MyScheduler::~MyScheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

MyScheduler::~MyScheduler()
{ 
    delete readyList; 
} 

//----------------------------------------------------------------------
// MyScheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
MyScheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());

    thread->setStatus(READY);
    readyList->Insert(thread);

}

//----------------------------------------------------------------------
// MyScheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
MyScheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    if (readyList->IsEmpty()) {
	return NULL;
    } else {
    	return readyList->RemoveFront();
    }
}

//----------------------------------------------------------------------
// MyScheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
MyScheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    ScheduleInterrupt();
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// MyScheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
MyScheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// MyScheduler::Print
// 	Print the myscheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
MyScheduler::Print()
{
    // cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
    cout << endl;

}
