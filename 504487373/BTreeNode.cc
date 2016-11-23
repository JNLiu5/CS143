#include "BTreeNode.h"

using namespace std;

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
	int pair_size = sizeof(RecordId) + sizeof(int);
	int count = 0;
	char* temp = buffer;
	int i;
	for(i = 0; i < KEYS_PER_NODE * pair_size; i += pair_size) {
		int key;
		memcpy(&key, temp, sizeof(int));
		if(key == 0) {
			break;
		}
		count++;
		temp += pair_size;
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
	PageId next = getNextNodePtr();
	int pair_size = sizeof(RecordId) + sizeof(int);
	int max_pairs = (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size;
	if(getKeyCount() >= max_pairs) {
		return RC_NODE_FULL;
	}

	char* temp = buffer;
	int i;
	//70 keys per node, defined in BTreeNode.h
	for(i = 0; i < KEYS_PER_NODE * pair_size; i += pair_size) {
		int buffer_key;
		memcpy(&buffer_key, temp, sizeof(int));
		if(buffer_key == 0 || key <= buffer_key) {
			break;
		}

		temp += pair_size;
	}
	char* new_buffer = (char*)malloc(PageFile::PAGE_SIZE);
	fill(new_buffer, new_buffer + PageFile::PAGE_SIZE, 0);

	//copy over preceding buffer space, then value, then succeeding buffer space
	memcpy(new_buffer, buffer, i);
	memcpy(new_buffer + i, &key, sizeof(int));
	memcpy(new_buffer + i + sizeof(int), &rid, sizeof(RecordId));
	memcpy(new_buffer + i + pair_size, buffer + i, getKeyCount() * pair_size - i);
	memcpy(new_buffer + PageFile::PAGE_SIZE - sizeof(PageId), &next, sizeof(PageId));

	//put things back
	memcpy(buffer, new_buffer, PageFile::PAGE_SIZE);

	free(new_buffer);

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
	PageId next = getNextNodePtr();
	int pair_size = sizeof(RecordId) + sizeof(int);
	int max_pairs = (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size;

	//check to see if node is full? Not necessary?

	if(sibling.getKeyCount() != 0) {
		return RC_INVALID_ATTRIBUTE;
	}
	fill(sibling.buffer, sibling.buffer + PageFile::PAGE_SIZE, 0);
	int half_keys = (getKeyCount() + 1)/2;
	int halfway= halfway * pair_size;
	memcpy(sibling.buffer, buffer + halfway, PageFile::PAGE_SIZE - sizeof(PageId) - halfway);
	sibling.numKeys = getKeyCount() - half_keys;
	sibling.setNextNodePtr()(getNextNodePtr());

	fill(buffer + half_index, buffer + PageFile::PAGE_SIZE, 0);
	numKeys = half_keys;

	memcpy(&siblingKey, sibling.buffer, sizeof(int));

	//insert into the corresponding node
	if(key >= siblingKey) {
		sibling.insert(key, rid);
	}
	else {
		insert(key, rid);
	}

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
	int pair_size = sizeof(RecordId) + sizeof(int);
	char* temp = buffer;
	int i;
	for(i = 0; i < getKeyCount() * pair_size; i += pair_size) {
		int key;
		memcpy(&key, temp, sizeof(int));
		if(key == searchKey) {
			eid = i/pair_size;
			return 0;
		}
		if(key > searchKey) {
			eid = (i - 1)/pair_size;
			return RC_NO_SUCH_RECORD;
		}
	}
	eid = getKeyCount();
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
	if(eid < 0 || eid >= getKeyCount()) {
		return RC_NO_SUCH_RECORD;
	}
	memcpy(&key, buffer + eid * pair_size, sizeof(int));
	memcpy(&rid, buffer + eid * pair_size + sizeof(int), sizeof(RecordId));
	return 0;
}

/*
 * Return the pid of the next sibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr() {
	PageId pid;
	memcpy(&pid, buffer + PageFile::PAGE_SIZE - sizeof(PageId), sizeof(PageId));
	return pid;
}

/*
 * Set the pid of the next sibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid) {
	if(pid < 0) {
		return RC_INVALID_PID;
	}
	memcpy(buffer + PageFile::PAGE_SIZE - sizeof(PageId), sizeof(PageId));
	return 0;
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
