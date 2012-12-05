#include "DDL_DML.h"
#include <string.h>

using namespace std;

int DDL_DML::CreateTable(string sTabName, vector<Attribute> & col_atts_vec, 
						  bool bSortedTable, vector<string> * pSortColAttsVec)
{
	// assign values to member variable
	int nNumAtts = col_atts_vec.size();
	Schema * pSchema = NULL;
	DBFile DbFileObj;

	if (check_existing_table(sTabName))
		return RET_TABLE_ALREADY_EXISTS;

	// Write this schema in the catalog file
	FILE * out = fopen ("catalog", "a");
	fprintf (out, "\nBEGIN\n%s\n%s.tbl", sTabName.c_str(), sTabName.c_str());
	// The loop needs to be in the reverse order as the attributes coming from 
	// the parser in "col_atts" are in reverse order.
	//for (int i = nNumAtts-1; i >= 0; i--)
	for (int i = 0; i < nNumAtts; i++)
	{
		fprintf (out, "\n%s ", col_atts_vec[i].name);
		if (col_atts_vec[i].myType == Int)
			fprintf (out, "Int");
		if (col_atts_vec[i].myType == Double)
			fprintf (out, "Double");
		if (col_atts_vec[i].myType == String)
			fprintf (out, "String");
	}
	fprintf (out, "\nEND\n");
	fclose(out);

	// Fetch this schema into schema object
	pSchema = new Schema("catalog", (char*)sTabName.c_str());
	
	// Make binary file path
	string sBinOutput = sTabName + ".bin";

	// Sorted file
	if (bSortedTable)
	{
		int nSortAtts = pSortColAttsVec->size();
        // Make an OrderMaker and store it
        OrderMaker * pOrderMaker = new OrderMaker();
        pOrderMaker->numAtts = nSortAtts;
	
		// Error check: make sure these column names are valid
		for (int i = 0; i < nSortAtts; i++)
		{
			char * sortedCol = (char*)pSortColAttsVec->at(i).c_str();
			int found = 0;
			found = pSchema->Find(sortedCol);
			if (found == -1)
				return RET_CREATE_TABLE_SORTED_COLS_DONOT_MATCH;
			else
			{
				pOrderMaker->whichAtts[i] = found;	// number of this col in the schema
				pOrderMaker->whichTypes[i] = pSchema->FindType(sortedCol);
			}
		}

		SortInfo sort_info_struct;
		sort_info_struct.myOrder = pOrderMaker;
		sort_info_struct.runLength = 50;

		DbFileObj.Create((char*)sBinOutput.c_str(), sorted, (void*)&sort_info_struct);	

		// delete order maker now
		delete pOrderMaker; 
		pOrderMaker = NULL;
	}
	else
	{
		// heap file
		DbFileObj.Create((char*)sBinOutput.c_str(), heap, NULL);
	}
	
	// Close the DB file
	DbFileObj.Close();

	// delete schema object
	delete pSchema;
	pSchema = NULL;

	cout << "\nTable " << sTabName.c_str() << " has been created successfully!\n";
	return RET_SUCCESS;
}

int DDL_DML::LoadTable(string sTabName, string sFileName)
{
	// Find current directory for the raw file
	char tbl_path[256];	// big enough to hold path
	getcwd(tbl_path, 256);
	string sRawFile = string(tbl_path) + "/" + sFileName;

	string sBinFile = sTabName + ".bin";

	// Open the file and then load it
	DBFile DbFileObj;
	int ret = DbFileObj.Open((char*)sBinFile.c_str());
	if (ret == 0)
		return RET_COULDNT_OPEN_FILE_TO_LOAD;

	//Fetch schema and load the file
	Schema file_schema("catalog", (char*)sTabName.c_str());
	DbFileObj.Load(file_schema, (char*)sRawFile.c_str());
	DbFileObj.Close();

	cout << "\nTable " << sTabName.c_str() << " has been loaded and  " 
		 << sBinFile.c_str() << " has been created successfully!\n";
	return RET_SUCCESS;
}


int DDL_DML::DropTable(string sTabName)
{
	// Read catalog file and put everything in a vector
	// But when you see table name as "sTabName", do not push in the vector
	// Then, delete catalog file and write the vector data into it
	// remove (sTabName.bin) file

	ifstream input_file;
    string line, line2;
	vector <string> vec_whole_file;

	input_file.open("catalog");
    if (!input_file)
        return RET_COULDNT_OPEN_CATALOG_FILE;

	// read catalog file line by line
	bool bTableFound = false;
	bool bSkipLines = false;
	while (!input_file.eof())
    {
		getline(input_file, line);
	
		// BEGIN of a table found
		if (bSkipLines == false)
		{
			if (line.compare("BEGIN") == 0)
			{
				getline(input_file, line2);
				// see if we read the start of the table we want to delete
				if (line2.compare(sTabName) == 0)
				{
					bSkipLines = true;
					bTableFound = true;
				}
				else
				{
					vec_whole_file.push_back(line);
					vec_whole_file.push_back(line2);
				}
			}
			else
				vec_whole_file.push_back(line);
		}
		if (bSkipLines == true && line.compare("END") == 0)
				bSkipLines = false;
	}
	input_file.close();

	if (bTableFound == false)
		return RET_TABLE_NOT_IN_DATABASE;

	// delete catalog file now
	remove("catalog");

	// remake catalog file
	ofstream output_file;
	output_file.open("catalog");
	for (int i = 0; i < vec_whole_file.size(); i++)
	{
		output_file << vec_whole_file.at(i).c_str() << endl;
	}
	output_file.close();

	// delete the binary file
	string sBinFile = sTabName + ".bin";
	remove(sBinFile.c_str());
	
	// delete meta.data file
	sBinFile = sBinFile + ".meta.data";
	remove(sBinFile.c_str());

	cout << "\nTable " << sTabName.c_str() << " has been dropped successfully!\n";
	return RET_SUCCESS;
}


bool DDL_DML::check_existing_table(string sTabName)
{
    ifstream input_file;
    string line, line2;

    input_file.open("catalog");
    if (!input_file)
    {
        cout<<"\nCouldn't open catalog file for reading\n";
        return false;
    }

    // read catalog file line by line
    bool bTableFound = false;
    while (!input_file.eof())
    {
        getline(input_file, line);

        // BEGIN of a table found
        if (line.compare("BEGIN") == 0)
        {
            getline(input_file, line2);
            // see if we read the start of the table we want to delete
            if (line2.compare(sTabName) == 0)
                bTableFound = true;
        }
	}
	input_file.close();

	// Table found in the catalog file
	if (bTableFound)
		return true;

	return false;
}


