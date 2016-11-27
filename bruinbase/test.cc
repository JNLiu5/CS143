#include "BTreeNode.h"
#include "BTreeIndex.h"
#include <stdio.h>
using namespace std;

int main() {
    // BTLeafNode node;
    // for(int i = 0; i < 3; i++) {
    //  RecordId rid;
    //  rid.pid = i;
    //  rid.sid = i + 1;
    //  int key = i;
    //  node.insert(key, rid);
    // }
    // // RecordId rid_2;
    // // RecordId rid_3;
    // // rid_2.pid = 3;
    // // rid_2.sid = 4;

    // // node.insert(4, rid_2);

    // // rid_3.pid = 5;
    // // rid_3.sid = 5;
    // // node.insert(5, rid_2);
    // node.print_node();


    // BTNonLeafNode asdf;

    // PageId pid_1;
    // pid_1 = 1;
    // asdf.insert(1,pid_1);

    // PageId pid_0;
    // pid_0 = 0;
    // asdf.insert(0,pid_0);

    // PageId pid_2;
    // pid_2 = 2;
    // asdf.insert(2,pid_2);


    // PageId pid_3;
    // pid_3 = 3;
    // asdf.insert(3,pid_3);

    // PageId pid_4;
    // pid_4 = 4;
    // asdf.insert(4,pid_4);


    // PageId childPtr;

    // asdf.locateChildPtr(2, childPtr);

    // cout << "ChildPtr: " << childPtr << endl;

    // asdf.print_node();

    BTreeIndex tree;

    string index = "index.txt";

    tree.open(index, 'w');

    for(int i = 0; i < 10000; i++) 
    {
        RecordId rid;
        rid.pid = i;
        rid.sid = i + 1;
        int key = i;
        tree.insert(key, rid);
    }

    // RecordId rid_3;
    // rid_3.pid = 3;
    // rid_3.sid = 3 + 1;
    // int key_3 = 3;
    // tree.insert(key_3, rid_3);

    // RecordId rid_2;
    // rid_2.pid = 2;
    // rid_2.sid = 2 + 1;
    // int key_2 = 2;
    // tree.insert(key_2, rid_2);



    tree.print_tree();
    tree.close();
    tree.open(index, 'r');
    tree.print_tree();
    remove("index.txt");
    return 0;
}
