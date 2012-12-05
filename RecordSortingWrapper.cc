#include "RecordSortingWrapper.h"

RecordSortingWrapper::RecordSortingWrapper()
{
    myRec = NULL;
    myComparison = NULL;
}

RecordSortingWrapper::~RecordSortingWrapper()
{
    myRec = NULL;
    myComparison = NULL;

}

RecordSortingWrapper::RecordSortingWrapper (const RecordSortingWrapper& that)
{
    myRec = that.myRec;
    myComparison = that.myComparison;
    runid = that.runid;
}

bool RecordSortingWrapper::operator < (const RecordSortingWrapper that) const
{
    int v = Compare (this,&that);
    if (v < 0)
        return false;
    return true;
}

