#include "BTreeNode.h"
#include "BTreeIndex.h"
#include <stdio.h>
using namespace std;

int main() {
	BTreeIndex tree;
	tree.open("test.txt", 'w');
	for(int i = 0; i < 8; i++) {
		int key = i;
		RecordId rid;
		rid.pid = i + 1;
		rid.sid = i + 2;
		tree.insert(key, rid);
	}
	tree.print_path(4);
}
  /*       6
    2 4   6 8 10
0 1   2 3   4 5   6 7   8 9   10 11


    2 4 6 8
0 1   2 3   4 5   6 7  8 9*/