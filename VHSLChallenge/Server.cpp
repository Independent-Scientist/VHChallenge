#include "stdafx.h"
#include "Common.h"
#include <windows.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <string>


#define BUF_SIZE 2048

int main()
{
	// Handles the interprocess method call
	ProcedureCallHandler* pch = new ProcedureCallHandler();
	pch->RegisterMethods();

	// Global Server Context
	ServerContext* ServerCTX = new ServerContext(pch);

	// Pipe communication interface.
	PipedComm pipeComm(const_cast<LPWSTR>(PIPE_NAME_SYNC),const_cast<LPWSTR>(PIPE_NAME_ASYNC));
	pipeComm.ServerCtx = ServerCTX;
	ServerCTX->PIPEComm = &pipeComm;

	// Create Server Main thread.
	HANDLE serverPipe = pipeComm.CreateServerPipeSync();

	if (serverPipe == INVALID_HANDLE_VALUE) {
		std::cout << "Pipe creation error!";
		return -1;
	}

	// Cleanup
	CloseHandle(serverPipe);

	return 0;
}
