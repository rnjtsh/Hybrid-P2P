#include <stdlib.h>
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <csignal>
#include <chrono>
#include <ctime>

using namespace std;

#define ERROR -1
#define MAX_CLIENTS 10
#define MAX_DATA 1024

#define HTTIMER 300

typedef struct client_info {
	string client_ip, client_port, downloading_port, client_root;
	chrono::time_point<chrono::system_clock> lastConnect;
} client_info;
struct socketInfo {
	int nw;
	struct sockaddr_in cli;
};
int sock;
map<pair<string, string>, string> main_rep_map;
map<string, client_info> client_info_map;
string main_repofile, client_list_file, server_root;
const char *server_ip, *server_port;
std::vector<pthread_t> threadV;

void loadMainRepoMap() {
	if (access((server_root + "/" + main_repofile).c_str(), F_OK) != 0) {
		FILE *fp = fopen((server_root + "/" + main_repofile).c_str(), "w+");
		fclose(fp);
	}
	ifstream inFile;
	string line;
	inFile.open(server_root + "/" +main_repofile);
	while(inFile.good()) {
		string pch;
		vector<string> tempV;
		getline(inFile,line); // get line from file
		if(!line.empty()) {
			stringstream data(line);
			while (getline(data, pch, ':')) {
				tempV.push_back(pch);
			}
			pair<string, string> fc_pair = make_pair(*(tempV.begin()), *(tempV.begin() + 2));
			main_rep_map.insert ( pair<pair<string, string>, string>(fc_pair, *(tempV.begin() + 1)) );
		}
	}
	inFile.close();
}

void loadClientInfoMap() {
	if (access((server_root + "/" + client_list_file).c_str(), F_OK) != 0) {
		FILE *fp = fopen((server_root + "/" + client_list_file).c_str(), "w+");
		fclose(fp);
	}
	ifstream inFile;
	string line;
	inFile.open(server_root + "/" + client_list_file);
	client_info cli_info;

	while(inFile.good()) {
		string pch;
		vector<string> tempV;
		getline(inFile,line); // get line from file
		if(!line.empty()) {
			stringstream data(line);
			while (getline(data, pch, ':')) {
				tempV.push_back(pch);
			}
			cli_info.client_ip = *(tempV.begin() + 1);
			cli_info.client_port = *(tempV.begin() + 2);
			cli_info.downloading_port = *(tempV.begin() + 3);
			client_info_map.insert ( pair<string, client_info>(*(tempV.begin()), cli_info) );
		}

	}
	inFile.close();
}

void writeMainRepoFileData() {
	ofstream fs;
	map<pair<string, string>, string>::iterator main_repo_it;
	string str = "";
  	fs.open ((server_root + "/" + main_repofile).c_str(), ofstream::out );

	for (main_repo_it = main_rep_map.begin(); main_repo_it != main_rep_map.end(); main_repo_it++) {
		str += (main_repo_it->first).first;
		str += ":";
		str += main_repo_it->second;
		str += ":";
		str += (main_repo_it->first).second;
		str += "\n";
	}
	fs << str;
	fs.close();
}

void writeClientInfoFileData() {
	ofstream fs;
	map<string, client_info>::iterator client_info_it;
	string str = "";
  	fs.open (server_root + "/" + client_list_file, ofstream::out);
	for (client_info_it = client_info_map.begin(); client_info_it != client_info_map.end(); client_info_it++) {
		str += client_info_it->first;
		str += ":";
		str += (client_info_it->second).client_ip;
		str += ":";
		str += (client_info_it->second).client_port;
		str += ":";
		str += (client_info_it->second).downloading_port;
		str += "\n";
	}
	fs << str;
	fs.close();
}

void disconnectInactiveClient() {
	for (auto i = client_info_map.begin(); i != client_info_map.end(); ++i) {
		chrono::duration<double, std::milli> fp_ms = chrono::system_clock::now() - (i->second).lastConnect;
		//count << "time: " << fp_ms.count() << " " << HTTIMER * 1000;
		if (fp_ms.count() > (HTTIMER + 10) * 1000)
			client_info_map.erase(i);
	}
}

int compareSearchKey(const char * a, const char * b) {
	//flag = 0;
	for (int i = 0; i < strlen(b); ++i) {
		if (a[i] != b[i] && (int)a[i] != (int)b[i] + 32 && (int)a[i] != (int)b[i] - 32) {
			return -1;
		}
	}
	return 0;

}

