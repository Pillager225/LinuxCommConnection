#include "CommConnection.h"
#include <cstdio>

void CommConnection::performReads() {
	char buff[MAX_DATA_LENGTH];
	memset(buff, 0, MAX_DATA_LENGTH);
	int bytesRead;
	while(!interruptRead) {
		bytesRead = getData(buff, MAX_DATA_LENGTH);
 	    if (bytesRead > 0) {
	        fillBuffer(buff, bytesRead);
	        cvBool = true;
	        cv.notify_one();
	    } else if(bytesRead < 0 && blockingTime < 0) {
			failedRead();
		} else if(blockingTime != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(blockingTime));
        }
	}
}

void CommConnection::fillBuffer(char *buff, const int &bytesRead) {
	int newWriteIndex = bytesRead+writeIndex;
	if(newWriteIndex < BUFFER_SIZE) {
		memcpy(&buffer[writeIndex], buff, bytesRead);
		writeIndex = newWriteIndex;
	} else {
		int overflow = newWriteIndex-BUFFER_SIZE;
		int underflow = BUFFER_SIZE-writeIndex;
		memcpy(&buffer[writeIndex], buff, underflow);
		memcpy(buffer, &buff[underflow], overflow);
		writeIndex = overflow;
	}
}

void CommConnection::closeThread() {
	interruptRead = true;
	if(readThread != NULL && readThread->joinable()) {
		readThread->join();
		delete readThread;
		readThread = NULL;
	}
}

CommConnection::CommConnection(const int &blockingTime, const bool &noReads) {
    this->blockingTime = blockingTime;
	this->noReads = noReads;
	connected = false;
	interruptRead = false;
	terminated = false;
	cvBool = false;
	buffer = new char[BUFFER_SIZE+1];
	memset(buffer, 0, BUFFER_SIZE+1);
	readIndex = 0;
	writeIndex = 0;	
}

bool CommConnection::begin() {
	if(connected) {
		begun = true;
		if(!noReads) {
			readThread = new std::thread(&CommConnection::performReads, this);
		}
		return true;
	} else {
		return false;
	}
}

int CommConnection::available() const {
	int retval = writeIndex-readIndex;
	if(retval < 0) 
		retval = writeIndex+BUFFER_SIZE-readIndex;
	return retval;
}

int CommConnection::waitForData() {
	std::unique_lock<std::mutex> lk(dataMutex);
	cv.wait(lk, [this]{return this->cvBool;});
	return available();
}

char CommConnection::read() {
	if (readIndex + 1 < BUFFER_SIZE) {
		return buffer[readIndex++];
	} else {
		char retval = buffer[readIndex];
		readIndex = 0;
		return retval;
	}
}

void CommConnection::read(char *buff, const unsigned int &bytesToRead) {
	int newReadIndex = readIndex+bytesToRead;
	if(newReadIndex < BUFFER_SIZE) {
		memcpy(buff, &buffer[readIndex], bytesToRead);
		readIndex = newReadIndex;
	} else {
		int overflow = newReadIndex-BUFFER_SIZE;
		int underflow = BUFFER_SIZE-readIndex;
		memcpy(buff, &buffer[readIndex], underflow);
		memcpy(&buff[underflow], buffer, overflow);
		readIndex = overflow;
	}
}

// does not put the delim character in the buff 
int CommConnection::readUntil(char *buff, const int &buffSize, const char &delim) { // TODO this needs work! Can't handle wrap around case
	int i = readIndex, count = 0, leftOff = 0;
	while(true) {
		if(available() > 0) {
			if(buffer[i] == delim) {
				memcpy(&buff[leftOff], &buffer[readIndex], i-readIndex);
				readIndex = i+1;
				return count;
			} else if(i == BUFFER_SIZE) {
				memcpy(&buff[leftOff], &buffer[readIndex], i-readIndex);
				leftOff = i-readIndex;
				readIndex = 0;
				i = 0;
			} else {
				i++;
			}
			count++;
			if(count == buffSize) {
				return count;
			}
		}
	}
}

bool CommConnection::isConnected() const {
	return connected;
}

void CommConnection::clearBuffer() {
	readIndex = writeIndex;
}

void CommConnection::terminate() {
	if(!terminated) {
		terminated = true;
		closeThread();
		//delete[] buffer;
		exitGracefully();
	}
}

CommConnection::~CommConnection() {
    terminate();
}
