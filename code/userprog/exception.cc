// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

// #include "copyright.h"
#include "../threads/main.h"
#include "syscall.h"
#include "ksyscall.h"
#include "addrspace.h"
#include <map>
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	is in machine.h.
//----------------------------------------------------------------------

std::map<int, Thread*> parentChild;

void Exit_POS(int tId){
        IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
	if(parentChild.find(tId) != parentChild.end()){
		int count = 0;
		Thread *t = parentChild[tId];
		std::map<int,Thread *>::iterator it;
		for (it = parentChild.begin(); it != parentChild.end(); ++it) {
      		if (it->second == t) {
         		count++;
      		}
   		}
                //IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
		if(count == 1){
			if(!kernel->scheduler->getReadyList().IsInList(t)){
				kernel->scheduler->ReadyToRun(t);
                                //kernel->currentThread->Yield();
				//t->Finish();
                                //t->Sleep(TRUE);
			}
		}

                kernel->currentThread->Sleep(TRUE);
                //(void) kernel->interrupt->SetLevel(oldLevel);
		parentChild.erase(tId);
	}

        /* set previous programm counter (debugging only)*/
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

          /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

          /* set next programm counter for brach execution */
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          (void) kernel->interrupt->SetLevel(oldLevel);
}

void ForkTest1(int id)
{
	printf("ForkTest1 is called, its PID is %d\n", id);
	for (int i = 0; i < 3; i++)
	{
		printf("ForkTest1 is in loop %d\n", i);
		for (int j = 0; j < 100; j++) 
			kernel->interrupt->OneTick();
	}
	Exit_POS(id);
}

void ForkTest2(int id)
{
	printf("ForkTest2 is called, its PID is %d\n", id);
	for (int i = 0; i < 3; i++)
	{
		printf("ForkTest2 is in loop %d\n", i);
		for (int j = 0; j < 100; j++) 
			kernel->interrupt->OneTick();
	}
	Exit_POS(id);
}

void ForkTest3(int id)
{
	printf("ForkTest3 is called, its PID is %d\n", id);
	for (int i = 0; i < 3; i++)
	{
		printf("ForkTest3 is in loop %d\n", i);
		for (int j = 0; j < 100; j++) 
			kernel->interrupt->OneTick();
	}
	Exit_POS(id);
}

void SysPageFaultHandler(int badVirtualAddr){
	int vpn = (unsigned) badVirtualAddr / PageSize;
	int ppn = kernel->availablePages->FindAndSet();
	char *buffer = new char[PageSize];
	TranslationEntry *e = kernel->currentThread->space->getTranslationEntryByVPN(vpn);
	if(ppn == -1){
		// ***** evict page from memory and move it to swap file
		int removeFirstPageNumber = kernel->physicalPageNumbers->RemoveFront();
		Thread *t = kernel->ppnThreadMap[removeFirstPageNumber];
		TranslationEntry *ent = t->space->getTranslationEntryByPPN(removeFirstPageNumber);
		int swapLoc = ent->swapLoc;
		kernel->swapFile->WriteAt(&kernel->machine->mainMemory[removeFirstPageNumber * PageSize], 
					PageSize, swapLoc * PageSize);
		// ***** update pagetable entry for this page
		ent->physicalPage = -1;
		ent->valid = FALSE;
		ent->use = FALSE;
		// ***** set ppn to new page number
		kernel->availablePages->Clear(removeFirstPageNumber);
		ppn = kernel->availablePages->FindAndSet();
	}

	kernel->swapFile->ReadAt(buffer, PageSize, e->swapLoc * PageSize);
	// ***** copy from buffer to main memory i.e. copyFromBufToMainMem(buff, &mainMemory, 
        //              start loc i.e e->physicalPage * PageSize)
	// ***** update pageTable entry e
	e->valid = TRUE;
	e->use = TRUE;
	e->physicalPage = ppn;
	bcopy(buffer, &(kernel->machine->mainMemory[e->physicalPage * PageSize]), PageSize);
	
//	kernel->currentThread->space->copyFromBufToMainMem(buffer, (e.physicalPage * PageSize));
	// ***** update map and list
	kernel->ppnThreadMap.insert(std::pair<int, Thread *> (kernel->currentThread->space->pageTable[vpn].physicalPage, 
											kernel->currentThread));
        kernel->physicalPageNumbers->Append(e->physicalPage);
}

