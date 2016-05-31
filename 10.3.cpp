                
#define _CRT_NON_CONFORMING_SWPRINTFS
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>

#define L 50000
#define NAME  100

#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_DOT 3

#define MAX_SEM_COUNT 10

HANDLE mutex;
HANDLE Semaphore;
HANDLE cSemaphore;
HANDLE fSemaphore;

typedef struct Threads
{
	DWORD thId;
	TCHAR name[NAME];
	DWORD level;
	DWORD equal;
}Threads_t;

typedef struct Threads_cc
{
	DWORD thId;
	DWORD nCount;
	//Threads_t th[MAX_SEM_COUNT];
	Threads_t *th;
	boolean equal;
}Threads_c;

DWORD WINAPI Compare(LPVOID);
DWORD WINAPI TraverseDirectoryRecursive(LPVOID);
DWORD FileType (LPWIN32_FIND_DATA pFileData);

int _tmain (int argc, LPTSTR argv [])
{
	HANDLE *ThreadHandle;
	int i=0;
	Threads_t *thread;
	Threads_c thread_c;

	thread_c.equal = 0;
	ThreadHandle = (HANDLE *)malloc(sizeof(HANDLE)*(argc-1));
	thread = (Threads_t *)malloc(sizeof(Threads_t)*(argc-1));
	thread_c.th = (Threads_t *)malloc(sizeof(Threads_t)*(argc-1));

	/// Create a semaphore with initial and max counts of MAX_SEM_COUNT/Create a mutex with no initial owner
	Semaphore = CreateSemaphore(
			NULL, // default security attributes
			0,// initially not owned
			MAX_SEM_COUNT,//maximum count
			NULL// unnamed semaphore
		);
	cSemaphore = CreateSemaphore(
			NULL, // default security attributes
			0,// initially not owned
			MAX_SEM_COUNT,//maximum count
			NULL// unnamed semaphore
		);
	//for main_thread informs the completement of other threads to the comparing thread
	fSemaphore = CreateSemaphore(
			NULL, // default security attributes
			0,// initially not owned
			1,//maximum count
			NULL// unnamed semaphore
		);
	if(Semaphore == NULL)
	{
		_tprintf(_T("CreateSemaphore error:%d\n"),GetLastError());
		Sleep(1000000);
		return 1;
	}

	for(i=1;i<argc;i++){
		_tprintf(_T("argv[%d]=%s "),i,argv[i]);
	}

	thread_c.nCount = argc-1;

	for(i=0;i<argc-1;i++){
		//thread[i].name=NULL;
		memset(thread[i].name,0,NAME);
		_tprintf(_T("\n-->Visit Dir %s\n"),argv[i]);
		//_stprintf(thread[i].name,_T("%s"),thread[i].name);
		_tcscpy(thread[i].name,argv[i]);
		thread[i].level = 1;
		
		ThreadHandle[i] = CreateThread(
			0,0,(LPTHREAD_START_ROUTINE)TraverseDirectoryRecursive,(PVOID)&thread[i],0,&thread[i].thId);
		thread_c.th[i] = thread[i];
	}
	ThreadHandle[i] = CreateThread(
		0,0,(LPTHREAD_START_ROUTINE)Compare,(PVOID)&thread_c,0,&thread_c.thId);


	DWORD re = WaitForMultipleObjects(argc-1,ThreadHandle,TRUE,INFINITE);
	if(re>=WAIT_OBJECT_0 && re<=(WAIT_OBJECT_0+argc-2))
	_tprintf(_T("all %d threads read finish!\n"),argc-1);
	else {
		_tprintf(_T("error in waiting %d threads!\n"),argc-1);
	}

	//inform the comparison thread
	if(!ReleaseSemaphore(
			fSemaphore,
			1,
			NULL
		))
	{
		_tprintf(_T("ReleaseSemaphore error:%d\n"),GetLastError());
	}
	else
	{
		_tprintf(_T(" have release semaphore,waiting now\n"));
	}
	WaitForSingleObject(ThreadHandle[i],INFINITE);
	//check the result
	if(thread_c.equal==1)
		_tprintf(_T("not all directories have same content\n"));
	else
		_tprintf(_T("all threads have same content\n"));

	Sleep(10000000);
	return 0;
}

