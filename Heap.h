#ifndef HEAP_H
#define HEAP_H

#include "GenericDBFile.h"

class Heap : public GenericDBFile
{
	private:
		FileUtil *m_pFile;		
		void WriteMetaData();

	public:
		Heap();
		~Heap();

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
