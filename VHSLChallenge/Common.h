#pragma once
#include "stdafx.h"
#include <Windows.h>
#include <strsafe.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <map>
#include <random>
#include <sstream>

LPCWSTR PIPE_NAME_SYNC = L"\\\\.\\pipe\\sync";
LPCWSTR PIPE_NAME_ASYNC = L"\\\\.\\pipe\\a_sync";
LPCWSTR SERVER_SENT_EVENT = L"SERVER_SENT_EVENT_";
LPCWSTR CLIENT_SENT_EVENT = L"CLIENT_SENT_EVENT_";

#define READWRITE_BUFFER_SIZE 1024*1024*10

DWORD WINAPI HandlePipeConnection(LPVOID lpvParam);

class ProcedureCallHandler;
class PipeConnectionThreadParams;
class PipedComm;
class APIFunction;
class APIFunctionConcatString;
class APIFunctionMultiplyDoubles;

/**********************************************************************************************************************************/
/* Some Helper functions
*/
constexpr char hexmap[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

std::string hexStr(byte* data, int len) {
	std::string s(len * 2, ' ');
	for (int i = 0; i < len; ++i) {
		s[2 * 1] = hexmap[(data[i] & 0xF0) >> 4];
		s[2 * i + 1] = hexmap[data[i] & 0x0F];
	}

	return s;
}

template<size_t N>
void genRandomString(char(&buffer)[N])
{
	std::random_device rd;

	const char alphanumeric[] = {
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
	};

	std::mt19937 eng(rd());
	std::uniform_int_distribution<> distr(0, 61);

	for (auto& c : buffer)
		c = alphanumeric[distr(eng)];

	buffer[N] = '\0';
}


void ErrorExit(LPCWSTR lpszFunction, bool fExit=false)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	
	_tprintf(TEXT("%s"), (LPCTSTR)lpDisplayBuf);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);

	if(fExit)
		ExitProcess(dw);
}

/**********************************************************************************************************************************/

/**********************************************************************************************************************************/
/*
*  Serialization Object related
*/
enum PROPERTY_TYPE {
	PROP_INTEGER = 0,
	PROP_FLOAT = 1,
	PROP_DOUBLE = 2,
	PROP_STRING = 3,
	PROP_BYTES = 4
};

struct Property {
	char Name[32];
	size_t Size;
	int Offset;
	byte* Value;
	byte Type; // 0-int 1-float 2-double 3-string 4-byte-buffer
};

// Some Helper Macros
#define SERIALIZABLE_NEW_STRING(x, name, value) (x->NewProperty(name, (void *)value, strlen(value)+1, PROP_STRING))
#define SERIALIZABLE_NEW_INTEGER(x, name, value) (x->NewProperty(name, (void *)&value, sizeof(value), PROP_INTEGER))
#define SERIALIZABLE_NEW_DOUBLE(x, name, value) (x->NewProperty(name, (void *)&value, sizeof(value), PROP_DOUBLE))
#define SERIALIZABLE_NEW_BYTEBUFFER(x, name, value, size) (x->NewProperty(name, (void *)value, size, PROP_BYTES))

#define SERIALIZABLE_GET_BYTEBUFFER(x, name) (x->GetBytes(name))
#define SERIALIZABLE_GET_INTEGER(x, name) (x->GetInteger(name))
#define SERIALIZABLE_GET_STRING(x, name) (x->GetString(name))

/**
* Binary Serialization
*/
class SerializableObject {

public:

	int PropertyCount;

	std::vector<Property*> Properties;

	typedef std::vector<Property*>::iterator PropertiesIter;

	SerializableObject() {

	}
	
	~SerializableObject() {

		for (PropertiesIter it = Properties.begin(); it != Properties.end(); ++it) {
			Property* prop = *it;
			if (prop->Value)
				delete prop->Value;
			delete prop;
		}

		Properties.clear();
	}

