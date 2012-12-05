#include "Sorted.h"

Sorted::Sorted() : m_pSortInfo(NULL), m_bReadingMode(true), m_pBigQ(NULL),
				   m_sMetaSuffix(".meta.data"), m_bPageFetched(false),
				   m_pQueryOrderMaker(NULL), m_bMatchingPageFound(false),
				   m_bQueryOMCreated(false)
{
	m_pFile = new FileUtil();
	m_pINPipe = new Pipe(PIPE_SIZE);
	m_pOUTPipe = new Pipe(PIPE_SIZE);
}

Sorted::~Sorted()
{
	delete m_pFile;
    m_pFile = NULL;

	delete m_pBigQ;
	m_pBigQ = NULL;

	m_bQueryOMCreated = false;
}

int Sorted::Create(char *f_path, void *sortInfo)
{
    m_pFile->Create(f_path);

	if (sortInfo == NULL)
	{
		cout << "\nSorted::Create --> sortInfo is NULL. ERROR!\n";
		return RET_FAILURE;
	}
	m_pSortInfo = (SortInfo*)sortInfo;
	WriteMetaData();
	#ifdef _DEBUG
    m_pSortInfo->myOrder->Print();
	#endif
	return RET_SUCCESS;
}

int Sorted::Open(char *fname)
{
    //read metadata here
    ifstream meta_in;
    meta_in.open((string(fname) + m_sMetaSuffix).c_str());
    string fileType;
    string type;
	if (!m_pSortInfo)
	{
	    m_pSortInfo = new SortInfo();
    	m_pSortInfo->myOrder = new OrderMaker();
	    meta_in >> fileType;
    	meta_in >> m_pSortInfo->runLength;
	    meta_in >> m_pSortInfo->myOrder->numAtts;
    	for (int i = 0; i < m_pSortInfo->myOrder->numAtts; i++)
	    {
    	    meta_in >> m_pSortInfo->myOrder->whichAtts[i];
        	meta_in >> type;
	        if (type.compare("Int") == 0)
    	        m_pSortInfo->myOrder->whichTypes[i] = Int;
        	else if (type.compare("Double") == 0)
            	m_pSortInfo->myOrder->whichTypes[i] = Double;
	        else
    	        m_pSortInfo->myOrder->whichTypes[i] = String;
	    }
	}
    return m_pFile->Open(fname);
}

// returns 1 if successfully closed the file, 0 otherwise
int Sorted::Close()
{
	m_bQueryOMCreated = false;
	MergeBigQToSortedFile();
    return m_pFile->Close();
}

/* Load function bulk loads the Sorted instance from a text file, appending
 * new data to it using the SuckNextRecord function from Record.h. The character
 * string passed to Load is the name of the data file to bulk load.
 */
void Sorted::Load (Schema &mySchema, char *loadMe)
{
    EventLogger *el = EventLogger::getEventLogger();
	m_bQueryOMCreated = false;

    FILE *fileToLoad = fopen(loadMe, "r");
    if (!fileToLoad)
    {
		el->writeLog("Can't open file name :" + string(loadMe));
		return;
    }

    /* Logic :
     * first read the record from the file using suckNextRecord()
     * then add this record to page using Add() function
     */

    Record aRecord;
    while(aRecord.SuckNextRecord(&mySchema, fileToLoad))
        Add(aRecord);

    fclose(fileToLoad);

	MergeBigQToSortedFile();
}

void Sorted::Add (Record &rec)
{
	m_bQueryOMCreated = false;

	// if mode reading, change mode to writing
	if (m_bReadingMode)
		m_bReadingMode = false;

	// push rec to IN-pipe
	m_pINPipe->Insert(&rec);

	// if !BigQ, instantiate BigQ(IN-pipe, OUT-pipe, ordermaker, runlen)
	if (!m_pBigQ)
		m_pBigQ = new BigQ(*m_pINPipe, *m_pOUTPipe, *(m_pSortInfo->myOrder), m_pSortInfo->runLength);
}

