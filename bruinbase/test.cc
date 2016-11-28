#include "BTreeNode.h"
#include "BTreeIndex.h"
#include <stdio.h>
using namespace std;

int main() {
	BTreeIndex tree;
	tree.open("test.txt", 'w');
	for(int i = 0; i < 1000; i++) {
		int key = i;
		RecordId rid;
		rid.pid = i + 1;
		rid.sid = i + 2;
		tree.insert(key, rid);
	}
	tree.print_path(500);
}