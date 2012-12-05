#include "FileUtil.h"

FileUtil::FileUtil(): m_sFilePath(), m_pPage(NULL), m_nTotalPages(0),
   				      m_bDirtyPageExists(false), m_nCurrPage(0),
					  m_bFileIsOpen(false)
{
	m_pFile = new File();
}

FileUtil::~FileUtil()
{
	// delete member File pointer
	if (m_pFile)
	{
		delete m_pFile;
		m_pFile = NULL;
	}

	// delete member Page pointer
	if (m_pPage)
	{
		delete m_pPage;
		m_pPage = NULL;
	}
}

int FileUtil::Create(char *f_path)
{
	// saving file path (name)
	m_sFilePath = f_path;
        
	// open a new file. If file with same name already exists
	// it is wiped clean
	if (m_pFile)
		m_pFile->Open(TRUNCATE, f_path);

    m_pPage = new Page();
    m_nTotalPages = 0;

	m_bFileIsOpen = true;
}

int FileUtil::Open(char *fname)
{
	EventLogger *el = EventLogger::getEventLogger();

	// check if file exists
	struct stat fileStat;
  	int iStatus;

	iStatus = stat(fname, &fileStat);
	if (iStatus != 0) 	// file doesn't exists
	{
        el->writeLog("File " + string(fname) + " does not exist.\n");
        return RET_FILE_NOT_FOUND;
	}

	// TODO for multithreaded env: 
	//		check if file is NOT open already - why ?
	// 		ans - coz we should not access an already open file
	// 		eg - currently open by another thread (?)

        //set file name to current file
        m_sFilePath = fname;
	// open file in append mode, preserving all prev content
	if (m_pFile)
	{
		m_pFile->Open(APPEND, const_cast<char*>(fname));
        //mode is passed by subclass. Possibilites are - 0 = TRUNCATE, 1= APPEND
        m_nTotalPages = m_pFile->GetLength() - 2;   //get total number of pages which are in the file
        //as File class returns length 0 if no data is written and at least 2 even if 1 byte is written

        if(!m_pPage)
        	m_pPage = new Page();
        if(m_nTotalPages >= 0)
        	m_pFile->GetPage(m_pPage, m_nTotalPages);   //fetch last page from file on disk
        else
        	m_nTotalPages = 0;

		m_bFileIsOpen = true;
	}
	
	return RET_SUCCESS;
}

// returns 1 if successfully closed the file, 0 otherwise 
int FileUtil::Close()
{
    //check if the current file instance has any dirty page,
    //if yes, flush it to disk and close the file.
    WritePageToFile();  //takes care of everything
    m_pFile->Close();
	m_bFileIsOpen = false;
    return RET_SUCCESS; // If control came here, return success
}

void FileUtil::Add (Record &rec, bool bstartFromNewPage)
{
    EventLogger *el = EventLogger::getEventLogger();
	if (!m_bFileIsOpen)
	{
		el->writeLog("FileUtil::Add --> File is not open for adding records\n");
		exit(0);
	}

    // Consume the record
    Record aRecord;
    aRecord.Consume(&rec);

    /* Logic:
     * Try adding the record to the current page
     * if adding fails, write page to file and create new page
	 * mark m_bDirtyPageExists = true after adding record to page
     */

    if (bstartFromNewPage)
	{
        WritePageToFile();  //this will write only if dirty page exists
		m_pPage->EmptyItOut();
		// we need one extra page
	        m_nTotalPages++;	
	}

    if (!m_pPage->Append(&aRecord)) // current page does not have enough space
    {
		// write current page to file
        // this function will fetch a new page too
        WritePageToFile();
        if (!m_pPage->Append(&aRecord))
        {
        	el->writeLog("FileUtil::Add --> Adding record to page failed.\n");
                return;
        }
		else 
			m_bDirtyPageExists = true;
	}
	else
	   	m_bDirtyPageExists = true;
}

void FileUtil::MoveFirst ()
{
    // Reset current page and record pointers
    m_nCurrPage = 0;
    m_pPage->EmptyItOut();
}

