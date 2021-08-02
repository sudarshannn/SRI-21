#include <unordered_map>
#include <cassert>
#include <vector>
#include <math.h>
#include "gd_variants.h"
using namespace std;
vector<int>  hit_count;
#define soc_size 512

/*
  GD: greedy dual eviction (base class)
*/
bool GreedyDualBase::lookup(SimpleRequest* req)
{
    CacheObject obj(req);
    auto it = _cacheMap.find(obj);
    if (it != _cacheMap.end()) {
        // log hit
        LOG("hit", 0, obj.id, obj.size);
        hit(req);
        return true;
    }
    return false;
}

void GreedyDualBase::admit(SimpleRequest* req)
{
    const uint64_t size = req->getSize();
    // object feasible to store?
    if (size >= _cacheSize) {
        LOG("error", _cacheSize, req->getId(), size);
        return;
    }
    // check eviction needed
    while (_currentSize + size > _cacheSize) {
        evict();
    }
    // admit new object with new GF value
    //long double ageVal = ageValue(req);
    long double ageVal = H_function(req);
    CacheObject obj(req);
    ageVal=abs(ageVal);
    fq[obj.id%total_type]++;
    total++;
    
    LOG("added", ageValue(req), obj.id, obj.size);
    _cacheMap[obj] = _valueMap.emplace(ageValue(req), obj);
    _currentSize += size;
}

void GreedyDualBase::evict(SimpleRequest* req)
{
    // evict the object match id, type, size of this request
    CacheObject obj(req);
    auto it = _cacheMap.find(obj);
    if (it != _cacheMap.end()) {
        auto lit = it->second;
        CacheObject toDelObj = it->first;
        LOG("evicted", lit->first, toDelObj.id, toDelObj.size);
        _currentSize -= toDelObj.size;
        _valueMap.erase(lit);
        _cacheMap.erase(it);
    }
}

void GreedyDualBase::evict()
{
    // evict first list element (smallest value)
    if (_valueMap.size() > 0) {
        ValueMapIteratorType lit  = _valueMap.begin();
        if (lit == _valueMap.end()) {
            std::cerr << "underun: " << _currentSize << ' ' << _cacheSize << std::endl;
        }
        assert(lit != _valueMap.end()); // bug if this happens
        CacheObject toDelObj = lit->second;
        LOG("evicted", lit->first, toDelObj.id, toDelObj.size);

        fq[toDelObj.id%total_type]--;
        total--;


        _currentSize -= toDelObj.size;
        _cacheMap.erase(toDelObj);
        // update L
        _currentL = lit->first;
        _valueMap.erase(lit);
    }
}

long double GreedyDualBase::ageValue(SimpleRequest* req)
{
    return _currentL + 1.0;
}

void GreedyDualBase::hit(SimpleRequest* req)
{
    CacheObject obj(req);
    // get iterator for the old position
    auto it = _cacheMap.find(obj);
    assert(it != _cacheMap.end());
    CacheObject cachedObj = it->first;
    ValueMapIteratorType si = it->second;
    // update current req's value to hval:
    _valueMap.erase(si);
    long double hval = ageValue(req);
    it->second = _valueMap.emplace(hval, cachedObj);
}

/*
  Greedy Dual Size policy
*/
long double GDSCache::ageValue(SimpleRequest* req)
{
    const uint64_t size = req->getSize();
    return _currentL + 1.0 / static_cast<double>(size);
}

/*
  Greedy Dual Size Frequency policy
*/
bool WGDSFCache::lookup(SimpleRequest* req)
{
    bool hit = GreedyDualBase::lookup(req);
    CacheObject obj(req);
    if (!hit) {
        _reqsMap[obj] = 1; //reset bec. reqs_map not updated when element removed
    } else {
        _reqsMap[obj]++;
    }
    return hit;
}


