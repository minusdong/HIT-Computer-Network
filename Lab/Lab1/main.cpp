#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <tchar.h>
#include <fstream>
#include <map>
#include <string>
#include <iostream>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define MAXSIZE 65507 //发送数据报文的最大长度
#define HTTP_PORT 80  //http 服务器端口
#define DISABLED_MAXSIZE 50

#define fishING_WEB_SRC "http://jwc.hit.edu.cn/"	// 钓鱼原网址
#define fishING_WEB_DEST "http://jwts.hit.edu.cn/" // 钓鱼目的网址

char *disabledUser[DISABLED_MAXSIZE] = {(char *)"172.20.62.32"};
char *disabledHost[DISABLED_MAXSIZE] = {(char *)"http://today.hit.edu.cn/"};

//Http 重要头部数据
struct HttpHeader {
	char method[4]; // POST or GET
	char url[1024]; // URL
	char host[1024];
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};

// 结构体cache
struct HttpCache
{
	char url[1024];
	char host[1024];
	char last_modified[200];
	char status[4];
	char buffer[MAXSIZE];
	HttpCache() {
		ZeroMemory(this, sizeof(HttpCache)); // 初始化cache
	}
};
HttpCache Cache[1024];
int cached_number = 0; //已经缓存的url数
int last_cache = 0;	   //上一次缓存的索引

BOOL InitSocket();
int ParseHttpHead(char *buffer, HttpHeader *httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
void ParseCache(char *buffer, char *status, char *last_modified);
bool UserFilter(in_addr sin_addr);
bool SiteFilter(char *host);


// Related parameters
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;


struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};

