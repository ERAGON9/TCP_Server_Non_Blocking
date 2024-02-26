#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <fstream>
#include <sstream>

struct SocketState
{
	SOCKET id;			    // Socket handle
	int	recv;			    // Receiving?
	int	send;			    // Sending?
	int sendSubType;	    // Sending sub-type
	char buffer[2048];
	int len;
	int time_last_request;   // time from epoch at secounds.
};

struct Header
{
	string fileName;	        // File name (file/website/test...)
	string lang;		        // Language (he/en/fr)
	string file_type;           // File type (txt/html)
	int requet_type;            // requet type (Get_Request=1/.../Trace_Request=7)
	int body_content_length;    // The body content message length
	string body_content;        // The body content
	string finish_code;         // the code returned ("200 OK"/"201 Created" /"404 Not Found"/...)
};

const int TIME_PORT = 8080;
const int MAX_SOCKETS = 60;

const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;

const int Get_Request = 1;
const int Post_Request = 2;
const int Put_Request = 3;
const int Delete_Request = 4;
const int Head_Request = 5;
const int Options_Request = 6;
const int Trace_Request = 7;


bool addSocket(SocketState* sockets, int* socketsCount, SOCKET id, int what);
void removeSocket(SocketState* sockets, int* socketsCount, int index);
void acceptConnection(SocketState* sockets, int* socketsCount, int index);
void receiveMessage(SocketState* sockets, int* socketsCount, int index);
void sendMessage(SocketState* sockets, int* socketsCount, int index);
struct Header FillHeadears(string request);
string CombineFilePath_QS(Header reqHeader);
string CombineFilePath(Header reqHeader);
string Create_response(Header reqHeader);
string GetTime();
void Delete_LastRequest_FromBuffer(SocketState* sockets, int index);


void main()
{
	// The sockets array, that include all the server sockets.
	// The socketsCount, the actual number of socket at the sockets array.
	struct SocketState sockets[MAX_SOCKETS] = { 0 };
	int socketsCount = 0;


	// Initialize Winsock (Windows Sockets).

	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows Sockets implementation.
	WSAData wsaData;

	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.

	// After initialization, a SOCKET object is ready to be instantiated.

	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to a network address.

	// Need to assemble the required data for connection in sockaddr structure.

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;
	// IP address. The sin_addr is a union (s_addr is a unsigned long (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.
	serverService.sin_addr.s_addr = INADDR_ANY;
	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order (which is big-endian).
	serverService.sin_port = htons(TIME_PORT);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	cout << "Server is up! " << "at 127.0.0.1:" << TIME_PORT << " wait for connections and request!\n" << endl;

	// Special socket, the listen socket.
	addSocket(sockets, &socketsCount, listenSocket, LISTEN);

	time_t timer;

	// Accept connections and handles them one by one.
	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O.
		// Use fd_sets for sets of handles for reading, writing and exceptions.
		// Select gets "timeout" for waiting and still performing other operations (Use NULL for blocking).
		// Finally, select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
			{
				time(&timer);
				if ((sockets[i].time_last_request != 0) && (timer - sockets[i].time_last_request > 120)) // Past more than 2 min from last recv.
					removeSocket(sockets, &socketsCount, i);
				else
					FD_SET(sockets[i].id, &waitRecv);
			}

		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL); // The main function. Select function.
		if (nfd == SOCKET_ERROR)
		{
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(sockets, &socketsCount, i);
					break;

				case RECEIVE:
					time(&timer);
					if ((sockets[i].time_last_request != 0) && (timer - sockets[i].time_last_request > 120)) // Past more than 2 min from last recv.
						removeSocket(sockets, &socketsCount, i);
					else
						receiveMessage(sockets, &socketsCount, i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					if ((sockets[i].time_last_request != 0) && (timer - sockets[i].time_last_request > 120)) // Past more than 2 min from last recv.
						removeSocket(sockets, &socketsCount, i);
					else
						sendMessage(sockets, &socketsCount, i);
					break;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "Time Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SocketState* sockets, int* socketsCount, SOCKET id, int what)
{
	// Set the socket to be in non-blocking mode.
	unsigned long flag = 1;
	if (ioctlsocket(id, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}


	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			sockets[i].time_last_request = 0;
			(*socketsCount)++;
			return (true);
		}
	}
	return (false);
}


void removeSocket(SocketState* sockets, int* socketsCount, int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	sockets[index].time_last_request = 0;
	(*socketsCount)--;
}


void acceptConnection(SocketState* sockets, int* socketsCount, int index) // Always: index == 0 ==> socket[0] == listenSocket
{
	SOCKET id = sockets[index].id;  // Always: index == 0
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	// Do accept(), accept new client by new socket between the client and the server.
	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected.\n" << endl;

	// Add the new socket to the socket array.
	if (addSocket(sockets, socketsCount, msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
		return;
	}
}


void receiveMessage(SocketState* sockets, int* socketsCount, int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len; // len != 0 , if the client already have request at the buffer!
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Web Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(sockets, socketsCount, index);
		return;
	}
	if (bytesRecv == 0) // The client already close the connection.
	{
		closesocket(msgSocket);
		removeSocket(sockets, socketsCount, index);
		return;
	}
	else
	{
		time_t timer;
		time(&timer);

		sockets[index].buffer[len + bytesRecv] = '\0'; // Add the null-terminating to make it a string.
		sockets[index].time_last_request = (int)timer;
		cout << "Web Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n\n";

		sockets[index].len += bytesRecv; // Add to len the size of the new request.

		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "GET", 3) == 0) // Client request-> GET
			{
				sockets[index].sendSubType = Get_Request;
			}
			else if (strncmp(sockets[index].buffer, "POST", 4) == 0) // Client request-> POST
			{
				sockets[index].sendSubType = Post_Request;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0) // Client request-> PUT 
			{
				sockets[index].sendSubType = Put_Request;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0) // Client request-> Delete 
			{
				sockets[index].sendSubType = Delete_Request;
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0) // Client request-> HEAD  
			{
				sockets[index].sendSubType = Head_Request;
			}
			else if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0) // Client request-> OPTIONS 
			{
				sockets[index].sendSubType = Options_Request;
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0) // Client request-> TRACE 
			{
				sockets[index].sendSubType = Trace_Request;
			}
			sockets[index].send = SEND; // For all request types, after inserting the sendSubType, need to set to SEND.
		}
	}
}