long double WTF(CacheObject obj,int id,vector<vector<int>> &Wtf)
{
    int i;
    long double ans=0.0;
    for(i=1;i<(int)Wtf[id].size();i++)
    {
        if((int)Wtf[id][i]<(int)hit_count.size()-1)
        {
            ans+= (hit_count[Wtf[id][i]] - hit_count[Wtf[id][i-1]]) / (Wtf[id][i]-Wtf[id][i-1]);
        }
    }
    return ans;
}
// 

long double WDT(int id,int total_type,int total,map<int,int> &fq)
{
    return (1.0*fq[id % total_type])/(total);
}


long double SC(int size)
{
    // = 
    return (2.0 + size/soc_size)/(1.0*log(1.0*size));
}
long double GreedyDualBase::H_function(SimpleRequest* req)
{
    CacheObject obj(req);
    return _currentL + (1.0 * WTF(obj,obj.id,this->wtf) * WDT(obj.id,total_type,total,fq) * SC(obj.size));
}



/*
  LRU-K policy
*/
LRUKCache::LRUKCache()
    : GreedyDualBase(),
      _tk(2),
      _curTime(0)
{
}

void LRUKCache::setPar(std::string parName, std::string parValue) {
    if(parName.compare("k") == 0) {
        const int k = stoi(parValue);
        assert(k>0);
        _tk = k;
    } else {
        std::cerr << "unrecognized parameter: " << parName << std::endl;
    }
}


bool LRUKCache::lookup(SimpleRequest* req)
{
    CacheObject obj(req);
    _curTime++;
    _refsMap[obj].push(_curTime);
    bool hit = GreedyDualBase::lookup(req);
    return hit;
}

void LRUKCache::evict(SimpleRequest* req)
{
    CacheObject obj(req);
    _refsMap.erase(obj); // delete LRU-K info
    GreedyDualBase::evict(req);
}

void LRUKCache::evict()
{
    // evict first list element (smallest value)
    if (_valueMap.size() > 0) {
        ValueMapIteratorType lit  = _valueMap.begin();
        if (lit == _valueMap.end()) {
            std::cerr << "underun: " << _currentSize << ' ' << _cacheSize << std::endl;
        }
        assert(lit != _valueMap.end()); // bug if this happens
        CacheObject obj = lit->second;
        _refsMap.erase(obj); // delete LRU-K info
        GreedyDualBase::evict();
    }
}

long double LRUKCache::ageValue(SimpleRequest* req)
{
    CacheObject obj(req);
    long double newVal = 0.0L;
    if(_refsMap[obj].size() >= _tk) {
        newVal = _refsMap[obj].front();
        _refsMap[obj].pop();
    }
    //std::cerr << id << " " << _curTime << " " << _refsMap[id].size() << " " << newVal << " " << _currentL << std::endl;
    return newVal;
}

/*
  LFUDA
*/
bool LFUDACache::lookup(SimpleRequest* req)
{
    bool hit = GreedyDualBase::lookup(req);
    CacheObject obj(req);
    if (!hit) {
        _reqsMap[obj] = 1; //reset bec. reqs_map not updated when element removed
    } else {
        _reqsMap[obj]++;
    }
    return hit;
}

long double LFUDACache::ageValue(SimpleRequest* req)
{
    CacheObject obj(req);
    return _currentL + _reqsMap[obj];
}

long double WGDSFCache::ageValue(SimpleRequest* req)
{
    CacheObject obj(req);
    if(soc_size==536)
        return _currentL + static_cast<double>(_reqsMap[obj]) / static_cast<double>( obj.size);
    else if(soc_size==512)
        return _currentL + static_cast<double>((1.0*_reqsMap[obj])/(_reqsMap.size())) / static_cast<double>( obj.size);
    else
    {
        int REM=soc_size% 7;
        long double ansh = pow(_reqsMap[obj] , REM+1);
        return _currentL + (ansh/ static_cast<double>( obj.size));
    }
}
