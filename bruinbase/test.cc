#include "BTreeNode.h"
#include "BTreeIndex.h"
#include <stdio.h>
using namespace std;

int main() {
	BTreeIndex tree;
	tree.open("test.txt", 'w');
	for(int i = 0; i < 12; i++) {
		int key = i;
		RecordId rid;
		rid.pid = i + 1;
		rid.sid = i + 2;
		tree.insert(key, rid);
	}
	tree.print_path(0);
	tree.print_path(2);
	tree.print_path(11);
}
