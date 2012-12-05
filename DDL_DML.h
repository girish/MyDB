/* 

Name: DDL_DML.h
Purpose: Create table, load data into table, drop table

*/

#ifndef DDL_DML_H
#define DDL_DML_H

#include "Schema.h"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include "DBFile.h"

class DDL_DML
{
	bool check_existing_table(string sTabName);
public:
	DDL_DML() {}
	~DDL_DML() {}
	int CreateTable(string sTabName, vector<Attribute> & col_atts_vec, 
					 bool sorted_table = false, vector<string> * sort_col_vec = NULL);
	int LoadTable(string sTabName, string sFileName);
	int DropTable(string sTabName);
};

#endif
