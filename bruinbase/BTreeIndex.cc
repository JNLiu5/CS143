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
	memset(buffer, 0xFF, PageFile::PAGE_SIZE);
	memcpy(&buffer, &rootPid, sizeof(PageId));
	memcpy(&buffer + sizeof(PageId), &treeHeight, sizeof(int));
	RC ret = pf.write(0, buffer);
	if(ret != 0) {
		cerr << "Error closing" << endl;
		return ret;
	}

    return pf.close();
}

// recursive function for inserting, returns the PageId to be inserted into the node above, or -1 if no further action needed
PageId BTreeIndex::insert_recursive(int key, const RecordId& rid, PageId pid, int height) {
	int pair_size = sizeof(PageId) + sizeof(int);
	if(height == treeHeight) {
		// base case - we're at a leaf, so insert (and split if needed)
		BTLeafNode node;
		node.read(pid, pf);
		int eid;
		node.locate(key, eid);
		// node needs to split
		if(node.getKeyCount() >= (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size) {
			BTLeafNode sibling;
			PageId sibling_key;
			node.insertAndSplit(key, rid, sibling, sibling_key);
			PageId to_write = pf.endPid();
			sibling.write(to_write, pf);
			node.setNextNodePtr(to_write);
			node.write(pid, pf);
			return to_write;
		}
		// node doesn't need to split
		node.insert(key, rid);
		node.write(pid, pf);
		return -1;
	}
	// traverse the tree to the leaf
	BTNonLeafNode node;
	node.read(pid, pf);
	PageId next_ptr;
	node.locateChildPtr(key, next_ptr);
	PageId to_insert = insert_recursive(key, rid, next_ptr, height + 1);
	if(to_insert == -1) {
		return -1;
	}
	// if needed, insert new key
	// if needed, split and return new PageId
	if(node.getKeyCount() >= (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size) {
		BTNonLeafNode sibling;
		PageId sibling_key;
		node.insertAndSplit(key, to_insert, sibling, sibling_key);
		PageId to_write = pf.endPid();
		node.write(pid, pf);
		sibling.write(to_write, pf);
		// if this is the top level of the tree and needs to be split, make a new root and set all variables accordingly
		if(height == 1) {
			BTNonLeafNode new_root;
			new_root.initializeRoot(pid, sibling_key, to_write);
			PageId new_root_pid = pf.endPid();
			new_root.write(new_root_pid, pf);
			rootPid = new_root_pid;
			treeHeight++;
			return -1;
		}
		return to_write;
	}
	node.insert(key, to_insert);
	node.write(pid, pf);
	return -1;
}

/*
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId& rid) {
	if(treeHeight == 0) {
		BTLeafNode root;
		root.insert(key, rid);
		rootPid = pf.endPid();
		// location cannot be negative, and page 0 is reserved for the variables of the B+ tree itself
		if(rootPid <= 0) {
			rootPid = 1;
		}
		treeHeight = 1;
		return root.write(rootPid, pf);
	}
	else{
		insert_recursive(key, rid, rootPid, 1);
		return 0;
	}
}

RC BTreeIndex::locate_recursive(int searchKey, IndexCursor& cursor, PageId pid, int height) {
	// if at leaf level of tree, search for value
	if(height == treeHeight) {
		PageId node_pid = pid;
		while(true) {
			BTLeafNode node;
			node.read(node_pid, pf);
			int eid;
			RC found = node.locate(searchKey, eid);
			if(found == RC_NO_SUCH_RECORD && eid == node.getKeyCount()) {
				node_pid = node.getNextNodePtr();
			}
			else {
				cursor.pid = node_pid;
				cursor.eid = eid;
				return found;
			}
		}
	}
	// otherwise, recurse to end of tree
	else {
		BTNonLeafNode node;
		node.read(pid, pf);
		PageId to_follow;
		node.locateChildPtr(searchKey, to_follow);
		return locate_recursive(searchKey, cursor, to_follow, height + 1);
	}
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
 
RC BTreeIndex::locate(int searchKey, IndexCursor& cursor) {
	return locate_recursive(searchKey, cursor, rootPid, 1);
}

/*
 * Read the (key, rid) pair at the location specified by the index cursor,
 * and move foward the cursor to the next entry.
 * @param cursor[IN/OUT] the cursor pointing to an leaf-node index entry in the b+tree
 * @param key[OUT] the key stored at the index cursor location.
 * @param rid[OUT] the RecordId stored at the index cursor location.
 * @return error code. 0 if no error
 */
RC BTreeIndex::readForward(IndexCursor& cursor, int& key, RecordId& rid) {
	BTLeafNode node;
	node.read(cursor.pid, pf);
	RC error = node.readEntry(cursor.eid, key, rid);
	if(error != 0) {
		return error;
	}
	if(cursor.eid + 1 < node.getKeyCount()) {
		cursor.eid++;
		return 0;
	}
	else {
		cursor.eid = 0;
		cursor.pid = node.getNextNodePtr();
		if(cursor.pid == 0xFFFFFFFFFFFFFFFF) {
			return RC_END_OF_TREE;
		}
		return 0;
	}
}