int main(int argc, char *argv[]) {
	printf("The proxy server is starting .....\n");
	printf("Initializing ...\n");
	if (!InitSocket()) {
		printf("Socket initialization failed\n");
		return -1;
	}
	printf("Proxy server running, listening on PORT: %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	SOCKADDR_IN acceptAddr;
    int nAddrlen = sizeof(acceptAddr);
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;

	// The proxy server keeps listening
	while (true) {
		acceptSocket = accept(ProxyServer, (SOCKADDR *)&acceptAddr, &nAddrlen);
		if (UserFilter(acceptAddr.sin_addr) == true) {
            printf("The user is Banned\n");
            printf("Closing socket...\n\n");
            continue;
        }
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL)
			continue;
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		Sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}

BOOL InitSocket() {
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	// version 2.2
	wVersionRequested = MAKEWORD(2, 2);
	// Load dll file from Socket library
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		// winsock.dll Not found
		printf("Failed to load winsock, Error code: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("Not find the correct version of winsock\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0); 
	// Create a stream socket of the TCP/IP protocol family
	if (INVALID_SOCKET == ProxyServer) {
		printf("Failed to create socket, Error code: %d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;

	if (bind(ProxyServer, (SOCKADDR *)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("Failed to bind socket\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("Failed to listen on port %d", ProxyPort);
		return FALSE;
	}
	return TRUE;
}

unsigned int __stdcall ProxyThread(LPVOID lpParameter)
{
	char Buffer[MAXSIZE];
	char sendBuffer[MAXSIZE];
	char fishBuffer[MAXSIZE];
	char *CacheBuffer;

	ZeroMemory(Buffer, MAXSIZE);
	ZeroMemory(sendBuffer, MAXSIZE);
	ZeroMemory(fishBuffer, MAXSIZE);

	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	int Have_cache;

	HttpHeader *httpHeader = new HttpHeader();

	// Receive client's request
	recvSize = recv(((ProxyParam *)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		goto error;
	}

	memcpy(sendBuffer, Buffer, recvSize);
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	Have_cache = ParseHttpHead(CacheBuffer, httpHeader);

	if (!ConnectToServer(&((ProxyParam *)lpParameter)->serverSocket, httpHeader->host)) {
		printf("Failed to connect to host %s\n", httpHeader->host);
		goto error;
	}
	printf("Connect to host %s successfully \n", httpHeader->host);

	// Site blocking
	if (SiteFilter(httpHeader->url)) {
		printf("Site %s is banned\n", httpHeader->host);
		goto error;
	}

	if (Have_cache == 1)
		printf("Have Cache\n");
	delete CacheBuffer;

	// Fish
	if (strstr(httpHeader->url, fishING_WEB_SRC) != NULL) {
		char *pr;
		int fishing_len;
		printf("Site %s be directed to %s\n", fishING_WEB_SRC, fishING_WEB_DEST);

		char temp_head[] = "HTTP/1.1 302 Moved Temporarily\r\n";
		fishing_len = strlen(temp_head);
		memcpy(fishBuffer, temp_head, fishing_len);
		pr = fishBuffer + fishing_len;

		char temp_head2[] = "Connection:keep-alive\r\n";
		fishing_len = strlen(temp_head2);
		memcpy(pr, temp_head2, fishing_len);
		pr += fishing_len;

		char temp_head3[] = "Cache-Control:max-age=0\r\n";
		fishing_len = strlen(temp_head3);
		memcpy(pr, temp_head3, fishing_len);
		pr += fishing_len;

		// redirect to jwts.hit.edu.cn
		char fishing_dest[] = "Location: ";
		strcat(fishing_dest, fishING_WEB_DEST);
		strcat(fishing_dest, "\r\n\r\n");
		fishing_len = strlen(fishing_dest);
		memcpy(pr, fishing_dest, fishing_len);

		// Return the 302 message to the client
		ret = send(((ProxyParam *)lpParameter)->clientSocket, fishBuffer, sizeof(fishBuffer), 0);
		goto error;
	}

	// The requested page is cached on the server
	if (Have_cache) {
		char cached_buffer[MAXSIZE];
		ZeroMemory(cached_buffer, MAXSIZE);
		memcpy(cached_buffer, Buffer, recvSize);

		// Construct the cached message header
		char *pr = cached_buffer + recvSize;
		memcpy(pr, "If-modified-since: ", 19);
		pr += 19;
		int length = strlen(Cache[last_cache].last_modified);
		memcpy(pr, Cache[last_cache].last_modified, length);
		pr += length;

		// Directly forward the HTTP data message sent by the client to the target server
		ret = send(((ProxyParam *)lpParameter)->serverSocket, cached_buffer, strlen(cached_buffer) + 1, 0);
		// Waiting for the target server to return data
		recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, cached_buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			goto error;
		}

		// Parse HTTP headers containing cache information
		CacheBuffer = new char[recvSize + 1];
		ZeroMemory(CacheBuffer, recvSize + 1);
		memcpy(CacheBuffer, cached_buffer, recvSize);

		char last_status[4];
		char last_modified[30];
		ParseCache(CacheBuffer, last_status, last_modified);

		delete CacheBuffer;

		// 304 status code, the file has not been modified
		if (strcmp(last_status, "304") == 0) {
			printf("Not modified, Cache URL:%s\n", Cache[last_cache].url);
			// Forward the cached data directly to the client
			ret = send(((ProxyParam *)lpParameter)->clientSocket, Cache[last_cache].buffer, sizeof(Cache[last_cache].buffer), 0);
			if (ret != SOCKET_ERROR)
				printf("Cache Send (NOT CHANGE)\n");
		}
		// 200 status code, indicating that the file has been modified
		else if (strcmp(last_status, "200") == 0) {
			printf("Modified, Cache URL:%s\n", Cache[last_cache].url);
			memcpy(Cache[last_cache].buffer, cached_buffer, strlen(cached_buffer));
			memcpy(Cache[last_cache].last_modified, last_modified, strlen(last_modified));

			// Forward the data returned by the target server directly to the client
			ret = send(((ProxyParam *)lpParameter)->clientSocket, cached_buffer, sizeof(cached_buffer), 0);
			if (ret != SOCKET_ERROR)
				printf("Cache Send (CHANGED)\n");
		}
	}
	// NO such cache
	else {
		ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
		recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0)
			goto error;

		ret = send(((ProxyParam *)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
		if (ret != SOCKET_ERROR) {
			printf("Buffer ret = %d \n", ret);
		}
	}
error:
	printf("Closing socket... \n\n");
	Sleep(200);
	closesocket(((ProxyParam *)lpParameter)->clientSocket);
	closesocket(((ProxyParam *)lpParameter)->serverSocket);
	_endthreadex(0);
	return 0;
}

void ParseCache(char *buffer, char *status, char *last_modified) {
	char *p;
	char *ptr;
	const char *delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);
	memcpy(status, &p[9], 3);
	status[3] = '\0';
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		if (strstr(p, "Last-Modified") != NULL) {
			memcpy(last_modified, &p[15], strlen(p) - 15);
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}

int ParseHttpHead(char *buffer, HttpHeader *httpHeader)
{
	// Used to indicate whether the Cache is hit or not
	int flag = 0;
	char *p;
	char *ptr;
	const char *delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);
	// GET
	if (p[0] == 'G') {
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
		printf("URL：%s\n", httpHeader->url);
		// Whether the currently visited URL already exists in the cache
		for (int i = 0; i < 1024; i++) {
			// URL already exists in the cache
			if (strcmp(Cache[i].url, httpHeader->url) == 0) {
				flag = 1;
				break;
			}
		}
		// Cache Not Full
		if (!flag && cached_number != 1023) {
			memcpy(Cache[cached_number].url, &p[4], strlen(p) - 13);
			last_cache = cached_number;
		}
		// Cache Full, cover the first one
		else if (!flag && cached_number == 1023) {
			memcpy(Cache[0].url, &p[4], strlen(p) - 13);
			last_cache = 0;
		}
	}
	 // POST
	else if (p[0] == 'P') {
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
		for (int i = 0; i < 1024; i++){
			if (strcmp(Cache[i].url, httpHeader->url) == 0) {
				flag = 1;
				break;
			}
		}
		if (!flag && cached_number != 1023) {
			memcpy(Cache[cached_number].url, &p[5], strlen(p) - 14);
			last_cache = cached_number;
		}
		else if (!flag && cached_number == 1023) {
			memcpy(Cache[0].url, &p[4], strlen(p) - 13);
			last_cache = 0;
		}
	}

	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
			case 'H': //HOST
				memcpy(httpHeader->host, &p[6], strlen(p) - 6);
				if (!flag && cached_number != 1023) {
					memcpy(Cache[last_cache].host, &p[6], strlen(p) - 6);
					cached_number++;
				}
				else if (!flag && cached_number == 1023)
					memcpy(Cache[last_cache].host, &p[6], strlen(p) - 6);
				break;
			case 'C': //Cookie
				if (strlen(p) > 8) {
					char header[8];
					ZeroMemory(header, sizeof(header));
					memcpy(header, p, 6);
					if (!strcmp(header, "Cookie"))
						memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
				break;
			default:
				break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	return flag;
}

BOOL ConnectToServer(SOCKET *serverSocket, char *host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent)
		return FALSE;
	in_addr Inaddr = *((in_addr *)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET)
		return FALSE;
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}

bool UserFilter(in_addr sin_addr) {
    for (int i = 0; i < DISABLED_MAXSIZE; i++) {
        if (disabledUser[i] == NULL)
            continue;
        if (strcmp(disabledUser[i], inet_ntoa(sin_addr)) == 0)
            return true;
    }
    return false;
}

bool SiteFilter(char *host) {
    for (int i = 0; i < DISABLED_MAXSIZE; i++) {
        if (disabledHost[i] == NULL)
            continue;
        if (strcmp(disabledHost[i], host) == 0)
            return true;
    }
    return false;
}