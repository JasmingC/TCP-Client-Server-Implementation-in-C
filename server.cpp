#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <random>

#define D_RTT				20
#define D_THRESHOLD			65536
#define D_MSS				1024
#define D_BUFFER_SIZE			524288


using namespace std;
pid_t pid;

typedef enum {
	TcpState_None,					
	TcpState_SlowStart,				
	TcpState_CongestionAvoid,		
	TcpState_FastRecover			
} TcpState;
	
typedef enum {
	PacketCmd_Data,				
	PacketCmd_ACK,				
	PacketCmd_SYN,				
	PacketCmd_SYNACK,			
	PacketCmd_FIN				
} PacketCmd;

class Server;
class Packet;


class Packet 
{
		public:

		short int srcPort;      			
		short int destPort;				
		
	
		int seqNum;       				
		int ackNum;       					
		short head_len:4,not_use:6;	
	
		bool flagURG;
		bool flagACK;						
		bool flagPSH;
		bool flagRST;
		bool flagSYN;					
		bool flagFIN;						
				
	
		short int rwnd;            		
		short int checksum;					
		short urg_ptr;
		int datasize;
		char data[D_MSS];   		
	
		Packet(){
			srcPort = 0;
			destPort = 0;
			seqNum = 0;
			ackNum = 0;
			flagACK = false;
			flagSYN = false;
			flagFIN = false;
			rwnd = 0;
			checksum = 0;
			memset(data, 0, sizeof(data));
		}
		Packet(PacketCmd command, Server server);
		PacketCmd parser();
		string getName();	
};

class Server {
	public:
		
		int MSS;
		int RTT;
		int THRESHOLD;
		int BufferSize;
		
	
		int fd;				
		struct sockaddr_in srcSocket;			
		struct sockaddr_in destSocket;	
		struct sockaddr_in tempSocket;
		
		int seqNum;
		int ackNum;
		int rwnd;
		int cwnd;					
		char fileBuffer[D_BUFFER_SIZE];		
		bool delay;					
		int duplicateACKcount;				
		TcpState state;
		string name;					//request file name
		
		Server(){
			MSS = D_MSS;
			RTT = D_RTT;
			THRESHOLD = D_THRESHOLD;
			BufferSize=D_BUFFER_SIZE;
			memset(fileBuffer,0,BufferSize);
		}
		
		void createSocket(const char *srcIP, int srcPort);
		void printInfo();
		void threeWayhandshake(int tmp_p);
		void send(Packet packet,bool printornot=true);
		Packet read(bool printornot=true);
		void updateNumber(const Packet packet);
		
