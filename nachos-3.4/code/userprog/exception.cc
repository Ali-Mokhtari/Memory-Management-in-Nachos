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
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include <stdio.h>        // FA98
#include <stdlib.h>
#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "addrspace.h"   // FA98
#include "sysdep.h"   // FA98

// begin FA98


static int SRead(int addr, int size, int id);
static void SWrite(char *buffer, int size, int id);
Thread * getID(int toGet);

/* Begin Changes made by ACM Group  */

Thread * IPT[NumPhysPages];

/* End Changes made by ACM Group  */

// end FA98

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
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

Thread* getID(int toGet)	// Goes through the list of active threads and returns one linked with the passed-in ID.
{
	Thread * tempThread = NULL;
	Thread * toReturn = NULL;
	bool found = false;
	int size = activeThreads->getSize();
	for(int i = 0; i < size; i++)
	{
		tempThread = (Thread*)activeThreads->Remove();	// Pop the top thread off.
		if (tempThread->getID() == toGet)	// If it's what we're looking for...
		{
			toReturn = tempThread;
			found = true;	// Trip the flag variable, and store the pointer of the thread.
		}
		activeThreads->Append(tempThread);	// Put it back onto the active list.
	}
	if (!found)
		return NULL;
	else return toReturn;
}
	
void processCreator(int arg)	// Used when a process first actually runs, not when it is created.
 {
	currentThread->space->InitRegisters();		// set the initial register values
    currentThread->space->RestoreState();		// load page table register
	
	if (threadToBeDestroyed != NULL){
		delete threadToBeDestroyed;
		threadToBeDestroyed = NULL;
	}

    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
 }

