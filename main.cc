#include <iostream>
#include "ParseTree.h"
#include "Statistics.h"
#include "QueryPlan.h"
#include "DDL_DML.h"
#include <deque>
#include <vector>

using namespace std;

extern "C" {
    int yyparse(void);   // defined in y.tab.c
}


extern "C" {
    int yyoldparse(void);   // defined in yyold.tab.c from previous assignments.
}
extern "C" struct YY_BUFFER_STATE *yy_scan_string(const char*);
extern "C" struct YY_BUFFER_STATE *yyold_scan_string(const char*);


extern struct FuncOperator *finalFunction;
extern struct TableList *tables;
extern struct AndList *boolean;
extern struct AndList *final;
extern struct NameList *groupingAtts; 	// grouping atts (NULL if no grouping)
extern struct NameList *attsToSelect; 	// the set of attributes in the SELECT (NULL if no such atts)
extern int distinctAtts; 				// 1 if there is a DISTINCT in a non-aggregate query
extern int distinctFunc;  				// 1 if there is a DISTINCT in an aggregate query

extern struct NameList *sortingAtts;   	// sort the file on these attributes (NULL if no grouping atts)
extern struct NameList *table_name;    	// create table name
extern struct NameList *file_name;     	// input file to load into table
extern struct AttsList *col_atts;		// Column name and type in create table
extern int selectFromTable;				// 1 if the SQL is select from table
extern int createTable;    				// 1 if the SQL is create table
extern int sortedTable;    				// 0 = create table as heap, 1 = create table as sorted (use sortingAtts)
extern int insertTable;    				// 1 if the command is Insert into table
extern int dropTable;      				// 1 is the command is Drop table
extern int printPlanOnScreen;  			// 1 if true
extern int executePlan;        			// 1 if true
extern struct NameList *outputFileName; // Name of the file where plan should be printed
map<string,int> cnfMap;

string getOperName(int opcode){
    // these are the types of operands that can appear in a CNF expression
    switch(opcode){
    case 1: return "DOUBLE";
    case 2: return "INT";
    case 4: return "STRING";
    case 5: return " < ";
    case 6: return " > ";
    case 3: return "NAME";
    case 7: return " = ";
    default: return "NOOPFOUND";
    }
}

void printAndList(struct AndList* boolean){
    while(boolean != NULL)
    {
        int code;     //Tells us whether the operator is greater, less than or equal
        OrList *orList;
        orList = boolean->left;
        while(orList != NULL)
        {
            ComparisonOp *compOp;
            compOp = orList->left;

            code = compOp->code;
            Operand *left, *right;
            left = compOp->left;
            right = compOp->right;

            double compOpEst;
            cout << left->value<<"(" << getOperName(left->code)<< ")"<<" "<<getOperName(code)<<" "<<right->value<<"(" << getOperName(left->code)<< ")"<<endl;

            orList = orList->rightOr;
        }
        boolean = boolean->rightAnd;
    }
}

string getJoinCondition(AndList* boolean, Schema& leftSchema, Schema& rightSchema){
    string ss;
    while(boolean != NULL)
    {
        int code;     //Tells us whether the operator is greater, less than or equal
        OrList *orList;
        orList = boolean->left;

        string sk;
        while(orList != NULL)
        {
            string sr;
            ComparisonOp *compOp;
            compOp = orList->left;

            code = compOp->code;
            Operand *left, *right;
            left = compOp->left;
            right = compOp->right;

            double compOpEst;
            if(((leftSchema.Find(left->value) != -1 && rightSchema.Find(right->value) != -1) ||
                (leftSchema.Find(right->value) != -1 && rightSchema.Find(left->value) != -1)) && (code == 7)){
                sr.append("(");
                sr.append(left->value);
                sr.append(getOperName(code));
                sr.append(right->value);
                sr.append(")");
                if(!sk.empty())
                    sk.append(" OR ");
                sk.append(sr);
            }

            orList = orList->rightOr;
        }
        boolean = boolean->rightAnd;
        if (!ss.empty() && !sk.empty())
            ss.append(" AND ");
        if(!sk.empty())
            ss.append(sk);
    }
    return ss;
}

