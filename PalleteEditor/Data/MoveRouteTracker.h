#pragma once
#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace PaletteData {
// Pure tracking foundation. Not yet connected to live palette writes.
// Tables allocate once; Observe/Reset allocate nothing and access no process memory.
struct MoveRouteDefinition {
    std::string id;
    std::vector<std::pair<int,int>> membership;
};
class MoveRouteTable {
    std::vector<MoveRouteDefinition> definitions;
    std::vector<std::vector<int>> candidates,incoming;
public:
    MoveRouteTable(int count,std::vector<MoveRouteDefinition> moves,const std::vector<std::pair<int,int>>& edges):definitions(std::move(moves)) {
        if(count<1 || count>65535 || definitions.empty() || definitions.size()>1024)throw std::runtime_error("Invalid move route table.");
        candidates.resize(count);incoming.resize(count);std::set<std::string> ids;
        for(int index=0;index<(int)definitions.size();++index) {
            const auto& move=definitions[index];
            if(move.id.empty() || !ids.insert(move.id).second || move.membership.empty())throw std::runtime_error("Invalid or duplicate move route.");
            int previous=-1;
            for(auto [first,last]:move.membership) {
                if(first<0 || last<first || last>=count || first<=previous)throw std::runtime_error("Invalid move route membership.");
                for(int animation=first;animation<=last;++animation)candidates[animation].push_back(index);
                previous=last;
            }
        }
        for(auto [source,target]:edges) {
            if(source<0 || source>=count || target<0 || target>=count)throw std::runtime_error("Invalid move route edge.");
            incoming[target].push_back(source);
        }
        for(auto& sources:incoming){std::sort(sources.begin(),sources.end());sources.erase(std::unique(sources.begin(),sources.end()),sources.end());}
    }
    int Count() const {return (int)candidates.size();}
    const std::vector<int>& Candidates(int animation) const {return candidates.at(animation);}
    bool Contains(int route,int animation) const {
        if(route<0 || route>=(int)definitions.size() || animation<0 || animation>=Count())return false;
        const auto& choices=candidates[animation];return std::binary_search(choices.begin(),choices.end(),route);
    }
    int Unique(int animation) const {
        if(animation<0 || animation>=Count() || candidates[animation].size()!=1)return -1;
        return candidates[animation][0];
    }
    bool Edge(int source,int target) const {
        if(source<0 || source>=Count() || target<0 || target>=Count())return false;
        return std::binary_search(incoming[target].begin(),incoming[target].end(),source);
    }
    const std::string& Id(int route) const {return definitions.at(route).id;}
};
struct MoveRouteIdentity {
    uintptr_t root=0,actor=0,entity=0,palette=0;
    uint64_t generation=0;
    int slot=-1;
    bool operator==(const MoveRouteIdentity&) const = default;
    bool Valid() const {return root && actor && entity && palette && slot>=0 && slot<6;}
};
enum class MoveRouteStatus { Inactive,Active,Unavailable };
struct MoveRouteResult {MoveRouteStatus status=MoveRouteStatus::Unavailable;int route=-1;};
class MoveRouteTracker {
    const MoveRouteTable* table=nullptr;
    MoveRouteIdentity identity;
    int route=-1,previousAnimation=-1;
public:
    void Reset() {table=nullptr;identity={};route=previousAnimation=-1;}
    MoveRouteResult Observe(const MoveRouteTable& current,const MoveRouteIdentity& key,int animation,int immediatePrevious) {
        if(!key.Valid() || animation<0 || animation>=current.Count()){Reset();return {};}
        if(table!=&current || identity!=key){Reset();table=&current;identity=key;}
        const auto& choices=current.Candidates(animation);
        if(choices.empty()){route=-1;previousAnimation=animation;return {MoveRouteStatus::Inactive,-1};}
        if(choices.size()==1)route=choices[0];
        else {
            // The immediate predecessor can establish a route on first entering a
            // shared state. It cannot preserve that route once a shared loop repeats.
            const int origin=current.Unique(immediatePrevious);
            if(origin>=0 && current.Contains(origin,animation) && current.Edge(immediatePrevious,animation))route=origin;
            else if(!current.Contains(route,animation) || (animation!=previousAnimation && !current.Edge(previousAnimation,animation)))route=-1;
        }
        previousAnimation=animation;
        return route>=0?MoveRouteResult{MoveRouteStatus::Active,route}:MoveRouteResult{};
    }
};
}
