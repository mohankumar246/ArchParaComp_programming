/*******************************************************
                          main.cc
                  Ahmad Samih & Yan Solihin
                           2009
                {aasamih,solihin}@ece.ncsu.edu
********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <cstring>
#include <memory.h>
using namespace std;

#include "cache.h"

int main(int argc, char *argv[])
{
	
    FILE *pFile;
	Cache *cache_temp;
	Cache **cache_mul_process;
	if(argv[1] == NULL){
		 printf("input format: ");
		 printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
		 exit(0);
        }

	/*****uncomment the next five lines*****/
	ulong cache_size = atoi(argv[1]);
	ulong cache_assoc= atoi(argv[2]);
	ulong blk_size   = atoi(argv[3]);
	ulong num_processors = atoi(argv[4]);/*1, 2, 4, 8*/
    ulong u_protocol = atoi(argv[5]);

    Cache_protocols protocol; 
	char *fname =  (char *)malloc(20);
 	fname = argv[6];
        
    if (u_protocol == 0)
        protocol = MSI;
    else if (u_protocol == 1)
        protocol = MESI;
    else
        protocol = DRAGON;
	
	printf("===== 506 Personal Information =====\n");
	printf("Mohankumar Nekkarakalaya\nUnityID 200089159\n");
	printf("ECE492 Students? NO\n");
	printf("===== 506 SMP Simulator configuration =====\n");
	printf("L1_SIZE: %ld\nL1_ASSOC: %ld\nL1_BLOCKSIZE: %ld\n",cache_size,cache_assoc,blk_size);
	printf("NUMBER OF PROCESSORS: %ld\n",num_processors);
	printf("COHERENCE PROTOCOL: ");
	if(protocol == MSI)
		printf("MSI\n");
	else if(protocol == MESI)
		printf("MESI\n");
	else printf("Dragon\n");
	
	printf("TRACE FILE: %s\n",fname);

	//*********************************************//
        //*****create an array of caches here**********//
	//*********************************************//	
    pFile = fopen(fname, "r");
    if (pFile)
    {
        cache_mul_process = new Cache*[num_processors];

        for (ulong i_count = 0; i_count < num_processors; i_count++)
        {
            cache_mul_process[i_count] = new Cache(cache_size, cache_assoc, blk_size, protocol);
        }
        ///******************************************************************//
        //**read trace file,line by line,each(processor#,operation,address)**//
        //*****propagate each request down through memory hierarchy**********//
        //*****by calling cachesArray[processor#]->Access(...)***************//
        ///******************************************************************//
        {
            char ac_line[25], ac_address[10];                
            char c_proc_signal;
            ulong i_address, i_processor = 0;
            Process_bus_signals input_proc_sig,input_copy_flag,bus_out_sig, bus_in_response;
#if DEBUG_CACHE
            printf("#count cache_l R-1/W-2 \t address \tsetnum \tblocktag \tdirty_bit  evicted block evict_dirty_b\n");
#endif

            while (fgets(ac_line,20,pFile))
            {
                
                i_address = -1;
                input_proc_sig = EMPTY, input_copy_flag = EMPTY, bus_out_sig = EMPTY;
                bus_in_response = EMPTY;
                i_processor = atoi(&ac_line[0]);
                c_proc_signal = ac_line[2];
                memcpy(ac_address, &ac_line[4], 8);
                ac_address[8] = '\0';
                {
                    /*Call the generic cache core to perform the opns recursively*/
                    if (c_proc_signal == 'r')
                        input_proc_sig = PR_RD;
                    else
                        input_proc_sig = PR_WR;

                    i_address = (ulong)strtoul(ac_address,NULL,16);
                    bus_out_sig = cache_mul_process[i_processor]->Access(i_address, input_proc_sig, input_copy_flag);
                }

                if (bus_out_sig != EMPTY)
                {
                    input_copy_flag = NO_COPY_EXISTS;
                    for (ulong i_count = 0; i_count < num_processors; i_count++)
                    {
                        if (i_processor != i_count)
                        {
                            bus_in_response = cache_mul_process[i_count]->Access(i_address, bus_out_sig, input_copy_flag);
                            if (bus_in_response == COPY_EXISTS)
                            {
                                input_copy_flag = COPY_EXISTS;
                            }
                        }
                    }
                    bus_out_sig = EMPTY;
                    bus_out_sig = cache_mul_process[i_processor]->Access(i_address, input_proc_sig, input_copy_flag);
                }
                
                input_copy_flag = EMPTY;

                if (bus_out_sig != EMPTY)
                {
                    for (ulong i_count = 0; i_count < num_processors; i_count++)
                    {
                        if (i_processor != i_count)
                            bus_in_response = cache_mul_process[i_count]->Access(i_address, bus_out_sig, input_copy_flag);   
                    }
                }
           }
        }

        //********************************//
        //print out all caches' statistics //
        //********************************//
        for (ulong i_count = 0; i_count < num_processors; i_count++)
        {
            cache_temp = cache_mul_process[i_count];
            int mem_trans_temp;
            int writes = cache_temp->getWrites(), reads= cache_temp->getReads(), RM = cache_temp->getRM(), WM = cache_temp->getWM();
            int WB = cache_temp->getWB();
            float MR = 0.0f;
            
            mem_trans_temp = WM + RM + WB;

            if ((reads + writes) != 0)
                MR = (((float)(RM + WM) * 100) / ((float)(reads + writes)));

            if (protocol == MSI)
            {
                WB += cache_temp->get_flushes();
                mem_trans_temp = WM + RM + WB + cache_temp->get_mem_trf();
                
            }
            else if (protocol == MESI)
            {
               WB += cache_temp->get_flushes();
               mem_trans_temp = WM + RM + WB - cache_temp->get_c_to_c_trf();
            }


            printf("============ Simulation results (Cache %ld) ============\n", i_count);
            printf("%-45s %10d\n","01. number of reads:", reads);
            printf("%-45s %10d\n","02. number of read misses:", RM);
            printf("%-45s %10d\n","03. number of writes:", writes);
            printf("%-45s %10d\n","04. number of write misses:", WM);
            printf("%-45s %9.2f%%\n","05. total miss rate:", MR);
            printf("%-45s %10d\n","06. number of writebacks:", WB);
            printf("%-45s %10ld\n","07. number of cache-to-cache transfers:",cache_temp->get_c_to_c_trf());
            printf("%-45s %10d\n","08. number of memory transactions:", mem_trans_temp);
            printf("%-45s %10ld\n","09. number of interventions:",cache_temp->get_interventions());
            printf("%-45s %10ld\n","10. number of invalidations:",cache_temp->get_invalidations());
            printf("%-45s %10ld\n","11. number of flushes:",cache_temp->get_flushes());
            printf("%-45s %10ld\n","12. number of BusRdX:",cache_temp->get_BusRdX());
        }

        for (ulong i_count = 0; i_count < num_processors; i_count++)
        {
            delete cache_mul_process[i_count];
        }
        delete[] cache_mul_process;
    }
    else
    {
        printf("Trace file problem\n");
        exit(0);
    }
    fclose(pFile);
}