	void NewProperty(const char* _name, void* _value, size_t _size, byte _type) {

		// Check property name collision.
		for (PropertiesIter it = Properties.begin(); it != Properties.end(); ++it) {
			Property* prop = *it;
			if (strcmp(prop->Name, _name) == 0) {
				return;
			}
		}
		
		Property* prop = new Property();
		prop->Size = _size;
		CopyMemory(&prop->Name[0], _name, strlen(_name));
		prop->Value = (byte*)malloc(_size);
		CopyMemory(prop->Value, _value, _size);
		prop->Type = _type;
		prop->Offset = 0;

		PropertyCount++;
		this->Properties.push_back(prop);
	}

	Property* GetProperty(const char* _name) {

		for (PropertiesIter it = Properties.begin(); it != Properties.end(); ++it) {
			Property* prop = *it;
			if (strcmp(prop->Name, _name) == 0) {
				return prop;
			}
		}
		return nullptr;
	}

	size_t GetPropertySize(const char* _name) {
		Property* prop = GetProperty(_name);
		return prop->Size;
	}

	int GetInteger(const char* _name) {

		Property* prop = GetProperty(_name);
		if (prop) {
			byte* b = prop->Value;
			int i = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | (b[0]);
			return i;
		}
		return std::numeric_limits<int>::infinity();
	}


	byte* GetBytes(const char* _name) {
		Property* prop = GetProperty(_name);
		if (prop) {
			return prop->Value;
		}
		return NULL;
	}

	std::string GetString(const char* _name) {

		Property* prop = GetProperty(_name);
		if (prop) {
			return std::string(reinterpret_cast<const char*>(prop->Value), prop->Size-1);
		}
		return std::string();
	}

	void RemoveProperty(const char* _name) {

		for (PropertiesIter it = Properties.begin(); it != Properties.end(); ++it) {
			Property* prop = *it;
			if (strcmp(prop->Name, _name) == 0) {

				Properties.erase(it);

				if(prop->Value)
					delete prop->Value;

				delete prop;

				PropertyCount--;
				break;
			}
		}
	}

	byte* Serialize(size_t* _outSize) {

		// A very crude Serialization technique, not taking in consideration, endianess, platform etc.

		// First Calculate the total buffer size.
		uint32_t offset = 0;
		uint32_t bufferSize = sizeof(PropertyCount);
		byte* outBuffer = NULL;

		bufferSize += sizeof(Property) * Properties.size();

		for (PropertiesIter it = Properties.begin(); it != Properties.end(); ++it) {
			Property* prop = *it;
			bufferSize += prop->Size;
		}

		// Allocate byte buffer.
		outBuffer = (byte*)malloc(bufferSize * sizeof(byte));
		ZeroMemory(outBuffer, bufferSize * sizeof(byte));

		// Copy property counter.
		CopyMemory(outBuffer+offset, &PropertyCount, sizeof(size_t));
		offset += sizeof(size_t);

		// Copy property struct data.
		for (PropertiesIter it = Properties.begin(); it != Properties.end(); ++it) {

			Property* prop = *it;
			CopyMemory(outBuffer+offset, prop, sizeof(Property));
			offset += sizeof(Property);
		}

		// Copy value pointers.
		for (PropertiesIter it = Properties.begin(); it != Properties.end(); ++it) {

			Property* prop = *it;
			CopyMemory(outBuffer+offset, prop->Value, prop->Size);
			offset += prop->Size;
		}

		*_outSize = bufferSize;

		return outBuffer;
	}

	static SerializableObject* Deserialize(byte* _buffer, size_t _buffSize) {

		SerializableObject* serialized = new SerializableObject();
		uint32_t offset = 0;

		// Copy property Count
		CopyMemory(&serialized->PropertyCount, _buffer+offset, sizeof(size_t));
		offset += sizeof(size_t);

		// Copy Property Structs
		for (int i = 0; i < serialized->PropertyCount; i++) {

			Property* prop = new Property();
			CopyMemory(prop, _buffer + offset, sizeof(Property));
			offset += sizeof(Property);

			serialized->Properties.push_back(prop);
		}

		// Copy Property Values
		for (int i = 0; i < serialized->PropertyCount; i++) {

			Property* prop = serialized->Properties[i];

			// Alloc
			prop->Value = (byte *)malloc(prop->Size);

			//Copy
			CopyMemory(serialized->Properties[i]->Value, _buffer + offset, prop->Size);
			offset += prop->Size;
		}

		return serialized;
	}

};
/**********************************************************************************************************************************/


