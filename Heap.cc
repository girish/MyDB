#include "Heap.h"

Heap::Heap()
{
	m_pFile = new FileUtil();
}

Heap::~Heap()
{ 
	delete m_pFile;
	m_pFile = NULL;
}

int Heap::Create(char *f_path, void *sortInfo)
{
    //ignore parameter sortInfo - not required for this file type
    m_pFile->Create(f_path);
    WriteMetaData();
	return RET_SUCCESS;
}

int Heap::Open(char *fname)
{
    return m_pFile->Open(fname);
}

// returns 1 if successfully closed the file, 0 otherwise 
int Heap::Close()
{
    return m_pFile->Close();
}

/* Load function bulk loads the Heap instance from a text file, appending
 * new data to it using the SuckNextRecord function from Record.h. The character
 * string passed to Load is the name of the data file to bulk load.
 */
void Heap::Load (Schema &mySchema, char *loadMe)
{
    EventLogger *el = EventLogger::getEventLogger();

    FILE *fileToLoad = fopen(loadMe, "r");
    if (!fileToLoad)
    {
            el->writeLog("Can't open file name :" + string(loadMe));
    }

    /* Logic :
     * first read the record from the file using suckNextRecord()
     * then add this record to page using Add() function
     */

    Record aRecord;
    while(aRecord.SuckNextRecord(&mySchema, fileToLoad))
        Add(aRecord);

	fclose(fileToLoad);
}

void Heap::Add (Record &rec)
{
    m_pFile->Add(rec);
}

void Heap::MoveFirst ()
{
	m_pFile->MoveFirst();
}

// Function to fetch the next record in the file in "fetchme"
// Returns 0 on failure
int Heap::GetNext (Record &fetchme)
{
	m_pFile->GetNext(fetchme);
}


// Function to fetch the next record in "fetchme" that matches 
// the given CNF, returns 0 on failure.
int Heap::GetNext (Record &fetchme, CNF &cnf, Record &literal)
{
	/* logic :
	 * first read the record from the file in "fetchme,
	 * pass it for comparison using given cnf and literal.
	 * if compEngine.compare returns 1, it means fetched record
	 * satisfies CNF expression so we simple return success (=1) here
	 */

	ComparisonEngine compEngine;

	while (GetNext(fetchme))
	{
		if (compEngine.Compare(&fetchme, &literal, &cnf))
			return RET_SUCCESS;
	}

	//if control is here then no matching record was found
	return RET_FAILURE;
}

// Create <table_name>.meta.data file
// And write total pages used for table loading in it
void Heap::WriteMetaData()
{
   if (!m_pFile->GetBinFilePath().empty())
   {
        ofstream meta_out;
        meta_out.open(string(m_pFile->GetBinFilePath() + ".meta.data").c_str(), ios::trunc);
        meta_out << "heap";
        meta_out.close();
   }
}

