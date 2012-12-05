#include "BigQ.h"
#include "math.h"
#include <queue>

using namespace std;

BigQ :: BigQ (Pipe &in, Pipe &out, OrderMaker &sortorder, int runlen)
: m_runFile(), m_nRunLen(runlen), m_sFileName(), m_nAppendCount(0)
{
    //init data structures
    m_pInPipe = &in;
    m_pOutPipe = &out;
    m_pSortOrder = &sortorder;
    //    m_sFileName = "runFile" + getTime();
    m_sFileName = "runFile" + System::getusec();

    m_runFile.Create(const_cast<char*>(m_sFileName.c_str()));
    m_runFile.Close();

#ifdef _DEBUG
    cout<<"BigQ : temp runFile name : " << m_sFileName << endl;
    //    m_pSortOrder->Print();
#endif

    // read data from in pipe sort them into runlen pages
    pthread_t sortingThread;
    pthread_create(&sortingThread, NULL, &getRunsFromInputPipeHelper, (void*)this);

}

BigQ::~BigQ ()
{
    // loop over m_vRuns and empty it out
    Run * r = NULL;
    for (int i=0; i < m_vRuns.size(); i++)
    {
        r = m_vRuns.at(i);
        delete r;
        r = NULL;
    }

    // remove runFile
    if(remove(m_sFileName.c_str()) != 0)
        perror("error in removing old file");
}

void* BigQ::getRunsFromInputPipeHelper(void* context)
{
#ifdef _DEBUG
    cout<<"calling from Helper"<<endl;
#endif
    ((BigQ *)context)->getRunsFromInputPipe();
#ifdef _DEBUG
    cout<<"called from Helper"<<endl;
#endif
}

void* BigQ::getRunsFromInputPipe()
{
#ifdef _DEBUG
    cout<<"\n\n "<< m_sFileName << " :inside getRunsFrom Pipe"<<endl;
#endif
    Record recFromPipe;
    vector<Record*> aRunVector;		
    Page currentPage;
    int pageCountPerRun = 0;
    bool allSorted = true;
    bool overflowRec = false;

    int recs = 0;

    while(m_pInPipe->Remove(&recFromPipe))
    {
        recs++;
        Record *copyRec = new Record();
        copyRec->Copy(&recFromPipe); //because currentPage.Append() would consume the record

        //initially pageCountPerRun is always less than m_nRunLen (as runLen can't be 0)
        if(!currentPage.Append(&recFromPipe))
        {
            //page full, start new page and increase the page count both global and local
            pageCountPerRun++;
            currentPage.EmptyItOut();
            currentPage.Append(&recFromPipe);
            if(pageCountPerRun >= m_nRunLen)
            {
                //this is the only check for pageCount >= runLen and is sufficient
                //because pageCount can never increase until a record spills over
                overflowRec = true;
                pageCountPerRun = 0;    //reset pageCountPerRun for next run as current run is full
            }
        }
        if(!overflowRec)
        {
            aRunVector.push_back(copyRec); //whether it's a new page or old page, just push back the copy onto vector
            allSorted = false;
            continue;   //don't sort i.e. go down until the run is full
        }

        //control here means one run is full, sort it and write to file
        //then insert the overflow Rec and start over

        sort(aRunVector.begin(), aRunVector.end(), CompareMyRecords(m_pSortOrder));
        appendRunToFile(aRunVector);

        //now clear the vector to begin for new run
        //pageRecord must have been reset already as run was full
        aRunVector.clear();
        allSorted = true;
        if(overflowRec)     //this would always be the case
        {
            aRunVector.push_back(copyRec);  //value in copyRec is still intact
            allSorted = false;
            overflowRec = false;
        }
    }
#ifdef _DEBUG
    cout<<"\n\n "<< m_sFileName << " : "<< recs << "recs removed from inPipe"<<endl;
#endif

    //done with all records in pipe, if there is anything in vector
    //it should be sorted and written out to file
    if(!allSorted)
    {
        //sort the vector
        sort(aRunVector.begin(), aRunVector.end(), CompareMyRecords(m_pSortOrder));
        appendRunToFile(aRunVector);    //flush everything to file

#ifdef _DEBUG
        //    //get the records from runfile and feed 'em to outpipe
        //    m_runFile.Open(const_cast<char*>(m_sFileName.c_str()));
        //    m_runFile.MoveFirst();
        //    Record *rc = new Record();
        //    int recCtr = 0;
        //    while (m_runFile.GetNext(*rc) == 1)
        //    {
        //    m_pOutPipe->Insert(rc);
        //            recCtr++;
        //    }
        //    cout << "\n\n Records sent to outpipe = " << recCtr <<endl;
        //    m_runFile.Close();
#endif


    }

#ifdef _DEBUG
    for (int l = 0; l < m_vRunLengths.size(); l++)
    {
        cout << "\nRun " << l << " length " << m_vRunLengths.at(l);
    }
#endif

    //now call mergeRuns here!
    MergeRuns();
    m_pOutPipe->ShutDown();
}