void sendMessage(SocketState* sockets, int* socketsCount, int index)
{
	int bytesSent = 0;
	char sendBuff[2048];

	SOCKET msgSocket = sockets[index].id;
	string request = string(sockets[index].buffer);
	string response;
	struct Header requestHeader = FillHeadears(request);

	if (sockets[index].sendSubType == Get_Request || sockets[index].sendSubType == Head_Request) // Client request-> GET/HEAD
	{
		requestHeader.requet_type = Get_Request;
		string file_path = CombineFilePath_QS(requestHeader);
		string file_contents;
		ifstream file(file_path);

		if (file.good()) // File exists.
		{
			cout << "File exists at: " << file_path << endl;
			stringstream buffer;
			buffer << file.rdbuf();
			file_contents = buffer.str();
			requestHeader.finish_code = "200 OK";
			requestHeader.body_content = file_contents;
		}
		else // File not exists.
		{
			cout << "File does not exist at: " << file_path << endl;
			requestHeader.finish_code = "404 Not Found";
			requestHeader.body_content = "404 - File Not Found";
		}
		file.close();
		if (sockets[index].sendSubType == Head_Request)
		{
			requestHeader.body_content = "";
			requestHeader.requet_type = Head_Request;
		}

		response = Create_response(requestHeader);
		strcpy(sendBuff, response.c_str());
	}
	else if (sockets[index].sendSubType == Post_Request)  // Client request-> POST 
	{
		requestHeader.requet_type = Post_Request;
		cout << requestHeader.body_content << endl << endl;
		requestHeader.finish_code = "200 OK";

		response = Create_response(requestHeader);
		strcpy(sendBuff, response.c_str());
	}
	else if (sockets[index].sendSubType == Put_Request)  // Client request-> PUT 
	{
		requestHeader.requet_type = Put_Request;
		string file_path = CombineFilePath(requestHeader);
		ifstream fileR(file_path);

		if (fileR.good()) // File exists.
			requestHeader.finish_code = "200 OK";
		else // File not exists.
			requestHeader.finish_code = "201 Created";
		fileR.close();

		ofstream fileW;
		fileW.open(file_path); // Open file in text mode for writing. if the file not exists it's create one.
		if (!fileW.is_open())
		{
			requestHeader.finish_code = "500 Internal Server Error";
			requestHeader.body_content = "500 Internal Server Error";
		}
		else
		{
			fileW << requestHeader.body_content;
			fileW.close();
		}

		response = Create_response(requestHeader);
		strcpy(sendBuff, response.c_str());
	}
	else if (sockets[index].sendSubType == Delete_Request)  // Client request-> DELETE 
	{
		requestHeader.requet_type = Delete_Request;
		string file_path = CombineFilePath(requestHeader);

		ifstream fileR(file_path);
		if (fileR.good()) // File exists.
		{
			fileR.close();
			if (remove(file_path.c_str()) == 0)
				requestHeader.finish_code = "200 OK";
			else 
				requestHeader.finish_code = "500 Internal Server Error";
		}
		else 
			requestHeader.finish_code = "404 Not Found";

		response = Create_response(requestHeader);
		strcpy(sendBuff, response.c_str());
	}
	else if (sockets[index].sendSubType == Options_Request)  // Client request-> OPTIONS 
	{
		requestHeader.requet_type = Options_Request;
		requestHeader.finish_code = "200 OK";

		response = Create_response(requestHeader);
		strcpy(sendBuff, response.c_str());

	}
	else if (sockets[index].sendSubType == Trace_Request)  // Client request-> TRACE 
	{
		requestHeader.requet_type = Trace_Request;
		requestHeader.finish_code = "200 OK";
		requestHeader.body_content = request;

		response = Create_response(requestHeader);
		strcpy(sendBuff, response.c_str());
	}
	else // Not supported type.
	{
		cout << "Web Server: Error at sendSubType" << endl;
		requestHeader.requet_type = -1;
		requestHeader.finish_code = "405 Method Not Allowed";
		requestHeader.body_content = "405 Method Not Allowed.";

		response = Create_response(requestHeader);
		strcpy(sendBuff, response.c_str());
	}

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Web Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Web Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n\n";

	Delete_LastRequest_FromBuffer(sockets, index);
	sockets[index].send = IDLE;
}


