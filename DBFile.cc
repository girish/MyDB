#include "TwoWayList.h"
#include "Record.h"
#include "Schema.h"
#include "File.h"
#include "Comparison.h"
#include "ComparisonEngine.h"
#include "DBFile.h"
#include "Defs.h"
#include <iostream>
// stub file .. replace it with your own DBFile.cc

DBFile::DBFile() {
    current_page = new Page();
    page_num = 0;
    writing = 0;
}

int DBFile::Create(char *f_path, fType f_type, void *startup) {
    disk_store.Open(0, f_path);
    return 1;
}

void DBFile::Load(Schema &f_schema, char *loadpath) {
    FILE *fp;
    fp = fopen(loadpath, "r");
    Record *r = new Record();
    while (r->SuckNextRecord(&f_schema, fp))
    {
        Add(*r);
        delete r;
        r = new Record();
    }
}

int DBFile::Open(char *f_path) {
    disk_store.Open(1, f_path);
    page_num = disk_store.GetLength()-2;
    current_page->EmptyItOut();
    delete current_page;
    current_page = new Page();
    disk_store.GetPage(current_page, page_num);
    return 1;
}

void DBFile::MoveFirst () {
    if(writing)
    {
        disk_store.AddPage(current_page, page_num);
    }
    if(current_page!= NULL)
    {
        current_page->EmptyItOut();
        delete current_page;
    }
    current_page = new Page();
    disk_store.GetPage(current_page, 0);
    page_num = 0;
}

int DBFile::Close () {
    if(writing)
        disk_store.AddPage(current_page, page_num);
    return disk_store.Close();
}

void DBFile::Add(Record &addMe) {
    writing = 1;
    if(!current_page->Append(&addMe))
    {
        disk_store.AddPage(current_page, page_num);
        page_num++;
        current_page->EmptyItOut();
        delete current_page;
        current_page = new Page();
        if(!current_page->Append(&addMe))
        {
            cout << "Error" << endl;
        }
    }
}

int DBFile::GetNext(Record &fetchMe) {
    writing = 0;
    Record rt;
    if(!current_page->GetFirst(&rt))
    {
        current_page->EmptyItOut();
        delete current_page;
        current_page = new Page();

        if (page_num == disk_store.GetLength()-2)
            return 0;
        page_num++;
        disk_store.GetPage(current_page, page_num);

        if(!current_page->GetFirst(&rt))
        {
            return 0;
        }
    }
    fetchMe.Consume(&rt);

    return 1;
}

int DBFile::GetNext(Record &fetchMe, CNF &cnf, Record &literal) {
    ComparisonEngine comp;
    Record *rt = new Record();
    while (GetNext(*rt))
    {
        if (comp.Compare(rt, &literal, &cnf))
        {
            fetchMe.Consume(rt);
            return 1;
        }
        else
        {
            delete rt;
            rt = new Record();
        }
    }
    return 0;
}

DBFile::~DBFile(){
    current_page->EmptyItOut();
    delete current_page;
}
