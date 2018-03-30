/* Copyright 2018 Ryan Cooper (RyanLoringCooper@gmail.com)
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "../SerialConnection.h"

// protected:
void SerialConnection::failedRead() {
	fprintf(stderr, "Failed to read from serial port with error: %d\n", errno);
	connected = false;
}

int SerialConnection::getData(char *buff, const int &buffSize) {
	if(!connected)
		return -1; 
	return ::read(ser, buff, buffSize);
}

void SerialConnection::exitGracefully() {
	if (connected && ser > 0) {
		connected = false;
		close(ser);
	}
}

// taken from https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
int SerialConnection::set_interface_attribs (const int &speed, const int &parity) {
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (ser, &tty) != 0) {
		fprintf(stderr, "error %d from tcgetattr\n", errno);
		return -1;
	}

	cfsetospeed (&tty, speed);
	cfsetispeed (&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;         // disable break processing
	tty.c_lflag = 0;                // no signaling chars, no echo,
	                                // no canonical processing
	tty.c_oflag = 0;                // no remapping, no delays
	tty.c_cc[VMIN]  = 0;            // read doesn't block
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
	                                // enable reading
	tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr (ser, TCSANOW, &tty) != 0) {
		fprintf(stderr, "error %d from tcsetattr\n", errno);
		return -1;
	}
	return 0;
}

// adapted from https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
bool SerialConnection::setBlocking(const int &blockingTime) {
	if(blockingTime == -1) {
        struct termios tty;
        memset (&tty, 0, sizeof(tty));
        if (tcgetattr (ser, &tty) != 0) {
            fprintf(stderr,"error %d from tggetattr\n", errno);
            return false;
        }

        tty.c_cc[VMIN]  = 1;            // sets blocking to true
        tty.c_cc[VTIME] = blockingTime/100;            // 0.5 seconds read timeout

        if (tcsetattr (ser, TCSANOW, &tty) != 0) {
            fprintf(stderr, "error %d setting term attributes\n", errno);
            return false;
        }
    }
	return true;
}


// public:
// speed should have a B in front of it (ie. B57600)
// I recommend parity 0
SerialConnection::SerialConnection(const char *portName, const int &speed, const int &parity, const int &blockingTime, const bool &debug, const bool &noReads) : CommConnection(blockingTime, debug, noReads) {
	connected = false;
	ser = open (portName, O_RDWR | O_NOCTTY | O_SYNC);
	if (ser < 0) {
		fprintf(stderr, "error %d opening %s\n", errno, portName);
		return;
	}
	if(set_interface_attribs (speed, parity) != 0) {
		fprintf(stderr, "Could not set serial parameters.\n");
		return;
	}
	connected = true;
}

SerialConnection::SerialConnection(const SerialConnection &other) : CommConnection(other) {
    if(this == &other) {
        return;
    }
    *this = other;
}

SerialConnection &SerialConnection::operator=(const SerialConnection &other) {
    if(this == &other) {
        return *this;
    }
    ser = other.ser;
    CommConnection::operator=(other);
    return *this;
}

// Sets errno when it returns false, indicating there was an error with writting
bool SerialConnection::write(const char *buff, const int &buffSize) {
	if(!connected) 
		return false;
	return ::write(ser, buff, buffSize) != -1;
}