void Sorted::MergeBigQToSortedFile()
{
	m_bQueryOMCreated = false;

	// Check if there's anything to merge
	if (!m_pBigQ)
		return;

	// shutdown IN pipe
	m_pINPipe->ShutDown();

	// fetch sorted records from BigQ and old-sorted-file
	// and do a 2-way merge and write into new-file (tmpFile)
	Record * pRecFromPipe = NULL, *pRecFromFile = NULL;
	ComparisonEngine ce;
	FileUtil tmpFile;

    string tmpFileName = "tmpFile" + getusec();    //time(NULL) returns time_t in seconds since 1970
	tmpFile.Create(const_cast<char*>(tmpFileName.c_str()));

	m_pFile->MoveFirst();
	int fetchedFromPipe = 0, fetchedFromFile = 0;

    //if file on disk is empty (initially it will be) then don't fetch anything
    if(m_pFile->GetFileLength() == 0)
		fetchedFromFile = 0;

    do
	{
		if (pRecFromPipe == NULL)
		{
			pRecFromPipe = new Record;
			fetchedFromPipe = m_pOUTPipe->Remove(pRecFromPipe);
		}
		if (pRecFromFile == NULL && m_pFile->GetFileLength() != 0)
		{
			pRecFromFile = new Record;
			fetchedFromFile = m_pFile->GetNext(*pRecFromFile);
		}

		if (fetchedFromPipe && fetchedFromFile)
		{
			if (ce.Compare(pRecFromPipe, pRecFromFile, m_pSortInfo->myOrder) < 0)
			{
				tmpFile.Add(*pRecFromPipe);
				delete pRecFromPipe;
				pRecFromPipe = NULL;
			}
			else
			{
				tmpFile.Add(*pRecFromFile);
				delete pRecFromFile;
				pRecFromFile = NULL;
			}
		}
	}
    while (fetchedFromPipe && fetchedFromFile);

    if(fetchedFromFile != 0)
        tmpFile.Add(*pRecFromFile);
    if(fetchedFromPipe != 0)
        tmpFile.Add(*pRecFromPipe);


	Record rec;
	while (m_pOUTPipe->Remove(&rec))
	{
		tmpFile.Add(rec);
	}
	while (m_pFile->GetFileLength() != 0 && m_pFile->GetNext(rec))
	{
		tmpFile.Add(rec);
        }

	tmpFile.Close();

	// if tmpFile is not empty, then delete old file
	// and rename tmpFile to old file's name
	if (tmpFile.GetFileLength() != 0)
	{
		// delete old file
        if(remove(m_pFile->GetBinFilePath().c_str()) != 0)
        	perror("error in removing old file");   //remove this as file might not exist initially

		// rename tmp file to original old name
        if(rename(tmpFileName.c_str(), m_pFile->GetBinFilePath().c_str()) != 0)
        	perror("error in renaming temp file");
	}

	// delete BigQ
	delete m_pBigQ;
	m_pBigQ = NULL;

	// invalidate the old query-order-maker
	m_pQueryOrderMaker = NULL;
	m_bMatchingPageFound = false;
}

void Sorted::MoveFirst ()
{
	m_bQueryOMCreated = false;
	m_pFile->MoveFirst();
	m_pQueryOrderMaker = NULL;
	m_bMatchingPageFound = false;
}

// Function to fetch the next record in the file in "fetchme"
// Returns 0 on failure
int Sorted::GetNext (Record &fetchme)
{
	m_bQueryOMCreated = false;
	// If mode is not reading i.e. it is writing mode currently
	// merge differential data to already sorted file
	if (!m_bReadingMode)
	{
		m_bReadingMode = true;
		MergeBigQToSortedFile();
	}

	// Now we can start reading
	m_bPageFetched = true;
	return m_pFile->GetNext(fetchme);
}


