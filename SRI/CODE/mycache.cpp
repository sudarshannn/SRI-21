#include <fstream>
#include <string>
#include <regex>
#include "caches/lru_variants.h"
#include "caches/gd_variants.h"
#include "request.h"

using namespace std;
int isPagging = 1;
int pagesize = 128;

int main (int argc, char* argv[])
{
  // output help if insufficient params
  if(argc < 4) {
    cerr << "webcachesim traceFile cacheType cacheSizeBytes [cacheParams]" << endl;
    return 1;
  }

  // trace properties
  const char* path = argv[1];

  // create cache
  const string cacheType = argv[2];
  unique_ptr<Cache> webcache = Cache::create_unique(cacheType);
  if(webcache == nullptr)
    return 1;

  // configure cache size
  const uint64_t cache_size  = std::stoull(argv[3]);
  webcache->setSize(cache_size);

  // parse cache parameters
  regex opexp ("(.*)=(.*)");
  cmatch opmatch;
  string paramSummary;
  for(int i=4; i<argc; i++) {
    regex_match (argv[i],opmatch,opexp);
    if(opmatch.size()!=3) {
      cerr << "each cacheParam needs to be in form name=value" << endl;
      return 1;
    }
    webcache->setPar(opmatch[1], opmatch[2]);
    paramSummary += opmatch[2];
  }

  ifstream infile;
  long long reqs = 0, hits = 0;
  long long t, id, size;

  cerr << "running..." << endl;

  infile.open(path);
  SimpleRequest* req = new SimpleRequest(0, 0);
  while (infile >> t >> id >> size)
    {
        reqs++;
        if(isPagging)
        {
            size = ceil((1.0*size/pagesize)) * pagesize;
        }
        //cout<<size<<endl;
        req->reinit(id,size);
        webcache->cur_time++;
        webcache->wtf[id].push_back(webcache->cur_time);
        if(webcache->lookup(req)) {
            hits++;
        } else {
            webcache->admit(req);
        }


       /* cout<<id<<"-->";
        for(auto x:webcache->wtf[id])
          cout<<x<<" ";
        cout<<endl;*/
    }

  delete req;

  infile.close();
  cout << cacheType << " Cache size=" << cache_size << " " << paramSummary << " Total Request="
       << reqs << " Hits=" << hits << " Miss=" << reqs-hits <<" HIT RATE=" 
       << double(hits)/reqs << endl;

  return 0;
}

//  g++ tracegenerator/basic_trace.cc -std=c++11 -o basic_trace
// ./basic_trace 1000 1000 1.8 1 10000 test.tr