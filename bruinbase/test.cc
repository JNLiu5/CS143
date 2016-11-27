#include "BTreeIndex.h"
using namespace std;

int main() {
    BTreeIndex tree;
    tree.open("test.txt", 'w');
    tree.print_tree();
    IndexCursor cursor;
    tree.locate(0, cursor);
    int key;
    RecordId rid;
    for(int i = 0; i < 3; i++) {
    	tree.readForward(cursor, key, rid);
    	cout << "key: " << key << "; pid: " << rid.pid << ", sid: " << rid.sid << endl;
    }
}
