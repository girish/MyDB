#include "DBFile.h"

DBFile::DBFile() : m_pGenDBFile(NULL)
{}

DBFile::~DBFile()
{
    delete m_pGenDBFile;
    m_pGenDBFile = NULL; 
}

// name = location of the file
// fType = heap, sorted, tree
// return value: 1 on success, 0 on failure
int DBFile::Create (char *name, fType myType, void *startup)
{
    if(myType == heap)
        m_pGenDBFile = new Heap();
    else if(myType == sorted)
        m_pGenDBFile = new Sorted();
    else
        return RET_UNSUPPORTED_FILE_TYPE;

    if(!m_pGenDBFile)
    {
        cout<<"Not enough memory. EXIT."<<endl;
        exit(1);
    }
    return m_pGenDBFile->Create(name, startup);
}

// This function assumes that the DBFile already exists
// and has previously been created and then closed.
int DBFile::Open (char *name)
{
    //read metadata and create appropriate object
    if(!m_pGenDBFile)
    {
        // check if file exists
        struct stat fileStat;
        int iStatus;
        string metadataFileName = name;
        metadataFileName = metadataFileName + ".meta.data";

        iStatus = stat(metadataFileName.c_str(), &fileStat);
        if (iStatus != 0)   // file doesn't exists
        {
            cout << "DBFile::Open: File " << metadataFileName << " does not exist.\n";
            return RET_FILE_NOT_FOUND;
        }

        ifstream ifile;
        ifile.open(metadataFileName.c_str());
        string line;
        while (!ifile.eof())
        {
            getline(ifile, line);
            if (line.compare("heap") == 0)
            {
                m_pGenDBFile = new Heap();
                break;
            }
            else if(line.compare("sorted") == 0)
            {
                m_pGenDBFile = new Sorted();
                break;
            }
        }
    }
    return m_pGenDBFile->Open(name);
}

// Closes the file.
// The return value is a 1 on success and a zero on failure
int DBFile::Close ()
{
    if(m_pGenDBFile)
        return m_pGenDBFile->Close();
    else
        return RET_FAILURE;
}

// Bulk loads the DBFile instance from a text file,
// appending new data to it using the SuckNextRecord function from Record.h
// loadMe is the name of the data file to bulk load.
void DBFile::Load (Schema &mySchema, char *loadMe)
{
    if(!m_pGenDBFile)
        cout<<"Attempted to Load an unopened file (DEBUG)";
    else
        m_pGenDBFile->Load(mySchema, loadMe);
}

// Forces the pointer to correspond to the first record in the file
void DBFile::MoveFirst()
{
    if(!m_pGenDBFile)
        cout<<"Attempted to do MoveFirst in an unopened file (DEBUG)";
    else
        m_pGenDBFile->MoveFirst();
}

// Add new record to the end of the file
// Note: addMe is consumed by this function and cannot be used again
void DBFile::Add (Record &addMe)
{
    if(!m_pGenDBFile)
        cout<<"Attempted to Add in an unopened file (DEBUG)";
    else
        m_pGenDBFile->Add(addMe);
}

// Fetch next record (relative to p_currPtr) into fetchMe
int DBFile::GetNext (Record &fetchMe)
{
    if(!m_pGenDBFile)
        cout<<"Attempted to Get Next from an unopened file (DEBUG)";
    else
        m_pGenDBFile->GetNext(fetchMe);
}

// Applies CNF and then fetches the next record
int DBFile::GetNext (Record &fetchMe, CNF &applyMe, Record &literal)
{
    if(!m_pGenDBFile)
        cout<<"Attempted to Get Next from an unopened file (DEBUG)";
    else
        m_pGenDBFile->GetNext(fetchMe, applyMe, literal);
}

