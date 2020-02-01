/*******************************************************
cache.cc
w : prWr
r : prRd
d: BusRd
x:BusRdX
u:BusUpgrade
p:BusUpdate
********************************************************/

#include "cache.h"
using namespace std;

//Constructor
Cache::Cache(int s,int a,int b, int num_processors, Cache **cachesArray1)
{
   ulong i, j;
   reads =  writes = 0; 
   readMisses = 0;
   writeMisses = writeBacks = lruUpgrd = 0;
   memTransactions = 0;
   procs = num_processors;
   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));  
   flushes = BusRdX = cache2cache = 0;
   interventions = 0;
   invalidations = 0;  
   cachesArray = cachesArray1;  
   tagMask =0;
   
   for(i=0;i<log2Sets;i++)
   {
	tagMask <<= 1;
        tagMask |= 1;
   }
   

   cache = new cacheLine*[sets];//two dimensional cache line size
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
	 cache[i][j].invalidate();   // Invalidate all lines initially
      }
   }      
   
}


int Cache::initialiseParams(int p,ulong addr,const char* rw){

   int currproc = p;            //store current processor value
   lruUpgrd++;    
         
   if( *rw == 'w') setwrites();
   else if ( *rw == 'r' ) setreads();


   int match = 0;

   for(int i=0;i<procs;i++)    //Check if other caches have the block.
   {
      if ( i == currproc)
         continue;
      match = cachesArray[i]->match(addr);
      if ( match == 1)
          break;
   }
   return match;

}


//Access function for MSI protocol

void Cache::AccessMSI(int p, ulong addr, const char* rw)
{
   int currproc = p;            //Denotes currproc processor number.
   lruUpgrd++;     
        	
   if( *rw == 'w') setwrites();
   else if ( *rw == 'r' ) setreads();


if ( (*rw == 'r') || (*rw == 'w') )   
{   
	cacheLine * line = findLine(addr);

	if(line == NULL)          
	{
	if( *rw == 'w' ) setwritemiss();  //misses in invalid state
	else setreadmiss();

      cacheLine *newline = fillLine(addr); 

      if ( *rw == 'w')
      {
         newline->setFlags(MODIFIED);


         BusRdX++;          //Issuing BusRdX request
         for(int i=0;i<procs;i++)
         {
            if ( i == currproc )
               continue;
            string rw = "x";
            cachesArray[i]->AccessMSI(i, addr, rw.c_str());
         }
      }

      if ( *rw == 'r' )
      {
         newline->setFlags(SHARED);

         //Issue BusRd
         for(int i=0;i<procs;i++)
         {
            if ( i == currproc )
               continue;
            string rw = "d";
            cachesArray[i]->AccessMSI(i, addr, rw.c_str());
         }
      }  
		
	}

	else     
	{
		
	updateLRU(line);//update LRU on hit


      if( line->getFlags() == MODIFIED )
      {
         line->setFlags(MODIFIED);   
      }  

      else if( line->getFlags() == SHARED)
      {
         if( *rw == 'r')
         {
            line->setFlags(SHARED);
         }

         if( *rw == 'w')
         {
            line->setFlags(MODIFIED);


            BusRdX++;             //Issue BusRdX request
            memTransactions++;
            for(int i=0;i<procs;i++)
            {
               if( i == currproc )    
               continue;
               string rw = "x";
               cachesArray[i]->AccessMSI(i, addr, rw.c_str());
            }
         }
      }  

	}  
} 


else
{
  Access_BusOp(currproc, addr, rw); //for bus operations
}

}



void Cache::setwrites(){
writes++;
}

void Cache::setwritemiss(){
writeMisses++;
}

void Cache::setreadmiss(){
readMisses++;
}

ulong Cache::getwrites(){
return writes;
}

void Cache::setreads(){
reads++;
}

ulong Cache::getreads(){
return reads;
}




