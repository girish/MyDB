#ifndef SORTED_H
#define SORTED_H

#include <cstdio>
#include <sys/time.h>
#include "GenericDBFile.h"
#include "Pipe.h"
#include "BigQ.h"
#define PIPE_SIZE 100

class BigQ;
struct SortInfo
{
	OrderMaker *myOrder;
	int runLength;
};

class Sorted : public GenericDBFile
{
	private:
		SortInfo *m_pSortInfo;
		bool m_bReadingMode;
		BigQ *m_pBigQ;
		FileUtil *m_pFile;
		Pipe *m_pINPipe, *m_pOUTPipe;
        string m_sMetaSuffix;
		// variables for GetNext(CNF)
		bool m_bPageFetched;
		OrderMaker *m_pQueryOrderMaker;
		bool m_bMatchingPageFound;
		bool m_bQueryOMCreated;

		// Private functions
		void WriteMetaData();
		void MergeBigQToSortedFile();
        string getusec();

		// Functions for GetNext(CNF)
		int LoadMatchingPage(Record&);
		int BinarySearch(int low, int high, Record&, int oldPagenum);	

	public:
		Sorted();
		~Sorted();

		// name = location of the file
		// return value: 1 on success, 0 on failure
		int Create (char *name,  void *startup);

		// This function assumes that the DBFile already exists
		// and has previously been created and then closed.
		int Open (char *name);

		// Closes the file.
		// The return value is a 1 on success and a zero on failure
		int Close ();

		// Bulk loads the DBFile instance from a text file,
		// appending new data to it using the SuckNextRecord function from Record.h
		// loadMe is the name of the data file to bulk load.
		void Load (Schema &mySchema, char *loadMe);

		// Forces the pointer to correspond to the first record in the file
		void MoveFirst();

		// Add new record to the end of the file
		// Note: addMe is consumed by this function and cannot be used again
		void Add (Record &addMe);

		// Fetch next record (relative to p_currPtr) into fetchMe
		int GetNext (Record &fetchMe);

		// Applies CNF and then fetches the next record
		int GetNext (Record &fetchMe, CNF &applyMe, Record &literal);
};

#endif
