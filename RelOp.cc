#include "RelOp.h"

void SelectFile::Run (DBFile &inFile, Pipe &outPipe, CNF &selOp, Record &literal)
{
    pthread_create(&m_thread, NULL, DoOperation,
        (void*)new Params(&inFile, &outPipe, &selOp, &literal));
}

/*Logic:
 * Keep scanning the file using GetNext(..) and apply CNF (within GetNext(..),
 * insert into output pipe whichever record matches.
 * Shutdown the output Pipe
 */
void* SelectFile::DoOperation(void* p)
{
    Params* param = (Params*)p;
    param->inputFile->MoveFirst();
    Record rec;
#ifdef _RELOP_DEBUG
    int cnt = 0;
#endif
    while(param->inputFile->GetNext(rec, *(param->selectOp), *(param->literalRec)))
    {
#ifdef _RELOP_DEBUG
        cnt++;
#endif
        param->outputPipe->Insert(&rec);
    }
#ifdef _RELOP_DEBUG
    cout<<"SelectFile : inserted " << cnt << " recs in output Pipe"<<endl;
#endif
    param->outputPipe->ShutDown();
    delete param;
    param = NULL;
}

void SelectFile::WaitUntilDone () {
    pthread_join (m_thread, NULL);
}

void SelectFile::Use_n_Pages (int runlen)
{
    /*This method has no impact in this class, probably.
      we have it just because it's pure virtual in
      */
}

//--------------- SelectPipe ------------
void SelectPipe::Run (Pipe &inPipe, Pipe &outPipe, CNF &selOp, Record &literal)
{
    pthread_create(&m_thread, NULL, DoOperation,
        (void*)new Params(&inPipe, &outPipe, &selOp, &literal));
}

/*Logic:
 * Keep scanning the inPipe using GetNext(..) and apply CNF (within GetNext(..),
 * insert into output pipe whichever record matches.
 * Shutdown the output Pipe
 */
void* SelectPipe::DoOperation(void* p)
{
    Params* param = (Params*)p;
    Record rec;
#ifdef _RELOP_DEBUG
    int cnt = 0;
#endif

    ComparisonEngine compEngine;
    while(param->inputPipe->Remove(&rec))
    {
#ifdef _RELOP_DEBUG
        cnt++;
#endif
        if (compEngine.Compare(&rec, (param->literalRec), (param->selectOp)) )
            param->outputPipe->Insert(&rec);
    }

#ifdef _RELOP_DEBUG
    cout<<"SelectPipe : inserted " << cnt << " recs in output Pipe"<<endl;
#endif

    param->outputPipe->ShutDown();
    delete param;
    param = NULL;
}

void SelectPipe::WaitUntilDone () {
    pthread_join (m_thread, NULL);
}

//--------------- Join ------------------

//int Join::m_nRunLen = -1;
int Join::m_nRunLen = 10;

void Join::Run(Pipe& inPipeL, Pipe& inPipeR, Pipe& outPipe, CNF& selOp, Record& literal)
{
    if (m_nRunLen == -1)
    {
        cerr << "\nError! Use_n_Page() must be called and "
            << "pages of memory allowed for operations must be set!\n";
        exit(1);
    }

    pthread_create(&m_thread, NULL, DoOperation, (void*)new Params(&inPipeL, &inPipeR, &outPipe, &selOp, &literal));
}

