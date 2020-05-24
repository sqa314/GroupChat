#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <time.h>

#define BUF_SIZE 100
#define MAX_CLNT 256
#define NAME_SIZE 20

char servAddr[100];
char servPort[100];
char servName[100];

unsigned WINAPI client(void* arg);
unsigned WINAPI server(void* arg);

unsigned WINAPI HandleClient(void* arg);
unsigned WINAPI servSend();
void SendClient(char* name, char* msg, unsigned int len);
void SendSecret(char* name, char* whisper, char* msg, unsigned int len);

unsigned WINAPI SendMsg(void* arg);
unsigned WINAPI RecvMsg(void* arg);

void ErrorHandling(char* msg);

unsigned int clientCount = 0;
SOCKET clientSocks[MAX_CLNT];
char* clientNames[50];
HANDLE hMutex;
char name[50] = "[DEFAULT]";
char msg[200];
unsigned int room;

clock_t ping;

#pragma comment(lib,"ws2_32")

int main()
{
	HANDLE hThread;
	while (1)
	{
		printf("IP : ");
		scanf("%s", servAddr);
		printf("Port : ");
		scanf("%s", servPort);
		getchar();
		if (!strcmp(servAddr, "test"))
			strcpy(servAddr, "127.0.0.1");

		if (!strcmp(servAddr, "host"))
			hThread = (HANDLE)_beginthreadex(NULL, 0, server, NULL, 0, NULL);
		else
			hThread = (HANDLE)_beginthreadex(NULL, 0, client, NULL, 0, NULL);
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		Sleep(3000);
		system("cls");
	}
	return 0;
}