void
ExceptionHandler(ExceptionType which)
{
    int type = kernel->machine->ReadRegister(2);

    DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

    switch (which) 
{
    case SyscallException:
      switch(type) {
      case SC_Halt:
        {
	DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

	SysHalt();

	ASSERTNOTREACHED();
	break;
        }
	case SC_Fork:
                {
                IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);	
		DEBUG(dbgSys, "Thread fork, initiated by user program.\n");

		// int result = SysForkPOS((int)kernel->machine->ReadRegister(4));
		int i = (int)kernel->machine->ReadRegister(4);
		Thread *cThread = new Thread("Forked child thread");
  		if(i == 1)
    		cThread->Fork((VoidFunctionPtr)ForkTest1, (void *) i);
  		else if(i == 2)
    		cThread->Fork((VoidFunctionPtr)ForkTest2, (void *) i);
  		else
    		cThread->Fork((VoidFunctionPtr)ForkTest3, (void *) i);

		kernel->machine->WriteRegister(2, (int)cThread->getId());
                kernel->currentThread->Yield();
	        {
	  /* set previous programm counter (debugging only)*/
	  kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

	  /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
	  kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
	  
	  /* set next programm counter for brach execution */
	  kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);

                (void) kernel->interrupt->SetLevel(oldLevel);
	}

	return;
        	ASSERTNOTREACHED();
		break;
                }
	case SC_Wait:
                {
		DEBUG(dbgSys, "Thread wait, initiated by user program.\n");
                IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
		int child = (int)kernel->machine->ReadRegister(4);
		parentChild.insert(pair <int, Thread*> (child, kernel->currentThread));
                //kernel->currentThread->Yield();
		//kernel->currentThread->Sleep(FALSE);
                {
	  /* set previous programm counter (debugging only)*/
	  kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

	  /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
	  kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
	  
	  /* set next programm counter for brach execution */
	  kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
	}

                (void) kernel->interrupt->SetLevel(oldLevel);
	return;	
	ASSERTNOTREACHED();
		break;
                }
	case SC_Write:
                {
		int ch;
		DEBUG(dbgSys, "Thread wait, initiated by user program.\n");
                IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
		int p1 = (int)kernel->machine->ReadRegister(4);
		int lenOfStr = (int)kernel->machine->ReadRegister(5);
                ch = p1;
		for(int i=0; i<lenOfStr; i++){
			if(kernel->machine->ReadMem(p1, 1, &ch))
				printf("%c", ch);
			p1++;
		}
                
                std::cout << endl;
                {
	  /* set previous programm counter (debugging only)*/
	  kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

	  /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
	  kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
	  
	  /* set next programm counter for brach execution */
	  kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
	}

                (void) kernel->interrupt->SetLevel(oldLevel);
	return;	
	ASSERTNOTREACHED();
		break;
                }
    case SC_Add:
        {
	DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");
	
	/* Process SysAdd Systemcall*/
	int result;
	result = SysAdd(/* int op1 */(int)kernel->machine->ReadRegister(4),
			/* int op2 */(int)kernel->machine->ReadRegister(5));

	DEBUG(dbgSys, "Add returning with " << result << "\n");
	/* Prepare Result */
	kernel->machine->WriteRegister(2, (int)result);
	
	/* Modify return point */
	{
	  /* set previous programm counter (debugging only)*/
	  kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

	  /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
	  kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
	  
	  /* set next programm counter for brach execution */
	  kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
	}

	return;
	
	ASSERTNOTREACHED();

	break;
        }
      case SC_Exit:
      {
          
          //kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

          /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
          //kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

          /* set next programm counter for brach execution */
          //kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
          kernel->currentThread->Finish();  
          (void) kernel->interrupt->SetLevel(oldLevel);       
          return;

          ASSERTNOTREACHED();

          break;
      }
      default:
	cerr << "Unexpected system call " << type << "\n";
	break;
      }
      break;

      case PageFaultException:
      {
                IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
		SysPageFaultHandler((int)kernel->machine->ReadRegister(39));
		/* set previous programm counter (debugging only)*/
		//kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

		/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
		//kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

		/* set next programm counter for brach execution */
		//kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
		(void)kernel->interrupt->SetLevel(oldLevel);
		return;
		break;      
      }
    default:
      cerr << "Unexpected user mode exception" << (int)which << "\n";
      break;
    }

    ASSERTNOTREACHED();
	
}