// Access function for MESI 
void Cache::AccessMESI(int p, ulong addr, const char* rw)
{
    int currproc=p;
    int check=0;
   check = initialiseParams(currproc,addr,rw);

// PrRd or PrWr 
if ( (*rw == 'r') || (*rw == 'w') )   
{   
   cacheLine * line = findLine(addr);

   if(line == NULL)  
   {
      if( *rw == 'w' ) setwritemiss();
      else setreadmiss();

      cacheLine *newline = fillLine(addr); 


      if ( *rw == 'w')
      {
         newline->setFlags(MODIFIED);

         if (check == 1)
            cache2cache++;
         BusRdX++;  //request BusRd
         for(int i=0;i<(int)procs;i++)
         {
            if ( i == currproc )
            continue;
            string rw = "x";
            cachesArray[i]->AccessMESI(i, addr, rw.c_str());
         }
      } 

      if ( *rw == 'r' )
      {

         if ( check == 1 )  //check if copy present in another block
         {
            newline->setFlags(SHARED);
            cache2cache++;

            //Issue BusRd.
            for(int i=0;i<(int)procs;i++)
            {
               if ( i == currproc )
               continue;
               string rw = "d";
               cachesArray[i]->AccessMESI(i, addr, rw.c_str());
            }
         }

         else if ( check == 0 )
         {
            newline->setFlags(EXCLUSIVE);

            //Issue BusRd in both cases.
            for(int i=0;i<(int)procs;i++)
            {
               if ( i == currproc )
               continue;
               string rw = "d";
               cachesArray[i]->AccessMESI(i, addr, rw.c_str());
            }
         }

      }  
      
   } 

   else     
   {
      updateLRU(line); //update LRU on hit
      
      if( line->getFlags() == MODIFIED )
      {
         line->setFlags(MODIFIED);   
      }  


      else if( line->getFlags() == SHARED )
      {
         if( *rw == 'r')
         {
            line->setFlags(SHARED);
         }

         if( *rw == 'w')
         {
            line->setFlags(MODIFIED);

            for(int i=0;i<(int)procs;i++)
            {
               if( i == currproc ) 
               continue;
               string rw = "u";
               cachesArray[i]->AccessMESI(i, addr, rw.c_str());
            }
         }

      }  


      else if ( line->getFlags() == EXCLUSIVE )
      {
         if ( *rw == 'r')
         {
            line->setFlags(EXCLUSIVE);
         }

         if ( *rw == 'w')
         {
            line->setFlags(MODIFIED);
         }
      }  

   } 
} 



else if ( (*rw == 'd') || (*rw == 'x') || (*rw == 'u'))
{
   cacheLine * line = findLine(addr);

   if(line == NULL) 
   {

   }

   else    
   {
      if ( line->getFlags() == MODIFIED )
      {
         if ( *rw == 'd')
         {
            line->setFlags(SHARED);
            interventions++;        
            writeBack(addr);  
            flushes++;  
         }

         if( *rw == 'x' )
         {
            line->setFlags(INVALID);
            invalidations++;
            writeBack(addr);
            flushes++;
         }


      } 

      else if ( line->getFlags() == SHARED )
      {
         if ( *rw == 'd')
         {
            line->setFlags(SHARED);

         }
         if ( *rw == 'x')
         {
            line->setFlags(INVALID);
            invalidations++;

         }
         if ( *rw == 'u')
         {
            line->setFlags(INVALID);
            invalidations++;
         }
      } 
      else if ( line->getFlags() == EXCLUSIVE )
      {
         if ( *rw == 'd')
         {
            line->setFlags(SHARED);
            interventions++;
            //Issue Flushrwt
         }

         if ( *rw == 'x' )
         {
            line->setFlags(INVALID);
            invalidations++;
         }

      } 

   }  
      
} 

}  

int Cache::match(ulong address)
{
   int found = 0;
   ulong tag, i;

   tag = calcTag(address);
   i   = calcIndex(address);

  for(ulong j=0; j<assoc; j++)
   if(cache[i][j].isValid())   
     if(cache[i][j].getTag() == tag)
      {
           found = 1; break; 
      }

   return found;
}