string getSelectCondition(AndList* boolean, Schema& wholeSchema, Schema& selectSchema){
    string ss;
    while(boolean != NULL)
    {
        int code;
        OrList *orList;
        orList = boolean->left;
        string sk;

        while(orList != NULL)
        {
            string sr;

            ComparisonOp *compOp;
            compOp = orList->left;

            code = compOp->code;
            Operand *left, *right;
            left = compOp->left;
            right = compOp->right;

            double compOpEst;
            if(((wholeSchema.Find(left->value) == -1 && selectSchema.Find(right->value) != -1) ||
                (selectSchema.Find(left->value) != -1 && wholeSchema.Find(right->value) == -1))){
                sr.append(left->value);
                sr.append(getOperName(code));
                if(selectSchema.FindType(left->value) == String)
                    sr.append("'");
                sr.append(right->value);
                if(selectSchema.FindType(left->value) == String)
                    sr.append("'");
                if(!sk.empty())
                    sk.append(" OR ");
                sk.append(sr);
            }
            else{
                sk.clear();
                break;
            }


            orList = orList->rightOr;
        }
        boolean = boolean->rightAnd;
        if(cnfMap.find(sk) == cnfMap.end()){
            cnfMap.insert(pair<string, int>(sk, 1));
            if (!ss.empty() && !sk.empty())
                ss.append(" AND ");
            if(!sk.empty())
            {
                ss.append("(");
                ss.append(sk);
                ss.append(")");
            }
        }
    }
    return ss;
}

struct CurrSession
{
    int nOnScreen, nExecute;
    string sFileName;
};

void ReadSession(struct CurrSession & cs);

