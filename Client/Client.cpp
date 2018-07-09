// Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\VHSLChallenge\Common.h"
#include <vector>
#include <string>
#include <iostream>

#define BUF_SIZE 2048

int main()
{
	// Create a simple request
	SerializableObject* request = new SerializableObject();
	SERIALIZABLE_NEW_STRING(request, "method", "concat_strings");
	SERIALIZABLE_NEW_STRING(request, "param0", "Hello ");
	SERIALIZABLE_NEW_STRING(request, "param1", "World!");

	size_t bufferSize;
	byte* serialized_data = request->Serialize(&bufferSize);

	// Our pipe communication interface.
	PipedComm pipedComm(const_cast<LPWSTR>(PIPE_NAME_SYNC), false);

	printf("Sending first call...\n");
	HANDLE clientPipe = pipedComm.ConnectToServerPipeSync();

	// Send Sync request
	SerializableObject* response = pipedComm.ClientSendRequestSync(request);
	if (SERIALIZABLE_GET_STRING(response, "status").compare("ok") == 0)
	{
		std::cout << "Got: " << SERIALIZABLE_GET_STRING(response, "response") << std::endl;
	}

	delete request;
	delete response;

	printf("Sending store_object RPC...\n");
	request = new SerializableObject();
	SERIALIZABLE_NEW_STRING(request, "method", "store_object");
	byte sendBuffer[8] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
	SERIALIZABLE_NEW_BYTEBUFFER(request, "object", sendBuffer, 8);

	response = pipedComm.ClientSendRequestSync(request);
	std::string returnId;
	if (SERIALIZABLE_GET_STRING(response, "status").compare("ok") == 0)
	{
		returnId = SERIALIZABLE_GET_STRING(response, "response");
		std::cout << "Got object id: " << returnId << std::endl;
	}

	delete request;
	delete response;

	Sleep(5 * 1000);

	CloseHandle(clientPipe);
    return 0;
}

