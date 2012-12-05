#include "GenericDBFile.h"

int GenericDBFile::Create(char *f_path, void *startup)
{
	EventLogger *el = EventLogger::getEventLogger();

	// saving file path (name)
	m_sFilePath = f_path;
        
    // ignore the startup parameter, subclasses can use as per their need

	// open a new file. If file with same name already exists
	// it is wiped clean
	if (m_pFile)
		m_pFile->Open(TRUNCATE, f_path);

	return Close();
}

int GenericDBFile::Open(char *fname)
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

	/* ------- Not Used Currently -----
	// Read m_nTotalPages from metadata file, if meta.data file exists
	string meta_file_name = m_sFilePath + ".meta.data";
	iStatus = stat(meta_file_name.c_str(), &fileStat);
	if (iStatus == 0)
	{
		ifstream meta_in;
		meta_in.open(meta_file_name.c_str(), ifstream::in);
		meta_in >> m_nTotalPages;
		meta_in.close();
	}
	-------------------------------------*/

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
	}
	
	return RET_SUCCESS;
}

// returns 1 if successfully closed the file, 0 otherwise 
int GenericDBFile::Close()
{
    //check if the current file instance has any dirty page,
    //if yes, flush it to disk and close the file.
    WritePageToFile();  //takes care of everything
    m_pFile->Close();

    return 1; // If control came here, return success
}

void GenericDBFile::Add (Record &rec, bool startFromNewPage)
{
    EventLogger *el = EventLogger::getEventLogger();

    // Consume the record
    Record aRecord;
    aRecord.Consume(&rec);

    /* Logic:
     * Try adding the record to the current page
     * if adding fails, write page to file and create new page
 * mark m_bDirtyPageExists = true after adding record to page
     */

    // Writing data in the file for the first time
    if (m_pPage == NULL)
    {
        m_pPage = new Page();
        m_nTotalPages = 0;
    }

    if(startFromNewPage)
    {
        WritePageToFile();  //this will write only if dirty page exists
        m_pPage->EmptyItOut();
        m_nTotalPages++;
    }
    // a page exists in memory, add record to it
    if (m_pPage)
    {
        if (!m_pPage->Append(&aRecord)) // current page does not have enough space
        {
            // write current page to file
            // this function will fetch a new page too
            WritePageToFile();
            if (!m_pPage->Append(&aRecord))
            {
                el->writeLog("GenericDBFile::Add --> Adding record to page failed.\n");
                return;
            }
            else
                m_bDirtyPageExists = true;
        }
        else
            m_bDirtyPageExists = true;
    }   //else part would never occur so we can remove this IF condition
}

void GenericDBFile::Load (Schema &mySchema, char *loadMe)
{
    EventLogger *el = EventLogger::getEventLogger();

    FILE *fileToLoad = fopen(loadMe, "r");
    if (!fileToLoad)
    {
            el->writeLog("Can't open file name :" + string(loadMe));
    }

    //open the dbfile instance
    Open(const_cast<char*>(m_sFilePath.c_str()));

    /* Logic :
     * first read the record from the file using suckNextRecord()
     * then add this record to page using Add() function
     */

    Record aRecord;
    while(aRecord.SuckNextRecord(&mySchema, fileToLoad))
        Add(aRecord);
}

void GenericDBFile::MoveFirst ()
{
    // Reset current page and record pointers
    m_nCurrPage = 0;
    m_pPage->EmptyItOut();
}

// Function to fetch the next record in the file in "fetchme"
// from the page m_pPage. It also updates the variable m_nCurrPage
// Returns 0 on failure
int GenericDBFile::GetNext (Record &fetchme)
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
					el->writeLog(string("GenericDBFile::GetNext --> End of file reached.") +
								string("Error trying to fetch more records\n"));
					return RET_FAILURE;
				}
				else
				{
					el->writeLog(string("GenericDBFile::GetNext --> End of file not reached, ") +
								string("but fetching record from file failed!\n"));
					return RET_FAILURE;
					//TODO : try changing the error code
				}
			}
		}
		else	// end of file reached, cannot read more
			return RET_FAILURE;
	}
	// Record fetched successfully
	return RET_SUCCESS;
}

// Write dirty page to file
void GenericDBFile::WritePageToFile()
{
    if (m_bDirtyPageExists)
    {
        m_pFile->AddPage(m_pPage, m_nTotalPages++);
        m_pPage->EmptyItOut();
        // everytime page count increases, set m_bIsDirtyMetadata to true
        m_bIsDirtyMetadata = true;
    }
    m_bDirtyPageExists = false;
}

/* ------- Not Used Currently -----
// Create <table_name>.meta.data file
// And write total pages used for table loading in it
void GenericDBFile::WriteMetaData()
{
   if (m_bIsDirtyMetadata && !m_sFilePath.empty())
   {
		ofstream meta_out;
		meta_out.open(string(m_sFilePath + ".meta.data").c_str(), ios::trunc);
		meta_out << m_nTotalPages;
		meta_out.close();
		m_bIsDirtyMetadata = false;
   }
}*/
