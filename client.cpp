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
#include <algorithm>
#include <fstream>
#include <pthread.h>

using namespace std;

#define ERROR -1
#define BUFFER 1024
#define FILE_BUFFER 16*1024
#define MAX_CLIENTS 1
#define HTTIMER 300

const char *client_alias, *client_ip, *client_port, *server_ip, *server_port, *downloading_port, *client_root;
struct sockaddr_in remote_server, server, RPCserver, dwnClient;
socklen_t sockaddr_len = sizeof(struct sockaddr_in);

void recieveFile(string infoFile) {
	struct sockaddr_in dwnServ;
	int sock;
	char in[FILE_BUFFER];
	vector<string> tempV;
	string pch;

	stringstream data(infoFile);
	size_t bytesRcvd;
	while (getline(data, pch, ':'))
		tempV.push_back(pch);	

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		perror("server socket: ");
		exit(-1);
	}

	dwnServ.sin_family = AF_INET;
	dwnServ.sin_port = htons(atoi((*(tempV.begin())).c_str()));
	dwnServ.sin_addr.s_addr = inet_addr((*(tempV.begin() + 1)).c_str());
	bzero(&dwnServ.sin_zero, 8);

	if ((connect(sock, (struct sockaddr*)&dwnServ, sizeof(struct sockaddr_in))) == ERROR)
		cout << "FAILURE:SERVER OFFLINE" << '\n';
	else {
		// int fslash = (*(tempV.begin() + 2)).find_last_of("/");
		// string file = (*(tempV.begin() + 2)).substr(fslash + 1);
 
		if ((*(tempV.begin() + 2)).front() == '/') 
			(*(tempV.begin() + 2)).erase(0,1);
		string path = (*(tempV.begin() + 2));
		path.push_back('\0');
		string savedFileName = (*(tempV.begin() + 3));
		savedFileName.push_back('\0');

		char file[BUFFER], root[BUFFER];
		strcpy(file, savedFileName.c_str());
		strcpy(root, client_root);
		strcat(root, "/");
		strcat(root, file);

		FILE *fp = fopen(root, "w+");
		fseek(fp, 0, SEEK_END);
		long fileSize = ftell(fp);
		long totalByteTransfer = 0;
		rewind(fp);
	    if (fp) {
	    	send(sock, path.c_str(), path.size(), 0);
	      	while(1) {
		      	bytesRcvd = recv(sock, in, sizeof(in), 0);
		      	//cout<< "Recieved " << bytesRcvd << " bytes" << endl;
		      	if (bytesRcvd < 0)
					//perror("recv");
					std::cout << "FAILURE:FILE RECEIVE ERROR" << '\n' << ">>> ";
		      	else if (bytesRcvd == 0)
					break;
		      	if (fwrite(in, 1, bytesRcvd, fp) != (size_t)bytesRcvd) {
		          	//perror("fwrite");
		          	std::cout << "FAILURE:FILE WRITE ERROR" << '\n' << ">>> ";
		          	break;
		        }
		        totalByteTransfer += (long)bytesRcvd;
		    }
	    	fclose(fp);
	    	cout << "SUCCESS:" << file << '\n';
	  	}
	  	else
			std::cout << "FAILURE:FILE OPENING ERROR IN RECEIVER" << '\n' << ">>> ";
	}
	close(sock);
}

void getFile(string fileInfo, int type, int sckt) {
	std::vector<string> tempV;
	string port, path, IP, alias, savedFileName, check = "", pch;
	char op[BUFFER];
	fileInfo = fileInfo.substr(fileInfo.find_first_of(' ') + 1);
	
	
	stringstream data(fileInfo);
	while (getline(data, pch, ':')) {
		tempV.push_back(pch);
	}
	savedFileName = *(tempV.rbegin());
	port = *(tempV.rbegin() + 1);
	IP = *(tempV.rbegin() + 3);
	alias = *(tempV.rbegin() + 4);
	path = *(tempV.begin() + 1);
	if (type == 1) {
		
		check = "check#@#" + alias + "#@#" + *(tempV.begin());
		check += '\0';
		send(sckt, check.c_str(), check.size(), 0);
		int len = recv(sckt, op, BUFFER, 0);
		op[len] = '\0';

		if(strcmp(op, "TRUE") == 0) {
			/*recieve file*/
			recieveFile(port + ":" + IP + ":" + path + ":" + savedFileName);
		}
		else
			std::cout << "FAILURE: CLIENT NOT IN NETWORK" << '\n' << ">>> ";
	}
	else if (type == 2) {
		recieveFile(port + ":" + IP + ":" + path + ":" + savedFileName);
	}
	
}