/**********************************************************************************************************************************/
/*
* Server Context
*/
class ServerContext
{
public:

	ServerContext(ProcedureCallHandler* _pch) {
		PCH = _pch;
	}

	ProcedureCallHandler* PCH;
	PipedComm* PIPEComm;

	std::string StoreObject(SerializableObject* _obj) {

		static const char alphanum[] = "0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

		std::stringstream ss;

		for (int i = 0; i < 8; ++i) {
			srand(time(NULL));
			ss << alphanum[rand() % (sizeof(alphanum) - 1)];
		}

		StoredObjects.insert(std::make_pair(ss.str(), _obj));

		return ss.str();
	}

	SerializableObject* RetrieveObject(std::string id) {
		SerializableObject* obj = StoredObjects.at(id);
		return obj;
	}

	std::vector<PipeConnectionThreadParams*> ClientSessions;
	std::map<std::string ,SerializableObject*> StoredObjects;

};

/*
*  Client Pipe Session Data
*/
class PipeConnectionThreadParams 
{
public:
	HANDLE hPipe;
	ServerContext* ServerCTX;

};

/****
* Helper Functors
*/
class APIFunction
{
public:

	virtual std::string GetType() = 0;
};

class APIFunctionMultiplyDoubles : public APIFunction
{
public:
	APIFunctionMultiplyDoubles() {};

	virtual std::string GetType() { return "multiply_doubles"; };
	double operator() (double _a, double _b) { return _a * _b; };

};

class APIFunctionConcatString : public APIFunction
{
public:
	APIFunctionConcatString() {};

	virtual std::string GetType() { return "concat_strings"; };
	std::string operator() (std::string _a, std::string _b) { return _a + _b; };

};

/**
*  Controls Piped Communication
*/
class PipedComm {

public:

	class ServerContext* ServerCtx = NULL;

	PipedComm(LPWSTR _pipeName, bool _isServer = true) {
		mPipeName = _pipeName;
		mServerPipeHandle = NULL;
		mClientPipeHandle = NULL;
		mIsServer = _isServer;
	};
	~PipedComm() {};

	HANDLE CreateServerPipeSync() {

		BOOL   fConnected = FALSE;
		DWORD  dwThreadId = 0;
		HANDLE hThread = NULL;

		for (;;)
		{
			_tprintf(TEXT("\nPipe Server: Main thread awaiting client connection on %s\n"), mPipeName);
			mServerPipeHandle = CreateNamedPipe(
				mPipeName,             // pipe name 
				PIPE_ACCESS_DUPLEX,       // read/write access 
				PIPE_TYPE_BYTE |       // message type pipe 
				PIPE_READMODE_BYTE |   // message-read mode 
				PIPE_WAIT,                // blocking mode 
				PIPE_UNLIMITED_INSTANCES, // max. instances  
				READWRITE_BUFFER_SIZE,   // output buffer size 
				READWRITE_BUFFER_SIZE,                  // input buffer size 
				0,                        // client time-out 
				NULL);                    // default security attribute 

			if (mServerPipeHandle == INVALID_HANDLE_VALUE)
			{
				ErrorExit(L"CreateNamedPipe");
			}

			// Wait for the client to connect; if it succeeds, 
			// the function returns a nonzero value. If the function
			// returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 

			fConnected = ConnectNamedPipe(mServerPipeHandle, NULL) ?
				TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

			if (fConnected)
			{
				printf("Client connected, creating a processing thread.\n");

				// Create Client session data.
				PipeConnectionThreadParams* pThreadParams = new PipeConnectionThreadParams();
				pThreadParams->hPipe = mServerPipeHandle;
				pThreadParams->ServerCTX = ServerCtx;
				ServerCtx->ClientSessions.push_back(pThreadParams);

				// Create a thread for this client. 
				hThread = CreateThread(
					NULL,              // no security attribute 
					0,                 // default stack size 
					HandlePipeConnection,    // thread proc
					(LPVOID)pThreadParams,    // thread parameter 
					0,                 // not suspended 
					&dwThreadId);      // returns thread ID 

				if (hThread == NULL)
				{
					_tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
					return INVALID_HANDLE_VALUE;
				}
				else CloseHandle(hThread);
			}
			else
				// The client could not connect, so close the pipe. 
				CloseHandle(mServerPipeHandle);
		}
	

		return mServerPipeHandle;
	}

