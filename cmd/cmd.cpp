/*
	Aseba - an event-based middleware for distributed robot control
	Copyright (C) 2006 - 2007:
		Stephane Magnenat <stephane at magnenat dot net>
		(http://stephane.magnenat.net)
		Valentin Longchamp <valentin dot longchamp at epfl dot ch>
		Laboratory of Robotics Systems, EPFL, Lausanne
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	any other version as decided by the two original authors
	Stephane Magnenat and Valentin Longchamp.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../msg/msg.h"
#include "../utils/utils.h"
#include "HexFile.h"
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>

namespace Aseba 
{
	using namespace std;
	
	//! Show list of known commands
	void dumpCommandList(ostream &stream)
	{
		stream << "* presence : broadcast presence message\n";
		stream << "* rdpage : bootloader read page [dest] [page number]\n";
		stream << "* whex : write hex file [dest] [file name]\n";
		stream << "* rhex : read hex file [source] [file name]\n";
	}
	
	//! Show usage
	void dumpHelp(ostream &stream, const char *programName)
	{
		stream << "Aseba cmd, send message over the aseba network, usage:\n";
		stream << programName << " [-a address] [-p port] [cmd destId (args)] ... [cmd destId (args)]\n";
		stream << "where cmd is one af the following:\n";
		dumpCommandList(stream);
	}
	
	//! Produce an error message and dump help and quit
	void errorMissingArgument(const char *programName)
	{
		cerr << "Error, missing argument.\n";
		dumpHelp(cerr, programName);
		exit(4);
	}
	
	//! Produce an error message and dump help and quit
	void errorUnknownCommand(const char *cmd)
	{
		cerr << "Error, unknown command " << cmd << endl;
		cerr << "Known commands are:\n";
		dumpCommandList(cerr);
		exit(5);
	}
	
	//! Produce an error message and quit
	void errorReadPage(int pageNumber)
	{
		cerr << "Error, can't read page " << pageNumber << endl;
		exit(6);
	}
	
	//! Produce an error message and quit
	void errorOpenFile(const char *fileName)
	{
		cerr << "Error, can't open file " << fileName << endl;
		exit(7);
	}
	
	//! Produce an error message and quit
	void errorWriteFile(const char *fileName)
	{
		cerr << "Error, can't write file " << fileName << endl;
		exit(8);
	}
	
	//! Produce an error message and quit
	void errorServerDisconnected()
	{
		cerr << "Server closed connection before command was completely sent." << endl;
		exit(9);
	}
	
	//! Produce an error message and quit
	void errorHexFile(const string &message)
	{
		cerr << "HEX file error: " << message << endl;
		exit(10);
	}
	
	//! Produce an error message and quit
	void errorWritePage(unsigned pageIndex)
	{
		cerr << "Error while writing page " << pageIndex << endl;
		exit(11);
	}
	
	//! Produce an error message and quit
	void errorReadPage(unsigned pageIndex)
	{
		cerr << "Error while reading page " << pageIndex << endl;
		exit(12);
	}
	
	//! Produce an error message and quit
	void errorReadPage(unsigned pageIndex, unsigned pagesStart, unsigned pagesCount)
	{
		cerr << "Error, page index " << pageIndex << " out of page range [ " << 	pagesStart << " : " << pagesStart + pagesCount << " ]" << endl;
		exit(13);
	}
	
	//! Open a TCP connection, exit on failure
	int openConnection(const char *address, uint16_t port)
	{
		// fill server description structure
		struct sockaddr_in serverAddress;
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(port);
		
		// get server ip
		struct hostent *hostp = gethostbyname(address);
		if (!hostp)
		{
			cerr << "Cannot get address of server: " << strerror(errno) << endl;
			exit(1);
		}
		memcpy(&serverAddress.sin_addr, hostp->h_addr, sizeof(serverAddress.sin_addr));
		
		// create socket
		int socket = ::socket(AF_INET, SOCK_STREAM, 0);
		if (socket < 0)
		{
			cerr << "Cannot create socket: " << strerror(errno) << endl;
			exit(2);
		}
		
		// connect
		int ret = connect(socket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
		if (ret < 0)
		{
			cerr << "Connection to server failed: " << strerror(errno) << endl;
			exit(3);
		}
		
		return socket;
	}
	
	//! Manage interactions with an aseba-compatible bootloader
	class BootloaderInterface
	{
	public:
		//! Create an interface to bootloader with id dest using a socket
		BootloaderInterface(int socket, int dest) :
			socket(socket),
			dest(dest)
		{
			// Wait until the bootloader answers
			while (true)
			{
				Message *message = Message::receive(socket);
				BootloaderDescription *bDescMessage = dynamic_cast<BootloaderDescription *>(message);
				if (bDescMessage && (bDescMessage->source == dest))
				{
					pageSize = bDescMessage->pageSize;
					pagesStart = bDescMessage->pagesStart;
					pagesCount = bDescMessage->pagesCount;
					delete message;
					break;
				}
				delete message;
			}
			pageSize = 2048;
			pagesStart = 0;
			pagesCount = 64;
		}
		
		//! Return the size of a page
		int getPageSize() { return pageSize; }
		
		//! Read a page
		bool readPage(unsigned pageNumber, uint8* data)
		{
			if ((pageNumber < pagesStart) || (pageNumber >= pagesStart + pagesCount))
			{
				errorReadPage(pageNumber, pagesStart, pagesCount);
				return false;
			}
			
			// send command
			BootloaderReadPage message;
			message.dest = dest;
			message.pageNumber = pageNumber;
			message.serialize(socket);
			unsigned dataRead = 0;
			
			// get data
			while (true)
			{
				Message *message = Message::receive(socket);
				
				// handle ack
				BootloaderAck *ackMessage = dynamic_cast<BootloaderAck *>(message);
				if (ackMessage && (ackMessage->source == dest))
				{
					uint16 errorCode = ackMessage->errorCode;
					delete message;
					if (errorCode == BootloaderAck::SUCCESS)
					{
						if (dataRead < pageSize)
							cerr << "Warning, got acknowledgement but page not fully read (" << dataRead << "/" << pageSize << ") bytes.\n";
						return true;
					}
					else
						return false;
				}
				
				// handle data
				BootloaderDataRead *dataMessage = dynamic_cast<BootloaderDataRead *>(message);
				if (dataMessage && (dataMessage->source == dest))
				{
					if (dataRead >= pageSize)
						cerr << "Warning, reading oversized page (" << dataRead << "/" << pageSize << ") bytes.\n";
					copy(dataMessage->data, dataMessage->data + sizeof(dataMessage->data), data);
					data += sizeof(dataMessage->data);
					dataRead += sizeof(dataMessage->data);
					cout << "Page read so far (" << dataRead << "/" << pageSize << ") bytes.\n";
				}
				
				delete message;
			}
			
			return true;
		}
		
		//! Write a page
		bool writePage(int pageNumber, const uint8 *data)
		{
			// send command
			BootloaderWritePage writePage;
			writePage.dest = dest;
			writePage.pageNumber = pageNumber;
			writePage.serialize(socket);
			
			// write data
			for (unsigned dataWritten = 0; dataWritten < pageSize;)
			{
				BootloaderPageDataWrite pageData;
				copy(data + dataWritten, data + dataWritten + sizeof(pageData.data), pageData.data);
				pageData.serialize(socket);
				dataWritten += sizeof(pageData.data);
			}
			
			// wait ack
			while (true)
			{
				Message *message = Message::receive(socket);
				
				// handle ack
				BootloaderAck *ackMessage = dynamic_cast<BootloaderAck *>(message);
				if (ackMessage && (ackMessage->source == dest))
				{
					uint16 errorCode = ackMessage->errorCode;
					delete message;
					return errorCode == BootloaderAck::SUCCESS;
				}
				
				delete message;
			}
		}
		
		//! Write an hex file
		void writeHex(const string &fileName)
		{
			// Load hex file
			HexFile hexFile;
			hexFile.read(fileName);
			
			// Build a map of pages out of the map of addresses
			typedef map<uint32, vector<uint8> > PageMap;
			PageMap pageMap;
			for (HexFile::ChunkMap::iterator it = hexFile.data.begin(); it != hexFile.data.end(); it ++)
			{
				// get page number
				unsigned chunkAddress = it->first;
				// index inside data chunk
				unsigned chunkDataIndex = 0;
				// size of chunk in bytes
				unsigned chunkSize = it->second.size();
				
				// copy data from chunk to page
				do
				{
					// get page number
					unsigned pageIndex = (chunkAddress + chunkDataIndex) / pageSize;
					// get address inside page
					unsigned byteIndex = (chunkAddress + chunkDataIndex) % pageSize;
				
					// if page does not exists, create it
					if (pageMap.find(pageIndex) == pageMap.end())
						pageMap[pageIndex] = vector<uint8>((uint8)0, pageSize);
					
					// copy data
					unsigned amountToCopy = min(pageSize - byteIndex, chunkSize - chunkDataIndex);
					copy(it->second.begin() + chunkDataIndex, it->second.begin() + chunkDataIndex + amountToCopy, &(pageMap[pageIndex][byteIndex]));
					
					// increment chunk data pointer
					chunkDataIndex += amountToCopy;
				}
				while (chunkDataIndex < chunkSize);
			}
			
			// Write pages
			for (PageMap::iterator it = pageMap.begin(); it != pageMap.end(); it ++)
			{
				unsigned pageIndex = it->first;
				if ((pageIndex >= pagesStart) && (pageIndex < pagesStart + pagesCount))
					if (!writePage(pageIndex, &it->second[0]))
						errorWritePage(pageIndex);
			}
		}
		
		//! Read an hex file
		void readHex(const string &fileName)
		{
			HexFile hexFile;
			
			// Create memory
			unsigned address = pagesStart * pageSize;
			hexFile.data[address] = vector<uint8>();
			hexFile.data[address].reserve(pagesCount * pageSize);
			
			// Read pages
			for (unsigned page = pagesStart; page < pagesCount; page++)
			{
				vector<uint8> buffer((uint8)0, pageSize);
				
				if (!readPage(page, &buffer[0]))
					errorReadPage(page);
				
				copy(&buffer[0], &buffer[pageSize], back_inserter(hexFile.data[address]));
			}
			
			// Write hex file
			hexFile.write(fileName);
		}
	
	protected:
		int socket;
		int dest;
		unsigned pageSize;
		unsigned pagesStart;
		unsigned pagesCount;
	};
	
	//! Process a command, return the number of arguments eaten (not counting the command itself)
	int processCommand(int socket, int argc, char *argv[])
	{
		const char *cmd = argv[0];
		int argEaten = 0;
		
		if (strcmp(cmd, "presence") == 0)
		{
			Presence message;
			message.serialize(socket);
		}
		else if (strcmp(cmd, "rdpage") == 0)
		{
			// first arg is dest, second is page number
			if (argc < 3)
				errorMissingArgument(argv[0]);
			argEaten = 2;
			
			BootloaderInterface bootloader(socket, atoi(argv[1]));
			
			vector<uint8> data(bootloader.getPageSize());
			if (bootloader.readPage(atoi(argv[2]), &data[0]))
			{
				ofstream file("page.bin");
				if (file.good())
					copy(data.begin(), data.end(), ostream_iterator<uint8>(file));
				else
					errorOpenFile("page.bin");
			}
			else
				errorReadPage(atoi(argv[2]));
		}
		else if (strcmp(cmd, "whex") == 0)
		{
			// first arg is dest, second is file name
			if (argc < 3)
				errorMissingArgument(argv[0]);
			argEaten = 2;
			
			// try to write hex file
			try
			{
				BootloaderInterface bootloader(socket, atoi(argv[1]));
				bootloader.writeHex(argv[2]);
			}
			catch (HexFile::Error &e)
			{
				errorHexFile(e.toString());
			}
		}
		else if (strcmp(cmd, "rhex") == 0)
		{
			// first arg is source, second is file name
			if (argc < 3)
				errorMissingArgument(argv[0]);
			argEaten = 2;
			
			// try to read hex file
			try
			{
				BootloaderInterface bootloader(socket, atoi(argv[1]));
				bootloader.readHex(argv[2]);
			}
			catch (HexFile::Error &e)
			{
				errorHexFile(e.toString());
			}
		}
		else
			errorUnknownCommand(cmd);
		
		return argEaten;
	}
}

int main(int argc, char *argv[])
{
	const char *address = "localhost";
	uint16_t port = ASEBA_DEFAULT_PORT;
	int argCounter = 1;
	
	//
	while (argCounter < argc)
	{
		const char *arg = argv[argCounter];
		if (strcmp(arg, "-p") == 0)
		{
			if (++argCounter < argc)
				port = static_cast<uint16_t>(atoi(argv[argCounter]));
			else
				Aseba::errorMissingArgument(argv[0]);
		}
		else if (strcmp(arg, "-a") == 0)
		{
			if (++argCounter < argc)
				address = argv[argCounter];
			else
				Aseba::errorMissingArgument(argv[0]);
		}
		else if ((strcmp(arg, "-h") == 0) || (strcmp(arg, "--help") == 0))
		{
			Aseba::dumpHelp(std::cout, argv[0]);
			return 0;
		}
		else
		{
			// aseba command
			if (argCounter + 2 > argc)
				Aseba::errorMissingArgument(argv[0]);
			
			// open connection to switch
			int socket = Aseba::openConnection(address, port);
			
			// process command
			try
			{
				 argCounter += Aseba::processCommand(socket, argc - argCounter, &argv[argCounter]);
			}
			catch (Aseba::Exception::FileDescriptorClosed e)
			{
				Aseba::errorServerDisconnected();
			}
			
			// close connection properly
			shutdown(socket, SHUT_RDWR);
			close(socket);
		}
		argCounter++;
	}
	return 0;
}