void sendFile(int sock, char *fileName) {
	char path[BUFFER], root[BUFFER];
	strcpy(root, client_root);
	strcat(root, "/");
	strcat(root, fileName);
	FILE *fp = fopen(root, "r");

	size_t bytesRead, readSize;
	char buff[FILE_BUFFER];

	if(fp) {

		while(!feof(fp)) {
			bytesRead = fread(buff, 1, FILE_BUFFER, fp);
			if (bytesRead <= 0) break;
			//std::cout << (int)bytesRead << " bytes sent.." << '\n';
			if (send(sock, buff, bytesRead, 0) != bytesRead) {
				perror("File send error");
				break;
			}
		}
		cout << "SUCCESS:FILE SHARED" << '\n';
		//cout << fileName << " has been successfully transferred." << '\n' << ">>> ";
	}
	else {
		std::cout << "FAILURE: FILE NOT FOUND" << '\n';
	}
}

void* sendHeartBeat(void * arg) {
	int len, sock;
	char heart[BUFFER];
	
	while(1) {
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
			perror("server socket: ");
			exit(EXIT_FAILURE);	
		}
		if ((connect(sock, (struct sockaddr*)&remote_server, sizeof(struct sockaddr_in))) == ERROR) {
			cout << "FAILURE:SERVER_OFFLINE" << '\n';
			exit(EXIT_FAILURE);
		}

		strcpy(heart, "heart");
		strcat(heart, "#@#");
		strcat(heart, "garbage");
		strcat(heart, "#@#");
		strcat(heart, client_alias);
		strcat(heart, "#@#");
		strcat(heart, client_ip);
		strcat(heart, "#@#");
		strcat(heart, client_port);
		strcat(heart, "#@#");
		strcat(heart, downloading_port);
		strcat(heart, "#@#");
		strcat(heart, client_root);
		heart[strlen(heart)] = '\0';
		send(sock, heart, strlen(heart), 0);
		len = recv(sock, heart, BUFFER, 0);
		heart[len] = '\0';
		bzero(heart, strlen(heart));
		// if(strcmp(heart, "ACK") != 0)
		// 	continue;
		close(sock);
		sleep(HTTIMER);
	}
}

void* execCommandOnMe(void * arg) {
	int len, RPCsock, RPCfd;
	char comm[BUFFER];
	if ((RPCsock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
			perror("server socket: ");
	}

	RPCserver.sin_family = AF_INET;
	RPCserver.sin_port = htons(atoi(client_port));
	RPCserver.sin_addr.s_addr = inet_addr(client_ip);
	bzero(&server.sin_zero, 8);

	if ((bind(RPCsock, (struct sockaddr*)&RPCserver, sockaddr_len)) == ERROR) {
		perror("bind : ");
		//pthread_exit();
	}
	std::cout << "RPC SERVER STARTED..." << '\n';
	if ((listen(RPCsock, 5)) == ERROR) {
		perror("listen");
		//pthread_exit();
	}
	while(1){
		if ((RPCfd = accept(RPCsock, (struct sockaddr*)&dwnClient, &sockaddr_len)) == ERROR) {
			std::cout << "Can't connect with client " << inet_ntoa(dwnClient.sin_addr) << '\n';
			//pthread_exit();
		}
	
		len = recv(RPCfd, comm, BUFFER, 0);
		char unixComm[BUFFER];
		strcpy(unixComm, "cd ");
		strcat(unixComm, client_root);
		strcat(unixComm, " && ");
		strcat(unixComm, comm);
		strcat(unixComm, " > testRPC.txt");
		std::system(unixComm);
		ifstream inFile;
		string line, comOp = "";
		strcpy(unixComm, client_root);
		strcat(unixComm, "/testRPC.txt");

		inFile.open(unixComm);
		while(inFile.good()) {
			getline(inFile,line);
			if(!line.empty()) {
				comOp += line;
				comOp += "\n";
			}
		}
		inFile.close();
		remove(unixComm);

		strcpy(unixComm, comOp.c_str());
		unixComm[strlen(unixComm)] = '\0';
		send(RPCfd, unixComm, BUFFER, 0);
		close(RPCfd);
	}
}

void execCommandOnRemote(vector<string> vec) {
	struct sockaddr_in rpcServ;
	int sock;
	char in[FILE_BUFFER];
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		perror("server socket: ");
		exit(-1);
	}

	rpcServ.sin_family = AF_INET;
	rpcServ.sin_port = htons(atoi((*(vec.begin() + 2)).c_str()));
	rpcServ.sin_addr.s_addr = inet_addr((*(vec.begin() + 1)).c_str());
	bzero(&rpcServ.sin_zero, 8);

	if ((connect(sock, (struct sockaddr*)&rpcServ, sizeof(struct sockaddr_in))) == ERROR)
		cout << "FAILURE:SERVER_OFFLINE" << '\n' << ">>> ";
	else {

		send(sock, (*(vec.end() - 1)).c_str(), (*(vec.end() - 1)).size(), 0);
		int rcvlen = recv(sock, in, BUFFER, 0);
		in[rcvlen] = '\0';
		cout << in << '\n';
		close(sock);
	}
}