void BigQ::appendRunToFile(vector<Record*>& aRun)
{
    m_runFile.Open(const_cast<char*>(m_sFileName.c_str()));     //open with the same name
    int length = aRun.size();

#ifdef _DEBUG
    cout << "\n\nFileName : " << m_sFileName.c_str() << endl;
    cout<<"Append Run to File count : "<< m_nAppendCount + 1 <<endl;
    cout << "\n\n---- BigQ::appendRunToFile aRun.size() = " << length;
#endif

    int nPagesBefore = m_runFile.GetFileLength();

    //insert first record into new page so that a clear demarcation can be established
    //start this demarcation from 2nd run (don't do it for first run )
    int i = 0;
    if(m_nAppendCount > 0)
    {
        m_runFile.Add(*aRun[0], true);
        i = 1;
    }
    for(; i < length; i++)
        m_runFile.Add(*aRun[i]);

    m_runFile.Close();
    int nPagesAfter = m_runFile.GetFileLength();
    m_nAppendCount++;

#ifdef _DEBUG
    cout <<"\n\n "<< m_sFileName << " :\n***\nm_vRunLengths.size() = " <<  m_vRunLengths.size();
    cout << "\nPages before: " << nPagesBefore;
    cout << "\nPages after: " << nPagesAfter << endl;
#endif

    // first run has one extra page count, as 0th page is blank
    if ( m_vRunLengths.size() == 0)
    {
        m_vRunLengths.push_back(nPagesAfter - nPagesBefore - 1) ;
#ifdef _DEBUG
        cout << "\n\n "<< m_sFileName << " :\n\nPush-0 : " << nPagesAfter - nPagesBefore -1<< endl;
#endif
    }
    else
    {
        m_vRunLengths.push_back(nPagesAfter - nPagesBefore);
#ifdef _DEBUG
        cout << "\n\n "<< m_sFileName << " :\n\nPush-0 : " << nPagesAfter - nPagesBefore << endl;
#endif
    }
}


/* --------------- Phase-2 of TPMMS: MergeRuns() --------------- */

/* Input parameters: None
 * Return type: RET_SUCCESS or RET_FAILURE
 * Function: push sorted records through outPipe
 */