		void reset();
		void transfer();
		bool setTimeout(int readFD,int msec);
};
Packet::Packet(PacketCmd command, Server server){
	flagACK = false;
	flagSYN = false;
	flagFIN = false;
	checksum = 0;
	memset(data, 0, 1024);
	srcPort = server.srcSocket.sin_port;
	destPort = server.destSocket.sin_port;
	seqNum = server.seqNum;
	ackNum = server.ackNum;
	rwnd = server.rwnd;
	switch(command){
		case PacketCmd_ACK:				
			flagACK = true;
			checksum = 1;
			break;
		case PacketCmd_SYN:				
			flagSYN = true;
			checksum = 1;
			break;
		case PacketCmd_SYNACK:			
			flagACK = true;
			flagSYN = true;
			checksum = 1;
			break;
		case PacketCmd_FIN:
			flagFIN = true;
			checksum = 1;
			break;
		case PacketCmd_Data:{				
			break;
		}
	}
}
PacketCmd Packet::parser(){
	if(flagACK && flagSYN) return PacketCmd_SYNACK;
	else if(flagACK) return PacketCmd_ACK;
	else if(flagSYN) return PacketCmd_SYN;
	else if(flagFIN) return PacketCmd_FIN;
	else return PacketCmd_Data;
}
string Packet::getName(){
	switch(this->parser()){
		case PacketCmd_Data:return "packet";			
		case PacketCmd_ACK:return "packet(ACK)";			
		case PacketCmd_SYN:return "packet(SYN)";			
		case PacketCmd_SYNACK:return "packet(SYNACK)";		
		case PacketCmd_FIN:return "packet(FIN)";
	}
	return "";
}
void Server::createSocket(const char *srcIP, int srcPort){
	fd = socket(AF_INET,SOCK_DGRAM,0);
	if (fd < 0) {perror("socket error\n");}
	memset((char*) &srcSocket, 0, sizeof(srcSocket) );
	srcSocket.sin_family = AF_INET;
	srcSocket.sin_port = srcPort; 
	srcSocket.sin_addr.s_addr = inet_addr(srcIP); 
	
	//inet_addr()的功能是將一個點分十進制的IP轉換成一個長整數型數（u_long類型）
	if (bind(fd, (struct sockaddr *)&srcSocket, sizeof(srcSocket)) < 0){perror("bind error\n");} 
}
void Server::printInfo(){
	cout<<"==========\n";
	cout<<"Set RTT delay = "<<RTT<<" ms\n";
	cout<<"Set threshold = "<<THRESHOLD<<" bytes\n";
	cout<<"Set MSS = "<<MSS<<" bytes\n";
	cout<<"Buffer size = "<<BufferSize<<" bytes\n";
	cout<<"Server's ip is "<<inet_ntoa(srcSocket.sin_addr)<<endl;
	cout<<"Server is listening on port "<<srcSocket.sin_port<<endl;
	cout<<"==========\n";
	cout<<"listening......\n";
}
Packet Server::read(bool printornot){
	Packet pkg;
	socklen_t pkg_l = sizeof(destSocket);
	usleep((this->RTT>>1)*1000);
	recvfrom(fd, &pkg, sizeof(Packet), 0, (struct sockaddr*)&destSocket, &pkg_l);
	
	switch(pkg.parser()){
		case PacketCmd_ACK:
		case PacketCmd_SYNACK:
		case PacketCmd_FIN:
		{
			if(printornot)
			cout<<"Received a " << pkg.getName() << " from " << inet_ntoa(destSocket.sin_addr)<<":"<<destSocket.sin_port<<endl;
		}
		case PacketCmd_Data:{
			if(pkg.checksum==0)cout<<"Packet loss!! ACK = "<<pkg.ackNum<<endl;
			else cout<<"\t\t" << "Receive a packet (" << "seq_num = " << pkg.seqNum << ", " << "ack_num = " << pkg.ackNum << ")"<<endl;
			break;
		}
	}
	return pkg;
}
void Server::updateNumber(const Packet packet){
	if(seqNum>0)
		seqNum=packet.ackNum;
	else
		seqNum=rand()%10001;
	ackNum = packet.seqNum + packet.checksum;
}