// Function to fetch the next record in the file in "fetchme"
// from the page m_pPage. It also updates the variable m_nCurrPage
// Returns 0 on failure
int FileUtil::GetNext (Record &fetchme)
{
	EventLogger *el = EventLogger::getEventLogger();

	// write dirty page to file
	// as it might happen that the record we want to fetch now
	// is still in the dirty page which has not been flushed to disk
	WritePageToFile();

	// coming for the first time
	// Page starts with 0, but data is stored from 1st page onwards
	// Refer to File :: GetPage (File.cc line 168)
	if (m_nCurrPage == 0)
	{
		m_pFile->GetPage(m_pPage, m_nCurrPage++);
	}

	// Try to fetch the first record from current_page
	// This function will delete this record from the page
	int ret = m_pPage->GetFirst(&fetchme);
	if (!ret)
	{
		// Check if pages are still left in the file
		// Note: first page in File doesn't store the data
		// So if GetFileLength() returns 2 pages, data is actually stored in only one page
		if (m_nCurrPage < GetFileLength() - 1)	
		{											
			// page ran out of records, so empty it and fetch next page
			m_pPage->EmptyItOut();
			m_pFile->GetPage(m_pPage, m_nCurrPage++);
			ret = m_pPage->GetFirst(&fetchme);
			if (!ret) // failed to fetch next record
			{
				// check if we have reached the end of file
				if (m_nCurrPage >= GetFileLength())
				{
					el->writeLog(string("FileUtil::GetNext --> End of file reached.") +
								string("Error trying to fetch more records\n"));
					return RET_READING_PAST_EOF;
				}
				else
				{
					el->writeLog(string("FileUtil::GetNext --> End of file not reached, ") +
								string("but fetching record from file failed!\n"));
					return RET_FETCHING_REC_FAIL;
				}
			}
		}
		else	// end of file reached, cannot read more
			return RET_FAILURE;
	}
	// Record fetched successfully
	return RET_SUCCESS;
}

// Function to fetch the next record in the file in "fetchme"
// from the page m_pPage. If the page is exhausted, return failure
int FileUtil::GetNext (Record &fetchme, bool searchInCurrentPage)
{
    // write dirty page to file
    // as it might happen that the record we want to fetch now
    // is still in the dirty page which has not been flushed to disk
    WritePageToFile();

    // coming for the first time
    // Page starts with 0, but data is stored from 1st page onwards
    // Refer to File :: GetPage (File.cc line 168)
    if (m_nCurrPage == 0)
    {
        m_pFile->GetPage(m_pPage, m_nCurrPage++);
    }

    // Try to fetch the first record from current_page
    // This function will delete this record from the page
    int ret = m_pPage->GetFirst(&fetchme);
    if (!ret)
		return RET_FAILURE;
	else
		return RET_SUCCESS;

}

// Write dirty page to file
void FileUtil::WritePageToFile()
{
    if (m_bDirtyPageExists)
    {
        m_pFile->AddPage(m_pPage, m_nTotalPages++);
        m_pPage->EmptyItOut();
    }
    m_bDirtyPageExists = false;
}

void FileUtil::SaveFileState(Page& oldPage, int& nOldPageNumber)
{
        char * pageBytes = new char[PAGE_SIZE];
	m_pPage->ToBinary(pageBytes);
	oldPage.FromBinary(pageBytes);
	nOldPageNumber = m_nCurrPage;
	delete pageBytes;
	pageBytes = NULL;
}

void FileUtil::RestoreFileState(Page& oldPage, int nOldPageNumber)
{
	char* pageBytes = new char[PAGE_SIZE];
	oldPage.ToBinary(pageBytes);
	m_pPage->FromBinary(pageBytes);
	m_nCurrPage = nOldPageNumber;
	delete pageBytes;
	pageBytes = NULL;
}

void FileUtil::SetCurrentPage(int pageNum)
{
	m_pFile->GetPage(m_pPage, pageNum);
	m_nCurrPage = pageNum+1;
}