void* Join::DoOperation(void* p)
{
    Params* param = (Params*)p;
    /*actual logic goes here
     * 1. we have to create BigQL for inPipeL and BigQR for inPipeR
     * 2. Try to Create OrderMakers (omL and omR) for both BigQL and BigQR using CNF (use the GetSortOrder method)
     * 3. If (omL != null && omR != null) then go ahead and merge these 2 BigQs.(after constructing them of course)
     * 4. else - join these bigQs using "block-nested loops join" - block-size?
     * 5. Put the results into outPipe and shut it down once done.
     */
    OrderMaker omL, omR;
    param->selectOp->GetSortOrders(omL, omR);
#ifdef _RELOP_DEBUG
    int recsMerged = 0;
    int lFetchCount = 0;
    int rFetchCount = 0;
    int tryingRecsMerge = 0;
#endif
    if (omL.numAtts == omR.numAtts && omR.numAtts > 0)
    {
        const int pipeSize = 100;
        Pipe outL(pipeSize), outR(pipeSize);
        BigQ bigqL(*(param->inputPipeL), outL, omL, m_nRunLen);
        BigQ bigR(*(param->inputPipeR), outR, omR, m_nRunLen);
        Record leftRec, rightRec;

        /*new logic
         *get all records from left until they don't change (based on omL)
         * get all recs from right until they don't change (based on omR)
         * merge these records in a nested for loop
         */
        vector<Record*> recsFromLeftPipe, recsFromRightPipe;
        Record *newRec = NULL, *prvsRec = NULL;
        ComparisonEngine ce;
        bool left_fetched = false;  //have an extra record fetched from left pipe
        bool right_fetched = false; //have an extra record fetched from right pipe

        bool fetchFromLeft = true, fetchFromRight = true;
        while(fetchFromLeft || fetchFromRight)
        {
            if(fetchFromLeft)
            {
                //also insert the record in last newRec before inserting new ones
                prvsRec = newRec = NULL;
                if(left_fetched)
                {
                    Record* copy = new Record();
                    copy->Copy(&leftRec);
                    recsFromLeftPipe.push_back(copy);
                    newRec = copy;
                    left_fetched = false;
                }

                while(outL.Remove(&leftRec))
                {
#ifdef _RELOP_DEBUG
                    lFetchCount++;
#endif
                    Record* copy = new Record();
                    copy->Copy(&leftRec);
                    prvsRec = newRec;
                    newRec = copy;
                    if((prvsRec == NULL) || (prvsRec && ce.Compare(prvsRec, newRec, &omL) == 0))
                        recsFromLeftPipe.push_back(copy);
                    else
                    {
                        delete copy;
                        copy = NULL;
                        left_fetched = true;  //we still hold 1 already fetched record in leftRec
                        break;
                    }

                }
                if(!left_fetched)
                {
                    fetchFromLeft = false;
                    outL.ShutDown();
                }
            }

            if(fetchFromRight)
            {
                prvsRec = newRec = NULL;
                //also insert the record in last newRec before inserting new ones
                if(right_fetched)
                {
                    Record* copy = new Record();
                    copy->Copy(&rightRec);
                    recsFromRightPipe.push_back(copy);
                    newRec = copy;
                    right_fetched = false;
                }
                while (outR.Remove(&rightRec))
                {
#ifdef _RELOP_DEBUG
                    rFetchCount++;
#endif
                    Record* copy = new Record();
                    copy->Copy(&rightRec);
                    prvsRec = newRec;
                    newRec = copy;
                    if((prvsRec == NULL) || (prvsRec && ce.Compare(prvsRec, newRec, &omR) == 0))
                        recsFromRightPipe.push_back(copy);
                    else
                    {
                        delete copy;
                        copy = NULL;
                        right_fetched = true;
                        break;
                    }
                }
                if(!right_fetched)
                {
                    fetchFromRight = false;
                    outR.ShutDown();
                }
            }

            if(recsFromLeftPipe.size() > 0 && recsFromRightPipe.size() > 0)
            {
                Record* fromLeftPipe = recsFromLeftPipe.at(0);
                Record* fromRightPipe = recsFromRightPipe.at(0);
                // Logic:
                // <size of record><byte location of att 1><byte location of attribute 2>...<byte location of att n><att 1><att 2>...<att n>
                // num atts in rec = (byte location of att 1)/(sizeof(int)) - 1
                int left_tot = ((int *) fromLeftPipe->bits)[1]/sizeof(int) - 1;
                int right_tot = ((int *) fromRightPipe->bits)[1]/sizeof(int) - 1;
                int numAttsToKeep = left_tot + right_tot;
                int attsToKeep[numAttsToKeep];

                // make attsToKeep - for final merged/joined record
                // <all from left> + <all from right>
                int i;
                for (i = 0; i < left_tot; i++)
                    attsToKeep[i] = i;
                for (int j = 0; j < right_tot; j++)
                    attsToKeep[i++] = j;

                Record joinResult;
                ComparisonEngine ce;
                // see if join attributes match
                // if equal, apply SelOp and join
                // if left < right, flush left pool and fetch next
                // if left > right, flush right pool and fetch next
                int ret = ce.Compare(recsFromLeftPipe.at(0), &omL, recsFromRightPipe.at(0), &omR);
                if (ret == 0)   //if first record from the pool matches, merge all recs in left pool
                {               // with all recs in right pool using nested loops
                    for(int i = 0; i < recsFromLeftPipe.size(); i++)
                    {
                        for(int j = 0; j < recsFromRightPipe.size(); j++)
                        {
#ifdef _RELOP_DEBUG
                            tryingRecsMerge++;
#endif
                            // see if CNF accepts the records
                            if (ce.Compare(recsFromLeftPipe.at(i), recsFromRightPipe.at(j), param->literalRec, param->selectOp) == 1)
                            {
                                Record copyRec;
                                copyRec.Copy(recsFromRightPipe.at(j));
                                // all is good, now merge left+right
                                joinResult.MergeRecords(recsFromLeftPipe.at(i), &copyRec, left_tot, right_tot,
                                    attsToKeep, numAttsToKeep, left_tot);
                                param->outputPipe->Insert(&joinResult);
#ifdef _RELOP_DEBUG
                                recsMerged++;
#endif
                            }
                        }
                    }
                    ClearAndDestroy(recsFromLeftPipe);
                    ClearAndDestroy(recsFromRightPipe);
                    fetchFromLeft = true;
                    fetchFromRight = true;
                }
                // left is smaller than right, clear current left pool and fetch new left pool
                else if (ret < 0)
                {
                    ClearAndDestroy(recsFromLeftPipe);
                    fetchFromLeft = true;
                    fetchFromRight = false;
                }
                else	// left is greater than right, clear current right pool and fetch new right pool
                {
                    ClearAndDestroy(recsFromRightPipe);
                    fetchFromLeft = false;
                    fetchFromRight = true;
                }
            }
        }
#ifdef _RELOP_DEBUG
        cout<<lFetchCount<< " records fetched from left pipe"<<endl;
        cout<<rFetchCount<< " records fetched from right pipe"<<endl;
        cout<<tryingRecsMerge<< " records were tried for merge in join!"<<endl;
        cout<<recsMerged<< " records merged in join!"<<endl;
#endif
    }
    else //-------- block-nested-loop-join -------------
    {
        vector<Record*> left_vec;
        vector<Record*> right_vec;
        Record rec, *copy_rec_left = NULL, *copy_rec_right = NULL;
        int numPagesUsedUp = 0;
        Page currentPage;
        // initialize variables
        bool bJoinSchCreated = false, bRightFileCreated = false, bLeftFileOver = true;
        int left_tot, right_tot, numAttsToKeep;
        int *attsToKeep;

        DBFile rightDBFile;

        /* Logic:
         * Get data from left pipe into N-2 pages
         * (leave 1 page for buffer, while page and vector are both full)
         * And leave 1 page for the data from the right pipe
         */
        do
        {
            bLeftFileOver = true;
            // push the whole LEFT table in left_vec
            while (param->inputPipeL->Remove(&rec))
            {
                copy_rec_left = new Record;
                copy_rec_left->Copy(&rec);
                if (!currentPage.Append(&rec))  // page full
                {
                    numPagesUsedUp++;
                    currentPage.EmptyItOut();
                    // atleast one page needed for right table, and one for buffer
                    if (numPagesUsedUp >= ((m_nRunLen*2)-2))
                    {
                        bLeftFileOver = false;
                        break;
                    }
                    else
                        currentPage.Append(&rec);
                }
                if (bLeftFileOver)
                    left_vec.push_back(copy_rec_left);
                // else, copy_rec_left contains a record
            }

            // set things up for the RIGHT table
            int numPagesAvailable = (m_nRunLen*2) - numPagesUsedUp;
            currentPage.EmptyItOut();

            // If left table fit in-memory, at this point bLeftFileOver will be true
            // so we don't have to write records from right pipe into file
            // But if bLeftFileOver = false, then we need to create rightDBFile
            if (bLeftFileOver == false && bRightFileCreated == false)
            {
                bRightFileCreated = true;
                rightDBFile.Create((char*)"right_file.data", heap, NULL);
                while (param->inputPipeR->Remove(&rec))
                    rightDBFile.Add(rec);
                rightDBFile.Close();
                rightDBFile.Open((char*)"right_file.data");
            }


            // push whatever possible (depending on how much memory is left)
            // from right table into right_vec
            bool bRightFileOver = false;
            do
            {
                Record *copy_rec = NULL;
                bool bCopyRecCanBePushed = true;
                numPagesUsedUp = 0;
                while (param->inputPipeR->Remove(&rec))
                {
                    copy_rec = new Record;
                    copy_rec->Copy(&rec);
                    if (!currentPage.Append(&rec))
                    {
                        numPagesUsedUp++;
                        currentPage.EmptyItOut();
                        // see if any more pages are available. If not, stop fetching from pipe
                        if (numPagesUsedUp >= numPagesAvailable)
                        {
                            //copy_rec contains data, and has not been pushed to vector yet
                            // and can't be put in a new-page/vector, as we are out of memory
                            bCopyRecCanBePushed = false;
                            break;
                        }
                        else
                            currentPage.Append(&rec);
                    }
                    if (bCopyRecCanBePushed)
                        right_vec.push_back(copy_rec);
                }

                // *** Point 1 ***
                // we are out of loop, so either current page limit is over
                // or pipe data is over.
                // if bCopyRecCanBePushed = false --> current page limit exceeded
                //                                    continue after joining this much
                // if bCopyRecCanBePushed = true --> pipe data is over

                // *** Point 2 ***
                // if bRightFileCreated = true, that means we need to fetch
                // records from rightDBFile (only 1 page) and populate right_vec
                if (bRightFileCreated)
                    bRightFileOver = PopulateVec(rightDBFile, right_vec);

                // make join-schema for joint record
                if (!bJoinSchCreated && left_vec.size() > 0 && right_vec.size() > 0)
                {
                    bJoinSchCreated = true;
                    left_tot = ((int *) left_vec.at(0)->bits)[1]/sizeof(int) - 1;
                    right_tot = ((int *) right_vec.at(0)->bits)[1]/sizeof(int) - 1;
                    numAttsToKeep = left_tot + right_tot;

                    attsToKeep = new int(numAttsToKeep);
                    int i;
                    for (i = 0; i < left_tot; i++)
                        attsToKeep[i] = i;
                    for (int j = 0; j < right_tot; j++)
                        attsToKeep[i++] = j;
                }

                Record joinResult;
                ComparisonEngine ce;
                // nested for loop, first left, then right
                for (int i = 0; i < left_vec.size(); i++)
                {
                    for (int j = 0; j < right_vec.size(); j++)
                    {
                        // see if CNF satisfies
                        if (ce.Compare(left_vec.at(i), right_vec.at(j), param->literalRec, param->selectOp) == 1)
                        {
                            Record copyRec;
                            copyRec.Copy(right_vec.at(j));
                            // merge left and right records
                            joinResult.MergeRecords(left_vec.at(i), &copyRec,
                                left_tot, right_tot, attsToKeep, numAttsToKeep, left_tot);
                            param->outputPipe->Insert(&joinResult);
                        }
                    }
                }

                // empty out right_vec
                ClearAndDestroy(right_vec);

                // see if there's more data in the RIGHT pipe
                if (!bRightFileCreated && bCopyRecCanBePushed == false)
                {
                    right_vec.push_back(copy_rec_right);
                    bCopyRecCanBePushed = true;
                }
                else
                    bRightFileOver = true;

            } while (bRightFileOver == false);

            ClearAndDestroy(left_vec);
            ClearAndDestroy(right_vec);

            if (!bLeftFileOver)
                left_vec.push_back(copy_rec_right);

        } while (bLeftFileOver == false);

        delete [] attsToKeep;
        attsToKeep = NULL;

        ClearAndDestroy(left_vec);
        ClearAndDestroy(right_vec);
    }

    // shutdown the output pipe
    param->outputPipe->ShutDown();
    delete param;
    param = NULL;
}

