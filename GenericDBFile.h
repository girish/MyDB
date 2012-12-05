#ifndef GENERIC_DBFILE_H
#define GENERIC_DBFILE_H

#include <string.h>
#include <iostream>
#include <stdlib.h>
#include "Defs.h"
#include "EventLogger.h"
#include "FileUtil.h"

class GenericDBFile
{
    public:
        GenericDBFile() {}
        virtual ~GenericDBFile() {}

        // name = location of the .bin file
        // return value: 1 on success, 0 on failure
        virtual int Create (char *name, void *startup)=0;

        // This function assumes that the GenericDBFile already exists
        // and has previously been created and then closed.
        virtual int Open (char *name)=0;

        // Closes the file. 
        // The return value is a 1 on success and a zero on failure
        virtual int Close ()=0;

        // Forces the pointer to correspond to the first record in the file
        virtual void MoveFirst()=0;

        // Add new record to the end of the file
        // Note: addMe is consumed by this function and cannot be used again
        virtual void Add (Record &addMe)=0;

        // Bulk loads the DBFile instance from a text file,
        // appending new data to it using the SuckNextRecord function from Record.h
        // loadMe is the name of the data file to bulk load.
        virtual void Load (Schema &mySchema, char *loadMe)=0;

        // Fetch next record (relative to p_currPtr) into fetchMe
        virtual int GetNext (Record &fetchMe)=0;

		// Applies CNF and then fetches the next record
        virtual int GetNext (Record &fetchMe, CNF &applyMe, Record &literal)=0;
};


#endif
