#ifndef SORTEDDBFILE_H
#define SORTEDDBFILE_H


#include <string.h>
#include <iostream>
#include <stdlib.h>
#include "Defs.h"
#include "Record.h"
#include "File.h"

class SortedDBFile
{
    private:

		// path of the .bin file
        string f_Path; 

		// .bin file where data will be loaded
        File *dbFile;  
        Page *dbPage;
        int db_TotalPages;
        bool db_DirtyPageExists;
        int db_currPage;
        bool db_fileOpen;

        // Private member functions
        void WritePageToFile();

    public:
        SortedDBFile();
        ~SortedDBFile();

        // name = location of the .bin file
        // return value: 1 on success, 0 on failure
        int Create (char *name);

        // This function assumes that the File already exists
        // and has previously been created and then closed.
        int Open (char *name);

        // Closes the file. 
        // Returns 1 on success and a 0 on failure
        int Close ();

        // Moves the pointer to the first record in the file
        void MoveFirst();

        // Add new record to the end of the file
        // Note: addMe is consumed by this function and cannot be used again
        void Add (Record &addMe, bool startFromNewPage = false);

        // Fetch next record (relative to p_currPtr) into fetchMe
        int GetNext (Record &fetchMe);

        // Returns next record in the current page
        // return failure when page becomjes empty
        int GetNext (Record &fetchme, bool searchInCurrentPage);

        // Given a page number "whichPage", fetch that page
        inline void GetPage(Page *putItHere, off_t whichPage)
        {
                dbFile->GetPage(putItHere, whichPage);
        }

 	void CurrPageSet(int pageNum);
                
       // Returns total pages in the file
        inline int LengthOfFile()
        {
            return dbFile->GetLength();
        }

	        //restores the state of the file
		void OldFileState(Page& pastPage, int pastPageNumber);
        
		//returns the path of the file
        inline string RetrieveBinaryPath()
        {
                return f_Path;
        }

        //saves the state of the file
		void FileStatePreserve(Page& pastPage, int pastPageNumber);


       
};

#endif