// Populate vector with 1 page worth of data from DBFile
// return true if file is over
bool Join::PopulateVec(DBFile &rightDBFile, vector<Record*> &v)
{
    Page currentPage;
    Record rec;
    bool bPageFull;
    do
    {
        bPageFull = false;
        if (rightDBFile.GetNext(rec))
        {
            v.push_back(&rec); // cheat a little, push one extra than page
            if (!currentPage.Append(&rec))
            {
                currentPage.EmptyItOut();
                bPageFull = true;
            }
        }
        else
            return true;	// indicate file is over
    } while (bPageFull == false);

    return false;			// indicate file is not over
}
void Join::ClearAndDestroy(vector<Record*> &v)
{
    Record *rec;
    for (int i = 0; i < v.size(); i++)
    {
        rec = v.at(i);
        delete rec;
        rec = NULL;
    }
    v.clear();
}

void Join::WaitUntilDone ()
{
    pthread_join (m_thread, NULL);
}

void Join::Use_n_Pages (int runlen)
{
    //runLen should be halved as
    //as 2 BigQs will be constructed
    m_nRunLen = runlen/2;
}

//--------------- Project ------------------
/* Input: inPipe = fetch input records from here
 *	      outPipe = push project output here
 *        keepMe = array containing the attributes to project eg [3,5,2,7]
 *        numAttsInput = Total atts in input rec
 *        numAttsOutput = Atts to keep in output rec
 *                        i.e. length of int * keepMe array
 */