	HANDLE ConnectToServerPipeSync() {

		// Try to open a named pipe; wait for it, if necessary. 
		while (1)
		{
			HANDLE hPipe = CreateFile(
				mPipeName,   // pipe name 
				GENERIC_READ |  // read and write access 
				GENERIC_WRITE,
				0,              // no sharing 
				NULL,           // default security attributes
				OPEN_EXISTING,  // opens existing pipe 
				0,              // default attributes 
				NULL);          // no template file 


			// Break if the pipe handle is valid. 
			if (hPipe != INVALID_HANDLE_VALUE) {
				mClientPipeHandle = hPipe;
				return mClientPipeHandle;
			}

			// Exit if an error other than ERROR_PIPE_BUSY occurs. 
			if (GetLastError() != ERROR_PIPE_BUSY)
			{
				_tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
				return INVALID_HANDLE_VALUE;
			}

			// All pipe instances are busy, so wait for 20 seconds. 
			if (!WaitNamedPipe(mPipeName, 20000))
			{
				printf("Could not open pipe: 20 second wait timed out.");
				return INVALID_HANDLE_VALUE;
			}
		}

	}

	SerializableObject* ClientSendRequestSync(SerializableObject* _request) {
		
		HANDLE hHeap = GetProcessHeap();
		byte* pResponseRawData = (byte*)HeapAlloc(hHeap, 0, READWRITE_BUFFER_SIZE);
		DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;

		BOOL fSuccess;
		int sync = 1;
		SERIALIZABLE_NEW_INTEGER(_request, "sync", sync);

		// 
		size_t outSize;
		byte* serializedData = _request->Serialize(&outSize);

		//Write the data to the named pipe
		DWORD writtenSize;
		if (!WriteFile(mClientPipeHandle,
			serializedData,
			outSize,
			&writtenSize,
			nullptr) || writtenSize != outSize) {
			return NULL;
		}

		Sleep(500);

		fSuccess = ReadFile(
			mClientPipeHandle,        // handle to pipe 
			pResponseRawData,    // buffer to receive data 
			READWRITE_BUFFER_SIZE, // size of buffer 
			&cbBytesRead, // number of bytes read 
			NULL);        // not overlapped I/O 


		if (!fSuccess || cbBytesRead == 0)
		{
			if (GetLastError() == ERROR_BROKEN_PIPE)
			{
				ErrorExit(L"ReadFile");
			}
			else
			{
				ErrorExit(L"ReadFile");
			}
		}

		// Deserialize the transmission
		SerializableObject* response = SerializableObject::Deserialize((byte*)pResponseRawData, cbBytesRead);
		return response;
	} 

	int ServerSendResponse(HANDLE _hPipe, SerializableObject* _response) {

		// 
		size_t outSize;
		byte* serializedData = _response->Serialize(&outSize);

		//Write the data to the named pipe
		DWORD writtenSize;
		if (!WriteFile(_hPipe,
			serializedData,
			outSize,
			&writtenSize,
			nullptr) || writtenSize != outSize) {
			return -1;
		}

		return 0;
	}

private:
	LPWSTR mPipeName;
	HANDLE mServerPipeHandle;
	HANDLE mClientPipeHandle;
	bool mIsServer;
};

/*
* API Interface
*/
class ProcedureCallHandler
{
public:

	ProcedureCallHandler() {

	}