void Server::send(Packet packet,bool printornot){
	sendto(fd,&packet,sizeof(Packet),0,(struct sockaddr*)&destSocket, sizeof(destSocket));
	switch(packet.parser()){
		case PacketCmd_SYN:
		case PacketCmd_ACK:
		case PacketCmd_SYNACK:
		case PacketCmd_FIN:
		{
			if(printornot)
			cout<<"Send a " << packet.getName() << " to " << inet_ntoa(destSocket.sin_addr)<<":"<<destSocket.sin_port<<endl;
			break;
		}
	}
	
}
void Server::threeWayhandshake(int tmp_p){
	Packet recv=read();
	struct sockaddr_in ori=srcSocket;
	int ori_fd=fd;
	int sd = socket(AF_INET,SOCK_DGRAM,0);
	if (sd < 0) {perror("socket error\n");}
	memset((char*) &tempSocket, 0, sizeof(tempSocket) );
	tempSocket.sin_family = AF_INET;
	tempSocket.sin_port = tmp_p; 
	tempSocket.sin_addr.s_addr = inet_addr("192.168.0.1"); 
	if (bind(sd, (struct sockaddr *)&tempSocket, sizeof(tempSocket)) < 0){perror("bind error\n");} 
	int reuseAddr = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr) );
	bool end=false;
	pid=fork();
	if(pid==0){
		cout<<"=====start the three-way handshake=====\n";
		srcSocket=tempSocket;
		fd=sd;
		cout<<"Server's ip is "<<inet_ntoa(srcSocket.sin_addr)<<endl;
		cout<<"Server is listening on port "<<srcSocket.sin_port<<endl;
		while(!end){		
			switch(recv.parser()){
				case PacketCmd_SYN:{
					updateNumber(recv);
					send(Packet(PacketCmd::PacketCmd_SYNACK,*this));
					break;
				}
				case PacketCmd_ACK:{
					updateNumber(recv);
					name=recv.data;			//request file name
					end = true;
					break;
				}
			}
			if(!end)recv=read();
		}
		cout<<"=====complete the three-way handshake=====\n";
		transfer();
	}
	srcSocket=ori;
	fd=ori_fd;
}
void Server::transfer(){
	long cBytes;
	FILE *input;
	if((input=fopen(name.c_str(),"r"))==NULL){cout<<"open error\n";return;}


	int wanttoloss=4096;

	//initialize
	state=TcpState::TcpState_SlowStart;
	int base=1,len,len_cnt=0;
	bool end=false,dup=false,ploss=true,dont=false;
	Packet recv,prev;
	cwnd=1;
	seqNum=base;
	rwnd=BufferSize;
	duplicateACKcount=0;

	cout<<"======slow start=========\n";
	while(1){
		cout<<"cwnd =  "<< cwnd << ", " <<"rwnd = "<< rwnd << ", "<<"threshold = " << THRESHOLD<<endl;
		if(state==TcpState::TcpState_SlowStart){							//slow start
			for(int j=0;j<cwnd;j++){
				Packet t=Packet(PacketCmd::PacketCmd_Data,*this);
				len=fread(t.data,sizeof(char),MSS,input);
				t.checksum=len;
				if(len_cnt==wanttoloss && ploss)ploss=false; 					//generate loss
				else {
					send(t);
					cout<<"\t\tSend a packet at : "<<len<<" byte\n";
				}
				base+=len;
				rwnd-=len;
				prev=recv;
				bool isTimeout=setTimeout(fd,500000);						//listen for 500ms
				if(isTimeout){									//stay slow start
					dont=true;
					THRESHOLD=cwnd*MSS/2;
					cwnd=1;
					duplicateACKcount=0;
					seqNum=base;
					ackNum++;
					if(len<MSS){end=true;break;}
					len_cnt+=len;
					cout<<"timeout!!\n";
					cout<<"========slow start======\n";
					break;

				}
				else{
					recv=read(false);
					if(prev.ackNum==recv.ackNum){
						duplicateACKcount++;
						//cout<<"dup++\n";
					}
					if(duplicateACKcount>=3){						//enter fast recovery
						dont=true;
						dup=true;
						end=false;
						duplicateACKcount=0;
						THRESHOLD=cwnd*MSS/2;
						if(THRESHOLD==0)THRESHOLD=1;
						cwnd=THRESHOLD/MSS+3;
						if(cwnd==0)cwnd=1;
						rwnd=BufferSize;
						if(len==1024){
							base-=(len*4);
							fseek(input,-4*len,SEEK_CUR);
							seqNum=base;
							ackNum-=4;
							len_cnt-=(len*4);
						}
						else{
							base-=(len+1024*3);
							fseek(input,-1*(len+3*1024),SEEK_CUR);
							seqNum=base;
							ackNum-=4;
							len_cnt-=(len+1024*3);
						}
						cout<<"Receive 3 duplicate ack!!\n";
						cout<<"======fast retransmit=====\n";
						cout<<"======fast recovery=====\n";
						state=TcpState::TcpState_FastRecover;
						break;
					}
					else{
						seqNum=base;
						ackNum++;
						if(len<MSS){end=true;break;}
						len_cnt+=len;
					}
				}
			}
			if(!end && !dont)cwnd*=2;
			if(cwnd*MSS>=THRESHOLD  && !end && !dont){
				cout<<"=====congetstion avoidance======\n";
				state=TcpState::TcpState_CongestionAvoid;
			}
		}
		else if(state==TcpState::TcpState_CongestionAvoid){						//congestion avoidance
			for(int j=0;j<cwnd;j++){
				Packet t=Packet(PacketCmd::PacketCmd_Data,*this);
				len=fread(t.data,sizeof(char),MSS,input);
				t.checksum=len;
				if(len_cnt==wanttoloss && ploss)ploss=false;
				else {
					send(t);
					cout<<"\t\tSend a packet at : "<<len<<" byte\n";
				}
				base+=len;
				rwnd-=len;
				prev=recv;
				bool isTimeout=setTimeout(fd,500000);						//listen for 500ms
				if(isTimeout){									//timeout!! go to slow start
					dont=true;
					THRESHOLD=cwnd*MSS/2;
					cwnd=1;
					duplicateACKcount=0;
					seqNum=base;
					ackNum++;
					if(len<MSS){end=true;break;}
					len_cnt+=len;
					cout<<"timeout!!\n";
					cout<<"========slow start======\n";

				}
				else{
					recv=read(false);
					if(prev.ackNum==recv.ackNum){
						duplicateACKcount++;
						//cout<<"dup++\n";
					}
					if(duplicateACKcount>=3){						//go to fast recovery
						dont=true;
						dup=true;
						end=false;
						duplicateACKcount=0;
						THRESHOLD=cwnd*MSS/2;
						if(THRESHOLD==0)THRESHOLD=1;
						cwnd=THRESHOLD/MSS+3;
						if(cwnd==0)cwnd=1;
						rwnd=BufferSize;
						if(len==1024){
							base-=(len*4);
							fseek(input,-4*len,SEEK_CUR);
							seqNum=base;
							ackNum-=4;
							len_cnt-=(len*4);
						}
						else{
							base-=(len+1024*3);
							fseek(input,-1*(len+3*1024),SEEK_CUR);
							seqNum=base;
							ackNum-=4;
							len_cnt-=(len+1024*3);
						}
						cout<<"Receive 3 duplicate ack!!\n";
						cout<<"======fast retransmit=====\n";
						cout<<"======fast recovery=====\n";
						state=TcpState::TcpState_FastRecover;
						break;
					}
					else{
						seqNum=base;
						ackNum++;
						if(len<MSS){end=true;break;}
						len_cnt+=len;
					}
				}
			}
			if(!end && !dont)cwnd++;
		}
		else if(state==TcpState::TcpState_FastRecover){												//fast recovery
			for(int j=0;j<cwnd;j++){
				Packet t=Packet(PacketCmd::PacketCmd_Data,*this);
				len=fread(t.data,sizeof(char),MSS,input);
				t.checksum=len;
				if(len_cnt==wanttoloss && ploss)ploss=false; 					//generate loss
				else {
					send(t);
					cout<<"\t\tSend a packet at : "<<len<<" byte\n";
				}
				base+=len;
				rwnd-=len;
				prev=recv;
				bool isTimeout=setTimeout(fd,500000);						//listen for 500ms
				if(isTimeout){									//timeout!! to slow start
					dont=true;
					THRESHOLD=cwnd*MSS/2;
					cwnd=1;
					duplicateACKcount=0;
					seqNum=base;
					ackNum++;
					if(len<MSS){end=true;break;}
					len_cnt+=len;
					cout<<"timeout!!\n";
					cout<<"======slow start=====\n";
					state=TcpState::TcpState_SlowStart;
					break;
					//continue;

				}
				else{
					recv=read(false);
					if(prev.ackNum==recv.ackNum){}
					else{									//new ack go to congestion avoid
						cwnd=THRESHOLD/MSS;
						duplicateACKcount=0;
						dont=true;
						seqNum=base;
						ackNum++;
						state=TcpState::TcpState_CongestionAvoid;
						cout<<"=====congestion avoidance======\n";
						break;
					}			
						seqNum=base;
						ackNum++;
						if(len<MSS){end=true;break;}
						len_cnt+=len;
					
				}
				
			}
			if(!end && !dont)cwnd*=2;
		}
		
		dont=false;
		if(end && !dup)break;
		dup=false;
	}
	Packet t=Packet(PacketCmd::PacketCmd_ACK, *this );
	send(t);
	fclose(input);
}
void Server::reset(){
	cwnd = 1;
	rwnd = BufferSize;
	THRESHOLD = D_THRESHOLD;
	duplicateACKcount = 0;
	state = TcpState::TcpState_None;
}
bool Server::setTimeout(int readFD, int msec)
{
	fd_set fdReadSet;
	struct timeval timer;	
	bool isTimeout = false;
	FD_ZERO(&fdReadSet);
	FD_SET(readFD, &fdReadSet);
	timer.tv_sec = 0;
	timer.tv_usec = msec;
	const int MaxFd = readFD + 1;
	switch(select(MaxFd, &fdReadSet, NULL, NULL, &timer) ) 
	{
		case -1:{perror("select");	break;	}
		case 0:{isTimeout = true;	break;	}
		default:{isTimeout = false;	break;	}
	}
	return isTimeout;
}

int main(int argc, char *argv[])
{

	Server server;
	string serverIP = "192.168.0.1";
	int serverPort;
	serverPort=atoi(argv[1]);

	server.createSocket(serverIP.c_str(),serverPort);
	int tmp_p=server.srcSocket.sin_port;

	while(1)
	{
		server.printInfo();
		tmp_p++;
		server.threeWayhandshake(tmp_p);

		server.reset();

		
	}
	return 0;
}