void Project::Run (Pipe &inPipe, Pipe &outPipe, int *keepMe,
    int numAttsInput, int numAttsOutput)
{
    // Create thread to do the project operation
    pthread_create(&m_thread, NULL, &DoOperation,
        (void*) new Params(&inPipe, &outPipe, keepMe, numAttsInput, numAttsOutput));

    return;
}

void * Project::DoOperation(void * p)
{
    Params* param = (Params*)p;
    Record rec;
    // While records are coming from inPipe,
    // modify records and keep only desired attributes
    // and push the modified records into outPipe
    while(param->inputPipe->Remove(&rec))
    {
        // Porject function will modify "rec" itself
        rec.Project(param->pAttsToKeep, param->numAttsToKeep, param->numAttsOriginal);
        // Push this modified rec in outPipe
        param->outputPipe->Insert(&rec);
    }

    //Shut down the outpipe
    param->outputPipe->ShutDown();
    delete param;
    param = NULL;
}

void Project::WaitUntilDone()
{
    // Block until thread is done
    pthread_join(m_thread, 0);
}


//--------------- WriteOut ------------------
/* Input: inPipe = fetch input records from here
 * 		  outFile = File where text version should be written
 *					It has been opened already
 *		  mySchema = the schema to use for writing records
 */
void WriteOut::Run (Pipe &inPipe, FILE *outFile, Schema &mySchema, int *count)
{
    // Create thread to do the project operation
    pthread_create(&m_thread, NULL, &DoOperation,
        (void*) new Params(&inPipe, &mySchema, outFile, count));

    return;
}

