/*******************************************************
                          cache.cc
                  Ahmad Samih & Yan Solihin
                           2009
                {aasamih,solihin}@ece.ncsu.edu
********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s,int a,int b, Cache_protocols protocol)
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;
   BusRdx = flush = invalidations = interventions = mem_transactions = cache_to_cache = 0;

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));   
   e_protocol   = protocol;

   //*******************//
   //initialize your counters here//
   //*******************//
 
   tagMask =0;
   for(i=0;i<log2Sets;i++)
   {
		tagMask <<= 1;
        tagMask |= 1;
   }
   
   /**create a two dimentional cache, sized as cache[sets][assoc]**/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
	   cache[i][j].invalidate();
      }
   }      
   
}

/**you might add other parameters to Access()
since this function is an entry point 
to the memory hierarchy (i.e. caches)**/
Process_bus_signals Cache::Access(ulong addr,Process_bus_signals input_sig, Process_bus_signals input_copy_flag)
{
    Process_bus_signals Out_signal = EMPTY;
  
    if (e_protocol == MESI)
        Out_signal = mesi_protocol(input_sig, input_copy_flag, addr);

    if (e_protocol == MSI)
        Out_signal = msi_protocol(input_sig, input_copy_flag, addr);

    if (e_protocol == DRAGON)
        Out_signal = dragon_protocol(input_sig, input_copy_flag, addr);

    return Out_signal;
}

/*look up line*/
cacheLine* Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
	if(cache[i][j].isValid())
	        if(cache[i][j].getTag() == tag)
		{
		     pos = j; break; 
		}
   if(pos == assoc)
	return NULL;
   else
	return &(cache[i][pos]); 
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
  line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) return &(cache[i][j]);     
   }   
   for(j=0;j<assoc;j++)
   {
	 if(cache[i][j].getSeq() <= min) { victim = j; min = cache[i][j].getSeq();}
   } 
   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   
   if((victim->getFlags() == MODIFIED) || (victim->getFlags() == SHARED_MODIFIED))
       writeBack(addr);
    	
   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(VALID);    
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

void Cache::printStats()
{ 
	printf("===== Simulation results      =====\n");
	/****print out the rest of statistics here.****/
	/****follow the ouput file format**************/
}

Process_bus_signals Cache::msi_protocol(Process_bus_signals input_sig, Process_bus_signals input_copy_flag, ulong addr)
{
    States_cache_line Current_state;
    Process_bus_signals Out_signal = EMPTY;
    cacheLine * line;

    if (input_sig < BUS_RD)
    {
        if ((input_copy_flag == NO_COPY_EXISTS) || (input_copy_flag == COPY_EXISTS))
            return Out_signal;

        currentCycle++;/*per cache global counter to maintain LRU order
                           among cache ways, updated on every cache access*/        
        if (input_sig == PR_WR) 
            writes++;
        else 
            reads++;
        
        line = findLine(addr);

        if (line == NULL)/*miss*/
        {
            line = fillLine(addr);
            line->setFlags(INVALID);
            if (input_sig == PR_WR)
            {                
                writeMisses++;
            }
            if (input_sig == PR_RD)
            {
                readMisses++;
            }
        }
        else
        {
            /**since it's a hit, update LRU and update dirty flag**/
            updateLRU(line);
        }
        Current_state = line->getFlags();

        /*Procssor requests*/
        switch (Current_state)
        {
        case MODIFIED:
            break;
        case SHARED:
            if (input_sig == PR_WR)
            {
                Out_signal = BUS_RDX;
                mem_transactions++;
                BusRdx++;
                line->setFlags(MODIFIED);
            }
            break;
        case INVALID:
            if (input_sig == PR_WR)
            {
                Out_signal = BUS_RDX;
                BusRdx++;
                line->setFlags(MODIFIED);
            }
            if (input_sig == PR_RD)
            {
                Out_signal = BUS_RD;
                line->setFlags(SHARED);
            }
            break;
        default:
            break;
        }
    }
    else
    {
        line = findLine(addr);

        if (line == NULL)
            Current_state = INVALID;
        else
            Current_state = line->getFlags();

        /*Bus requests*/
        switch (Current_state)
        {
        case MODIFIED:
            if (input_sig == BUS_RDX || input_sig == BUS_RD)
            {
                flush++;
            }
            if (input_sig == BUS_RDX)
            {
                invalidations++;
                line->setFlags(INVALID);
            }
            if (input_sig == BUS_RD)
            {
                interventions++;
                line->setFlags(SHARED);
            }
            break;
        case SHARED:
            if (input_sig == BUS_RDX)
            {
                invalidations++;
                line->setFlags(INVALID);
            }
            break;
        case INVALID:
            break;
        default:
            break;
        }
    }   
    return Out_signal;
}