// Access function for Dragon protocol
void Cache::AccessDragon(int p, ulong addr, const char* rw)
{ 
    int currproc=p;
    int check=0;
   check = initialiseParams(currproc,addr,rw);


   

//Processor read request.
if (*rw == 'r')    
{   
    cacheLine *  line = findLine(addr);   //Check for miss or hit 
    //PrRdMiss
    if(line == NULL)
    {
         readMisses++;

         if( check == 1 )
         {
            cacheLine *newline = fillLine(addr);
            newline->setFlags(Sc);

            //Issue BusRd
            for(int i=0;i<procs;i++)
            {
               if( i == currproc )
                  continue;
               string rw = "d";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            }
         }

         else if( check == 0 )
         {
            cacheLine *newline = fillLine(addr);
            newline->setFlags(EXCLUSIVE);

            //Issue BusRd
            for(int i=0;i<procs;i++)
            {
               if( i == currproc )
                  continue;
               string rw = "d";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            }
         }
    }
    else   
    {
      
      updateLRU(line);
      if( line->getFlags() == MODIFIED )
      {
         line->setFlags(MODIFIED);   //Stay in modified.
      }  
      else if(line->getFlags() == EXCLUSIVE)
      {
         line->setFlags(EXCLUSIVE);
      }
     else if( line->getFlags() == Sm)
      {
         line->setFlags(Sm);
      }
      else if ( line->getFlags() == Sc)
      {
         line->setFlags(Sc);
      } 
    }
} 

//Processor write request.
if ( *rw == 'w')
{
   
    cacheLine *  line = findLine(addr);

    //PrWrMiss
    if(line == NULL)
    {
         writeMisses++;
         if(check == 1)
         {
            cacheLine *newline = fillLine(addr);
            newline->setFlags(Sm);

            //Issue BusRd, then issue BusUpdate
            //Issue BusRd
            for(int i=0;i<procs;i++)
            {
               if( i == currproc )
               continue;
               string rw = "d";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            }
            //Issue BusUpdate
            for(int i=0;i<procs;i++)
            {
               if ( i==currproc )
               continue;
               string rw = "p";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            }
         }

         else if(check == 0)
         {
            cacheLine *newline = fillLine(addr);
            newline->setFlags(MODIFIED);

            //Issue BusRd
            for(int i=0;i<procs;i++)
            {
               if( i == currproc )
               continue;
               string rw = "d";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            }
         }
    }
    else   
    {
      updateLRU(line);  
      if( line->getFlags() == MODIFIED )
      {
         line->setFlags(MODIFIED);   //Stay in modified.
      }  
      else if(line->getFlags() == EXCLUSIVE)
      {
         line->setFlags(MODIFIED);
      }
      else if( line->getFlags() == Sm)
      {
         if ( check == 1)
         {
            line->setFlags(Sm);
            //Issue BusUpdate
            for(int i=0;i<procs;i++)
            {
               if( i == currproc)
               continue;
               string rw = "p";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            }
         }
         else if ( check == 0)
         {
            line->setFlags(MODIFIED);
            
            //Issue BusUpdate
            for(int i=0;i<procs;i++)
            {
               if( i == currproc)
               continue;
               string rw = "p";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            }     
         }
      } 
      else if ( line->getFlags() == Sc)
      {
         if ( check == 1)
         {
            line->setFlags(Sm);

            //Issue BusUpdate
            for(int i=0;i<procs;i++)
            {
               if( i == currproc)
               continue;
               string rw = "p";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            }   
         }  
         else if ( check == 0)
         {
            line->setFlags(MODIFIED);
            //Issue BusUpdate
            for(int i=0;i<procs;i++)
            {
               if( i == currproc )
               continue;
               string rw = "p";
               cachesArray[i]->AccessDragon(i, addr, rw.c_str());
            } 
         }  
      } 
    } 
} 




if ( (*rw == 'd') || (*rw == 'p'))
{
   cacheLine *  line = findLine(addr);
   if (line == NULL)
   {
      //do nothing.
   }
   else
   {
      if ( line->getFlags() == MODIFIED)
      {
         if ( *rw == 'd')
         {
            line->setFlags(Sm);
            interventions++;
            //Issue Flush
            flushes++;
         }
         if ( *rw == 'p')
         {
	 //do nothing
	 }
      } 
      else if ( line->getFlags() == Sm)
      {
         if ( *rw == 'd')
         {
            line->setFlags(Sm);

            //Issue Flush
            flushes++;
         }
         if ( *rw == 'p')
         {
            line->setFlags(Sc);

            ulong tag = addr >> (log2Blk);//calcTag(addr);
            line->setTag(tag);  
         }
      }  
      else if ( line->getFlags() == Sc)
      {
         if ( *rw == 'd')
         {
            line->setFlags(Sc);
         }

         if ( *rw == 'p' )
         {
            line->setFlags(Sc);

            ulong tag = addr >> (log2Blk);//calcTag(addr);
            line->setTag(tag);
            
         }
      } 
      else if ( line->getFlags() == EXCLUSIVE)
      {
         if ( *rw == 'd')
         {
            line->setFlags(Sc);
            interventions++;
         }
         if ( *rw == 'p')
         {}
      } 
   }
} 
}  





cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
	if(cache[i][j].isValid())   //Either SHARED or MODIFIED
	  if(cache[i][j].getTag() == tag)
		{
		     pos = j; break; 
		}
   if(pos == assoc)
	return NULL;
   else
	return &(cache[i][pos]); 
}


void Cache::updateLRU(cacheLine *line)
{
  line->setSeq(lruUpgrd);  
}

/*return an invalid line as LRU, if it exists, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = lruUpgrd;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) return &(cache[i][j]);     
   }   

   for(j=0;j<assoc;j++)
   {
	 if(cache[i][j].getSeq() <= min) 
    { 
      victim = j; 
      min = cache[i][j].getSeq();
    }
   } 
   assert(victim != assoc);
   
   return &(cache[i][victim]);
}


cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}


cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   if ( (victim->getFlags() == MODIFIED) || (victim->getFlags() == Sm) )
      writeBack(addr);
   assert(victim != 0);
    	
   tag = calcTag(addr);   
   victim->setTag(tag);   



   return victim;
}

void Cache::printStats(int proto)
{ 
   ulong mt;
   if(proto==0){
   mt=memTransactions+readMisses+writeMisses+writeBacks;
   }
   if(proto==1){
    mt=readMisses+writeMisses+writeBacks-cache2cache;
   }
   if(proto==2){
   mt=readMisses+writeMisses+writeBacks;
   }
   
   cout 
   << "01. number of reads:      \t\t\t" << getreads() << '\n'
   << "02. number of read misses:\t\t\t" << readMisses << '\n'
   << "03. number of writes:     \t\t\t" << getwrites() << '\n'   
   << "04. number of write misses:\t\t\t" << writeMisses << '\n'
   << "05. total miss rate:      \t\t\t" << setprecision(3) << (float) (100 * ((float)(readMisses + writeMisses)/(float)(reads+writes))) << "%" << '\n'
   << "06. number of writebacks: \t\t\t" << writeBacks << '\n'
   << "07. number of cache-to-cache transfers: \t" <<  cache2cache << '\n'
   << "08. number of memory transactions: \t\t" << mt << '\n'
   << "09. number of interventions:  \t\t\t" << interventions << '\n' 
   << "10. number of invalidations:   \t\t\t" << invalidations << '\n'
   << "11. number of flushes:    \t\t\t" << flushes << '\n'
   << "12. number of BusRdX:     \t\t\t" <<  BusRdX << endl;
}


void Cache::Access_BusOp(int currproc, ulong addr, const char* rw)
{
   cacheLine * line = findLine(addr);

   if(line == NULL)  //INVALID state
   {
   } 
   else    
   {

      if ( line->getFlags() == MODIFIED )
      {
         if ( *rw == 'd')
         {
            line->setFlags(SHARED);
		      interventions++;	     
	

	   writeBack(addr);	//flush request
           flushes++;
         }

         if( *rw == 'x' )
         {
            line->setFlags(INVALID);
		      invalidations++;

	        writeBack(addr);
           flushes++;
         }
      } 


      else if ( line->getFlags() == SHARED )
      {
         if ( *rw == 'd')
         {
            line->setFlags(SHARED);
         }

         if ( *rw == 'x')
         {
            line->setFlags(INVALID);
	    invalidations++;
         }
      } 

   }  
      

} 