void WriteOut::Run (Pipe &inPipe, FILE *outFile, Schema &mySchema)
{
    // Create thread to do the project operation
    pthread_create(&m_thread, NULL, &DoOperation,
        (void*) new Params(&inPipe, &mySchema, outFile));

    return;
}
void * WriteOut::DoOperation(void * p)
{
    Params* param = (Params*)p;
    Record rec;
    int count = 0;
    // While records are coming from inPipe,
    // Write out the attributes in text form in outFile
    while(param->inputPipe->Remove(&rec))
    {
        // Increment the count
        count++;

        // Basically copy the code from Record.cc (Record::Print function)
        int n = param->pSchema->GetNumAtts();
        Attribute *atts = param->pSchema->GetAtts();

        // loop through all of the attributes
        for (int i = 0; i < n; i++)
        {

            // print the attribute name
            fprintf(param->pFILE, "%s: ", atts[i].name);

            // use the i^th slot at the head of the record to get the
            // offset to the correct attribute in the record
            int pointer = ((int *) rec.bits)[i + 1];

            // here we determine the type, which given in the schema;
            // depending on the type we then print out the contents
            fprintf(param->pFILE, "[");

            // first is integer
            if (atts[i].myType == Int)
            {
                int *myInt = (int *) &(rec.bits[pointer]);
                fprintf(param->pFILE, "%d", *myInt);
                // then is a double
            }
            else if (atts[i].myType == Double)
            {
                double *myDouble = (double *) &(rec.bits[pointer]);
                fprintf(param->pFILE, "%f", *myDouble);
                // then is a character string
            }
            else if (atts[i].myType == String)
            {
                char *myString = (char *) &(rec.bits[pointer]);
                fprintf(param->pFILE, "%s", myString);
            }
            fprintf(param->pFILE, "]");

            // print out a comma as needed to make things pretty
            if (i != n - 1)
            {
                fprintf(param->pFILE, ", ");
            }
        }		// end of for loop
        fprintf(param->pFILE, "\n");
    }			// no more records in the inPipe

    param->pCount = &count;

    delete param;
    param = NULL;
}

