#ifndef RECORDSORTINGWRAPPER_H
#define RECORDSORTINGWRAPPER_H

# include "Record.h"
# include "Comparison.h"
# include "ComparisonEngine.h"
# include <string>
# include <iostream>

class RecordSortingWrapper{
public:
    Record *myRec;
    OrderMaker *myComparison;
    int runid;
    RecordSortingWrapper();
    RecordSortingWrapper (const RecordSortingWrapper&);

    ~RecordSortingWrapper();
    bool operator < (const RecordSortingWrapper that) const;
    static int Compare (const void * a, const void * b)
    {
        RecordSortingWrapper* left = (RecordSortingWrapper*) a;
        RecordSortingWrapper* right = (RecordSortingWrapper*) b;

        ComparisonEngine *ce = new ComparisonEngine ();
        int result = ce->Compare( left->myRec, right->myRec, left->myComparison);

        delete ce;
        return result;
    }
};
#endif
