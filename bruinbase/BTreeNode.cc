#include "BTreeNode.h"

using namespace std;

//constructor, fills the buffer with 0s
BTLeafNode::BTLeafNode() {
	//0xFF is the empty buffer
	memset(buffer, 0xFF, PageFile::PAGE_SIZE);
}
/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::read(PageId pid, const PageFile& pf) {
	return pf.read(pid, buffer);
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::write(PageId pid, PageFile& pf) {
	return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount() {
	//cout << "getKeyCount start" << endl;
	int count = 0;
	int pair_size = sizeof(RecordId) + sizeof(int);
	int max_count = (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size;
	while(true) {
		//cout << (int)buffer[count * pair_size] << endl;
		if(count + 1 > max_count || (int)buffer[count * pair_size] == -1) {
			break;
		}
		count++;
	}
	return count;
}

/*
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId& rid) { 
	//cout << "Insert start" << endl;
	int count = getKeyCount();
	int pair_size = sizeof(RecordId) + sizeof(int);
	if(count * pair_size >= PageFile::PAGE_SIZE - sizeof(PageId)) {
		//cout << "Node full" << endl;
		return RC_NODE_FULL;
	}
	// find position, in terms of pairs, that the new key should go in the buffer
	int pos = 0;
	locate(key, pos);
	// to insert into the middle, copy everything after to another buffer, insert, and copy back
	int copy_size = PageFile::PAGE_SIZE - (pos * pair_size) - sizeof(PageId) - pair_size;
	char temp_buffer[copy_size];
	memcpy(temp_buffer, buffer + (pos * pair_size), copy_size);
	// insert rid
	memcpy(buffer + (pos * pair_size), &rid, sizeof(RecordId));
	// insert key
	memcpy(buffer + (pos * pair_size) + sizeof(RecordId), &key, sizeof(int));
	// put rest of buffer back
	memcpy(buffer + ((pos + 1) * pair_size), temp_buffer, copy_size);
	//cout << "Info: " << sizeof(buffer) << " " << sizeof(temp_buffer) << " " << copy_size << " " << pair_size << endl;
	//cout << "Insert end" << endl;
	return 0;
}

/*
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId& rid, 
                              BTLeafNode& sibling, int& siblingKey) {
	cout << "insertAndSplit start" << endl;
	int count = getKeyCount();
	int first_half = (count + 1)/2;
	int pair_size = sizeof(RecordId) + sizeof(int);

	// create buffer that contains buffer and inserted data, sorted correctly
	int size_of_temp = PageFile::PAGE_SIZE + sizeof(RecordId) + sizeof(int);
	char temp_buffer[size_of_temp];
	int pos = 0;
	locate(key, pos);

	// insert first half of buffer
	memcpy(temp_buffer, buffer, pos * pair_size);
	// insert rid and key at the correct position
	memcpy(temp_buffer + (pos * pair_size), &rid, sizeof(RecordId));
	memcpy(temp_buffer + (pos * pair_size) + sizeof(RecordId), &key, sizeof(int));
	// copy over rest of buffer
	memcpy(temp_buffer + ((pos + 1) * pair_size), buffer + pos * pair_size, PageFile::PAGE_SIZE - pos * pair_size);

	// debugging print loop
	for(int i = 0; i < size_of_temp/pair_size; i++) {
		RecordId temp_rid;
		int temp_key;
		memcpy(&temp_rid, temp_buffer + (i * pair_size), sizeof(RecordId));
		memcpy(&temp_key, temp_buffer + (i * pair_size) + sizeof(RecordId), sizeof(int));
		if(temp_key != -1 && temp_rid.pid != -1 && temp_rid.sid != -1) {
			cout << temp_rid.pid << ", " << temp_rid.sid << "; " << temp_key << " at " << (unsigned int)(temp_buffer + (i * pair_size)) << endl;
		}
		else {
			break;
		}
	}
	cout << endl;

	cout << "Copying to sibling" << endl;
	// copy over second half of buffer to sibling
	for(int i = first_half; i < count + 1; i++) {
		RecordId sibling_rid;
		int sibling_key;
		memcpy(&sibling_rid, temp_buffer + (i * pair_size), sizeof(RecordId));
		memcpy(&sibling_key, temp_buffer + (i * pair_size) + sizeof(RecordId), sizeof(int));
		cout << i << "; " << sibling_rid.pid << ", " << sibling_rid.sid << "; " << sibling_key << " at " << (unsigned int)(temp_buffer + (i * pair_size)) << endl;
		sibling.insert(sibling_key, sibling_rid);
	}
	cout << endl;
	// copy over nextNodePtr
	sibling.setNextNodePtr(getNextNodePtr());
	// set second half of current node buffer to be empty again
	memset(buffer + (first_half * pair_size), 0xFF, PageFile::PAGE_SIZE - (first_half * pair_size) - sizeof(PageId));
	// we can't set the nextNodePtr here because we don't have that information, BTreeIndex should handle this
	// set siblingKey to first key in sibling
	memcpy(&siblingKey, sibling.buffer + sizeof(RecordId), sizeof(int));
	return 0; 
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int& eid) { 
	int pos = 0;
	int pair_size = sizeof(RecordId) + sizeof(int);
	int num_keys = getKeyCount();
	while(pos < num_keys) {
		int current_key;
		memcpy(&current_key, buffer + (pos * pair_size) + sizeof(RecordId), sizeof(int));
		if(current_key == searchKey) {
			eid = pos;
			return 0;
		}
		else if(searchKey < current_key) {
			break;
		}
		pos++;
	}
	eid = pos;
	return RC_NO_SUCH_RECORD;
}

/*
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int& key, RecordId& rid) { 
	int pair_size = sizeof(RecordId) + sizeof(int);
	memcpy(&rid, buffer + (eid * pair_size), sizeof(RecordId));
	memcpy(&key, buffer + (eid * pair_size) + sizeof(RecordId), sizeof(int));
	return 0;
}

/*
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr() { 
	PageId next_ptr;
	memcpy(&next_ptr, buffer + PageFile::PAGE_SIZE - sizeof(PageId), sizeof(PageId));
	return next_ptr;
}

/*
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid) {
	if(pid < 0) {
		return RC_INVALID_PID;
	}
	memcpy(buffer + PageFile::PAGE_SIZE - sizeof(PageId), &pid, sizeof(PageId));
	return 0;
}

void BTLeafNode::print_node() {
	cout << "Leaf node" << endl;
	int pair_size = sizeof(RecordId) + sizeof(int);
	int empty_pairs = 0;
	for(int i = 0; i < (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size; i++) {
		RecordId rid;
		int key;
		memcpy(&rid, buffer + (i * pair_size), sizeof(RecordId));
		memcpy(&key, buffer + (i * pair_size) + sizeof(RecordId), sizeof(int));
		if(rid.pid == -1 && rid.sid == -1 && key == -1) {
			empty_pairs++;
		}
		else {
			cout << "Rid: " << rid.pid << ", " << rid.sid << "; key: " << key << " at " << (unsigned int)(buffer + (i * pair_size)) << endl;
		}
	}
	cout << "Empty pairs: " << empty_pairs << endl;
	PageId next_ptr;
	memcpy(&next_ptr, buffer + PageFile::PAGE_SIZE - sizeof(PageId), sizeof(PageId));
	cout << "Next pointer: " << next_ptr << endl;
}

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf)
{ return 0; }
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf)
{ return 0; }

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount()
{ return 0; }


/*
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid)
{ return 0; }

/*
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode& sibling, int& midKey)
{ return 0; }

/*
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid)
{ return 0; }

/*
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2)
{ return 0; }
