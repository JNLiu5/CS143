#include "BTreeNode.h"
#include "BTreeIndex.h"
#include <stdio.h>
using namespace std;

int main() {
	BTreeIndex tree;
	tree.open("movie.idx", 'r');
	tree.print_path(831);
}