#include "SortedDBFile.h"

SortedDBFile::SortedDBFile(): f_Path()                                          
{
        dbFile = new File();
	 db_currPage=0;
	 db_TotalPages=0;
	 dbPage=NULL;
	 db_fileOpen=false;
	  db_DirtyPageExists=false;
}

SortedDBFile::~SortedDBFile()
{
        // deleting the dbFile pointer if exists
        if (dbFile)
        {
                delete dbFile;
                dbFile = NULL;
        }

        // delete dbPage pointer if it exists
        if (dbPage)
        {
                delete dbPage;
                dbPage = NULL;
        }
}

int SortedDBFile::Create(char *f_path)
{
        // saving file path
        f_Path = f_path;
        
        // open a new file. If file with same name already exists, else cleans out
        if (dbFile)
                dbFile->Open(0, f_path);

    dbPage = new Page();
    db_TotalPages = 0;
    db_fileOpen = true;
}

int SortedDBFile::Open(char *fname)
{
       

        // check if file exists
        struct stat fileStat;
        int iStatus;

        iStatus = stat(fname, &fileStat);
        if (iStatus != 0)       
        {
       
        return 0;
        }

        //set file name to current file
        f_Path = fname;
        // open file in append mode, preserving all prev content
        if (dbFile)
        {
                dbFile->Open(1, const_cast<char*>(fname));
        //mode is passed by subclass. Possibilites are - 0 = TRUNCATE, 1= APPEND
        db_TotalPages = dbFile->GetLength() - 2;   //get total number of pages which are in the file
        //as File class returns length 0 if no data is written and at least 2 even if 1 byte is written

        if(!dbPage)
                dbPage = new Page();
        if(db_TotalPages >= 0)
                dbFile->GetPage(dbPage, db_TotalPages);   //fetch last page from file on disk
        else
                db_TotalPages = 0;

                db_fileOpen = true;
        }
        
        return 1;
}
 
// returns 1 if successfully closed the file, 0 otherwise 
int SortedDBFile::Close()
{
    //check if the current file instance has any dirty page,
    //if yes, flush it to disk and close the file.
    WritePageToFile();  //takes care of everything
    dbFile->Close();
        db_fileOpen = false;
    return 1; // If control came here, return success
}

void SortedDBFile::Add (Record &rec, bool bstartFromNewPage)
{

        if (!db_fileOpen)
        {

                exit(0);
        }

    // Consume the record
    Record aRecord;
    aRecord.Consume(&rec);

    /* Logic:
     * Try adding the record to the current page
     * if adding fails, write page to file and create new page
         * mark db_DirtyPageExists = true after adding record to page
     */

    if (bstartFromNewPage)
        {
        WritePageToFile();  //this will write only if dirty page exists
                dbPage->EmptyItOut();
                // we need one extra page
        db_TotalPages++;        
        }

    if (!dbPage->Append(&aRecord)) // current page does not have enough space
    {
                // write current page to file
        // this function will fetch a new page too
        WritePageToFile();
        if (!dbPage->Append(&aRecord))
        {
                
                return;
        }
                else 
                        db_DirtyPageExists = true;
        }
        else
                db_DirtyPageExists = true;
}

void SortedDBFile::MoveFirst ()
{
    // Reset current page and record pointers
   db_currPage = 0;
   dbPage->EmptyItOut();
//cout<<"end of move first"<<endl;
}

// Function to fetch the next record in the file in "fetchme"
// from the page dbPage. It also updates the variabledb_currPage
// Returns 0 on failure
int SortedDBFile::GetNext (Record &fetchme)
{
        

        // write dirty page to file
        // as it might happen that the record we want to fetch now
        // is still in the dirty page which has not been flushed to disk
        WritePageToFile();

        // coming for the first time
        // Page starts with 0, but data is stored from 1st page onwards
        // Refer to File :: GetPage (File.cc line 168)
        if (db_currPage == 0)
        {
                dbFile->GetPage(dbPage, db_currPage++);
        }

        // Try to fetch the first record from current_page
        // This function will delete this record from the page
        int ret = dbPage->GetFirst(&fetchme);
        if (!ret)
        {
                // Check if pages are still left in the file
                // Note: first page in File doesn't store the data
                // So if LengthOfFile() returns 2 pages, data is actually stored in only one page
                if (db_currPage < LengthOfFile() - 1)  
                {                                                                                       
                        // page ran out of records, so empty it and fetch next page
                        dbPage->EmptyItOut();
                        dbFile->GetPage(dbPage, db_currPage++);
                        ret = dbPage->GetFirst(&fetchme);
                        if (!ret) // failed to fetch next record
                        {
                                // check if we have reached the end of file
                                if (db_currPage >= LengthOfFile())
                                {
                                       
                                        return 0;
                                }
                                else
                                {
                                       
                                        return 0; 
                                }
                        }
                }
                else    
					// end of file reached, cannot read more
                        return 0;
        }
        // If Record fetched successfully
        return 1;
}

// Function to fetch the next record in the file in "fetchme"
// from the page dbPage. If the page is exhausted, return failure
int SortedDBFile::GetNext (Record &fetchme, bool searchInCurrentPage)
{
    // write dirty page to file, as it might happen that the record we want to fetch now
    // is still in the dirty page which has not been flushed to disk
    WritePageToFile();

    // coming for the first time, Page starts with 0, but data is stored from 1st page onwards
    // Refer to File :: GetPage (File.cc line 168)
    if (db_currPage == 0)
    {
        dbFile->GetPage(dbPage, db_currPage++);
    }

    // Try to fetch the first record from current_page
    // This function will delete this record from the page
    int ret = dbPage->GetFirst(&fetchme);
    if (!ret)
                return 0;
        else
                return 1;

}


// Writes dirty page to file
void SortedDBFile::WritePageToFile()
{
    if (db_DirtyPageExists)
    {
        dbFile->AddPage(dbPage, db_TotalPages++);
        dbPage->EmptyItOut();
    }
    db_DirtyPageExists = false;
}

void SortedDBFile::FileStatePreserve(Page& pastPage, int pastPageNumber)
{
        char * pageBytes = new char[PAGE_SIZE];
        dbPage->ToBinary(pageBytes);
        pastPage.FromBinary(pageBytes);
        pastPageNumber = db_currPage;
        delete pageBytes;
        pageBytes = NULL;
}

//Restores File State to the given page number
void SortedDBFile::OldFileState(Page& pastPage, int pastPageNumber)
{
        char* pageBytes = new char[PAGE_SIZE];
        pastPage.ToBinary(pageBytes);
        dbPage->FromBinary(pageBytes);
        db_currPage = pastPageNumber;
        delete pageBytes;
        pageBytes = NULL;
}

//sets current page to gven  page number
void SortedDBFile::CurrPageSet(int pNum)
{
        dbFile->GetPage(dbPage, pNum);
        db_currPage = pNum+1;
}
