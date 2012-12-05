#ifndef BIGQ_H
#define BIGQ_H

#include <pthread.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include "Pipe.h"
#include "File.h"
#include "Record.h"
#include "Defs.h"
#include "FileUtil.h"

using namespace std;

// class to store run information
class Run
{
private:
    Page *pPage;	// current page from the run
    int m_nCurrPage; // page number wrt whole file, needed to fetch next page using m_runFile.GetPage()
    int m_nPagesFetched; // keep track of how many pages have been fetched from this run
    int  m_nRunLen; // run length of TPMMS
    bool m_bIsAlive;

public:
    Run()
    {
        pPage = NULL;
        m_nCurrPage = 0;
        m_nPagesFetched = 0;
        m_nRunLen = 0;
        m_bIsAlive = true;
    }

    Run(int nRunLen)
    {
        pPage = new Page();
        m_nCurrPage = 0;
        m_nPagesFetched = 0;
        m_nRunLen = nRunLen;
        m_bIsAlive = true;
    }

    ~Run()
    {
        delete pPage;
        pPage = NULL;
        m_nCurrPage = 0;
        m_nPagesFetched = 0;
        m_nRunLen = 0;
        m_bIsAlive = false;
    }

    bool canFetchPage(int total_pages)
    {
        // more pages can be fetched from this run
        if (m_nPagesFetched < m_nRunLen &&
            m_nCurrPage < total_pages)
            return true;

        // otherwise mark that run is over
        m_bIsAlive = false;
        return false;
    }

    Page * getPage()
    {
        return pPage;
    }

    bool is_alive()
    {
        return m_bIsAlive;
    }

    void set_curPage(int currentPageNum)
    {
        m_nCurrPage = currentPageNum;
    }

    int get_and_inc_pagecount()
    {
        int tmp = m_nCurrPage;
        m_nCurrPage++;
        m_nPagesFetched++;
        return tmp;
    }
};

// Class to store records and comparator for priority queue
class Record_n_Run
{
private:
    OrderMaker *m_pSortOrder;
    ComparisonEngine *m_pCE;
    Record * m_pRec;
    int m_nRun;

public:

    Record_n_Run(OrderMaker *pOrderMaker, ComparisonEngine *pCE, Record *rec, int run)
        : m_pSortOrder(pOrderMaker), m_pCE(pCE), m_pRec(rec), m_nRun(run)
    {}

    ~Record_n_Run() {}

    Record * get_rec()
    {
        return m_pRec;
    }

    int get_run()
    {
        return m_nRun;
    }

    bool operator< (const Record_n_Run& r) const
    {
        Record_n_Run* rr = const_cast<Record_n_Run*>(&r);
        if(m_pCE->Compare(m_pRec, rr->get_rec(), m_pSortOrder) > 0)
            return true;
        return false;
    }
};

class BigQ
{
private:
    FileUtil m_runFile;
    Pipe *m_pInPipe, *m_pOutPipe;
    OrderMaker *m_pSortOrder;
    int m_nRunLen;
    string m_sFileName;
    ComparisonEngine ce;
    vector<int> m_vRunLengths;
    int m_nAppendCount;

private:
    // -------- phase - 1 --------------
    void appendRunToFile(vector<Record*>&);
    void* getRunsFromInputPipe();
    static void* getRunsFromInputPipeHelper(void*);

    struct CompareMyRecords
    {
        OrderMaker *pSortOrder;
        CompareMyRecords(OrderMaker *pOM): pSortOrder(pOM) {}

        bool operator()(Record* const& r1, Record* const& r2)
        {
            Record* r11 = const_cast<Record*>(r1);
            Record* r22 = const_cast<Record*>(r2);

            ComparisonEngine ce;
            if (ce.Compare(r11, r22, pSortOrder) < 0)
                return true;
            else
                return false;
        }
    };

    // Function to fetch timestamp and append it to filename
    string getTime();

    // -------- phase - 2 --------------
    vector<Run *> m_vRuns;  // max size of this vector will be m_nPageCount/m_nRunLen
    int MergeRuns();

public:
    BigQ (Pipe &in, Pipe &out, OrderMaker &sortorder, int runlen);
    ~BigQ ();
};

#endif
