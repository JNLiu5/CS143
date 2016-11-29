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

  bool hasIDXcond = false; // check if key condition or value range condition
  bool hasNQcond = false; // check to see if not equal value so we don't look at B+ tree
  bool valCond = false; // check to see if valcond so that we don't look at values sometimes

  // bool hasKeyRange = !(minRange ^ -9999) | !(maxRange ^ -9999); // see if has key range
  int minRange = -9999; // min number in range
  int maxRange = -9999; // max number in range
  int keyMustEqual = -9999; // key must equal this
  // bool keyMustNotEqual = false; // key mustn't equal something, never use B+ tree for this


  bool valGE = false; // attr = 2 has GE condition
  bool valLE = false; // attr = 2 has LE condition

  string valMin = ""; // min value of string, depends on GE
  string valMax = ""; // max value of string, depends on LE
  string valMustEqual = ""; // val must equal this


  // only way we don't use B+ tree is if we have to find NOT key or NOT value

  // START SELECT CONDITION LOGIC
  // FIND IF CONDITION VECTOR HAS BAD CONDITIONS (I.E. IMPOSSIBLE)


  int len = cond.size();
  for(int i = 0; i < len; i++)
  {
    if(cond[i].attr == 1)
    {
      int potKeyVal = atoi(cond[i].value);

      switch (cond[i].comp)
      {
        case SelCond::EQ: // also check min and max values conflict
          if(keyMustEqual == -9999) // if "uninitialized"
          {
            keyMustEqual = potKeyVal;
            hasIDXcond = true;
          }
          else if(potKeyVal != keyMustEqual) // if the potential key val is not the keymustequal value we set beforehand, then we have a bad condition
            badConds = true;
          break;
        case SelCond::NE: // check not equal conflict
          if(potKeyVal == keyMustEqual && keyMustEqual != -9999)
          {
            badConds = true;
          }
          else
            hasNQcond = true;
          break;
        case SelCond::LT: // also check min value conflict
          if(maxRange == -9999 || maxRange > potKeyVal)
          {
            maxRange = potKeyVal;
            hasIDXcond = true;
          }
          break;
        case SelCond::GT: // also check max value confilct
          if( minRange == -9999 || potKeyVal > minRange)
          {
            minRange = potKeyVal;
            hasIDXcond = true;
          }
          break;
        case SelCond::LE: // also check min value conflict
          if( maxRange == -9999 || maxRange > potKeyVal + 1)
          {
            maxRange = potKeyVal + 1;
            hasIDXcond = true;
          }
          break;
        case SelCond::GE:
          if( minRange == -9999 || minRange < potKeyVal -1 )
          {
            minRange = potKeyVal - 1;
            hasIDXcond = true;
          }
          break;
      } // end switch
    }
    else if(cond[i].attr == 2)
    {
      valCond = true;

      switch (cond[i].comp)
      {
        case SelCond::EQ:
          if(valMustEqual == "")
          {
            valMustEqual = cond[i].value;
            
          }
          else if(strcmp(valMustEqual.c_str(), cond[i].value)) // if the potential key val is not the keymustequal value we set beforehand, then we have a bad condition
            badConds = true;
          break;
        case SelCond::NE:
          if(strcmp(valMustEqual.c_str(), cond[i].value)==0 && valMustEqual != "")
          {
            badConds = true;
          }
          else
            hasNQcond = true;
          break;
        case SelCond::LT: // also check min value conflict
          if(valMax == "" || strcmp(valMax.c_str(), cond[i].value)>=0)
          {
            valMax = cond[i].value;
            
            valLE = false;
          }
          break;
        case SelCond::GT: // also check min value conflict
          if(valMin == "" || strcmp(valMin.c_str(), cond[i].value)<=0)
          {
            valMin = cond[i].value;
            
            valGE = false;
          }
          break;
        case SelCond::LE: // also check min value conflict
          if(valMax == "" || strcmp(valMax.c_str(), cond[i].value)>=0)
          {
            valMax = cond[i].value;
            
            valLE = true;
          }
          break;
        case SelCond::GE: // also check min value conflict
          if(valMin == "" || strcmp(valMin.c_str(), cond[i].value)<=0)
          {
            valMin = cond[i].value;
            
            valGE = true;
          }
          break;
      } // end switch
    }
  }

  if( (minRange != -9999 && maxRange != -9999 && minRange >= maxRange - 1)
    || ((keyMustEqual != -9999 && minRange != -9999) && (keyMustEqual <= minRange))
    || ((keyMustEqual != -9999 && maxRange != -9999) && (keyMustEqual >= maxRange)) )
  {
    badConds = true;
  }


  if(valMin != "" && valMax != "" && valMustEqual != "")
  {
    if(valLE && valGE)
    {
      if( strcmp(valMin.c_str(), valMax.c_str()) > 0  || strcmp(valMustEqual.c_str(), valMin.c_str()) < 0 || strcmp(valMustEqual.c_str(), valMax.c_str()) > 0)
        badConds = true;
    }
    else if(valLE && !valGE)
    {
      if( strcmp(valMin.c_str(), valMax.c_str()) >= 0  || strcmp(valMustEqual.c_str(), valMin.c_str()) < 0 || strcmp(valMustEqual.c_str(), valMax.c_str()) >= 0)
        badConds = true;
    }
    else if(!valLE && valGE)
    {
      if( strcmp(valMin.c_str(), valMax.c_str()) >= 0  || strcmp(valMustEqual.c_str(), valMin.c_str()) <= 0 || strcmp(valMustEqual.c_str(), valMax.c_str()) > 0)
        badConds = true;
    }
  }
  else if(valMin != "" && valMax != "" && valMustEqual == "")
  {
    if(valLE && valGE)
    {
      if( strcmp(valMin.c_str(), valMax.c_str()) > 0)
        badConds = true;
    }
    else if(valLE && !valGE)
    {
      if( strcmp(valMin.c_str(), valMax.c_str()) >= 0)
        badConds = true;
    }
    else if(!valLE && valGE)
    {
      if( strcmp(valMin.c_str(), valMax.c_str()) >= 0)
        badConds = true;
    }
  }

  else if(valMin != "" && valMax =="" && valMustEqual != "")
  {
    if(valLE && valGE)
    {
      if( strcmp(valMustEqual.c_str(), valMin.c_str()) < 0)
        badConds = true;
    }
    else if(valLE && !valGE)
    {
      if(strcmp(valMustEqual.c_str(), valMin.c_str()) < 0)
        badConds = true;
    }
    else if(!valLE && valGE)
    {
      if( strcmp(valMustEqual.c_str(), valMin.c_str()) <= 0)
        badConds = true;
    }
  }
  else if(valMin == "" && valMax != "" && valMustEqual != "")
  {
    if(valLE && valGE)
    {
      if( strcmp(valMustEqual.c_str(), valMax.c_str()) > 0)
        badConds = true;
    }
    else if(valLE && !valGE)
    {
      if( strcmp(valMustEqual.c_str(), valMax.c_str()) >= 0)
        badConds = true;
    }
    else if(!valLE && valGE)
    {
      if( strcmp(valMustEqual.c_str(), valMax.c_str()) > 0)
        badConds = true;
    }
  }

  if(badConds)
  {
    goto bad_condition_count;
  }

  // END SELECT CONDITION LOGIC

  if(btree.open(table + ".idx", 'r') || (!hasIDXcond && attr != 4)) // no index condition or count(*) because count(*) likes b+ tree when there are key conditions
  {
    // scan the table file from the beginning
  rid.pid = rid.sid = 0;
  
  while (rid < rf.endRid()) {
    // read the tuple
    if ((rc = rf.read(rid, key, value)) < 0) 
    {
      fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
      goto exit_select;
    }

    // check the conditions on the tuple
    for (unsigned i = 0; i < cond.size(); i++) 
    {
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
      switch (cond[i].comp) 
      {
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
    switch (attr) 
    {
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

  }
  else // WE TRAVERSE THE TREEEEEEEEEEEEEEE
  {
    RC error;
    // start scanning table from beginning
    IndexCursor cursor;
  if(keyMustEqual != -9999)
  {
    error = btree.locate(keyMustEqual, cursor);
    if(error != 0 && error != RC_NO_SUCH_RECORD) 
    {
      // //cerr << "Error locating beginning of tree in SqlEngine select" << endl;
      goto exit_select;
    }
  }
  else if(minRange != -9999)
  {
    error = btree.locate(minRange, cursor);
    if(error != 0 && error != RC_NO_SUCH_RECORD) 
    {
      // //cerr << "Error locating beginning of tree in SqlEngine select" << endl;
      goto exit_select;
    }
  }
  else
  {
    error = btree.locate(0x80000000, cursor);
    if(error != 0 && error != RC_NO_SUCH_RECORD) 
    {
      // //cerr << "Error locating beginning of tree in SqlEngine select" << endl;
      goto exit_select;
    }
  }
    // for each tuple:
      // read RecordFile and get key, value
      // check conditions on tuple
        // compute difference between tuple value and condition value
        // check against conditions, break out if any fail
        // if none fail, increment count
        // print tuple
    while(true) {
      next_rid:

      int key;
      RecordId rid;
      error = btree.readForward(cursor, key, rid);
      //cout << "next_rid --- key: " << key << "; rid: " << rid.pid << ", " << rid.sid << endl;
      if(error != 0) {
        // if the error is end of tree, that's a normal return value, otherwise alert the user
        if(error != RC_END_OF_TREE) {
          //cerr << "Error reading forward in SqlEngine select" << endl;
          goto exit_select;
        }
        goto bad_condition_count;
      }

    if(!valCond && attr==4) // no value conditions and count(*) so we don't have to even read anything
    {
      if(minRange != -9999)
      {
        if(key < minRange)
          goto bad_condition_count;
      }
      if(maxRange != -9999)
      {
        if(key > maxRange)
          goto bad_condition_count;
      }
      if(keyMustEqual != -9999 && key != keyMustEqual) // we located the key it must equal using locate.(keyMustEqual, cursor) so once we readforward we should pass this
        goto bad_condition_count;

      count++;
      continue;
    }

      int record_key;
      string record_value;
      error = rf.read(rid, record_key, record_value);
      if(error != 0) {
        //cerr << "Error reading RecordFile in SqlEngine select" << endl;
        goto exit_select;
      }

    // check the conditions on the tuple
      for (unsigned i = 0; i < cond.size(); i++) 
      {
        // when this is true, we may check to see if we can exit the function and bypass the rest of the tree
        bool can_exit = false;
          // compute the difference between the tuple value and the condition value
          switch (cond[i].attr) {
            case 1:
                diff = record_key - atoi(cond[i].value);
                can_exit = true;
              break;
            case 2:
                diff = strcmp(record_value.c_str(), cond[i].value);
                break;
          }

          // skip the tuple if any condition is not met
          // also determine if we should just exit select entirely
          switch (cond[i].comp) {
            case SelCond::EQ:
              if (diff != 0) {
                if(can_exit && record_key > atoi(cond[i].value)) {
                  goto bad_condition_count;             
                }
                goto next_rid;
              }
              break;
            case SelCond::NE:
              if (diff == 0) {
                goto next_rid;
              }
              break;
            case SelCond::GT:
              if (diff <= 0) {
                goto next_rid;
              }
              break;
            case SelCond::LT:
              if (diff >= 0) {
                if(can_exit && record_key >= atoi(cond[i].value)) {
                  goto bad_condition_count;             
                }
                goto next_rid;
              }
              break;
            case SelCond::GE:
              if (diff < 0) {
                goto next_rid;
              }
              break;
            case SelCond::LE:
              if (diff > 0) {
                if(can_exit && key > atoi(cond[i].value)) {
                  goto bad_condition_count;             
                }
                goto next_rid;
              }
              break;
        }
      }
    // the condition is met for the tuple. 
      // increase matching tuple counter
      count++;

      // print the tuple 
      switch (attr) {
          case 1:  // SELECT key
            fprintf(stdout, "%d\n", record_key);
          break;
          case 2:  // SELECT value
            fprintf(stdout, "%s\n", record_value.c_str());
          break;
          case 3:  // SELECT *
            fprintf(stdout, "%d '%s'\n", record_key, record_value.c_str());
          break;
      }
    }
  }  

  bad_condition_count: // if bad condition but still wants count(*) and don't want to exit select
  // print matching tuple count if "select count(*)"
  //cout << "bad_condition_count" << endl;
  if (attr == 4) 
  {
    fprintf(stdout, "%d\n", count);
  }
  rc = 0;

  // close the table file and return
  exit_select:
  //cout << "exit_select" << endl;
  if(!(table + ".idx", 'r') || (!hasIDXcond && attr != 4))
    btree.close();
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
    //cerr << "Error opening RecordFile in SqlEngine load" << endl;
    l_file.close();
    return error;
  }

  BTreeIndex tree;
  if(index) {
    error = tree.open(table + ".idx", 'w');
    if(error != 0) {
      //cerr << "Error opening tree in SqlEngine load: " << error << endl;
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
      //cerr << "Error reading line from loadfile in SqlEngine load" << endl;
      rf.close();
      l_file.close();
      return error;
    }

    RecordId rid;
    error = rf.append(key, value, rid);
    if(error != 0) {
      //cerr << "Error appending line to RecordFile in SqlEngine load" << endl;
      rf.close();
      l_file.close();
      return error;
    }
    if(index) {
      error = tree.insert(key, rid);
      if(error != 0) {
        //cerr << "Error inserting index into tree in SqlEngine load" << endl;
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
      //cerr << "Error closing tree in SqlEngine load" << endl;
      return error;
    }
  }
  error = rf.close();
  if(error != 0) {
    //cerr << "Error closing RecordFile in SqlEngine load" << endl;
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