void* clientServerComm(void *sck) {
	int data_len, fcount;
	char data[MAX_DATA], fileInfo[MAX_DATA];
	char * pch;
	map<string, client_info>::iterator client_info_it;

	size_t fspace, fslash;
	string comm, fstr, ack, client_alias;
	map<pair<string, string>, string>::iterator main_repo_it;
	client_info cli_info;
	socketInfo si = *((socketInfo *)sck);
	//pthread_detach(pthread_self());
	cout << "New client from port " << ntohs(si.cli.sin_port) << " and IP " << inet_ntoa(si.cli.sin_addr) << '\n';
	data_len = 1;
	while(data_len) {
		vector<string> tempV;
		data_len = recv(si.nw, data, MAX_DATA, 0);
		/*Data parsing*/
		//cout << "data " << data << '\n';
		if (data == "EXIT") {
			exit(0);
		}
		if(data_len) {
			tempV.clear();

			pch = strtok (data,"#@#");
			while (pch != NULL) {
				tempV.push_back(pch);
				pch = strtok (NULL, "#@#");
			}
			comm = *(tempV.begin());
			cout << "tempv len " << tempV.size() <<endl;
			if (comm.compare("check") == 0) {
				std::string ret = "FALSE";
				std::cout << *(tempV.begin()) << *(tempV.begin()+1) <<*(tempV.begin()+2) << '\n';
				client_info_it = client_info_map.find(*(tempV.begin() + 1));
				for (main_repo_it = main_rep_map.begin(); main_repo_it != main_rep_map.end(); main_repo_it++) {
					if((main_repo_it->first).first.find(*(tempV.begin()+2)) != string::npos && client_info_it != client_info_map.end()) {
						ret = "TRUE";
						break;
					}
				}
				send(si.nw, ret.c_str(), ret.size(), 0);
			}
			else if (comm.compare("*get") == 0) {

				string searchstr = "";
					string fileName = *(tempV.begin() + 1);
					string clientName = *(tempV.begin() + 2);
					fcount = 0;
					for (auto i = tempV.begin(); i != tempV.end(); ++i)
					{
						cout<<*i<<endl;
					}
					for (main_repo_it = main_rep_map.begin(); main_repo_it != main_rep_map.end(); main_repo_it++) {
						if((main_repo_it->first).first.compare(fileName) != 0 && (main_repo_it->first).second.compare(clientName) != 0) {
							client_info_it = client_info_map.find((main_repo_it->first).second);
							if (client_info_it != client_info_map.end()) {
								searchstr += (char)91 + to_string(fcount) + (char)93 + (char)32;
								searchstr += (main_repo_it->first).first;
								searchstr += ":";
								searchstr += main_repo_it->second;
								searchstr += ":";
								searchstr += (main_repo_it->first).second;
								searchstr += ":";

								std::cout << client_info_it->first << '\n';
								searchstr += (client_info_it->second).client_ip;
								searchstr += ":";
								searchstr += (client_info_it->second).client_port;
								searchstr += ":";
								searchstr += (client_info_it->second).downloading_port;
								
								searchstr += ":";
								searchstr += *(tempV.begin() + 3);
								searchstr.push_back('\0');
								searchstr += 10;
								fcount++;
								break;
							}
						}
					}
					if (fcount == 0)
						searchstr += "File is not shared by the client!!";					
					send(si.nw, searchstr.c_str(), searchstr.size(), 0);
			}
			else{
				client_alias = *(tempV.begin() + 2);
				cli_info.client_ip = *(tempV.begin() + 3);
				cli_info.client_port = *(tempV.begin() + 4);
				cli_info.downloading_port = *(tempV.begin() + 5);
				cli_info.client_root = *(tempV.begin() + 6);

				if (comm.compare("search") == 0) {

					string searchstr = "";
					if(tempV.size() != 7)
						ack = "FAILURE:INVALID COMMAND\n";
					else {
						string keyword = *(tempV.begin() + 1);
						fcount = 0;
						for (main_repo_it = main_rep_map.begin(); main_repo_it != main_rep_map.end(); main_repo_it++) {
							if(compareSearchKey(((main_repo_it->first).first).c_str(), keyword.c_str()) == 0) {
								client_info_it = client_info_map.find((main_repo_it->first).second);
								if (client_info_it != client_info_map.end()) {
									fcount++;
									searchstr += (char)91 + to_string(fcount) + (char)93 + (char)32;
									searchstr += (main_repo_it->first).first;
									searchstr += ":";
									searchstr += main_repo_it->second;
									searchstr += ":";
									searchstr += (main_repo_it->first).second;
									searchstr += ":";

									std::cout << client_info_it->first << '\n';
									searchstr += (client_info_it->second).client_ip;
									searchstr += ":";
									searchstr += (client_info_it->second).client_port;
									searchstr += ":";
									searchstr += (client_info_it->second).downloading_port;
									searchstr += 10;
								}
							}
						}
						searchstr.insert(0, "FOUND:" + to_string(fcount) + (char)10);
					}
					send(si.nw, searchstr.c_str(), searchstr.size(), 0);
				}
				else if (comm.compare("share") == 0) {
					string path, file;
					if(tempV.size() != 7)
						ack = "FAILURE:INVALID COMMAND\n";
					else {
						path = *(tempV.begin() + 1);
						path = "/" + path;
						fslash = path.find_last_of("/");
						if(fslash != string::npos)
			        		file = path.substr(fslash + 1);
						else
							file = path;

						pair<string, string> fc_pair = make_pair(file, client_alias);
						main_rep_map.insert ( pair<pair<string, string>, string>(fc_pair, path) );
						//writeMainRepoFileData();
						ack = "SUCCESS:FILE SHARED\n";
					}
					send(si.nw, ack.c_str(), ack.size(), 0);
				}
				else if (comm.compare("del") == 0) {
					string path, file, client_alias;
					if (tempV.size() != 7)

						ack = "FAILURE:INVALID COMMAND\n";
					else {
						path = *(tempV.begin() + 1);
						fslash = path.find_last_of("/");
						if(fslash != string::npos)
			        		file = path.substr(fslash + 1);
						else
							file = path;
						int delflg = 1;
						client_alias = *(tempV.begin() + 2);
						for (main_repo_it = main_rep_map.begin(); main_repo_it != main_rep_map.end(); main_repo_it++) {
							// cout << "del func " << (main_repo_it->first).first << " " << file <<endl;;
							if((main_repo_it->first).first.compare(file) == 0) {
								if (((main_repo_it->first).second).compare(client_alias) == 0) {
									main_rep_map.erase(main_repo_it);
									delflg = 0;
								}
							}
						}
						//writeMainRepoFileData();
						if(delflg == 0)
							ack = "FAILURE:FILE REMOVED\n";
						else
							ack = "FAILURE:FILE DOESN'T EXIST IN CRS\n";
					}
					send(si.nw, ack.c_str(), ack.size(), 0);
				}
				else if (comm.compare("heart") == 0) {
					cli_info.lastConnect = chrono::system_clock::now();
					client_info_map.insert ( pair<string, client_info>(client_alias, cli_info) );
					// cout << "heartBeat updated" << '\n';
					// for (auto i = client_info_map.begin(); i != client_info_map.end(); ++i)
					// 		cout << i->first << '\n';
					writeClientInfoFileData();
					ack = "ACK";
					send(si.nw, ack.c_str(), ack.size(), 0);
				}
				else if (comm.compare("exec") == 0) {
					string searchstr = "";
					if (tempV.size() != 12) {
						searchstr += "#";
						searchstr += "INVALID COMMAND";
					}
					else{
						string remote_client_alias = *(tempV.begin() + 1);
						client_info_it = client_info_map.find(remote_client_alias);
						if (client_info_it != client_info_map.end()) {
							searchstr += remote_client_alias;
							searchstr += "#@#"; 
							searchstr += (client_info_it->second).client_ip;
							searchstr += "#@#";
							searchstr += (client_info_it->second).client_port;
							searchstr += "#@#";
							searchstr += *(tempV.begin() + 2);
						}
						else {
							searchstr += "#";
							//searchstr += remote_client_alias;
							searchstr += "FAILURE:REMOTE CLIENT NOT IN THE NETWORK\n";
						}
					}
					
					send(si.nw, searchstr.c_str(), searchstr.size(), 0);
				}
				else {}
			}
		}
		bzero(data, strlen(data));
	}
	cout << "Client Closed " << si.nw << '\n';
	close(si.nw);
	pthread_exit(0);
}

