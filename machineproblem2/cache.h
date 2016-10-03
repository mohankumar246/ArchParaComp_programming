/*******************************************************
                          cache.h
                  Ahmad Samih & Yan Solihin
                           2009
                {aasamih,solihin}@ece.ncsu.edu
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/
enum States_cache_line{
	INVALID = 0,
	VALID, 
    MODIFIED,
    SHARED,
    EXCLUSIVE,
    SHARED_CLEAN,
    SHARED_MODIFIED
};


enum Cache_protocols{
	MSI= 0,
	MESI,
	DRAGON
};

enum Process_bus_signals {
    EMPTY = 0,
    PR_RD,
    PR_RD_MISS,
    PR_WR,
    PR_WR_MISS,
    BUS_RD,
    BUS_RDX,
    BUS_UPGR,
    BUS_UPD,
    UPDATE,
    FLUSH,
    FLUSH_OPT,
    COPY_EXISTS,
    NO_COPY_EXISTS
};

class cacheLine 
{
protected:
   ulong tag;
   States_cache_line Flags;   // 0:invalid, 1:valid, 2:dirty 
   ulong seq; 
 
public:
   cacheLine()            { tag = 0; Flags = INVALID; }
   ulong getTag()         { return tag; }
   States_cache_line getFlags()			{ return Flags;}
   ulong getSeq()         { return seq; }
   void setSeq(ulong Seq)			{ seq = Seq;}
   void setFlags(States_cache_line flags)			{  Flags = flags;}
   void setTag(ulong a)   { tag = a; }
   void invalidate()      { tag = 0; Flags = INVALID; }//useful function
   bool isValid()         { return ((Flags) != INVALID); }
};

class Cache
{
protected:
    ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
    Cache_protocols e_protocol;
   ulong reads,readMisses,writes,writeMisses,writeBacks;
   ulong BusRdx, flush, invalidations, interventions, mem_transactions,cache_to_cache;

   cacheLine **cache;
   ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
   ulong calcIndex(ulong addr)  { return ((addr >> log2Blk) & tagMask);}
   ulong calcAddr4Tag(ulong tag)   { return (tag << (log2Blk));}
   
public:
    ulong currentCycle;  
     
    Cache(int,int,int,Cache_protocols);
    Cache();
   ~Cache() { delete cache;}
   
   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine * findLine(ulong addr);
   cacheLine * getLRU(ulong);
   
   ulong getRM(){return readMisses;} ulong getWM(){return writeMisses;} 
   ulong getReads(){return reads;}ulong getWrites(){return writes;}
   ulong getWB(){return writeBacks;}
   ulong get_c_to_c_trf() { return cache_to_cache; }
   ulong get_mem_trf() { return mem_transactions; }
   ulong get_interventions() { return interventions; }
   ulong get_invalidations() { return invalidations; }
   ulong get_flushes() { return flush; }
   ulong get_BusRdX() { return BusRdx; }
   
   void writeBack(ulong)   {writeBacks++;}
   Process_bus_signals Access(ulong addr, Process_bus_signals input_sig, Process_bus_signals input_copy_flag);
   void printStats();
   void updateLRU(cacheLine *);
   
   Process_bus_signals msi_protocol(Process_bus_signals input ,
                       Process_bus_signals input_copy_flag, 
       ulong addr);
   Process_bus_signals mesi_protocol(Process_bus_signals input ,
                       Process_bus_signals input_copy_flag, 
       ulong addr);
   Process_bus_signals dragon_protocol(Process_bus_signals input ,
                       Process_bus_signals input_copy_flag, 
       ulong addr);

};

#endif
