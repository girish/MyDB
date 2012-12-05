#ifndef OPTIMIZER_H_
#define OPTIMIZER_H_

#include <map>
#include <utility>
#include <vector>
#include <string>
#include <string.h>
#include <iostream>
#include "ParseTree.h"
#include "EventLogger.h"
#include "Statistics.h"
#include "QueryPlan.h"
#include "Record.h"
#include "Schema.h"

using namespace std;


//#define _DEBUG_OPTIMIZER 1

class Optimizer
{
private:
	// -------- members, coming from yyparse
	struct FuncOperator * m_pFuncOp;
	struct TableList * m_pTblList;
	struct AndList * m_pCNF;
	struct NameList * m_pGroupingAtts; // grouping atts (NULL if no grouping)
	struct NameList * m_pAttsToSelect; // the set of attributes in the SELECT (NULL if no such atts)
	int m_nDistinctAtts; 			   // 1 if there is a DISTINCT in a non-aggregate query 
	int m_nDistinctFunc; 			   // 
	int m_nPrintPlanOnScreen;		   // 1 means print the plan on screen
	string m_sPrintPlanFile;		   // Name of the file where plan should be printed

	// --------- internal members
	int m_nNumTables, m_nGlobalPipeID;
	vector <string> m_vSortedAlias;
	vector <string> m_vTabCombos;
    QueryPlanNode *m_pFinalNode;

	char ** m_aTableNames;
	vector <string> m_vWholeCNF;	// break the AndList into tokens
    struct JoinValue
    {
        Statistics *stats;
        QueryPlanNode *queryPlanNode;
        string joinOrder;
        //Schema schema;
    };
    map <string, JoinValue> m_mJoinEstimate;
	Statistics m_Stats;								// master copy of the stats object
    map <string, string> m_mAliasToTable;           // alias to original table name
	map <int, string> m_mOutPipeToCombo;			// map of outpipe to combo name
    map <string, AndList*> m_mAliasToAndList;       // map of alias and their corresponding AndList
    map <AndList*, bool> m_mAndListToUsage;         // map of AndList* to track if they have been used already or not


	// -------------- Member functions
	Optimizer();
	int SortAlias();
	void TokenizeAndList(AndList*);
	void PopulateTableNames(vector<string> & vec_rels);		// Populate m_aTableNames char** array from vec
	void ComboToVector(string, vector <string> & vec_rels); // break A.B.C into vector(A,B,C)
	void TableComboBaseCase(vector <string> &);				// combo of 2 tables
	int ComboAfterCurrTable(vector<string> &, string);		// find the next combo to join with curr table
	AndList* GetSelectionsFromAndList(string aTableName);
	AndList* GetJoinsFromAndList(vector<string>&);
    void RemoveAliasFromColumnName(AndList* parseTreeNode);
    void RemoveAliasFromColumnName(FuncOperator * func_node);
	void ConcatSchemas(Schema *pRSch, Schema *pLSch, string sName);
	void FindFirstAttInTable(Schema &sch, string &);
    void FindOptimalPairing(vector<string>& vAliases,  AndList* parseTree, pair<string, string> &);
	vector<string> PrintTableCombinations(int combo_len);

	// Functions for debugging
	void PrintTokenizedAndList();	
	void print_map();
	void PrintFuncOpRecur(struct FuncOperator *func_node);
    void PrintOrList(struct OrList *pOr);
    void PrintComparisonOp(struct ComparisonOp *pCom);
    void PrintOperand(struct Operand *pOperand);
    void PrintAndList(struct AndList *pAnd);
    void PrintAndListToUsageMap();
    void PrintAliasToAndListMap();

public:
	Optimizer(Statistics & s,
			  struct FuncOperator *finalFunction,
			  struct TableList *tables,
			  struct AndList * boolean,
			  struct NameList * pGrpAtts,
              struct NameList * pAttsToSelect,
              int distinct_atts, int distinct_func,
			  int print_on_screen, string sOutFileName);
	~Optimizer();

	void PrintFuncOperator();
	void PrintTableList();
	void MakeQueryPlan();
    void ExecuteQuery();
	
};

#endif
