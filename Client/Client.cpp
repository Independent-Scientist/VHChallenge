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
	SerializableObject* response;
	SerializableObject* request;
	HANDLE clientPipe;

	// Our pipe communication interface.
	PipedComm pipedComm(const_cast<LPWSTR>(PIPE_NAME_SYNC), false);
	clientPipe = pipedComm.ConnectToServerPipeSync();

	/******************************************************************************************************************/
	/* Storing object into server.
	*/
	printf("Sending store_object RPC...\n");

	// Create request object.
	request = new SerializableObject();
	SERIALIZABLE_NEW_STRING(request, "method", "store_object");

	// Sending a binary object, but it could be of any type.
	byte sendBuffer[11] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x64, 0x21};
	SERIALIZABLE_NEW_BYTEBUFFER(request, "object", sendBuffer, 11);

	// Send sync call
	response = pipedComm.ClientSendRequestSync(request);

	// Get the reply from the sever
	std::string returnId;
	if (SERIALIZABLE_GET_STRING(response, "status").compare("ok") == 0)
	{
		returnId = SERIALIZABLE_GET_STRING(response, "response");
		std::cout << "Got object id: " << returnId << std::endl;
	}

	// Cleanup
	delete request;
	delete response;

	/******************************************************************************************************************/
	/* Retrieving object from server.
	*/
	printf("Sending retrieve_object RPC...\n");

	// Create request object.
	request = new SerializableObject();

	// Add params.
	SERIALIZABLE_NEW_STRING(request, "method", "retrieve_object");
	SERIALIZABLE_NEW_STRING(request, "id", returnId.c_str());

	// Call sync.
	response = pipedComm.ClientSendRequestSync(request);

	// Get response.
	if (SERIALIZABLE_GET_STRING(response, "status").compare("ok") == 0)
	{
		// Print contents of the object.
		byte* buffer = SERIALIZABLE_GET_BYTEBUFFER(response, "object");
		size_t bufLen = response->GetPropertySize("object");
		std::stringstream ss;
		for (int i = 0; i < bufLen; ++i)
			ss << std::hex << (int)buffer[i];
		std::cout << "Got object: " << ss.str() << std::endl;
	}

	// Cleanup
	delete request;
	delete response;

	// a little pause before exit.
	Sleep(5 * 1000);

	CloseHandle(clientPipe);
    return 0;
}

