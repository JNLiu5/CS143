#include "BTreeNode.h"
using namespace std;

int main() {
    BTLeafNode node;
    for(int i = 0; i < 3; i++) {
    	RecordId rid;
    	rid.pid = i;
    	rid.sid = i + 1;
    	int key = i;
    	node.insert(key, rid);
    	//node.print_node();
    }
    RecordId rid_2;
    rid_2.pid = 3;
    rid_2.sid = 4;
    BTLeafNode sibling;
    int sibling_key;
    node.setNextNodePtr(15);
    cout << node.getNextNodePtr() << endl;
    node.insertAndSplit(3, rid_2, sibling, sibling_key);
    node.print_node();
    cout << endl;
    sibling.print_node();
    cout << "Sibling key: " << sibling_key << endl;
    return 0;
}