	/*
	* Call method within the serializable
	*/
	void CallMethod(PipeConnectionThreadParams* _clientSession, SerializableObject* _request) {

		std::string method = SERIALIZABLE_GET_STRING(_request, "method");

		if (method.compare("concat_strings") == 0) {

			std::string param0 = SERIALIZABLE_GET_STRING(_request, "param0");
			std::string param1 = SERIALIZABLE_GET_STRING(_request, "param1");
			int isSync = SERIALIZABLE_GET_INTEGER(_request, "sync");

			if (isSync) {

				APIFunctionConcatString* concat = (APIFunctionConcatString*)mFunctionMap.at("concat_strings");
				std::string result = (*concat)(param0, param1);

				Sleep(100);

				SerializableObject* response = new SerializableObject();
				SERIALIZABLE_NEW_STRING(response, "status", "ok");
				SERIALIZABLE_NEW_STRING(response, "response", result.c_str());
				_clientSession->ServerCTX->PIPEComm->ServerSendResponse(_clientSession->hPipe, response);
			}
		}

		if (method.compare("store_object") == 0) {

			Sleep(200);

			std::string objId = _clientSession->ServerCTX->StoreObject(_request);

			SerializableObject* response = new SerializableObject();
			SERIALIZABLE_NEW_STRING(response, "status", "ok");
			SERIALIZABLE_NEW_STRING(response, "response", objId.c_str());
			_clientSession->ServerCTX->PIPEComm->ServerSendResponse(_clientSession->hPipe, response);
		}

		if (method.compare("retrieve_object") == 0) {
			
			Sleep(200);

			std::string id = SERIALIZABLE_GET_STRING(_request, "id");
			SerializableObject* obj = _clientSession->ServerCTX->RetrieveObject(id);
			SERIALIZABLE_NEW_STRING(obj, "status", "ok");
			_clientSession->ServerCTX->PIPEComm->ServerSendResponse(_clientSession->hPipe, obj);


		}

	}

	int RegisterMethods() {

		mFunctionMap.insert(std::make_pair("multiply_double", new APIFunctionMultiplyDoubles()));
		mFunctionMap.insert(std::make_pair("concat_strings", new APIFunctionConcatString()));
		
		return 0;
	}

private:
	std::map<std::string, APIFunction*> mFunctionMap;

};

/*
*	Client Pipe Thread processing
*/
DWORD WINAPI HandlePipeConnection(LPVOID lpvParam)
{
	PipeConnectionThreadParams* threadParams = (PipeConnectionThreadParams*)lpvParam;

	HANDLE hHeap = GetProcessHeap();
	char* pRequestRawData = (char*)HeapAlloc(hHeap, 0, READWRITE_BUFFER_SIZE);
	char* pReply = (char*)HeapAlloc(hHeap, 0, READWRITE_BUFFER_SIZE);

	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
	BOOL fSuccess = FALSE;
	HANDLE hPipe = NULL;

	hPipe = threadParams->hPipe;
	ProcedureCallHandler* PCH = threadParams->ServerCTX->PCH;

	while (1) {

		// Read client requests from the pipe. This simplistic code only allows messages
		// up to READWRITE_BUFFER_SIZE characters in length.
		fSuccess = ReadFile(
			hPipe,        // handle to pipe 
			pRequestRawData,    // buffer to receive data 
			READWRITE_BUFFER_SIZE, // size of buffer 
			&cbBytesRead, // number of bytes read 
			NULL);        // not overlapped I/O 


		if (!fSuccess || cbBytesRead == 0)
		{
			if (GetLastError() == ERROR_BROKEN_PIPE)
			{
				ErrorExit(L"ReadFile");
			}
			else
			{
				ErrorExit(L"ReadFile");
			}
			break;
		}

		// Deserialize the transmission
		SerializableObject* request = SerializableObject::Deserialize((byte*)pRequestRawData, cbBytesRead);
		if (request) {
			PCH->CallMethod(threadParams, request);
		}

		FlushFileBuffers(hPipe);
	}

	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);

	HeapFree(hHeap, 0, pRequestRawData);
	HeapFree(hHeap, 0, pReply);

	printf("HandlePipeConnection exitting....\n");


	return 0;
}

