/**
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include "Bruinbase.h"
#include "SqlEngine.h"

#include "BTreeIndex.h"

using namespace std;

// external functions and variables for load file and sql command parsing 
extern FILE* sqlin;
int sqlparse(void);


RC SqlEngine::run(FILE* commandline)
{
  fprintf(stdout, "Bruinbase> ");

  // set the command line input and start parsing user input
  sqlin = commandline;
  sqlparse();  // sqlparse() is defined in SqlParser.tab.c generated from
               // SqlParser.y by bison (bison is GNU equivalent of yacc)

  return 0;
}

RC SqlEngine::select(int attr, const string& table, const vector<SelCond>& cond)
{
  RecordFile rf;   // RecordFile containing the table
  RecordId   rid;  // record cursor for table scanning

  BTreeIndex btree;
  IndexCursor cursor;

  RC     rc;
  int    key;     
  string value;
  int    count;
  int    diff;
  count = 0;

  // open the table file
  if ((rc = rf.open(table + ".tbl", 'r')) < 0) {
    fprintf(stderr, "Error: table %s does not exist\n", table.c_str());
    return rc;
  }

  // BOOLEAN/STATE VARIABLES

  bool badConds = false; // if there is a condition that contradicts another, make true

  bool hasKeyCond = false; // check if key condition or value range condition
  bool valCond = false; // check to see if need val condition so we don't read it in index

  // bool hasKeyRange = !(minRange ^ -9999) | !(maxRange ^ -9999); // see if has key range
  int minRange = -9999; // min number in range
  int maxRange = -9999; // max number in range
  int keyMustEqual = -9999; // key must equal this
  int keyMustNotEqual = -9998; // key mustn't equal this, never use B+ tree for this

  bool hasValRange = false; // prob don't need this
  string valMin = ""; //  prob don't need this
  string valMax = ""; // prob don't need this
  string valMustEqual = ""; // prob don't need this


  // START SELECT CONDITION LOGIC, finds and sets up key stuff, 

  int len = cond.size();
  for(int i = 0; i < len; i++)
  {
    int potKeyVal = atoi(cond[i].value);

    if(cond[i].attr = 1)
    {
      switch (cond[i].comp)
      {
        case SelCond::EQ: // also check min and max values conflict
          if(keyMustEqual != -9998)
            keyMustEqual = potKeyVal;
          else if(potKeyVal != keyMustEqual) // if the potential key val is not the keymustequal value we set beforehand, then we have a bad condition
            badConds = true;
          break;
        case SelCond::LT: // also check min value conflict
          if(maxRange == -9999 || maxRange > potKeyVal)
          {
            maxRange = potKeyVal;
          }
          break;
        case SelCond::GT: // also check max value confilct
          if( minRange == -9999 || potKeyVal > minRange)
          {
            minRange = potKeyVal;
          }
          break;
        case SelCond::LE: // also check min value conflict
          if( maxRange == -9999 || maxRange > potKeyVal + 1)
          {
            maxRange = potKeyVal + 1;
          }
          break;
        case SelCond::GE:


      }
    }


  }















  // END SELECT CONDITION LOGIC

  if(tree.open(table + ".idx", 'r') || SOME BOOLEAN STUFF)
  {

  }
  else // WE TRAVERSE THE TREEEEEEEEEEEEEEE
  {

  }




  // scan the table file from the beginning
  rid.pid = rid.sid = 0;
  
  while (rid < rf.endRid()) {
    // read the tuple
    if ((rc = rf.read(rid, key, value)) < 0) {
      fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
      goto exit_select;
    }

    // check the conditions on the tuple
    for (unsigned i = 0; i < cond.size(); i++) {
      // compute the difference between the tuple value and the condition value
      switch (cond[i].attr) 
      {
        case 1:
          diff = key - atoi(cond[i].value);
          break;
        case 2:
          diff = strcmp(value.c_str(), cond[i].value);
          break;
      }

      // skip the tuple if any condition is not met
      switch (cond[i].comp) {
      case SelCond::EQ:
  if (diff != 0) goto next_tuple;
  break;
      case SelCond::NE:
  if (diff == 0) goto next_tuple;
  break;
      case SelCond::GT:
  if (diff <= 0) goto next_tuple;
  break;
      case SelCond::LT:
  if (diff >= 0) goto next_tuple;
  break;
      case SelCond::GE:
  if (diff < 0) goto next_tuple;
  break;
      case SelCond::LE:
  if (diff > 0) goto next_tuple;
  break;
      }
    }

    // the condition is met for the tuple. 
    // increase matching tuple counter
    count++;

    // print the tuple 
    switch (attr) {
    case 1:  // SELECT key
      fprintf(stdout, "%d\n", key);
      break;
    case 2:  // SELECT value
      fprintf(stdout, "%s\n", value.c_str());
      break;
    case 3:  // SELECT *
      fprintf(stdout, "%d '%s'\n", key, value.c_str());
      break;
    }

    // move to the next tuple
    next_tuple:
    ++rid;
  }

  // print matching tuple count if "select count(*)"
  if (attr == 4) {
    fprintf(stdout, "%d\n", count);
  }
  rc = 0;

  // close the table file and return
  exit_select:
  rf.close();
  return rc;
}

RC SqlEngine::load(const string& table, const string& loadfile, bool index)
{
  ifstream l_file;
  l_file.open(loadfile.c_str());
  RC error;
  RecordFile rf;
  error = rf.open(table + ".tbl", 'w');
  if(error != 0) {
    cerr << "Error opening RecordFile in SqlEngine load" << endl;
    l_file.close();
    return error;
  }

  BTreeIndex tree;
  if(index) {
    error = tree.open(table + ".idx", 'w');
    if(error != 0) {
      cerr << "Error opening tree in SqlEngine load" << endl;
      tree.close();
      l_file.close();
      return error;
    }
  }
  string line;
  while(getline(l_file, line)) {
    int key;
    string value;
    error = parseLoadLine(line, key, value);
    if(error != 0) {
      cerr << "Error reading line from loadfile in SqlEngine load" << endl;
      rf.close();
      l_file.close();
      return error;
    }

    RecordId rid;
    error = rf.append(key, value, rid);
    if(error != 0) {
      cerr << "Error appending line to RecordFile in SqlEngine load" << endl;
      rf.close();
      l_file.close();
      return error;
    }
    if(index) {
      error = tree.insert(key, rid);
      if(error != 0) {
        cerr << "Error inserting index into tree in SqlEngine load" << endl;
        tree.close();
        rf.close();
        l_file.close();
        return error;
      }
    }
  }
  if(index) {
    error = tree.close();
    if(error != 0) {
      cerr << "Error closing tree in SqlEngine load" << endl;
      return error;
    }
  }
  error = rf.close();
  if(error != 0) {
    cerr << "Error closing RecordFile in SqlEngine load" << endl;
    return error;
  }
  l_file.close();
  return 0;
}

RC SqlEngine::parseLoadLine(const string& line, int& key, string& value)
{
    const char *s;
    char        c;
    string::size_type loc;
    
    // ignore beginning white spaces
    c = *(s = line.c_str());
    while (c == ' ' || c == '\t') { c = *++s; }

    // get the integer key value
    key = atoi(s);

    // look for comma
    s = strchr(s, ',');
    if (s == NULL) { return RC_INVALID_FILE_FORMAT; }

    // ignore white spaces
    do { c = *++s; } while (c == ' ' || c == '\t');
    
    // if there is nothing left, set the value to empty string
    if (c == 0) { 
        value.erase();
        return 0;
    }

    // is the value field delimited by ' or "?
    if (c == '\'' || c == '"') {
        s++;
    } else {
        c = '\n';
    }

    // get the value string
    value.assign(s);
    loc = value.find(c, 0);
    if (loc != string::npos) { value.erase(loc); }

    return 0;
}