void WriteOut::WaitUntilDone()
{
    // Block until thread is done
    pthread_join(m_thread, 0);
}


//--------------- Sum ------------------
/* Input: inPipe = fetch input records from here
 *	      outPipe = push project output here
 *		  computeMe = Function using which sum must be computed
 */
void Sum::Run (Pipe &inPipe, Pipe &outPipe, Function &computeMe)
{
    // Create thread to do the project operation
    pthread_create(&m_thread, NULL, &DoOperation,
        (void*) new Params(&inPipe, &outPipe, &computeMe));

    return;
}

void * Sum::DoOperation(void * p)
{
    Params* param = (Params*)p;
    Record rec;
    double sum = 0;
    // While records are coming from inPipe,
    // Use function on them and calculate sum
    while(param->inputPipe->Remove(&rec))
    {
        int ival = 0; double dval = 0;
        param->computeFunc->Apply(rec, ival, dval);
        sum += (ival + dval);
        cout << "insum" << endl;
#ifdef _RELOP_DEBUG
        //			cout << "\nsum = "<< sum;
#endif
    }

    // create temperory schema, with one attribute - sum
    Attribute att = {(char*)"sum", Double};

    Schema sum_schema((char*)"tmp_sum_schema_file", // filename, not used
        1, 					 		// number of attributes
        &att);				 		// attribute pointer

    // Make a file that contains this sum
    FILE * sum_file = fopen("tmp_sum_data_file", "w");
    fprintf(sum_file, "%f|", sum);
    fclose(sum_file);
    sum_file = fopen("tmp_sum_data_file", "r");
    // Make record using the above schema and data file
    rec.SuckNextRecord(&sum_schema, sum_file);
    fclose(sum_file);

    // Push this record to outPipe
    param->outputPipe->Insert(&rec);

    // Shut down the outpipe
    param->outputPipe->ShutDown();

    // delete file "tmp_sum_data_file"
    if(remove("tmp_sum_data_file") != 0)
        perror("\nError in removing tmp_sum_data_file!");

    delete param;
    param = NULL;
}

void Sum::WaitUntilDone()
{
    // Block until thread is done
    pthread_join(m_thread, 0);
}


//--------------- DuplicateRemoval ------------------
/* Input: inPipe = fetch input records from here
 *        outPipe = push project output here
 *        mySchema = Schema of the records coming in inPipe
 */
//int DuplicateRemoval::m_nRunLen = -1;
int DuplicateRemoval::m_nRunLen = 10;