int main ()
{
    CurrSession cs;
    ReadSession(cs);

    cout << "\n\n----------------------------------------------------------\n"
        << "The database is ready. Please enter a valid SQL statement: \n\n";

    // Get SQL query from the user
    yyparse();

    // --------- SELECT query -------------
    if (selectFromTable == 1)
    {
        const char *fileName = "Statistics.txt";
        //Statistics::PrepareStatisticsFile(fileName);
        CNF cnf;
        //yyparse();
        map<string, int> tablesMap;
        map<string, Schema*> schemaMap;
        deque<QueryPlanNode*> execTree;
        //char *str =" SELECT SUM (l.l_discount) FROM customer AS c, orders AS o, lineitem AS l WHERE (c.c_custkey = o.o_custkey) AND (o.o_orderkey = l.l_orderkey) AND (c.c_name = 'Customer#000070919') AND (l.l_quantity > 30.00) AND (l.l_discount < 0.03)";

        //yy_scan_string(str);

        //yyparse();
        int i=0;
        Schema* wholeSchema = NULL;
        int pipecount = 0;
        for(TableList* iter = tables; iter !=NULL; iter= iter->next, i++){
            //tablesMap.insert(pair<string, int>(iter->tableName, i));
            char prefix[100];
            strcpy(prefix, iter->aliasAs);
            strcat(prefix, ".");
            //string cat("catalog");
            Schema *s = new Schema("catalog",iter->tableName, prefix);
            cout << iter->tableName << endl;
            if (wholeSchema == NULL)
                wholeSchema = new Schema(*s);
            else
                wholeSchema = new Schema(*wholeSchema, *s);
            schemaMap.insert(pair<string, Schema*>(iter->aliasAs, s));
        }

        for(TableList* iter = tables; iter !=NULL; iter= iter->next, i++){
            QueryPlanNode* q;
            Schema *s   = schemaMap[iter->aliasAs];
            string cnf1 = getSelectCondition(boolean, *wholeSchema, *s);
            CNF *pcnf   = new CNF();
            Record *r   = new Record();
            string filename(iter->tableName);
            filename.append(".bin");
            cout << cnf1 << endl;
            if (!cnf1.empty()){
                yyold_scan_string(cnf1.c_str());
                yyoldparse();
                printAndList(final);
                pcnf->GrowFromParseTree(final, s, *r);
            }
            q    = new Node_SelectFile(filename, pipecount++, pcnf, r);
            q->s = s; //important
            execTree.push_back(q);
        }

        while(execTree.size() > 1){
            cout << execTree.size() << endl;
            QueryPlanNode* q, *q1, *q2;
            q1 = execTree.back();
            execTree.pop_back();
            q2 = execTree.back();
            execTree.pop_back();
            string cnf1 = getJoinCondition(boolean, *(q1->s), *(q2->s));
            cout << "Join cnf:"  << cnf1 << endl;
            CNF *pcnf = new CNF();
            pcnf->Print();
            Record *r = new Record();
            if (!cnf1.empty()){
                yyold_scan_string(cnf1.c_str());
                yyoldparse();
                //printAndList(final);
                pcnf->GrowFromParseTree(final, q1->s, q2->s, *r);
            }
            else{
                //join condition not found so it's a cross product
                //don't do it.
                execTree.push_front(q1);
                execTree.push_back(q2);
                continue;
            }
            Schema *s = new Schema(*(q1->s), *(q2->s));
            q         = new Node_Join(q1->m_nOutPipe, q2->m_nOutPipe, pipecount++, pcnf, s, r);
            q->s      = s;
            q->left   = q1;
            q->right  = q2;
            // check for select cnf after join imp for q7.
            string cnf2 = getSelectCondition(boolean, *wholeSchema, *s);
            if (!cnf2.empty()){
                CNF *pcnf2 = new CNF();
                Record *r2 = new Record();
                yyold_scan_string(cnf2.c_str());
                yyoldparse();
                //printAndList(final);
                pcnf2->GrowFromParseTree(final, q->s, *r2);
                QueryPlanNode *qsp = new Node_SelectPipe(q->m_nOutPipe, pipecount++, pcnf2, r2);
                qsp->left          = q;
                qsp->s             = s;
                q                  = qsp;
            }
            cout << cnf2 << endl;
            execTree.push_back(q);
        }
        //wholeSchema->print();
        cout << execTree.size()<<endl;
        QueryPlanNode *q = execTree.back();
        //execTree.top()->PrintNode();

        if (groupingAtts != NULL) { // groupAtts and sum
            Function *func = new Function();
            func->GrowFromParseTree(finalFunction, *(q->s));
            Schema *s    = q->s;
            int attCount = 0;
            for (NameList* iter = groupingAtts; iter != NULL; iter=iter->next)
                attCount++;

            Attribute *atts     = new Attribute[attCount];
            int i               = 0;
            for (NameList* iter = groupingAtts; iter != NULL; iter = iter->next){
                atts[i].name    = strdup(iter->name);
                atts[i].myType  = s->FindType(iter->name);
                i++;
            }
            s = new Schema(attCount, atts);

            OrderMaker *o     = new OrderMaker(s);
            Attribute satts[] = {{"sum", Double}};
            s                 = new Schema(*s, *(new Schema(1, satts)));
            QueryPlanNode *qg = new Node_GroupBy(q->m_nOutPipe, pipecount++, func, o);
            qg->left          = q;
            q                 = qg;
            q->s              = s;
            NameList *sum     = new NameList();
            sum->name         = "sum";
            sum->next         = attsToSelect;
            attsToSelect      = sum;
        }
        if (finalFunction != NULL && (groupingAtts == NULL))
        {
            Function *func    = new Function();
            func->GrowFromParseTree(finalFunction, *(q->s));
            QueryPlanNode *qs = new Node_Sum(q->m_nOutPipe, pipecount++, func, true);
            qs->left          = q;
            q                 = qs;
            Attribute atts[]  = {{"sum", Double}};
            qs->s             = new Schema(1, atts);

        }
        if (attsToSelect != NULL){
            Schema *s    = q->s;
            int attCount = 0;
            for (NameList* iter = attsToSelect; iter != NULL; iter=iter->next)
                attCount++;

            Attribute *atts = new Attribute[attCount];
            int *attsToKeep = new int[attCount];
            int i           = 0;
            for (NameList* iter = attsToSelect; iter != NULL; iter = iter->next){
                atts[i].name   = strdup(iter->name);
                atts[i].myType = s->FindType(iter->name);
                attsToKeep[i]  = s->Find(iter->name);
                i++;
                cout << iter->name << endl;
            }
            //s->print();
            s                    = new Schema(attCount, atts);
            QueryPlanNode *qproj = new Node_Project(q->m_nOutPipe, pipecount++, attsToKeep, attCount, q->s->GetNumAtts(), q->s, false );
            qproj->left          = q;
            qproj->s             = s;
            q                    = qproj;

        }
        if (distinctAtts == 1)
        {
            QueryPlanNode *distFunc = new Node_Distinct(q->m_nOutPipe, pipecount++, q->s, false);
            distFunc->left          = q;
            distFunc->s             = q->s;
            q                       = distFunc;
        }
        if(1) // print on screen
        {
            QueryPlanNode *qwo = new Node_WriteOut(q->m_nOutPipe, "file.out", q->s);
            qwo->s             = q->s;
            qwo->left          = q;
            q                  = qwo;
        }


        q->PrintNode();
        clock_t  start = clock();
        //q->ExecutePostOrder();
        clock_t  end = clock();
        double timetaken = (end-start)/CLOCKS_PER_SEC;
        cout << "Time Taken:" << timetaken << endl;
        return 0;
    }

    // --------- CREATE TABLE query -------------
    else if (createTable == 1)
    {
        DDL_DML ddObj;
        cout << "\nExecuting... Create table statement\n";
        vector <Attribute> ColAttsVec;
        string sTableName;

        // Fetch table name to create
        if (table_name != NULL)
            sTableName = table_name->name;
        else
        {
            cerr << "\nERROR! No table name specified!\n";
            return 1;
        }

        // Fetch column attributes
        AttsList * temp = col_atts;
        while (temp != NULL)
        {
            Attribute Atts;
            Atts.name = temp->name;
            if (temp->code == 1)
                Atts.myType = Double;
            else if (temp->code == 2)
                Atts.myType = Int;
            else if (temp->code == 4)
                Atts.myType = String;

            ColAttsVec.push_back(Atts);

            temp = temp->next;
        }

        // SORTED table
        if (sortedTable == 1)
        {
            cout << "\tCreate table as sorted\n";
            if (sortingAtts == NULL)
            {
                cerr << "\nERROR! Sorted table needs columns on which it is sorted!\n";
                return 1;
            }
            else
            {
                vector <string> sort_cols_vec;
                NameList * temp = sortingAtts;
                while (temp != NULL)
                {
                    sort_cols_vec.push_back(temp->name);
                    temp = temp->next;
                }
                int ret = ddObj.CreateTable(sTableName, ColAttsVec, true, &sort_cols_vec);
                if (ret == RET_TABLE_ALREADY_EXISTS)
                    cerr << "Table " << sTableName.c_str() << " already exists in the database!\n";
                else if (ret == RET_CREATE_TABLE_SORTED_COLS_DONOT_MATCH)
                    cerr << "\nERROR! Sorted column doesn't match table column\n";

            }
        }
        else	// HEAP table
        {
            int ret = ddObj.CreateTable(sTableName, ColAttsVec);
            if (ret == RET_TABLE_ALREADY_EXISTS)
                cerr << "Table " << sTableName.c_str() << " already exists in the database!\n";
        }
    }

    // --------- INSERT INTO TABLE query -------------
    else if (insertTable == 1)
    {
        DDL_DML ddObj;
        cout << "\nExecuting... Insert into table command\n";
        if (table_name== NULL || file_name == NULL)
        {
            cerr << "\nERROR! No table-name or file-name specified to load!\n";
            return 1;
        }
        else
        {
            string sTableName = table_name->name;
            string sFileName = file_name->name;
            int ret = ddObj.LoadTable(sTableName, sFileName);
            if (ret == RET_COULDNT_OPEN_FILE_TO_LOAD)
                cerr << "\nERROR! Could not locate metadata for table " << sTableName.c_str()
                    << endl;
        }
    }

    // --------- DROP TABLE query -------------
    else if (dropTable == 1)
    {
        DDL_DML ddObj;
        cout << "\nExecuting... Drop table command\n";
        if (table_name == NULL)
            cout << "\nTable name is NULL!\n";
        else
        {
            string sTableName = table_name->name;
            int ret = ddObj.DropTable(sTableName);
            if (ret == RET_COULDNT_OPEN_CATALOG_FILE)
                cerr << "\nCouldn't open catalog file for reading\n";
            else if (ret == RET_TABLE_NOT_IN_DATABASE)
                cerr << "\nTable " << sTableName.c_str() << " not found in the database!\n";
        }
    }

    // ----------- session variable --------------
    else
    {
        if (printPlanOnScreen == 0 && executePlan == 0 && outputFileName == NULL)
            return 0;		// Can't have all this, probably syntax error?

        ofstream session;
        session.open("Session.conf");
        session << printPlanOnScreen << endl;
        session << executePlan << endl;
        if (outputFileName != NULL)
            session << outputFileName->name << endl;
        else
            session << "NONE" << endl;
        session.close();
        cout << "\nNew session variables have been stored.\n";
    }

    return 0;
}

void ReadSession(struct CurrSession & cs)
{
    ifstream session;
    session.open("Session.conf");
    if (!session)
        cerr << "\nERROR! Session.conf not found!\n\n";
    else
    {
        int print_on_screen, execute_plan;
        string file_name;
        session >> print_on_screen;
        session >> execute_plan;
        session >> file_name;
        //getline(session, file_name);
        cout << "\n\n-------------------------\n"
            << "Current Session settings:\n"
            << "-------------------------\n";

        if (print_on_screen == 1)
            cout << "Print query plan on screen : TRUE\n";
        else
            cout << "Print query plan on screen : FALSE\n";

        if (execute_plan == 1)
            cout << "Execute query plan : TRUE\n";
        else
            cout << "Execute query plan : FALSE\n";

        cout << "Write query plan in file: " << file_name.c_str() << endl;

        cs.nOnScreen = print_on_screen;
        cs.nExecute = execute_plan;
        cs.sFileName = file_name;

        session.close();
    }
}