Process_bus_signals Cache::mesi_protocol(Process_bus_signals input_sig, Process_bus_signals input_copy_flag, ulong addr)
{
    States_cache_line Current_state;
    Process_bus_signals Out_signal = EMPTY;
    cacheLine * line; 
    cacheLine line_temp;

    if (input_sig < BUS_RD)
    { 
        currentCycle++;/*per cache global counter to maintain LRU order
                       among cache ways, updated on every cache access*/

        if (input_sig == PR_WR)
            writes++;
        else
            reads++;

        line = findLine(addr);

        if (line == NULL)/*miss*/
        {
            if (input_sig == PR_WR)
            {
                if (input_copy_flag != EMPTY)
                {
                    line = fillLine(addr);
                    line->setFlags(INVALID);
                    writeMisses++;
                }
                else
                {
                    line_temp.setFlags(INVALID);
                    line = &line_temp;
                }
            }
            if (input_sig == PR_RD )
            {
                if (input_copy_flag != EMPTY)
                {
                    line = fillLine(addr);
                    line->setFlags(INVALID);
                    readMisses++;
                }
                else
                {
                    line_temp.setFlags(INVALID);
                    line = &line_temp;
                }
            }
        }
        else
        {
            /**since it's a hit, update LRU and update dirty flag**/
            updateLRU(line);
        }
        
        Current_state = line->getFlags();
        /*Procssor requests*/
        switch (Current_state)
        {
            case MODIFIED:
                break;
            case SHARED:
                if (input_sig == PR_WR)
                {
                    Out_signal = BUS_UPGR;
                    writes--;
//                    BusRdx++;
                    line->setFlags(MODIFIED);
                }                
                break;
            case INVALID:
                if (input_sig == PR_WR)
                {
                    if (input_copy_flag == EMPTY)
                    {
                        Out_signal = BUS_RDX;
                        BusRdx++;
                    }

                    if (input_copy_flag == COPY_EXISTS)
                    {
                        cache_to_cache++;
                        line->setFlags(MODIFIED);
                        writes--;
                        currentCycle--;
                    }

                    if (input_copy_flag == NO_COPY_EXISTS)
                    {
                        line->setFlags(MODIFIED);
                        writes--;
                        currentCycle--;
                    }
                }
           
                if (input_sig == PR_RD)
                {
                    if (input_copy_flag == EMPTY)
                        Out_signal = BUS_RD;

                    if (input_copy_flag == COPY_EXISTS)
                    {
                        cache_to_cache++;
                        line->setFlags(SHARED);
                        reads--;
                        currentCycle--;
                    }
                    else if (input_copy_flag == NO_COPY_EXISTS)
                    {
                        line->setFlags(EXCLUSIVE);
                        reads--;
                        currentCycle--;
                    }
                }
                break;
            case EXCLUSIVE:
                if (input_sig == PR_WR)
                    line->setFlags(MODIFIED);
                
            default:
                break;
        }
    }
    else
    {
        line = findLine(addr);

        if (line == NULL)
            Current_state = INVALID;
        else
            Current_state = line->getFlags();

        if (input_sig == BUS_RD  || input_sig == BUS_RDX)
        {
            if (line == NULL)
                Out_signal = NO_COPY_EXISTS;
            else
                Out_signal = COPY_EXISTS;
        }
        /*Bus requests*/
        switch (Current_state)
        {
            case MODIFIED:
                if (input_sig == BUS_RDX)
                {
                    flush++;
                    line->setFlags(INVALID);
                    invalidations++;
                }
                if (input_sig == BUS_RD)
                {
                    flush++;
                    line->setFlags(SHARED);
                    interventions++;
                }
                break;

            case SHARED:
                if (input_sig == BUS_UPGR)
                {
                    line->setFlags(INVALID);
                    invalidations++;
                }

                if (input_sig == BUS_RDX)
                {
                    line->setFlags(INVALID);
                    invalidations++;
                }
                break;

            case INVALID:
                break;

            case EXCLUSIVE:
                if (input_sig == BUS_RD)
                {
                    line->setFlags(SHARED);
                    interventions++;
                }
                if (input_sig == BUS_RDX)
                {
                    line->setFlags(INVALID);
                    invalidations++;
                }
                break;
            
            default:
                break;
        }
    }

    return Out_signal;
}
Process_bus_signals Cache::dragon_protocol(Process_bus_signals input_sig, Process_bus_signals input_copy_flag, ulong addr)
{
    States_cache_line Current_state;
    Process_bus_signals Out_signal = EMPTY;
    cacheLine * line = findLine(addr);
    
    if (input_sig < BUS_RD)
    {    
        if (line == NULL)/*miss*/
        {
            if (input_copy_flag == EMPTY)
                Out_signal = BUS_RD;

            if (input_copy_flag == COPY_EXISTS || input_copy_flag == NO_COPY_EXISTS)
                currentCycle++;/*per cache global counter to maintain LRU order
                           among cache ways, updated on every cache access*/

            if (input_sig == PR_WR)
            {
                if (input_copy_flag == COPY_EXISTS)
                {
                    writes++;
                    writeMisses++;
                    Out_signal = BUS_UPD;
                    line = fillLine(addr);
                    line->setFlags(SHARED_MODIFIED);
                }

                if (input_copy_flag == NO_COPY_EXISTS)
                {
                    writes++;
                    writeMisses++;
                    line = fillLine(addr);
                    line->setFlags(MODIFIED);
                }
            }

            if (input_sig == PR_RD)
            {
                if (input_copy_flag == COPY_EXISTS)
                {
                    reads++;
                    readMisses++;
                    line = fillLine(addr);
                    line->setFlags(SHARED_CLEAN);
                }

                if (input_copy_flag == NO_COPY_EXISTS)
                {
                    reads++;
                    readMisses++;
                    line = fillLine(addr);
                    line->setFlags(EXCLUSIVE);
                }
            }
        }
        else
        {

            if (input_copy_flag == EMPTY)
            {
                currentCycle++;/*per cache global counter to maintain LRU order
                               among cache ways, updated on every cache access*/

                if (input_sig == PR_WR)
                    writes++;
                if (input_sig == PR_RD)
                    reads++;
                /**since it's a hit, update LRU and update dirty flag**/
                updateLRU(line);
            }

            Current_state = line->getFlags();

            /*Procssor requests*/
            switch (Current_state)
            {
                case MODIFIED:
                    break;

                case SHARED_CLEAN:
                    if (input_sig == PR_WR)
                    {
                        if (input_copy_flag == EMPTY)
                            Out_signal = BUS_UPD;
                        if (input_copy_flag == COPY_EXISTS)
                            line->setFlags(SHARED_MODIFIED);
                        if (input_copy_flag == NO_COPY_EXISTS)
                            line->setFlags(MODIFIED);
                    }
                    break;

                case SHARED_MODIFIED:
                    if (input_sig == PR_WR)
                    {
                        if (input_copy_flag == EMPTY)
                            Out_signal = BUS_UPD;
                        if (input_copy_flag == NO_COPY_EXISTS)
                            line->setFlags(MODIFIED);
                    }
                    break;

                case EXCLUSIVE:
                    if (input_sig == PR_WR)
                        line->setFlags(MODIFIED);

                default:
                    break;
            }
        }
    }
    else
    {      
        if (line == NULL)
            Current_state = INVALID;
        else
            Current_state = line->getFlags();

        if (input_sig == BUS_RD || input_sig == BUS_UPD)
        {
            if (line == NULL)
                Out_signal = NO_COPY_EXISTS;
            else
                Out_signal = COPY_EXISTS;
        }

        /*Bus requests*/
        switch (Current_state)
        {
            case MODIFIED:
                if (input_sig == BUS_RD)
                {
                    flush++;
                    interventions++;
                    line->setFlags(SHARED_MODIFIED);                    
                }
                break;

            case SHARED_CLEAN:
                 break;

            case SHARED_MODIFIED:
                if (input_sig == BUS_UPD)
                {
                    line->setFlags(SHARED_CLEAN);
                }
                if (input_sig == BUS_RD)
                    flush++;
                break;

            case EXCLUSIVE:
                if (input_sig == BUS_RD)
                {
                    line->setFlags(SHARED_CLEAN);
                    interventions++;
                }

            default:
                break;
        }
    }
    return Out_signal;
}