void
ExceptionHandler(ExceptionType which)
{
	int type = machine->ReadRegister(2);

	int arg1 = machine->ReadRegister(4);
	int arg2 = machine->ReadRegister(5);
	int arg3 = machine->ReadRegister(6);
	int Result;
	int i, j;
	char *ch = new char [500];

	switch ( which )
	{
	case NoException :
		break;

	
	/* Begin Changes made by ACM Group  */

	case PageFaultException:
		{
		stats->numPageFaults++;
		printf("Page Fault : #%d \n", stats->numPageFaults);
		int badVAddr = machine->ReadRegister(BadVAddrReg);
		int virtPageNum = badVAddr / PageSize; 
		printf(" -- Process %d requested virtual page %d \n", currentThread->getID(), virtPageNum);
		int emptyPhysPage = bitMap->Find();		
		OpenFile *swapFile = fileSystem->Open(currentThread->space->swapFileName); 

		if (emptyPhysPage != -1){
			
			currentThread->space->pageTable[virtPageNum].physicalPage = emptyPhysPage;
			printf("  --- Free physical page#%d assigned to process %d\n", emptyPhysPage, currentThread->getID()) ; 			
			swapFile->ReadAt(&(machine->mainMemory[emptyPhysPage*PageSize]), PageSize, virtPageNum*PageSize); 
			currentThread->space->pageTable[virtPageNum].valid = TRUE;
			// update IPT
			//printf("Inverted Page Table [ %d ] is updated to be used by thread %d \n",emptyPhysPage, currentThread->getID() );
			IPT[emptyPhysPage]=currentThread;
			int* x = new int (emptyPhysPage);
			//void * pEmptyPage = &emptyPhysPage;
			void * pEmptyPage = (void*) x ;
			//printf("Address of free page :%p \n", pEmptyPage);

			FIFOList->Append(pEmptyPage);			
			//printf("\n\nFirst = %d , Last = %d\n", * ((int*) FIFOList->getFirst()->item) ,* ((int*) FIFOList->getLast()->item) );
			
		}
		else{
			printf("  --- No free page is available \n");
			// go for FIFO or Random Page replacement
			
			if (pageReplacementAlg == 1){
				
				void * FirstPage =  FIFOList->Remove(); 
				FIFOList->Append(FirstPage);
				

				int* fpage = (int*) FirstPage; 
				int firstPage = *fpage; 
				int virtualPageReplaced;
				
				for (int i = 0; i < IPT[firstPage]->space->numPages; i++)
				{
					if (IPT[firstPage]->space->pageTable[i].physicalPage == firstPage) 
						virtualPageReplaced = i ;
				}
				printf("Page %d is the candidate for page replacement \n", firstPage);
				printf("Virtual page %d  of thread %d is going to be swapped out from MM \n", virtualPageReplaced, IPT[firstPage]->getID());
				// if dirt is true 
				if (IPT[firstPage]->space->pageTable[virtualPageReplaced].dirty == TRUE){
					printf("Page is dirty \n");
					OpenFile *replacedFile = fileSystem->Open(IPT[firstPage]->space->swapFileName);
					replacedFile->WriteAt(&(machine->mainMemory[firstPage*PageSize]), PageSize, (IPT[firstPage]->space->pageTable[virtualPageReplaced].virtualPage)*PageSize);
					printf("data swapped out from MM to %s \n", IPT[firstPage]->space->swapFileName);
					IPT[firstPage]->space->pageTable[virtualPageReplaced].valid = FALSE;
					delete replacedFile;
				}
				
				swapFile->ReadAt(&(machine->mainMemory[firstPage*PageSize]), PageSize, virtPageNum*PageSize);  
				currentThread->space->pageTable[virtPageNum].valid = TRUE;
				currentThread->space->pageTable[virtPageNum].physicalPage = firstPage;
				// update IPT
				IPT[firstPage]=currentThread;

			}



			else if (pageReplacementAlg == 2 ){// Random replacement: 

				int rndPage = rand() % 32;
				//printf("random page = %d \n", rndPage);
				int virtualPageReplaced;
				for (int i = 0; i < IPT[rndPage]->space->numPages; i++)
				{
					if (IPT[rndPage]->space->pageTable[i].physicalPage == rndPage){
						int virtualPageReplaced = i;
					   // break;
					}
				}
				printf("Page %d is the candidate for page replacement \n", rndPage);
				printf("Virtual page %d  of thread %d is going to be swapped out from MM \n", virtualPageReplaced, IPT[rndPage]->getID());
				// if dirt is true 
				if (IPT[rndPage]->space->pageTable[virtualPageReplaced].dirty == TRUE){
					printf("Page is dirty \n");
					OpenFile *replacedFile = fileSystem->Open(IPT[rndPage]->space->swapFileName);
					replacedFile->WriteAt(&(machine->mainMemory[rndPage*PageSize]), PageSize, (IPT[rndPage]->space->pageTable[virtualPageReplaced].virtualPage)*PageSize);
					printf("data swapped out from MM to %s \n", IPT[rndPage]->space->swapFileName);
					IPT[rndPage]->space->pageTable[virtualPageReplaced].valid = FALSE;

					delete replacedFile;
				}
				
				swapFile->ReadAt(&(machine->mainMemory[rndPage*PageSize]), PageSize, virtPageNum*PageSize); 
				currentThread->space->pageTable[virtPageNum].physicalPage = rndPage; 
				currentThread->space->pageTable[virtPageNum].valid = TRUE;
				// update IPT
				IPT[rndPage]=currentThread;
			}
			else{
				printf(" The process is terminated \n");
				delete currentThread->space;
				currentThread->Finish() ;

			}
			// printf("BitMap After PageFaulException Replace/Allocate\n");
			// bitMap->Print();
		}
		}
		break;
	
	/*End Changes made by ACM Group*/

	case SyscallException :

		// for debugging, in case we are jumping into lala-land
		// Advance program counters.
		machine->registers[PrevPCReg] = machine->registers[PCReg];
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[NextPCReg] + 4;

		switch ( type )
		{

		case SC_Halt :
			printf("SYSTEM CALL: Halt, called by thread %i.\n",currentThread->getID());
			DEBUG('t', "Shutdown, initiated by user program.\n");
			interrupt->Halt();
			break;

			
		case SC_Read :
			if (arg2 <= 0 || arg3 < 0){
				printf("\nRead 0 byte.\n");
			}
			Result = SRead(arg1, arg2, arg3);
			machine->WriteRegister(2, Result);
			DEBUG('t',"Read %d bytes from the open file(OpenFileId is %d)",
			arg2, arg3);
			break;

		case SC_Write :
			for (j = 0; ; j++) {
				if(!machine->ReadMem((arg1+j), 1, &i))
					j=j-1;
				else{
					ch[j] = (char) i;
					if (ch[j] == '\0') 
						break;
				}
			}
			if (j == 0){
				printf("\nWrite 0 byte.\n");
				// SExit(1);
			} else {
				DEBUG('t', "\nWrite %d bytes from %s to the open file(OpenFileId is %d).", arg2, ch, arg3);
				SWrite(ch, j, arg3);
			}
			break;
		case SC_Exec :	// Executes a user process inside another user process.
		   {
				printf("SYSTEM CALL: Exec, called by thread %i.\n",currentThread->getID());

				// Retrieve the address of the filename
				int fileAddress = arg1; // retrieve argument stored in register r4

				// Read file name into the kernel space
				char *filename = new char[100];
				
				for(int m = 0; m < 100; m++)
					filename[m] = NULL;

				// Free up allocation space and get the file name
				if(!machine->ReadMem(fileAddress,1,&j))return;
				i = 0;
				while(j != 0)
				{
					filename[i]=(char)j;
					fileAddress += 1;
					i++;
					if(!machine->ReadMem(fileAddress,1,&j))return;
				}
				// Open File
				OpenFile *executable = fileSystem->Open(filename);
				
				if (executable == NULL) 
				{
					printf("Unable to open file %s\n", filename);
					delete filename;
					break;
				}
				delete filename;

				// Calculate needed memory space
				AddrSpace *space;
				/*Begin  Changes made by ACM Group*/
				space = new AddrSpace(executable, threadID);
				/*End Changes made by ACM Group*/

				delete executable;
				// Do we have enough space?
				if(!currentThread->killNewChild)	// If so...
				{
					Thread* execThread = new Thread("thrad!");	// Make a new thread for the process.
					execThread->space = space;	// Set the address space to the new space.
					execThread->setID(threadID);	// Set the unique thread ID
					activeThreads->Append(execThread);	// Put it on the active list.
					machine->WriteRegister(2, threadID);	// Return the thread ID as our Exec return variable.
					threadID++;	// Increment the total number of threads.
					execThread->Fork(processCreator, 0);	// Fork it.
				}
				else	// If not...
				{
					machine->WriteRegister(2, -1 * (threadID + 1));	// Return an error code
					currentThread->killNewChild = false;	// Reset our variable
				}
				break;	// Get out.
			}
			case SC_Join :	// Join one process to another.
			{
				printf("SYSTEM CALL: Joined, called by thread %i.\n",currentThread->getID());
				if(arg1 < 0)	// If the thread was not properly created...
				{
					printf("ERROR: Trying to join process %i to process %i, which was not created successfully! Process %i continuing normally.\n", currentThread->getID(), -arg1, currentThread->getID());	// Return an error message, continue as normal.
					break;
				}
				
				if(getID(arg1) != NULL)	// If the thread exists...
				{
					if(!currentThread->isJoined)	// And it's not already joined...
					{
						printf("Joining process %i with process %i.  Thread %i now shutting down.\n", getID(arg1)->getID(), currentThread->getID(), currentThread->getID());	// Inform the user.
						getID(arg1)->setParent(currentThread);	// Set the process' parent to the current thread.
						currentThread->isJoined = true;	// Let the parent know it has a child
						(void) interrupt->SetLevel(IntOff);	// Disable interrupts for Sleep();
						currentThread->Sleep();	// Put the current thread to sleep.
						break;
					}
					else{	// We've got an error message.
						printf("ERROR: Trying to join process %i, which is already joined! Continuing normally.", currentThread->getID());
						break;
						}
				}
				else printf("ERROR: Trying to a join process %i to nonexistant process %i! Process %i continuing normally.\n", currentThread->getID(), -arg1, currentThread->getID());	// Error message if the thread we're trying to join to doesn't exist for some reason.
				break;
			}
			case SC_Exit :	// Exit a process.
			{
				printf("SYSTEM CALL: Exit, called by thread %i.\n",currentThread->getID());				
				printf(" Process %i exited with the value of %d \n\n", currentThread->getID(), arg1);
				
				List* tempList = new List();
				if(currentThread->space){	// Delete the used memory from the process.
						
					ListElement* first = FIFOList->getFirst() ;									
					int it = 0;
					int * element;
					//printf("FIFO before update: \n");
					for (ListElement *ptr = first; ptr != NULL; ptr = ptr->next){
						element = (int*) ptr->item;
						//printf("FIFO[%d] = %d \n ", it, *element);
						it++;
					}

					it = 0 ;
					first = FIFOList->getFirst() ;
					int FIFOSize = FIFOList->getSize();
					for (it = 0 ; it < FIFOSize ; it++){
						//printf("item = %d \n ", it);
						element = (int*) FIFOList->getFirst()->item;						
						void* removedItem = FIFOList->Remove();
						
						
						//printf("page %d removed from FIFO \n", *((int*) removedItem));						
						if (IPT[*element] != currentThread){														
							tempList->Append(removedItem);
							//printf("tempList last: %d \n",  *((int*) tempList->getLast()->item)) ; 
						}
					}
					
					it = 0;
					ListElement* uFirst = tempList->getFirst() ;
					for (ListElement *ptr = uFirst; ptr != NULL; ptr = ptr->next){
						element = (int*) ptr->item;
						//printf("FIFO[%d] = %d \n ", it, *element);
						FIFOList->Append(ptr->item);						
						it++;
					}
					delete tempList;
					//printf("\n\n\n Updated FIFO: current size = %d \n", FIFOList->getSize());
					it = 0 ;
					ListElement* FFirst = FIFOList->getFirst() ;
					for (ListElement *ptr = FFirst; ptr != NULL; ptr = ptr->next){
						element = (int*) ptr->item;
						//printf("FIFO[%d] = %d \n ", it, *element);
						it++;
					}


					
					
					
					for (int i = 0; i < NumPhysPages; i++)
					{	
						
						if (IPT[i] == currentThread){
							//printf("Bitmap of thread %d is deleted \n", currentThread->getID());
							bitMap->Clear(i);
							//bitMap->Print();
						}
					}
					bitMap->Print();
				}
				printf(" Thread %d finished \n\n", currentThread->getID());
				
				fileSystem->Remove(currentThread->space->swapFileName);
				delete currentThread->space;
				currentThread->Finish();	// Delete the thread.
				break;
			}
           case SC_Yield :	// Yield to a new process.
		   {
			   printf("SYSTEM CALL: Yield, called by thread %i.\n",currentThread->getID());

			   //Save the registers and yield CPU control.
			   currentThread->space->SaveState();
			   currentThread->Yield();
			   //When the thread comes back, restore its registers.
			   currentThread->space->RestoreState();

               break;
			}
           default :
	       //Unprogrammed system calls end up here
			   printf("SYSTEM CALL: Unknown, called by thread %i.\n",currentThread->getID());
               break;
           }         // Advance program counters, ends syscall switch
           break;

	case ReadOnlyException :
		printf("ERROR: ReadOnlyException, called by thread %i.\n",currentThread->getID());
		if (currentThread->getName() == "main")
			ASSERT(FALSE);  //Not the way of handling an exception.
		if(currentThread->space)	// Delete the used memory from the process.
			delete currentThread->space;
		currentThread->Finish();	// Delete the thread.
		break;
	case BusErrorException :
		printf("ERROR: BusErrorException, called by thread %i.\n",currentThread->getID());
		if (currentThread->getName() == "main")
			ASSERT(FALSE);  //Not the way of handling an exception.
		if(currentThread->space)	// Delete the used memory from the process.
			delete currentThread->space;
		currentThread->Finish();	// Delete the thread.
		break;
	case AddressErrorException :
		printf("ERROR: AddressErrorException, called by thread %i.\n",currentThread->getID());
		if (currentThread->getName() == "main")
			ASSERT(FALSE);  //Not the way of handling an exception.
		if(currentThread->space)	// Delete the used memory from the process.
			delete currentThread->space;
		currentThread->Finish();	// Delete the thread.
		break;
	case OverflowException :
		printf("ERROR: OverflowException, called by thread %i.\n",currentThread->getID());
		if (currentThread->getName() == "main")
			ASSERT(FALSE);  //Not the way of handling an exception.
		if(currentThread->space)	// Delete the used memory from the process.
			delete currentThread->space;
		currentThread->Finish();	// Delete the thread.
		break;
	case IllegalInstrException :
		printf("ERROR: IllegalInstrException, called by thread %i.\n",currentThread->getID());
		if (currentThread->getName() == "main")
			ASSERT(FALSE);  //Not the way of handling an exception.
		if(currentThread->space)	// Delete the used memory from the process.
			delete currentThread->space;
		currentThread->Finish();	// Delete the thread.
		break;
	case NumExceptionTypes :
		printf("ERROR: NumExceptionTypes, called by thread %i.\n",currentThread->getID());
		if (currentThread->getName() == "main")
			ASSERT(FALSE);  //Not the way of handling an exception.
		if(currentThread->space)	// Delete the used memory from the process.
			delete currentThread->space;
		currentThread->Finish();	// Delete the thread.
		break;

		default :
		//      printf("Unexpected user mode exception %d %d\n", which, type);
		//      if (currentThread->getName() == "main")
		//      ASSERT(FALSE);
		//      SExit(1);
		break;
	}
	delete [] ch;
}


static int SRead(int addr, int size, int id)  //input 0  output 1
{
	char buffer[size+10];
	int num,Result;

	//read from keyboard, try writing your own code using console class.
	if (id == 0)
	{
		scanf("%s",buffer);

		num=strlen(buffer);
		if(num>(size+1)) {

			buffer[size+1] = '\0';
			Result = size+1;
		}
		else {
			buffer[num+1]='\0';
			Result = num + 1;
		}

		for (num=0; num<Result; num++)
		{  machine->WriteMem((addr+num), 1, (int) buffer[num]);
			if (buffer[num] == '\0')
			break; }
		return num;

	}
	//read from a unix file, later you need change to nachos file system.
	else
	{
		for(num=0;num<size;num++){
			Read(id,&buffer[num],1);
			machine->WriteMem((addr+num), 1, (int) buffer[num]);
			if(buffer[num]=='\0') break;
		}
		return num;
	}
}



static void SWrite(char *buffer, int size, int id)
{
	//write to terminal, try writting your own code using console class.
	if (id == 1)
	printf("%s", buffer);
	//write to a unix file, later you need change to nachos file system.
	if (id >= 2)
	WriteFile(id,buffer,size);
}
// end FA98