void DuplicateRemoval::Run(Pipe &inPipe, Pipe &outPipe, Schema &mySchema)
{
    if (m_nRunLen == -1)
    {
        cerr << "\nError! Use_n_Page() must be called and "
            << "pages of memory allowed for operations must be set!\n";
        exit(1);
    }
    // Create thread to do the project operation
    pthread_create(&m_thread, NULL, &DoOperation,
        (void*) new Params(&inPipe, &outPipe, &mySchema));
    return;
}

void * DuplicateRemoval::DoOperation(void * p)
{
    Params* param = (Params*)p;
    Record lastSeenRec, currentRec;
    OrderMaker sortOrder;
    int pipeSize = 100;

    // make sort order maker for bigQ
    int n = param->pSchema->GetNumAtts();
    Attribute *atts = param->pSchema->GetAtts();
    // loop through all of the attributes, and list as it is
    sortOrder.numAtts = n;
    for (int i = 0; i < n; i++)
    {
        sortOrder.whichAtts[i] = i;
        sortOrder.whichTypes[i] = atts[i].myType;
    }

    // create local outPipe
    Pipe localOutPipe(pipeSize);
    // start bigQ
    BigQ B(*(param->inputPipe), localOutPipe, sortOrder, m_nRunLen);

    bool bLastSeenRecSet = false;
    ComparisonEngine ce;
    while (localOutPipe.Remove(&currentRec))
    {
        if (bLastSeenRecSet == false)
        {
            lastSeenRec.Copy(&currentRec);
            bLastSeenRecSet = true;
        }

        // compare currentRec with lastSeenRec
        // if same, do nothing
        // if different, send last seem rec to param->outputPipe
        //				 and update lastSeenRec

        if (ce.Compare(&lastSeenRec, &currentRec, &sortOrder) != 0)
        {
            // send the last seen rec out and then update last seen rec
            param->outputPipe->Insert(&lastSeenRec);
            lastSeenRec.Copy(&currentRec);
        }
    }

    // always push last rec to pipe
    param->outputPipe->Insert(&lastSeenRec);

    //Shut down the outpipe
    localOutPipe.ShutDown();
    param->outputPipe->ShutDown();

    delete param;
    param = NULL;
}

void DuplicateRemoval::Use_n_Pages(int n)
{
    // set runLen for bigQ as the number of pages allowed for internal use
    m_nRunLen = n;
}

void DuplicateRemoval::WaitUntilDone()
{
    // Block until thread is done
    pthread_join(m_thread, 0);
}

//--------------- GroupBy ------------------

void GroupBy::Use_n_Pages(int n)
{
    m_nRunLen = n;
}

void GroupBy::Run(Pipe& inPipe, Pipe& outPipe, OrderMaker& groupAtts, Function& computeMe)
{
    pthread_create(&m_thread, NULL, DoOperation, (void*)new Params(&inPipe, &outPipe, &groupAtts, &computeMe, m_nRunLen));
}

void GroupBy::WaitUntilDone()
{
    pthread_join(m_thread, 0);
}

