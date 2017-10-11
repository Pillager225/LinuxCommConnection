#include <errno.h>
#include "NetworkConnection.h"

// protected

bool NetworkConnection::setupServer(const int &port) {
	bzero((char *) &mAddr, sizeof(struct sockaddr_in));
	mAddr.sin_family = AF_INET;
	mAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	mAddr.sin_port = htons(port);
	mSocket = socket(mAddr.sin_family, connectionType, 0);
	if (mSocket < 0) {
		fprintf(stderr, "ERROR opening socket: %d\n", errno);
		return false;
	}
	if (bind(mSocket, (struct sockaddr *) &mAddr, sizeof(mAddr)) < 0) {
		fprintf(stderr, "ERROR on binding to port %d. Is it already taken?\n", port);
		return false;
	} 
	if(connectionType == SOCK_STREAM) {
		return waitForClientConnection();
	} else {
		// UDP connection
		connected = true;
		printf("Successfully setup socket server.\n");
		return true;
	}
}

bool NetworkConnection::setupClient(const char *ipaddr, const int &port) {
	struct hostent *server = gethostbyname(ipaddr);
	bzero((char *) &mAddr, sizeof(mAddr));
	bzero((char *) &rAddr, sizeof(rAddr));
    mAddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&mAddr.sin_addr, server->h_length);
	mAddr.sin_port = htons(port);
	mSocket = socket(mAddr.sin_family, connectionType, 0);
	if (mSocket < 0) {
		fprintf(stderr, "ERROR opening socket: %d\n", errno);
		return false;
	}
    if(blockingTime >= 0) {
        fcntl(mSocket, F_SETFL, O_NONBLOCK);
    }
	if(connectionType == SOCK_STREAM) {
		return connectToServer();
	} else {
		// UDP connection
		connected = true;
		return true;
	}
}

bool NetworkConnection::connectToServer() {
	while(connect(mSocket,(struct sockaddr *) &mAddr,sizeof(mAddr)) < 0 && !interruptRead) {
        fprintf(stderr, "Couldn't connect to server. Will retry in a second.\n");
        std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	printf("Connected to server.\n");
	connected = true;
    return true;
}

bool NetworkConnection::waitForClientConnection() {
	bzero((char *) &mAddr, sizeof(mAddr));
	socklen_t connLen = sizeof(mAddr);
	listen(mSocket,5);
	printf("Waiting for client connection...\n");
	// accept a client socket
	clientSocket = accept(mSocket, (struct sockaddr *) &mAddr, &connLen);
	if (clientSocket < 0) {
		fprintf(stderr, "Accepting a connection failed with errno %d\n", errno);
		close(mSocket);
		return false;
	} else {
        if(blockingTime >= 0) {
            fcntl(clientSocket, F_SETFL, O_NONBLOCK);
        }
		printf("IPv4 client connected!\n");
		connected = true;
		return true;
	}
}

void NetworkConnection::exitGracefully() {
	// shutdown the send half of the connection since no more data will be sent
	// cleanup
	if(clientSocket > 0)
		close(clientSocket);
	if(mSocket > 0)
		close(mSocket);
}

int NetworkConnection::getData(char *buff, const int &buffSize) {
	if(connected && !interruptRead) {
		if(clientSocket > 0) {
			return recv(clientSocket, buff, buffSize, 0);
		} else {
			socklen_t len = sizeof(rAddr);
			return recvfrom(mSocket, buff, buffSize, 0, (struct sockaddr *)&rAddr, &len);
		}
	}
	return -1;
}

void NetworkConnection::failedRead() {
	connected = false;
	if(connectionType == SOCK_STREAM) {
		if(clientSocket > 0) {
			close(clientSocket);
			waitForClientConnection();
		} else {
			connectToServer();
		}
	}
}

// public 
bool NetworkConnection::write(const char *buff, const int &buffSize) {
	if(!connected) 
		return false;
    int *socket;
    sockaddr_in *addr;
	if(clientSocket > 0) {
        socket = &clientSocket;
        addr = &rAddr;
	} else if(server) {
        socket = &mSocket;
        addr = &rAddr;
	} else {
        socket = &mSocket;
        addr = &mAddr;
    }
	return sendto(*socket, buff, buffSize, 0, (sockaddr *) addr, (socklen_t) sizeof(*addr)) >= 0;
}
