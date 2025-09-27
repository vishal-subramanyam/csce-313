/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Vishal Subramanyam
	UIN: 333004049
	Date: 9/25/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <fstream>
#include <sys/wait.h>
#include <vector>
#include <algorithm> // 

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = MAX_MESSAGE;
	bool new_chan = false;
	vector<FIFORequestChannel*> channels;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi(optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}

	// give arguments for the server
	// server needs './server', '-m', '<val for -m arg>', 'NULL'
	// fork
	// in the child, run execvp using the server arguments
	pid_t pid = fork();
	if (pid == 0) {
		char m_str[20];
		sprintf(m_str, "%d", m);

		char* args[] = {
			(char*)"./server",
			(char*)"-m",
			m_str,
			nullptr
		};

		// replacing child process with server
		execvp(args[0], args);

		cerr << "exec failed\n";
		return 1;
	}
	else if (pid < 0) {
		cerr << "fork failed\n";
		return 1;
	}

	// create control channel
    FIFORequestChannel control_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&control_chan);

	if (new_chan) {
		// send new channel request to the server
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
		control_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));

		// create a variable to hold the name
		char new_name[30];
		// cread the response from the server
		control_chan.cread(new_name, sizeof(new_name));
		cout << "New channel name: " << new_name << "\n";
		// call the FIFORequestChannel constructor with the name from the server
		FIFORequestChannel* data_chan = new FIFORequestChannel(new_name, FIFORequestChannel::CLIENT_SIDE);
		// push the new channel into the vector
		channels.push_back(data_chan);
	}

	// use the last channel in vector (pointer now, not copy)
	FIFORequestChannel* chan = channels.back();
	
	// single data point when p, t, e != -1
	// example data point request
	if (p != -1 && t != -1 && e != -1) {
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e); // change from hardcoding to user's values
		
		memcpy(buf, &x, sizeof(datamsg));
		chan->cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan->cread(&reply, sizeof(double)); // answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}
	
	// else, if p != -1, request 1000 data points
	// loop over 1st 1000 lines
	// send request for ecg 1
	// send request for ecg 2
	// write line to received/x1.csv
	else if (p != -1) {
		ofstream fout("received/x1.csv");
		for (int i = 0; i < 1000; i++) {
			double time = i * 0.004;

			// ecg1
			datamsg m1(p, time, 1);
			chan->cwrite(&m1, sizeof(datamsg));
			double ecg1;
			chan->cread(&ecg1, sizeof(double));

			// ecg2
			datamsg m2(p, time, 2);
			chan->cwrite(&m2, sizeof(datamsg));
			double ecg2;
			chan->cread(&ecg2, sizeof(double));

			fout << time << "," << ecg1 << "," << ecg2 << "\n";
		}

		fout.close();
		cout << "First 1000 data points in received/x1.csv for patient " << p << "\n";
	}

	if (!filename.empty()) {
		// sending a non-sense message, you need to change this
		filemsg fm(0, 0);
		string fname = filename;
		
		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan->cwrite(buf2, len);  // I want the file length;

		__int64_t filesize = 0;
		chan->cread(&filesize, sizeof(__int64_t));
		cout << "Filesize: " << filesize << " bytes\n";

		string outpath = "received/" + fname;
		ofstream fout(outpath, ios::binary);

		// loop over the segments in the file filesize / buff capacity (m)
		// create filemsg instance
		char* buf3 = new char[m]; // create buffer of size buff capacity (m)
		filemsg* file_req = (filemsg*)buf2;

		__int64_t remaining = filesize;
		__int64_t offset = 0;

		while (remaining > 0) {
			file_req->offset = offset; // set offset in the file
			file_req->length = min((int)remaining, m); // set length, be careful of last segment

			chan->cwrite(buf2, len); // send the request (buf2)
			chan->cread(buf3, file_req->length); // cread into buf3 length file_req->length
			fout.write(buf3, file_req->length);  // write buf3 into file, received/filename

			offset += file_req->length;
			remaining -= file_req->length;
		}

		fout.close();

		delete[] buf2;
		delete[] buf3;
	}

	// close and delete new channel if necessary
	if (new_chan) {
		MESSAGE_TYPE quit = QUIT_MSG;
		channels.back()->cwrite(&quit, sizeof(MESSAGE_TYPE));
		delete channels.back();
		channels.pop_back();
	}
	
	// closing the channel    
    MESSAGE_TYPE quit = QUIT_MSG;
    channels[0]->cwrite(&quit, sizeof(MESSAGE_TYPE));

	wait(nullptr);
	return 0;
}