void* GroupBy::DoOperation(void* p)
{
    Params* param = (Params*)p;
    //create a local outputPipe and a BigQ and an feed it with current inputPipe
    const int pipeSize = 100;
    Pipe localOutPipe(pipeSize);
    BigQ localBigQ(*(param->inputPipe), localOutPipe, *(param->groupAttributes), param->runLen);
    Record rec;
    Record *currentGroupRecord = new Record();
    bool currentGroupActive = false;
    ComparisonEngine ce;
    double sum = 0.0;
#ifdef _RELOP_DEBUG
    bool printed = false;
    int recordsInAGroup = 0;
    ofstream log_file("log_file");
    ofstream groupRecordLogFile("groupRecordLogFile");
#endif

    while(localOutPipe.Remove(&rec) || currentGroupActive)
    {
#ifdef _RELOP_DEBUG
        Schema suppSchema("catalog", "supplier");
        if(!printed)
        {
            cout<< "param->groupAttributes->numAtts" << param->groupAttributes->numAtts << endl;
            for (int i = 0; i < param->groupAttributes->numAtts; i++)
                cout<<"param->groupAttributes->whichAtts["<<i<<"] = "<<param->groupAttributes->whichAtts[i]<<endl;
            cout<<"columns in Record = "<<((int*)rec.bits)[1]/sizeof(int) - 1<<endl;
            printed = true;
        }
#endif

        if(!currentGroupActive)
        {
            currentGroupRecord->Copy(&rec);
            currentGroupActive = true;
#ifdef _RELOP_DEBUG
            rec.PrintToFile(&suppSchema, log_file);
            currentGroupRecord->PrintToFile(&suppSchema, groupRecordLogFile);
#endif
        }

        //either no new record fetched (end of pipe) or new group started so just go to else part and finish the last group
        if(rec.bits != NULL && ce.Compare(currentGroupRecord, &rec, param->groupAttributes) == 0)
        {
            int ival = 0; double dval = 0;
            param->computeMeFunction->Apply(rec, ival, dval);
            sum += (ival + dval);
            delete rec.bits;
            rec.bits = NULL;
#ifdef _RELOP_DEBUG
            recordsInAGroup++;
#endif
        }
        else
        {
#ifdef _RELOP_DEBUG
            cout<<"Records in a Group = "<<recordsInAGroup<<", and sum = "<<sum<<endl;
            //recordsInAGroup = 0;
#endif
            //store old sum and group-by attribtues concatenated in outputPipe
            //and also start new group from here

            // create temperory schema, with one attribute - sum
            Attribute att = {(char*)"sum", Double};
            Schema sum_schema((char*)"tmp_sum_schema_file", // filename, not used
                1, // number of attributes
                &att); // attribute pointer

            // Make a file that contains this sum
            string tempSumFileName = "tmp_sum_data_file" + System::getusec();
            FILE * sum_file = fopen(tempSumFileName.c_str(), "w");
            fprintf(sum_file, "%f|", sum);
            fclose(sum_file);
            sum_file = fopen(tempSumFileName.c_str(), "r");
            // Make record using the above schema and data file
            Record sumRec;
            sumRec.SuckNextRecord(&sum_schema, sum_file);
            fclose(sum_file);

#ifdef _RELOP_DEBUG
            //            cout<<"Sum Record : ";
            //            sumRec.Print(&sum_schema);
#endif

            //glue this record with the one we have from groupAttribute - not needed in q6
            int numAttsToKeep = param->groupAttributes->numAtts + 1;
            int attsToKeep[numAttsToKeep];
            attsToKeep[0] = 0;  //for sumRec
            for(int i = 1; i < numAttsToKeep; i++)
            {
                attsToKeep[i] = param->groupAttributes->whichAtts[i-1];
            }
            Record tuple;
            tuple.MergeRecords(&sumRec, currentGroupRecord, 1, numAttsToKeep - 1, attsToKeep,  numAttsToKeep, 1);
            // Push this record to outPipe

            param->outputPipe->Insert(&tuple);

            //initialize sum from last unused record (if any)
            if(rec.bits != NULL)
            {
                int ival = 0; double dval = 0;
                param->computeMeFunction->Apply(rec, ival, dval);
                sum = (ival + dval);    //not += here coz we are re-initializing sum
                delete rec.bits;
                rec.bits = NULL;
            }

            //start new group for this record
            currentGroupActive = false;
            // delete file "tmp_sum_data_file"
            if(remove(tempSumFileName.c_str()) != 0)
                perror("\nError in removing tmp_sum_data_file!");
        }
    }

#ifdef _RELOP_DEBUG
    log_file.flush();
    log_file.close();
    groupRecordLogFile.flush();
    groupRecordLogFile.close();
    cout<<"sum in last group (after finish) = "<< sum<<endl;
    cout<<"recs in last group (after finish) = "<< recordsInAGroup<<endl;
#endif

    // Shut down the outpipe
    param->outputPipe->ShutDown();
    delete param;
    param = NULL;
}