void signalHandler_KillingThreads( int signum ) {
	std::vector<pthread_t>::iterator thit;
	for (thit = threadV.begin(); thit != threadV.end(); thit++) {
		//std::cout << "Killing thread : " << *thit << '\n';
		pthread_kill(*thit, signum);
	}
	threadV.empty();
	close(sock);
	exit(signum);
}

void * heartBeat(void * arg) {
	while(1){
		disconnectInactiveClient();
		//writeClientInfoFileData();
		sleep(HTTIMER);
	}
}

void * flushDS(void * arg) {
	while(1){
		writeMainRepoFileData();
		writeClientInfoFileData();
		sleep(20);
	}
}

int main(int argc, char const *argv[]) {
	struct sockaddr_in server; //port to which server is bound locally
	struct sockaddr_in client;
	socklen_t sockaddr_len = sizeof(struct sockaddr_in);
	socketInfo sck;
	int nw, eflag = 0;
	pthread_t thread;
	server_ip = argv[1];
	server_port = argv[2], main_repofile = argv[3], client_list_file = argv[4], server_root = argv[5];
	signal(SIGINT, signalHandler_KillingThreads); //registers the signal handler

	loadMainRepoMap();
	//loadClientInfoMap();
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		perror("server socket: ");
		exit(-1);
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(server_port));
	server.sin_addr.s_addr = inet_addr(server_ip);
	bzero(&server.sin_zero, 8);

	if ((bind(sock, (struct sockaddr*)&server, sockaddr_len)) == ERROR) {
		perror("bind : ");
		exit(1);
	}


	pthread_create(&thread, NULL, &heartBeat, NULL);
	threadV.push_back(thread);
	pthread_create(&thread, NULL, &flushDS, NULL);
	threadV.push_back(thread);
	
	std::cout << "Server started..." << '\n';

	if ((listen(sock, MAX_CLIENTS)) == ERROR) {
		perror("listen");
		exit(-1);
	}

	while(1) {
		if ((sck.nw = accept(sock, (struct sockaddr*)&client, &sockaddr_len)) == ERROR) {
			std::cout << "Can't connect with client " << inet_ntoa(client.sin_addr) << '\n';
		}
		else {
			sck.cli = client;
			pthread_create(&thread, NULL, &clientServerComm, (void *) &sck);
			threadV.push_back(thread);
		}
	}
	close(sock);
	return 0;
}
