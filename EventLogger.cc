/*
 * ErrorLogger.cc
 *
 *  Created on: Jan 19, 2011
 */

#include "EventLogger.h"
using namespace std;

EventLogger* EventLogger::el = NULL;

EventLogger::EventLogger() : fout("errorlog.txt"){
//	cout<<"***creating EventLogger***\n";
}

EventLogger::~EventLogger()
{
//	cout<<"***Destroying EventLogger Object***\n";
}

void EventLogger::writeLog(const string& msg)
{
	fout<<msg<<endl;
}

EventLogger* EventLogger::getEventLogger()
{
	if(!el)
	{
		static EventLogger s;
		el = &s;
	}
	return el;
}