int main(int argc, char const *argv[]) {
	client_alias = argv[1]; client_ip = argv[2]; client_port = argv[3]; server_ip = argv[4]; 
	server_port = argv[5]; downloading_port = argv[6]; client_root = argv[7];
	pthread_t thread;

	int len, flag = 0, i, dqcnt, nw, connFlag = 0;
	char query[BUFFER], output[BUFFER], f[BUFFER], fileName[BUFFER];
	
	string fstr;
	pid_t cpid;
	int cliSock, servSock;


	//Start server
	if ((servSock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
			perror("server socket: ");
			connFlag = 1;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(downloading_port));
	server.sin_addr.s_addr = inet_addr(client_ip);
	bzero(&server.sin_zero, 8);

	if ((bind(servSock, (struct sockaddr*)&server, sockaddr_len)) == ERROR) {
		perror("bind : ");
		connFlag = 1;
	}
	std::cout << "SERVER STARTED..." << '\n';
	if ((listen(servSock, MAX_CLIENTS)) == ERROR) {
		perror("listen");
		connFlag = 1;
	}

	if (connFlag == 0) {
		//Start client
		if ((cliSock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
			perror("server socket: ");
			connFlag = 1;
		}
		remote_server.sin_family = AF_INET;
		remote_server.sin_port = htons(atoi(server_port));
		remote_server.sin_addr.s_addr = inet_addr(server_ip);
		bzero(&remote_server.sin_zero, 8);
		if ((connect(cliSock, (struct sockaddr*)&remote_server, sizeof(struct sockaddr_in))) == ERROR) {
			cout << "FAILURE:SERVER_OFFLINE" << '\n';
			connFlag = 1;
		}
		std::cout << "CONNECTION WITH CRS ESTABLISHED.." << '\n';
	}
	


	if (connFlag == 0)
		cpid = fork();
	else {
		close(cliSock);
		close(servSock);
		exit(EXIT_FAILURE);
	}

	if (cpid == 0) {
		/* child */	
		while(1) {

			if ((nw = accept(servSock, (struct sockaddr*)&dwnClient, &sockaddr_len)) == ERROR)
				std::cout << "Can't connect with client " << inet_ntoa(dwnClient.sin_addr) << '\n';
			//cout << "New client from port " << ntohs(dwnClient.sin_port) << " and IP " << inet_ntoa(dwnClient.sin_addr) << '\n';
			//std::cout << "Data transfer started.." << '\n';
			
			int fileName_len = recv(nw, fileName, BUFFER, 0);
			cout << "filename_len" << fileName_len << fileName << endl;
			if (fileName_len > 0){
				sendFile(nw, fileName);
				close(nw);
			}
			else {
				fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
				std::cout << "FAILURE:FILENAME RECEIVE ERROR" << '\n' << ">>> ";
			}
		}

		close(servSock);
	}

	else if(cpid > 0) {
	//Parent

		pthread_create(&thread, NULL, &sendHeartBeat, NULL);
		pthread_create(&thread, NULL, &execCommandOnMe, NULL);
		sleep(1);
		while(1) {
			std::vector<string> tempV;
			char data[BUFFER];

			string::iterator it;
			dqcnt = 0;
			i = 0;
			flag = 0;

			std::cout << ">>> ";
			getline(cin,fstr);
			/*Processing commands*/
			if (fstr.compare("EXIT") == 0)
				strcpy(query, fstr.c_str());
			else if (fstr.compare("DISCONNECT") == 0){
				close(servSock);
				break;
			}
			else {

				for (it = fstr.begin(); it != fstr.end(); it++) {
					if (*it == 32 && dqcnt == 0) {
						query[i++] = '#';
						query[i++] = '@';
						query[i++] = '#';
					}
					else {
						if (*it == 34 && dqcnt == 0)
							dqcnt++;
						else if (*it == 34 && dqcnt == 1)
							dqcnt--;
						else if(*it == 92 && *(it + 1))
							query[i++] = *(it++);
						else
							query[i++] = *it;
					}

				}
				strcat(query, "#@#");
				strcat(query, client_alias);
				strcat(query, "#@#");
				strcat(query, client_ip);
				strcat(query, "#@#");
				strcat(query, client_port);
				strcat(query, "#@#");
				strcat(query, downloading_port);
				strcat(query, "#@#");
				strcat(query, client_root);
			}
			strcpy(data, query);
			char * pch = strtok (data,"#@#");
			while (pch != NULL) {
				tempV.push_back(pch);
				pch = strtok (NULL, "#@#");
			}
			string comm = *(tempV.begin());

			if (comm.compare("share") == 0) {
				string path = *(tempV.begin() + 1);
				// cout << path << '\n';
				path = "/" + path;
				path = client_root + path;

				if (access(path.c_str(), F_OK) != 0){
					flag = 1;
					std::cout << "FAILURE:FILE DOESN'T EXIST" << '\n';
				}
			}
			else if (comm.compare("get") == 0) {
				string fileInfo;
				
				if(tempV.size() == 8) {
					string slno = *(tempV.begin() + 1);
					tempV.clear();
					pch = strtok (output,"\n");
					while (pch != NULL) {
						tempV.push_back(pch);
						pch = strtok (NULL, "\n");
					}

					if ((*(tempV.begin())).find("FOUND") != string::npos) {

						for (std::vector<string>::iterator tempIt = tempV.begin(); tempIt < tempV.end(); tempIt++)
							if ((*tempIt).find(slno) != string::npos)
								fileInfo = *tempIt;
					}
					else
						std::cout << "First search the file you are looking for." << '\n';
					if (!fileInfo.empty()) {
						getFile(fileInfo + ":" + *(tempV.begin() + 2), 1, cliSock);
					}
				}
				else if (tempV.size() == 9) {
					char q2[BUFFER];
					strcpy(q2, "*");
					strcat(q2, query);

					send(cliSock, q2, strlen(q2), 0);
					int len = recv(cliSock, output, BUFFER, 0);
					output[len] = '\0';
					if(strcmp(output,"File is not shared by the client!!") == 0)
						cout << output << '\n';
					else {
						getFile(output, 2, cliSock);
					}
					if (!fileInfo.empty()) {
						getFile(fileInfo, 1, cliSock);
					}
				}
				
				else {
					std::cout << "FAILURE:INVALID GET COMMAND" << '\n';
				}
				continue;
				// pthread_create(&thread, NULL, &, (void *) &);
			}

			if (flag == 0) {
				query[strlen(query)] = '\0';
				send(cliSock, query, strlen(query), 0);
				len = recv(cliSock, output, BUFFER, 0);
				output[len] = '\0';
				bzero(query, strlen(query));
				if (comm.compare("exec") == 0) {
					if (output[0] == '#') {
						for (int i = 1; i < strlen(output); ++i)
							cout << output[i];	
					}
					else {
						tempV.clear();
						pch = strtok (output,"#@#");
						while (pch != NULL) {
							tempV.push_back(pch);
							pch = strtok (NULL, "#@#");
						}
						execCommandOnRemote(tempV);
					}
					
				}
				else
					cout << output << '\n';
				
			}
		}
		std::cout << "CONNECTION WITH CRS CLOSED.." << '\n';
		close(cliSock);
	}
	else
		std::cout << "Fork Error!!" << '\n';

	return 0;
}