DWORD WINAPI  TraverseDirectoryRecursive (LPVOID para)//use of level?
{
	Threads_t *th = (Threads_t *)para;
	HANDLE SearchHandle;
	WIN32_FIND_DATA FindData;
	DWORD FType, i;
	DWORD dwWaitResult;


	SetCurrentDirectory(th->name);

	SearchHandle = FindFirstFile(_T("*"),&FindData);

	do {
		FType = FileType(&FindData);

		if(FType == TYPE_FILE){
			_tcscpy(th->name,FindData.cFileName);
			for(i=0;i<th->level;i++)
				_tprintf(_T("  "));
			_tprintf(_T("%d level=%d FILE:%s\n"),th->thId,th->level,FindData.cFileName);
			if (!ReleaseSemaphore( 
                        Semaphore,  // handle to semaphore
                        1,            // increase count by one
                        NULL) )       // not interested in previous count
                {
                    printf("ReleaseSemaphore error: %d\n", GetLastError());
                }
			else{
				_tprintf(_T("%d have released semaphore\n"),th->thId);
			}
			dwWaitResult = WaitForSingleObject(
				cSemaphore,
				INFINITE
				);
			if(th->equal==1){
				_tprintf(_T("not equal,so %d exit\n"),th->thId);
				return 2;
			}
		}
		if(FType == TYPE_DIR){
			//int i=CopyFile(FindData.cFileName,fullNewPath,FALSE);

			for(i=0;i<th->level;i++)
				_tprintf(_T("  "));
			_tprintf(_T("%d level=%d DIR: %s\n"),th->thId,th->level,FindData.cFileName);
			
			if (!ReleaseSemaphore( 
                        Semaphore,  // handle to semaphore
                        1,            // increase count by one
                        NULL) )       // not interested in previous count
                {
                    printf("ReleaseSemaphore error: %d\n", GetLastError());
                }
			dwWaitResult = WaitForSingleObject(
				cSemaphore,
				INFINITE
				);
			if(th->equal==1){
				_tprintf(_T("not equal,so %d exit\n"),th->thId);
				return 2;
			}

			th->level++;
			_tcscpy(th->name,FindData.cFileName);
			TraverseDirectoryRecursive(th);
			//SetCurrentDirectory(fullNewPath);
			
			SetCurrentDirectory(_T(".."));
		}
		
	} while (FindNextFile(SearchHandle,&FindData));
	
	FindClose(SearchHandle);

	return 0;
}

DWORD FileType (LPWIN32_FIND_DATA pFileData)
{
	BOOL IsDir;
	DWORD FType;
	FType = TYPE_FILE;
	IsDir = (pFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	if(IsDir)
		if(lstrcmp(pFileData->cFileName,_T(".")) == 0
			|| lstrcmp(pFileData->cFileName,_T(".."))==0)
			FType = TYPE_DOT;
		else FType = TYPE_DIR;
		return FType;
}

DWORD WINAPI Compare(LPVOID para){
	
	DWORD dwRet;
	Threads_c *thread = (Threads_c *)para;
	DWORD i;
	TCHAR name[NAME];
	DWORD level;

	//don't need to compare root 
	WaitForMultipleObjects(thread->nCount,&Semaphore,FALSE,INFINITE);
	while(1)
	{
		dwRet = WaitForSingleObject(
			fSemaphore,
			0L //zeros timeout interval????
			);
		if(dwRet==WAIT_OBJECT_0)
		{
			_tprintf(_T("Comparison thread finishes\n"));
			return 0;
		}

	WaitForMultipleObjects(thread->nCount,&Semaphore,FALSE,INFINITE);
	//compare starts....
	_tcscpy(name,(thread->th[0]).name);
	for(i=1;i<thread->nCount;i++)
	{
		if( _tcscmp((thread->th[i]).name,name)!=0 || (thread->th[i].level!=level) ){
			thread->equal = 1;
			break;
		}
	}
	if(thread->equal!=0)
	{
		for(i=0;i<thread->nCount;i++)
			(thread->th[i]).equal == 1;
	}
	else
	{
		for(i=0;i<thread->nCount;i++)
			(thread->th[i]).equal == 0;
	}

	if (!ReleaseSemaphore(
		cSemaphore,
		thread->nCount,
		NULL
		))
	{
		_tprintf(_T("ReleaseSemaphore error:%d\n"),GetLastError());
	}
}
	return 0;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            