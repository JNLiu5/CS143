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
    }
    RecordId rid_2;
    rid_2.pid = 3;
    rid_2.sid = 4;
    node.insert(4, rid_2);
    node.print_node();
    return 0;
}
