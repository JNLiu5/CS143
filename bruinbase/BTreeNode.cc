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
	// cout << "insertAndSplit start" << endl;
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
			//cout << temp_rid.pid << ", " << temp_rid.sid << "; " << temp_key << " at " << (unsigned int)(temp_buffer + (i * pair_size)) << endl;
		}
		else {
			break;
		}
	}
	//cout << endl;

	//cout << "Copying to sibling" << endl;
	// copy over second half of buffer to sibling
	for(int i = first_half; i < count + 1; i++) {
		RecordId sibling_rid;
		int sibling_key;
		memcpy(&sibling_rid, temp_buffer + (i * pair_size), sizeof(RecordId));
		memcpy(&sibling_key, temp_buffer + (i * pair_size) + sizeof(RecordId), sizeof(int));
		// cout << i << "; " << sibling_rid.pid << ", " << sibling_rid.sid << "; " << sibling_key << " at " << (unsigned int)(temp_buffer + (i * pair_size)) << endl;
		sibling.insert(sibling_key, sibling_rid);
	}
	// cout << endl;
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
	cout << "Next pointer: " << next_ptr << endl << endl;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BTNonLeafNode::BTNonLeafNode()
{
	//0xFF is the empty buffer
	memset(buffer, 0xFF, PageFile::PAGE_SIZE);
}

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf)
{
	return pf.read(pid, buffer);
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf)
{
	return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount()
{
	int count = 0;
	int pair_size = sizeof(PageId) + sizeof(int);
	int max_count = (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size;

	while(true) 
	{
		if(count + 1 > max_count || (int)buffer[count * pair_size + sizeof(PageId)] == -1) 
			break;
		count++;
	}
	return count;

}


/*
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid)
{
	//cout << "Insert start" << endl;
	int key_count = 0;
	int num_keys = getKeyCount();
	int pair_size = sizeof(PageId) + sizeof(int);
	if((num_keys + 1)* pair_size >= PageFile::PAGE_SIZE - sizeof(PageId)) {
		//cout << "Node full" << endl;
		return RC_NODE_FULL;
	}
	// find position, in terms of pairs, that the new key should go in the buffer
	int pos = 0;


	PageId current_pid = -2;

	while(pos < PageFile::PAGE_SIZE - 2*sizeof(PageId) && key_count != num_keys) 
	{
		int current_key;
		memcpy(&current_pid, buffer + pos, sizeof(int));
		if(current_pid == -1)
		{
			break;
		}
		memcpy(&current_key, buffer + pos + sizeof(PageId), sizeof(int));
		if(current_key > key) 
			break;
		pos+= pair_size;
		key_count++;
	}

	// to insert into the middle, copy everything after to another buffer, insert, and copy back
	int copy_size = PageFile::PAGE_SIZE - pos - sizeof(PageId);
	char temp_buffer[copy_size];
	memcpy(temp_buffer, buffer + pos + sizeof(PageId), copy_size);
	// insert key
	memcpy(buffer + pos + sizeof(PageId), &key, sizeof(int));
	// insert pid
	memcpy(buffer + pos + sizeof(PageId) + sizeof(int), &pid, sizeof(PageId));
	
	// put rest of buffer back
	memcpy(buffer + pos + pair_size + sizeof(PageId), temp_buffer, copy_size);
	//cout << "Info: " << sizeof(buffer) << " " << sizeof(temp_buffer) << " " << copy_size << " " << pair_size << endl;
	//cout << "Insert end" << endl;
	return 0;


}

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
{
	cout << "I was called!!!!!!!!!!!!!"  << endl << endl;
	int count = getKeyCount();
	int first_half = (count + 1)/2;
	int half_ind = first_half*8;
	int pair_size = sizeof(PageId) + sizeof(int);
	int max_count = (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size;


	// can have 3 potential medians to give to midKey
	// odd number of keys, so assuming key is added makes 128
	// the 2 potential medians originally in buffer are at positions 500 and 508
	// i.e. half_ind - 12 and half_ind - 4
	// key1 @ 500 and key2@508 key1 > key2
	// the one in the middle of key1, key2, and key will be the midKey
	// this is because only the 63rd and 64th key positions in the current node can potentially be the
	// the 64th position in the "new" configuration
	// obviously good coding practice is to do this in a general way, not hard-coded
	int potKey1, potKey2;

	memcpy(&potKey1, buffer + half_ind - pair_size - sizeof(int), sizeof(int));
	memcpy(&potKey2, buffer + half_ind - sizeof(int), sizeof(int));
	// assume no duplicate value, so don't need any <=
	if(key > potKey1 && key < potKey2) // key is median
	{
		midKey = key;
		// put right half of node in sibling
		memcpy(sibling.buffer+4, buffer + half_ind - sizeof(int), PageFile::PAGE_SIZE - half_ind + sizeof(int));
		memcpy(sibling.buffer, &pid, sizeof(PageId)); // sibling first pid becomes pid of mid
		memset(buffer + half_ind - sizeof(int), 0xFF, half_ind + sizeof(int)); // set rest of currNode as -1
	}
	else if(potKey1 > key) // potKey1 is median
	{
		midKey = potKey1;
		memcpy(sibling.buffer, buffer + half_ind - pair_size, PageFile::PAGE_SIZE - half_ind + pair_size);
		memset(buffer + half_ind - pair_size, 0xFF, PageFile::PAGE_SIZE - half_ind + pair_size);
		insert(key, pid); // insert the pairing into current buffer, could be anywhere

	}
	else // at this point potKey2 has to to be the median, since key > potKey2, also means key goes into first ind of sibling
	{
		midKey = potKey2;
		memcpy(sibling.buffer, buffer + half_ind, PageFile::PAGE_SIZE - half_ind);
		memset(buffer + half_ind, 0xFF, PageFile::PAGE_SIZE - half_ind);
		sibling.insert(key, pid);

	}

	return 0;
}

/*
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid)
{
	int pos = 0;
	int pair_size = sizeof(PageId) + sizeof(int);
	int key_count = 0;
	int num_keys = getKeyCount();
	while(pos < PageFile::PAGE_SIZE - 2*sizeof(PageId) && key_count != num_keys) 
	{
		int current_key;
		memcpy(&current_key, buffer + pos + sizeof(PageId), sizeof(int));
		if(current_key >= searchKey) 
		{
			memcpy(&pid, buffer +  pos, sizeof(PageId));
			return 0;
		}
		pos += pair_size;
		key_count++;
	}
	memcpy(&pid, buffer + pos, sizeof(PageId)); // if we've reached the end i.e. infinity
	return 0;

}

/*
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2)
{	

	int pair_size = sizeof(PageId) + sizeof(int);

	memcpy(buffer, &pid1, sizeof(PageId));
	memcpy(buffer+sizeof(PageId), &key, sizeof(int));
	memcpy(buffer + pair_size, &pid2, sizeof(PageId));

	return 0;
}
void BTNonLeafNode::print_node() {
	cout << "Non Leaf node" << endl;
	cout << "KeyCount: " << getKeyCount() << endl;

	int pair_size = sizeof(PageId) + sizeof(int);
	int empty_pairs = 0;

	for(int i = 0; i < (PageFile::PAGE_SIZE - sizeof(PageId))/pair_size; i++) 
	{
		PageId pid;
		int key;
		memcpy(&pid, buffer + (i * pair_size), sizeof(PageId));
		memcpy(&key, buffer + (i * pair_size) + sizeof(PageId), sizeof(int));
		if(pid == -1  && key == -1) {
			empty_pairs++;
		}
		else {
			cout << "Pid: " << pid << ", " << "; key: " << key << " at " << (unsigned int)(buffer + (i * pair_size)) << endl;
		}
	}
	cout << "Num pairs left: " << empty_pairs << endl;
}
