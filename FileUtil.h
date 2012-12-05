#ifndef FILE_UTIL_H 
#define FILE_UTIL_H 

#include <string.h>
#include <iostream>
#include <stdlib.h>
#include "Defs.h"
#include "Record.h"
#include "File.h"
#include "EventLogger.h"

class FileUtil
{
    private:
        string m_sFilePath; // path of the .bin file
        File *m_pFile;      // .bin file where data will be loaded
        Page *m_pPage;
        int m_nTotalPages;
        bool m_bDirtyPageExists;
        int  m_nCurrPage;
		bool m_bFileIsOpen;

        // Private member functions
        void WritePageToFile();

    public:
        FileUtil();
        ~FileUtil();

        // name = location of the .bin file
        // return value: 1 on success, 0 on failure
        int Create (char *name);

        // This function assumes that the File already exists
        // and has previously been created and then closed.
        int Open (char *name);

        // Closes the file. 
        // The return value is a 1 on success and a zero on failure
        int Close ();

        // Forces the pointer to correspond to the first record in the file
        void MoveFirst();

        // Add new record to the end of the file
        // Note: addMe is consumed by this function and cannot be used again
        void Add (Record &addMe, bool startFromNewPage = false);

        // Fetch next record (relative to p_currPtr) into fetchMe
        int GetNext (Record &fetchMe);

		// Fetch next record only in the current page
		// return failure when page exhausts
		int GetNext (Record &fetchme, bool searchInCurrentPage);

        // Given a page number "whichPage", fetch that page
        inline void GetPage(Page *putItHere, off_t whichPage)
        {
                m_pFile->GetPage(putItHere, whichPage);
        }
		
		// Return total pages in the file
        inline int GetFileLength()
        {
            return m_pFile->GetLength();
        }
		
        inline string GetBinFilePath()
        {
                return m_sFilePath;
        }

        void SaveFileState(Page& oldPage, int& nOldPageNum);

        void RestoreFileState(Page& oldPage, int nOldPageNumber);

        void SetCurrentPage(int pageNum);
};


#endif
