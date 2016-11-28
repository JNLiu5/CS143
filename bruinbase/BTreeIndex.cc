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
   	memcpy(&rootPid, buffer, sizeof(PageId));
   	memcpy(&treeHeight, buffer + sizeof(PageId), sizeof(int));
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
	memcpy(buffer, &rootPid, sizeof(PageId));
	memcpy(buffer + sizeof(PageId), &treeHeight, sizeof(int));
	RC ret = pf.write(0, buffer);
	if(ret != 0) {
		cerr << "Error closing" << endl;
		return ret;
	}

    return pf.close();
}

  /**
   * Recursive function for insert
   * @param key[IN] key to insert
   * @param rid[IN] rid to insert
   * @param pid[IN] pid of node
   * @param height[IN] level of node in tree
   * @param sibling_pid[OUT] the PageId of the newly created sibling, or unchanged if no sibling created
   * @param sibling_key[OUT] the first key of the newly create sibling, or unchanged if no sibling created
  */
RC BTreeIndex::insert_recursive(int key, const RecordId& rid, PageId pid, int height, PageId& sibling_pid, int& sibling_key) {
	int pair_size_leaf = sizeof(RecordId) + sizeof(int);
	int pair_size_non = sizeof(PageId) + sizeof(int);
	RC error;
	if(height == treeHeight) {
		// base case - we're at a leaf, so insert (and split if needed)
		BTLeafNode node;

		error = node.read(pid, pf);
		if(error != 0) {
			cerr << "Error reading leaf node in tree insert" << endl;
			return error;
		}

		// node needs to split
		if(node.getKeyCount() >= (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size_leaf) {
			BTLeafNode sibling;

			error = node.insertAndSplit(key, rid, sibling, sibling_key);
			if(error != 0) {
				cerr << "Error inserting and splitting leaf node in tree insert" << endl;
				return error;
			}

			sibling_pid = pf.endPid();
			if(sibling_pid < 2) {
				// since 0 is BTreeIndex variables and 1 is the first leaf node, we know that the pid must be at least 2 now
				cerr << "Invalid PageId for sibling in tree insert" << endl;
				return RC_INVALID_PID;
			}

			error = sibling.write(sibling_pid, pf);
			if(error != 0) {
				cerr << "Error writing sibling in tree insert" << endl;
				return error;
			}

			error = node.setNextNodePtr(sibling_pid);
			if(error != 0) {
				cerr << "Error in setNextNodePtr in tree insert" << endl;
				return error;
			}

			error = node.write(pid, pf);
			if(error != 0) {
				cerr << "Error writing node in tree insert" << endl;
				return error;
			}

			return 0;
		}
		// node doesn't need to split
		error = node.insert(key, rid);
		if(error != 0) {
			cerr << "Error inserting into node in tree insert" << endl;
			return error;
		}

		error = node.write(pid, pf);
		if(error != 0) {
			cerr << "Error writing node in tree insert" << endl;
			return error;
		}
	    return 0;
	}
	// traverse the tree to the leaf
	BTNonLeafNode node;

	error = node.read(pid, pf);
	if(error != 0) {
		cerr << "Error reading nonleaf node in tree insert" << endl;
		return error;
	}

	PageId next_ptr;
	error = node.locateChildPtr(key, next_ptr);
	if(error != 0) {
		cerr << "Error locating child pointer in tree insert" << endl;
		return error;
	}

	PageId s_pid = -1;
	int s_key = -1;
	error = insert_recursive(key, rid, next_ptr, height + 1, s_pid, s_key);
	if(error != 0) {
		cerr << "Recursive function returned error in tree insert" << endl;
		return error;
	}
	if(s_pid == -1 && s_key == -1) {
		return 0;
	}

	// if needed, insert new key
	// if needed, split and return new PageId
	if(node.getKeyCount() >= (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size_non) {
		BTNonLeafNode sibling;

		error = node.insertAndSplit(key, s_pid, sibling, sibling_key);
		if(error != 0) {
			cerr << "Error inserting and splitting nonleaf node in tree insert" << endl;
			return error;
		}

		sibling_pid = pf.endPid();
		if(sibling_pid < 2) {
			// since 0 is BTreeIndex variables and 1 is the first leaf node, we know that the pid must be at least 2 now
			cerr << "Invalid PageId for sibling in tree insert" << endl;
			return RC_INVALID_PID;
		}

		error = node.write(pid, pf);
		if(error != 0) {
			cerr << "Error writing nonleaf node in tree insert" << endl;
			return error;
		}

		error = sibling.write(sibling_pid, pf);
		if(error != 0) {
			cerr << "Error writing sibling nonleaf node in tree insert" << endl;
			return error;
		}
		return 0;
	}
	error = node.insert(key, s_pid);
	if(error != 0) {
		cerr << "Error inserting into nonleaf node in tree insert" << endl;
		return error;
	}

	error = node.write(pid, pf);
	if(error != 0) {
		cerr << "Error writing nonleaf node in tree insert" << endl;
		return error;
	}
	return 0;
}

/*
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId& rid) {
	// error checking
	if(key < 0) {
		cerr << "Invalid key in tree insert" << endl;
		return RC_INVALID_ATTRIBUTE;
	}
	if(rid.pid < 0 || rid.sid < 0) {
		cerr << "Invalid rid in tree insert" << endl;
		return RC_INVALID_RID;
	}
	RC error;
	// trivial case - no tree, create a new one
	if(treeHeight == 0) {
		BTLeafNode root;
		root.insert(key, rid);
		rootPid = pf.endPid();
		// location cannot be negative, and page 0 is reserved for the variables of the B+ tree itself
		if(rootPid <= 0) {
			rootPid = 1;
		}
		treeHeight = 1;

		error = root.write(rootPid, pf);
		if(error != 0) {
			cerr << "Error writing new tree in tree insert" << endl;
			return error;
		}
		return 0;
	}
	else {
		PageId sibling_pid = -1;
		int sibling_key = -1;

		error = insert_recursive(key, rid, rootPid, 1, sibling_pid, sibling_key);
		if(error != 0) {
			cerr << "Recursive function returned error in tree insert" << endl;
			return error;
		}
		if(sibling_pid != -1 && sibling_key != -1) {
			cerr << "Variables to insert into new root: " << sibling_pid << "; " << sibling_key << endl;
			BTNonLeafNode new_root;
			new_root.initializeRoot(rootPid, sibling_key, sibling_pid);
			rootPid = pf.endPid();
			if(rootPid < 2) {
				// since 0 is BTreeIndex variables and 1 is the first leaf node, we know that the rootPid must be at least 2 now
				cerr << "Incorrect root pid value in tree insert" << endl;
				return RC_INVALID_PID;
			}
			treeHeight++;
			error = new_root.write(rootPid, pf);
			if(error != 0) {
				cerr << "Error writing new root node in tree insert" << endl;
				return error;
			}
			return 0;
		}
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

void BTreeIndex::print_path(int key) {
	cout << "Tree: " << endl;
	if(treeHeight == 0) {
		cout << "Empty tree" << endl;
		return;
	}
	cout << "rootPid: " << rootPid << endl;
	cout << "Height: " << treeHeight << endl;

	int height = 1;
	PageId pid = rootPid;
	while(height != treeHeight) {
		BTNonLeafNode node;
		node.read(pid, pf);
		cout << "PageId: " << pid << endl;
		node.print_node();
		node.locateChildPtr(key, pid);
		height++;
		cout << endl;
	}
	BTLeafNode node;
	node.read(pid, pf);
	cout << "PageId: " << pid << endl;
	node.print_node();
	return;
}