// Function to fetch the next record in "fetchme" that matches
// the given CNF, returns 0 on failure.
int Sorted::GetNext (Record &fetchme, CNF &cnf, Record &literal)
{
    /* Logic:
     *
     * Prepare "query" OrderMaker from applyMe (CNF) -
     * If the attribute used in Sorted file's order maker is also present in CNF, append to "query" OrderMaker
     * else - stop making "query" OrderMaker */

    // If mode is not reading i.e. it is writing mode currently
    // merge differential data to already sorted file
    if (!m_bReadingMode)
    {
        m_bReadingMode = true;
        MergeBigQToSortedFile();
    }

	if (m_bPageFetched == false)
	{
		m_pFile->SetCurrentPage(0);
		m_bPageFetched = true;
	}

	// Make query-order-maker only if it is not already made
	if (m_bQueryOMCreated == false)
	{
		#ifdef _DEBUG
	    //m_pSortInfo->myOrder->Print();
		#endif

        m_pQueryOrderMaker = cnf.GetMatchingOrder(*(m_pSortInfo->myOrder));
		m_bQueryOMCreated = true;


		#ifdef _Sorted_DEBUG
        if (m_pQueryOrderMaker != NULL)
            m_pQueryOrderMaker->Print();
        else
            cout<<"NULL query order maker"<<endl;
		#endif
	}


    /* find the first matching record -
     * If query OrderMaker is empty, first record (from current pointer or from the beginning) is the matching record.
     * if query OrderMaker is not empty, perform binary search on file (from the current pointer) to find out a record -
     * which is equal to the literal (Record) using "query" OrderMaker and CNF (probably using only "query" OrderMaker)

     * returning apropriate value -
     * if no matching record found, return 0
     * if there is a possible matching record - start scanning the file matching every record found one by one.
     * First evaluate the record on "query" OrderMaker, then evaluate the CNF instance.
     * if the query OrderMaker doesn't accept the record, return 0
     * if query OrderMaker does accept it, match it with CNF
     * if CNF accepts, return the record.
     * if CNF doesn't accept, move on to next record.
     * repeat 1-2 until EOF.
     * if it's EOF, return 0.
     * Keep the value of "query" OrderMaker and current pointer safe until user performs "MoveFirst" or some write operation.
     */

	if (!m_pQueryOrderMaker)
	{
	    ComparisonEngine compEngine;
		while (GetNext(fetchme))
	    {
    	    if (compEngine.Compare(&fetchme, &literal, &cnf))
        	    return RET_SUCCESS;
		}
		//if control is here then no matching record was found
	    return RET_FAILURE;
	}
	else
	{
        ComparisonEngine compEngine;

		// Binary search must be done once per query-order-maker
		if (m_bMatchingPageFound == false)
		{
			m_bMatchingPageFound = true;
			LoadMatchingPage(literal);
			// find the page where query-order-maker first matches (using binary search)
            bool foundMatchingRec = false;
            while(m_pFile->GetNext(fetchme, true))
            {
                // match with queryOM, until we find a matching record
                if (compEngine.Compare(&literal, m_pQueryOrderMaker, &fetchme, m_pSortInfo->myOrder) == 0)
                {
                    if (compEngine.Compare(&fetchme, &literal, &cnf))
                        return RET_SUCCESS;
                    foundMatchingRec = true;
                    break;
                }
            }
            if(!foundMatchingRec)
                return RET_FAILURE;
		}

		while(true)
        {
        	if(GetNext(fetchme))
            {
            	// match with queryOM, if matches, compare with CNF
                if (compEngine.Compare(&literal, m_pQueryOrderMaker, &fetchme, m_pSortInfo->myOrder) == 0)
                {
                	if (compEngine.Compare(&fetchme, &literal, &cnf))
                    	return RET_SUCCESS; 
					// otherwise CNF didn't match, try next record
                }
                else
                {
                	//if queryOM doesn't match, stop searching, return false
                    return RET_FAILURE;
                }
			}
            else
            	return RET_FAILURE;
		}
	}

    //if control is here then no matching record was found
    return RET_FAILURE;
}

