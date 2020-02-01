/*******************************************************
                          main.cc
********************************************************/

#include "cache.h"
#include <string>
using namespace std;

int main(int argc, char *argv[])
{	
	ifstream fin;
	
	ulong address;
	
	string fileName;
	char mystr[40];	
	char *tf;
	tf = &mystr[0];
	ifstream traceFile;

	/*****uncomment the next five lines*****/
	int cache_size = atoi(argv[1]);
	int cache_assoc= atoi(argv[2]);
	int blk_size   = atoi(argv[3]);
	int num_processors = atoi(argv[4]);/*1, 2, 4, 8*/
	int protocol   = atoi(argv[5]);	 /*0:MSI, 1:MESI, 2:Dragon*/
 	fileName = argv[6];
	
	// Personal Information 
	printf("===== 506 Personal information =====\n");
        printf("Ayaan Mohammed\n");
        printf("amohamm4\n");
        printf("ECE492 Students? NO\n");
	
	/** Print Simulation Parameters and Cache Specifications */
	printf("===== 506 SMP Simulator configuration =====\n");
        printf("L1_SIZE: %d\n", cache_size);
        printf("L1_ASSOC: %d\n", cache_assoc);
        printf("L1_BLOCKSIZE: %d\n", blk_size);
        printf("NUMBER OF PROCESSORS: %d\n", num_processors);
        
        switch(protocol)
        {
            case 0: printf("COHERENCE PROTOCOL: MSI\n");
                    break;
                    
            case 1: printf("COHERENCE PROTOCOL: MESI\n");
                    break;
                    
            case 2: printf("COHERENCE PROTOCOL: Dragon\n");
                    break;
                    
            default:printf("Wrong Protocol Value");
        }
	
        printf("TRACE FILE: %s\n", fileName.c_str());



	Cache **cachesArray = new Cache *[num_processors];

	for(int i=0;i<num_processors;i++)
	{
		cachesArray[i] = new Cache(cache_size, cache_assoc, blk_size, num_processors, cachesArray);
	}


	strncpy(mystr, fileName.c_str(), fileName.length()); //read tracefile
	mystr[fileName.length()]=0;
	traceFile.open(tf);
	
	int proc = 0;
	int count = 0;
	string sub2;

	
	//Read processor number, r/w address from trace file
	while(traceFile>>proc>>sub2>>hex>>address)
	{
	
	switch(protocol)
	{
	count++;
		case 0:
			cachesArray[proc]->AccessMSI(proc, (ulong) address, sub2.c_str());
			break;

		case 1:
			cachesArray[proc]->AccessMESI(proc, (ulong) address, sub2.c_str());
			break;

		case 2:
			cachesArray[proc]->AccessDragon(proc, (ulong) address, sub2.c_str()); 	
			break;
				
	} 

	
	}
	traceFile.close();

	
	for(int i=0;i<num_processors;i++)
	{	
		cout << "============ Simulation results (Cache " << i << ") ============" <<  endl;
		cachesArray[i]->printStats(protocol);
	}

}
