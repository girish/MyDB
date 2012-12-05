#ifndef STATISTICS_
#define STATISTICS_

//#define _STATISTICS_DEBUG 0

#include "ParseTree.h"
#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include <string>
#include <fstream>
#include <iostream>
using namespace std;

// Structure to store stats of a relation
struct TableInfo
{
    unsigned long long int numTuples;			// total num of rows in this relation
    int numPartition;							// partition this relation belongs to
    // -1 signifies that its still singleton
    map <string, unsigned long long int> Atts;	// <attribute-name, distinct values>

    TableInfo(): numTuples(0), numPartition(-1)
    {}
};


class Statistics
{
private:
    int m_nPartitionNum;
    bool m_bRelStatsCopied;
    map <string, TableInfo> m_mRelStats;		// is modified with Apply()
    map <string, TableInfo> m_mRelStatsCopy;	// is written to file
    map <int, vector<string> > m_mPartitionInfoMap;
    map <string, vector <string> > m_mColToTable;

    //used for estimation, need to declare as class member or global
    //due to template restrictions
    struct ColCountAndEstimate
    {
        int repeatCount;  //number of times same column is repeated
        long double estimate;   //this might change depending on how many columns participate
    };

    //returns true if parseTree is error free, false otherwise
    bool checkParseTreeForErrors(struct AndList *parseTree, char *relNames[],
        int numToJoin, vector<string>&, set<string>&);

public:
    Statistics();
    Statistics(Statistics &copyMe);	 // Performs deep copy
    ~Statistics();


    void AddRel(const char *relName, int numTuples);
    void AddAtt(const char *relName, const char *attName, int numDistincts);

    void CopyRel(const char *oldName, const char *newName);

    void Read(const char *fromWhere);
    void Write(const char *fromWhere);

    void  Apply(struct AndList *parseTree, char *relNames[], int numToJoin);
    double Estimate(struct AndList *parseTree, char **relNames, int numToJoin);

    static void PrepareStatisticsFile(const char* fileName, const char* prefix[]);

    // getter functions
    int GetPartitionNumber()
    {
        return m_nPartitionNum;
    }

    map<string, TableInfo> * GetRelStats()
    {
        return &m_mRelStats;	// return pointer to m_mRelStats
    }

    map<int, vector<string> > * GetPartitionInfoMap()
    {
        return &m_mPartitionInfoMap; // return pointer to PartitionInfoMap
    }

    map<string, vector <string> > * GetColToTableMap()
    {
        return &m_mColToTable;
    }
};

#endif