int BigQ::MergeRuns()
{
    m_runFile.Close();
    m_runFile.Open((char*)m_sFileName.c_str());
    ComparisonEngine ce;
    int nPagesFetched = 0;
    int nRunsAlive = 0;
    int nTotalPages = m_runFile.GetFileLength() - 1;

    // delete this
    int recs = 0;

    // we need to do an m-way merge
    // m = total pages/run length
    const int nMWayRun = m_vRunLengths.size();

    // Priority queue that contains sorted records
    priority_queue < Record_n_Run, vector <Record_n_Run>,
                   less<vector<Record_n_Run>::value_type> > pqRecords;

    // ---- Initial setup ----
    // There are m_nRunLen pages in one run
    // so fetch 1st page from each run
    // and put them in m_vRuns vector (sized nMWayRun)

    int runHeadPage = 0;
    Run * pRun = NULL;
    Record * pRec = NULL;

#ifdef _DEBUG
    cout<< m_sFileName <<" : nMWayRun = "<<nMWayRun<<endl;
#endif

    for (int i = 0; i < nMWayRun; i++)
    {
        // Run length of 0th run is 1 extra, as 0th page doesn't contain data
        pRun = new Run(m_vRunLengths.at(i));
        pRun->set_curPage(runHeadPage);
        m_runFile.GetPage(pRun->getPage(), pRun->get_and_inc_pagecount());
        nPagesFetched++;
        nRunsAlive++;

#ifdef _DEBUG
        cout <<"\n\n "<< m_sFileName << " :\n\n runHeadPage = " << runHeadPage
            << "\n nPagesFetched = "<< nPagesFetched
            << "\n nRunsAlive = "<< nRunsAlive;
#endif

        // fetch 1st record and push in the priority queue
        pRec = new Record();
        int ret = pRun->getPage()->GetFirst(pRec);
        if (!ret)
        {
            // initially every page should have at least one record
            // error here... really bad!
            return RET_FAILURE;
        }

        if (pRec)
        {
            Record_n_Run rr(m_pSortOrder, &ce, pRec, i);
            pqRecords.push(rr);
            pRec = NULL;
        }
        else
        {
            // pRec cannot be NULL here... very wrong
            return RET_FAILURE;
        }

        // increment page-counter to go to the next run
        runHeadPage += m_vRunLengths.at(i);
        m_vRuns.push_back(pRun);
    }

#ifdef _DEBUG
    cout <<"\n\n "<< m_sFileName << " : m_vRuns.size() = " << m_vRuns.size()
        << "\n pqRecords.size() = " << pqRecords.size() << endl;
#endif


    // fetch 1st record from each page
    // and push it in the min priority queue
    bool bFileEmpty = false;
    int nRunToFetchRecFrom = 0;
    while (bFileEmpty == false)
    {
        if (pqRecords.size() < nRunsAlive)
        {
            pRec = new Record;
            int ret = m_vRuns.at(nRunToFetchRecFrom)->getPage()->GetFirst(pRec);
            if (!ret)
            {
                // records from this page are over
                // see if new page from this run can be fetched
                if (m_vRuns.at(nRunToFetchRecFrom)->canFetchPage(nTotalPages))
                {
#ifdef _DEBUG
                    cout << "\n\n records outed till now = "<< recs;
#endif

                    // fetch next page
                    m_runFile.GetPage(m_vRuns.at(nRunToFetchRecFrom)->getPage(),
                        m_vRuns.at(nRunToFetchRecFrom)->get_and_inc_pagecount());
                    nPagesFetched++;

#ifdef _DEBUG
                    cout << "\n\ncanFetchPage is true for run " << nRunToFetchRecFrom
                        << "\n so nPagesFetched = " << nPagesFetched <<endl;
#endif

                    // fetch first record from this page now
                    int ret2 = m_vRuns.at(nRunToFetchRecFrom)->getPage()->GetFirst(pRec);
                    if (!ret2)
                    {
                        cout << "\nBigQ::MergeRuns --> fetching record from page x of run "
                            << nRunToFetchRecFrom << " failed. Fatal!\n\n";
                        return RET_FAILURE;
                    }
                }
                else
                {
                    // all the pages from this run have been fetched
                    // size of M in the m-way run would reduce by one.. logically
                    nRunsAlive--;

#ifdef _DEBUG
                    cout << "\n\ncanFetchPage is *false* for run " << nRunToFetchRecFrom
                        << "\n--- nPagesFetched = "<< nPagesFetched
                        << "\n--- nRunsAlive = "<< nRunsAlive;
                    cout << "\n\n records outed till now = "<< recs;
#endif

                    // this run is over, fetch next record from the next alive run
                    nRunToFetchRecFrom = 0;
                    while (nRunToFetchRecFrom < nMWayRun)
                    {
                        if (m_vRuns.at(nRunToFetchRecFrom)->is_alive())
                            break;
                        nRunToFetchRecFrom++;
                    }

                    // if no run is alive, all pages from all runs have been fetched
                    if (nRunToFetchRecFrom == nMWayRun)
                        bFileEmpty = true;

                    delete pRec;
                    pRec = NULL;	// because no record was fetched
                }
            }

            // got the record, push it in the min priority queue
            // for now, push it in a vector
            if (pRec)
            {
                Record_n_Run rr(m_pSortOrder, &ce, pRec, nRunToFetchRecFrom);
                pqRecords.push(rr);
                pRec = NULL;
            }
        }

        // priority queue is full, pop min record
        if (nRunsAlive != 0 && pqRecords.size() == nRunsAlive)
        {
            // find min record
            Record_n_Run rr = pqRecords.top();
            pqRecords.pop();
            // push min element through out-pipe
            m_pOutPipe->Insert(rr.get_rec());
            // keep track of which run this record belonged too
            // need to fetch next record from the run of that page
            nRunToFetchRecFrom = rr.get_run();
            // do not delete memory allocated for record,
            recs++;
        }
    }

#ifdef _DEBUG
    cout << "\n\n records outed till now = "<< recs;
#endif

    // empty the priority queue
    while (pqRecords.size() > 0)
    {
        // find min record
        Record_n_Run rr = pqRecords.top();
        pqRecords.pop();
        // push min element through out-pipe
        m_pOutPipe->Insert(rr.get_rec());
        // keep track of which run this record belonged too
        // need to fetch next record from the run of that page
        nRunToFetchRecFrom = rr.get_run();
        // do not delete memory allocated for record,
        recs++;
    }

#ifdef _DEBUG
    cout << "\n\n records outed till now = "<< recs;
#endif

    return RET_SUCCESS;
}

string BigQ::getTime()
{
    time_t now;
    char time_val[50];

    time_val[0] = '\0';

    now = time(NULL);

    if (now != -1)
    {
        strftime(time_val, 50, "%k.%M.%S", gmtime(&now));
    }

    return std::string(time_val);
}
