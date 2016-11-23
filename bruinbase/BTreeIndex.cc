/*
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */
 
#include "BTreeIndex.h"
#include "BTreeNode.h"

using namespace std;

/*
 * BTreeIndex constructor
 */
BTreeIndex::BTreeIndex()
{
    rootPid = -1;
    treeHeight = 0;
}

/*
 * Open the index file in read or write mode.
 * Under 'w' mode, the index file should be created if it does not exist.
 * @param indexname[IN] the name of the index file
 * @param mode[IN] 'r' for read, 'w' for write
 * @return error code. 0 if no error
 */
RC BTreeIndex::open(const string& indexname, char mode)
{
	RC ret = pf.open(indexname, mode);
   	if(ret != 0) {
   		cerr << "Error opening" << endl;
   		return ret;
   	}
   	char buffer[PageFile::PAGE_SIZE];
   	ret = pf.read(0, buffer);
   	if(ret != 0) {
   		cerr << "Error reading" << endl;
   		return ret;
   	}
   	memcpy(&rootPid, &buffer[0], sizeof(PageId));
   	memcpy(&treeHeight, &buffer[0] + sizeof(PageId), sizeof(int));
   	return 0;
}

/*
 * Close the index file.
 * @return error code. 0 if no error
 */
RC BTreeIndex::close()
{
	char buffer[PageFile::PAGE_SIZE];
	memset(buffer, 0 , PageFile::PAGE_SIZE);
	memcpy(&buffer, &rootPid, sizeof(PageId));
	memcpy(&buffer + sizeof(PageId), &treeHeight, sizeof(int));
	RC ret = pf.write(0, buffer);
	if(ret != 0) {
		cerr << "Error closing" << endl;
		return ret;
	}

    return pf.close();
}

/*
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId& rid)
{
	if(treeHeight == 0) {
		BTLeafNode root;
		return root.insert(key, rid);
	}
	IndexCursor index;
	RC ret = locate(key, &index);
	BTLeafNode node;
	node.read(index.pid, pf);
	if(node.getKeyCount() < KEYS_PER_NODE - 1) {
		ret = node.insert(key, rid);
		if(ret != 0) {
			return ret;
		}
	}
	else {
		BTLeafNode new_node;
		int new_node_key;
		node.insertAndSplit(key, rid, new_node, new_node_key);
	}


	
	return 0;
}

/*
 * Run the standard B+Tree key search algorithm and identify the
 * leaf node where searchKey may exist. If an index entry with
 * searchKey exists in the leaf node, set IndexCursor to its location
 * (i.e., IndexCursor.pid = PageId of the leaf node, and
 * IndexCursor.eid = the searchKey index entry number.) and return 0.
 * If not, set IndexCursor.pid = PageId of the leaf node and
 * IndexCursor.eid = the index entry immediately after the largest
 * index key that is smaller than searchKey, and return the error
 * code RC_NO_SUCH_RECORD.
 * Using the returned "IndexCursor", you will have to call readForward()
 * to retrieve the actual (key, rid) pair from the index.
 * @param key[IN] the key to find
 * @param cursor[OUT] the cursor pointing to the index entry with
 *                    searchKey or immediately behind the largest key
 *                    smaller than searchKey.
 * @return 0 if searchKey is found. Othewise an error code*/
 
RC BTreeIndex::locate(int searchKey, IndexCursor& cursor)
{
 /* Returns leaf node C and index i such that C.Pi points to first record
* with search key value V */
	/*
Set C = root node
while (C is not a leaf node) begin
Let i = smallest number such that V ≤ C.Ki
if there is no such number i then begin
Let Pm = last non-null pointer in the node
Set C = C.Pm
end
else if (V = C.Ki )
then Set C = C.Pi+1
else C = C.Pi /* V < C.Ki */
	/*
end
/* C is a leaf node *//*
Let i be the least value such that Ki = V
if there is such a value i
then return (C, i)
else return null ;*/ /* No record with key value V exists*/
	return 0;
}

/*
 * Read the (key, rid) pair at the location specified by the index cursor,
 * and move foward the cursor to the next entry.
 * @param cursor[IN/OUT] the cursor pointing to an leaf-node index entry in the b+tree
 * @param key[OUT] the key stored at the index cursor location.
 * @param rid[OUT] the RecordId stored at the index cursor location.
 * @return error code. 0 if no error
 */
RC BTreeIndex::readForward(IndexCursor& cursor, int& key, RecordId& rid)
{
    return 0;
}