unsigned WINAPI server(void* arg)
{
	WSADATA wsaData;
	SOCKET serverSock, clientSock;
	SOCKADDR_IN serverAddr, clientAddr;
	unsigned int clientAddrSize;
	HANDLE hThread;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	printf("Name of Room : ");
	fgets(servName, 100, stdin);
	servName[(room = strlen(servName)) - 1] = 0;
	hMutex = CreateMutex(NULL, FALSE, NULL);
	serverSock = socket(PF_INET, SOCK_STREAM, 0);

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(atoi(servPort));
	if (bind(serverSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		ErrorHandling("bind() error");
	if (listen(serverSock, 5) == SOCKET_ERROR)
		ErrorHandling("listen() error");

	(HANDLE)_beginthreadex(NULL, 0, servSend, NULL, 0, NULL);
	while (1)
	{
		clientAddrSize = sizeof(clientAddr);
		clientSock = accept(serverSock, (SOCKADDR*)&clientAddr, &clientAddrSize);

		WaitForSingleObject(hMutex, INFINITE);
		clientSocks[clientCount] = clientSock;
		++clientCount;
		ReleaseMutex(hMutex);

		hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClient, (void*)&clientSock, 0, NULL);
	}
	closesocket(serverSock);
	WSACleanup();
	return 0;
}


unsigned WINAPI HandleClient(void* arg)
{
	unsigned int i;
	unsigned int u = 0;
	SOCKET clientSock = *((SOCKET*)arg);

	char cName[50];
	size_t strLen = 0;
	char msg[200];
	char whisper[50];
	char farewell[200];

	while (!u)
	{
		strLen = recv(clientSock, cName, sizeof(cName), 0);
		cName[strLen] = 0;
		WaitForSingleObject(hMutex, INFINITE);

		for (i = 0; i < clientCount; ++i)
			if (clientNames[i] && !strcmp(cName, clientNames[i]))
				break;
		if (i != clientCount || !strcmp(cName, "exit") || !strcmp(cName, "refresh") || !strcmp(cName, "host") ||!strcmp(cName,"ping"))
			send(clientSock, "", 1, 0);
		else
			u = 1;
		ReleaseMutex(hMutex);
	}
	send(clientSock, servName, room, 0);
	sprintf(farewell, "\n----New Client [%s] entered.----\n\n", cName);

	if (strcmp(cName, "/list"))
		fputs(farewell, stdout);

	WaitForSingleObject(hMutex, INFINITE);

	for (u = 0; clientSock != clientSocks[u]; ++u);
	clientNames[u] = (char*)malloc(sizeof(char) * NAME_SIZE);
	strcpy(clientNames[u], cName);
	unsigned int count = clientCount - !strcmp(cName, "/list");
	send(clientSock, (char*)&count, sizeof(int), 0);
	for (i = 0; i < clientCount; ++i)
	{
		send(clientSock, clientNames[i], strlen(clientNames[i]), 0);
		recv(clientSock, msg, sizeof(msg), 0);
	}
	if (strcmp(cName, "/list"))
		for (i = 0; i < clientCount; ++i)
			send(clientSocks[i], farewell, strlen(farewell), 0);
	ReleaseMutex(hMutex);

	if (!strcmp(cName, "/list"))
		goto B;

	while ((strLen = recv(clientSock, msg, sizeof(msg), 0)) != -1)
	{
		msg[strLen] = 0;
		if (msg[0] != '/')
			SendClient(cName, msg, strLen);
		else if (!strcmp(msg, "/refresh\n"))
		{
			recv(clientSock, msg, sizeof(msg), 0);
			send(clientSock, (char*)&clientCount, sizeof(int), 0);
			for (i = 0; i < clientCount; ++i)
			{
				recv(clientSock, msg, sizeof(msg), 0);
				send(clientSock, clientNames[i], strlen(clientNames[i]), 0);
			}
		}
		else if (!strcmp(msg, "/ping\n"))
			send(clientSock, msg, strlen(msg), 0);
		else
		{
			i = strcspn(msg, " ");
			if (i >= strlen(msg))
				continue;
			strncpy(whisper, msg + 1, i - 1);
			whisper[i - 1] = 0;
			strncpy(farewell, msg + i + 1, strLen - i);
			farewell[strLen - i - 1] = 0;
			SendSecret(cName, whisper, farewell, strLen - i);
		}
	}
	sprintf(farewell, "\n----Client [%s] left.----\n\n", cName);
	fputs(farewell, stdout);
B:
	WaitForSingleObject(hMutex, INFINITE);
	for (i = 0; clientSock != clientSocks[i]; ++i);
	for (; i < clientCount - 1; ++i)
	{
		strcpy(clientNames[i], clientNames[i + 1]);
		clientSocks[i] = clientSocks[i + 1];
	}
	--clientCount;
	for (i = 0; i < clientCount; ++i)
		send(clientSocks[i], farewell, strlen(farewell), 0);
	ReleaseMutex(hMutex);
	closesocket(clientSock);
	return 0;
}

void SendClient(char* cName, char* msg, unsigned int len)
{
	unsigned int i;
	unsigned int nameLen = strlen(cName);
	char nameMsg[255];
	sprintf(nameMsg, "[%s] : %s", cName, msg);
	nameMsg[nameLen + len + 5] = 0;
	if (strcmp(cName, "host"))
		fputs(nameMsg, stdout);
	WaitForSingleObject(hMutex, INFINITE);
	for (i = 0; i < clientCount; ++i)
		if (strcmp(clientNames[i], cName))
			send(clientSocks[i], nameMsg, strlen(nameMsg), 0);
	ReleaseMutex(hMutex);
}

void SendSecret(char* cName, char* whisper, char* msg, unsigned int len)
{
	unsigned int i;
	unsigned int nameLen = strlen(cName);
	char nameMsg[255];
	sprintf(nameMsg, ">[%s] : %s", cName, msg);
	nameMsg[nameLen + len + 6] = 0;
	if (!strcmp(whisper, "host"))
	{
		fputs(nameMsg, stdout);
		return;
	}
	WaitForSingleObject(hMutex, INFINITE);
	for (i = 0; i < clientCount; ++i)
		if (!strcmp(clientNames[i], whisper) && strcmp(clientNames[i], cName))
			send(clientSocks[i], nameMsg, strlen(nameMsg), 0);
	ReleaseMutex(hMutex);
}

unsigned WINAPI servSend()
{
	size_t strLen = 0;
	char msg[200];
	char whisper[50];
	char farewell[200];

	unsigned int i;
	system("cls");
	printf("-----------------------------------------------\n");
	printf(" I AM HOST\n");
	printf(" Server Address : %s\n", servAddr);
	printf(" Server Port : %s\n", servPort);
	printf(" Server Name : %s\n\n", servName);
	printf("\n Existing Members\n\n");
	WaitForSingleObject(hMutex, INFINITE);
	if (clientCount)
		for (i = 0; i < clientCount; ++i)
			printf(" - %s\n", clientNames[i]);
	else
		printf(" No One Exists\n");
	ReleaseMutex(hMutex);
	putchar('\n');
	printf(" Enter \"/refresh\" to refresh messages\n");
	printf("-----------------------------------------------\n");
	while (1)
	{
		fgets(msg, BUF_SIZE, stdin);
		msg[strLen = strlen(msg)] = 0;
		if (msg[0] == '\n')
			continue;
		if (msg[0] != '/')
			SendClient("host", msg, strLen);
		else if (!strcmp(msg, "/refresh\n"))
		{
			system("cls");
			printf("-----------------------------------------------\n");
			printf(" I AM HOST\n");
			printf(" Server Address : %s\n", servAddr);
			printf(" Server Port : %s\n", servPort);
			printf(" Server Name : %s\n\n", servName);
			printf("\n Existing Members\n\n");
			WaitForSingleObject(hMutex, INFINITE);
			if (clientCount)
				for (i = 0; i < clientCount; ++i)
					printf(" - %s\n", clientNames[i]);
			else
				printf(" No One Exists\n");
			ReleaseMutex(hMutex);
			putchar('\n');
			printf(" Enter \"/refresh\" to refresh messages\n");
			printf("-----------------------------------------------\n");
			continue;
		}
		else
		{
			i = strcspn(msg, " ");
			if (i >= strlen(msg))
				continue;
			strncpy(whisper, msg + 1, i - 1);
			whisper[i - 1] = 0;
			strncpy(farewell, msg + i + 1, strLen - i);
			farewell[strLen - i - 1] = 0;
			SendSecret("host", whisper, farewell, strLen - i);
		}
	}
	return 0;
}

unsigned WINAPI client(void* arg)
{
	unsigned int i;
	WSADATA wsaData;
	SOCKET sock;
	SOCKADDR_IN serverAddr;
	HANDLE sendThread, recvThread;
	size_t strLen;
	unsigned int clientNum;
	char friend[50];
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");


	sock = socket(PF_INET, SOCK_STREAM, 0);

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(servAddr);
	serverAddr.sin_port = htons(atoi(servPort));

	if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		ErrorHandling("connect() error");
	while (1)
	{
		printf("Name : ");
		fgets(name, 100, stdin);
		name[strlen(name) - 1] = 0;
		send(sock, name, strlen(name), 0);
		strLen = recv(sock, servName, 100, 0);
		if (servName[0] == 0)
			printf("The ID you entered is already in use.\n");
		else
			break;
	}
	servName[strLen] = 0;
	if (!strcmp(name, "/list"))
	{
		recv(sock, (char*)&clientNum, sizeof(int), 0);
		system("cls");
		printf("-----------------------------------------------\n");
		printf(" Server Address : %s\n", servAddr);
		printf(" Server Port : %s\n", servPort);
		printf(" Server Name : %s\n", servName);
		printf("\n Existing Members\n");
		if (clientNum)
			for (i = 0; i < clientNum; ++i)
			{
				send(sock, "!", 2, 0);
				strLen = recv(sock, friend, 50, 0);
				friend[strLen] = 0;
				printf(" - %s \n", friend);
			}
		else
			printf(" No One Exists\n");
		printf("-----------------------------------------------\n");
		goto A;
	}
	while (1)
	{
		recv(sock, (char*)&clientNum, sizeof(int), 0);
		system("cls");
		printf("-----------------------------------------------\n");
		printf(" Server Address : %s\n", servAddr);
		printf(" Server Port : %s\n", servPort);
		printf(" Server Name : %s\n", servName);
		printf(" My Name : %s\n", name);
		printf("\n Existing Members\n");
		for (i = 0; i < clientNum; ++i)
		{
			send(sock, "!", 2, 0);
			strLen = recv(sock, friend, 50, 0);
			friend[strLen] = 0;
			printf(" - %s \n", friend);
		}
		putchar('\n');
		printf(" Enter \"/refresh\" to refresh messages and members\n");
		printf(" Enter \"/exit\" to leave\n");
		printf("-----------------------------------------------\n");

		sendThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&sock, 0, NULL);
		recvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&sock, 0, NULL);

		WaitForSingleObject(sendThread, INFINITE);
		TerminateThread(recvThread, 0);
		Sleep(100);
		send(sock, "!", 2, 0);
	}
A:
	closesocket(sock);
	WSACleanup();
	return 0;
}

unsigned WINAPI SendMsg(void* arg)
{
	SOCKET sock = *((SOCKET*)arg);
	char message[100];
	while (1)
	{
		fgets(message, 100, stdin);
		if (message[0] == '\n')
			continue;
		if (!strcmp(message, "/refresh\n"))
		{
			send(sock, message, strlen(message), 0);
			return 0;
		}
		if (!strcmp(message, "/ping\n"))
		{
			ping = clock();
		}
		if (!strcmp(message, "/exit\n"))
		{
			closesocket(sock);
			exit(0);
		}
		send(sock, message, strlen(message), 0);
	}
	return 0;
}

unsigned WINAPI RecvMsg(void* arg)
{
	SOCKET sock = *((SOCKET*)arg);
	char message[120];
	size_t strLen;
	while ((strLen = recv(sock, message, 120, 0)) != -1)
	{
		message[strLen] = 0;
		if (message[0] == '>')
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
		else if (!strcmp(message,"/ping\n"))
			sprintf(message, "지연시간 %dms\n",clock()-ping);
		fputs(message, stdout);
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
	}
	return 0;
}

void ErrorHandling(char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}