int Sorted::LoadMatchingPage(Record &literal)
{
	// copy the position of current pointer in the file
    // m_nCurrPage and m_pPage should be saved
    Page OldPage;
    int nOldPageNumber;
    m_pFile->SaveFileState(OldPage, nOldPageNumber);

	// nOldPageNumber (FileIUtil::m_nCurrPage) points to the page after the one thats in memory
    int low = nOldPageNumber - 1;	
    int high = m_pFile->GetFileLength()-2;
    int foundPage = BinarySearch(low, high, literal, nOldPageNumber-1);

    if (foundPage == -1)    // nothing found
    {
        // put file in old state, as binary search might have changed it
        m_pFile->RestoreFileState(OldPage, nOldPageNumber);
        return foundPage;
    }
	else
	{
		if (foundPage > 0)
		{
			// fetch that page
			m_pFile->SetCurrentPage(foundPage);

			// if foundPage is the same as oldPage in memory
			// just return and continue with that page
			if (foundPage == (nOldPageNumber-1))
			{
				m_pFile->RestoreFileState(OldPage, nOldPageNumber);
				return foundPage;
			}

			// otherwise, pages before "foundPage" might also have a matching record
			// So keep going back by one page, till the 1st rec doesn't match
			// and set foundPage = the first page where rec doesn't match
			Record rec;
		    ComparisonEngine compEngine;

			int pageNum = foundPage;
			while (pageNum > 0)
			{
				// if record fetched but it is not less than literal, stop going to prev page
				if (GetNext(rec) && compEngine.Compare(&literal, m_pQueryOrderMaker, &rec, m_pSortInfo->myOrder) > 0)
					break;

				// if 1st rec matches, goto prev page and check with 1st rec again
                pageNum--;
				if (pageNum == (nOldPageNumber-1))
    	            m_pFile->RestoreFileState(OldPage, nOldPageNumber);
				else // fetch that page
					m_pFile->SetCurrentPage(pageNum);
			}
			foundPage = pageNum;
		}

		return foundPage;
	}
}

int Sorted::BinarySearch(int low, int high, Record &literal, int oldCurPageNum)
{
	// if there is only one page, then return that page
	if (low == high)
		return low;

	// error condition, should never reach here
	if (low > high)
		return -1;

	Record rec;
	ComparisonEngine compEngine;
	int mid = (low + high)/2;
	m_pFile->SetCurrentPage(mid); // fetch mid-page in memory

	if (GetNext(rec))
	{
		if (compEngine.Compare(&literal, m_pQueryOrderMaker, &rec, m_pSortInfo->myOrder) == 0)
			return mid;
		// if record is greater than what we need, search in top half
		else if (compEngine.Compare(&literal, m_pQueryOrderMaker, &rec, m_pSortInfo->myOrder) < 0)
		{
			if (low == mid)
		        return mid;
			else
				return BinarySearch(low, mid-1, literal, oldCurPageNum);
		}		
		else // if record is smaller than what we need, search in bottom half
			return BinarySearch(mid+1, high, literal, oldCurPageNum);	
	}
	return -1;
}

// Create <table_name>.meta.data file
// And write total pages used for table loading in it
void Sorted::WriteMetaData()
{
   if (!m_pFile->GetBinFilePath().empty())
   {
        ofstream meta_out;
        meta_out.open(string(m_pFile->GetBinFilePath() + ".meta.data").c_str(), ios::trunc);

		//---- <tbl_name>.meta.data file looks like this ----
		//sorted
		//<runLength>
		//<number of attributes in orderMaker>
		//<attribute index><type>
		//<attribute index><type>
		//....

        meta_out << "sorted\n";
		meta_out << m_pSortInfo->runLength << "\n";
        meta_out << m_pSortInfo->myOrder->ToString();
        meta_out.close();
   }
}

string Sorted :: getusec()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    stringstream ss;
    ss << tv.tv_sec;
    ss << ".";
    ss << tv.tv_usec;
    return ss.str();
}