struct Header FillHeadears(string request)
{
	struct Header reqHeader;
	size_t start_pos, end_pos;

	start_pos = request.find("/") + 1;
	end_pos = request.find(".");
	reqHeader.fileName = request.substr(start_pos, end_pos - start_pos); // Extract the substring between '/' and '.' ("file")

	start_pos = request.find("lang="); // Find the position of "lang=" and move 5 positions ahead to skip "lang="
	if (start_pos == string::npos) // There is no "lang=" at the string request.
	{
		reqHeader.lang = "en"; // Default value.

		start_pos = request.find(".") + 1;
		end_pos = request.find(" ", start_pos);
		reqHeader.file_type = request.substr(start_pos, end_pos - start_pos); // Extract the substring between "." and the space ("txt")
	}
	else
	{
		start_pos += 5;
		end_pos = request.find(" ", start_pos);
		reqHeader.lang = request.substr(start_pos, end_pos - start_pos); // Extract the substring between "lang=" and the space ("en")

		start_pos = request.find(".") + 1;
		end_pos = request.find("?");
		reqHeader.file_type = request.substr(start_pos, end_pos - start_pos); // Extract the substring between "." and '?' ("txt")
	}

	start_pos = request.find("Content-Length:");
	if (start_pos == string::npos) // There is no body content at the string request.
	{
		reqHeader.body_content_length = 0;
		reqHeader.body_content = "";
	}
	else
	{
		start_pos += 16; // + Content-Length: + space.
		end_pos = request.find("\r", start_pos);
		string length = request.substr(start_pos, end_pos - start_pos); // Extract the substring between 'Content-Length: ' and '\n' (number)
		reqHeader.body_content_length = stoi(length);

		start_pos = request.find("\r\n\r\n") + 4;
		reqHeader.body_content = request.substr(start_pos);
	}
	
	return reqHeader;
}

string CombineFilePath_QS(Header reqHeader)
{
	string file_name = reqHeader.fileName;
	string lang = reqHeader.lang;
	string file_type = reqHeader.file_type;
	string combined_string = "C:\\temp\\" + file_name + "-" + lang+ "." + file_type;
	return combined_string;
}

string CombineFilePath(Header reqHeader)
{
	string file_name = reqHeader.fileName;
	string file_type = reqHeader.file_type;
	string combined_string = "C:\\temp\\" + file_name + "." + file_type;
	return combined_string;
}

string Create_response(Header reqHeader)
{
	// Generate the HTTP response headers
	string response = "HTTP/1.1 "+ reqHeader.finish_code +"\r\n";
	response += "Cache-Control: no-cache, private\r\n";
	response += "Date: "+ GetTime() +" GMT\r\n";
	response += "Server: LiorServer/1.0\r\n";
	if (reqHeader.requet_type == Options_Request)
		response += "Allow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE\r\n";
	if(reqHeader.requet_type != Trace_Request)
		response += "Content-Type: text/html; charset=utf-8\r\n";
	else
		response += "Content-Type: message/http; charset=utf-8\r\n";
	response += "Content-Length: " + to_string(reqHeader.body_content.size()) + "\r\n";
	response += "Content-Language: he, fr\r\n\r\n";

	// Generate the response body
	response += reqHeader.body_content;

	return response;
}

string GetTime()
{
	string printable_Time;
	
	time_t timer;
	time(&timer);

	printable_Time = string(ctime(&timer)); // Parse the current time to printable string.

	size_t pos = printable_Time.find('\n');
	if (pos != string::npos)  // The sring printable_Time include '\n' so need to erase it.
	{
		printable_Time.erase(pos, 1);
	}

	return printable_Time;
}

void Delete_LastRequest_FromBuffer(SocketState* sockets, int index)
{
	int bufferLen = strlen(sockets[index].buffer);
	memcpy(sockets[index].buffer, &sockets[index].buffer[bufferLen], bufferLen);
	sockets[index].len -= bufferLen;
}

