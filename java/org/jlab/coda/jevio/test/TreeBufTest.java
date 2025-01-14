/*
 * Copyright (c) 2025, Jefferson Science Associates, all rights reserved.
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 */

package org.jlab.coda.jevio.test;
import org.jlab.coda.jevio.*;

import javax.swing.tree.TreeNode;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.Vector;

public class TreeBufTest {


    public static void main(String args[]) {
        try {
            treeTest();
        }
        catch (EvioException e) {
            throw new RuntimeException(e);
        }
    }



        // Test the BaseStructure's tree methods
    static void treeTest() throws EvioException {

        // check tree structure stuff
        EvioBank topBank   = new EvioBank(0, DataType.BANK, 0);
        EvioBank midBank   = new EvioBank(1, DataType.BANK, 1);
        EvioBank midBank2  = new EvioBank(2, DataType.BANK, 2);
        EvioBank childBank = new EvioBank(4, DataType.FLOAT32, 4);

        // Child's float data
        float[] fData = new float[3];
        fData[0] = 0.F;
        fData[1] = 1.F;
        fData[2] = 2.F;
        childBank.appendFloatData(fData);

        System.out.println("EvioBank: local intData size = " + fData.length);

        System.out.println("\n--------------------------------------------");
        System.out.println("----------   Test Tree Methods  ------------");
        System.out.println("--------------------------------------------\n");

        // Create tree
        topBank.insert(midBank2);
        System.out.println("Added midBank2 to topBank (" + topBank.getChildCount() + " kids)");
        topBank.insert(midBank);
        System.out.println("Added midBank to topBank (" + topBank.getChildCount() + " kids)");
        // add it again should make no difference
        topBank.insert(midBank2);
        System.out.println("Added midBank2 to topBank again (" + topBank.getChildCount() + " kids)");
        midBank.insert(childBank);
        System.out.println("Added childBank to midBank, topBank has " + topBank.getChildCount() + " kids");

        System.out.println("\nTopBank = " + topBank);
        System.out.println("Is child descendant of Top bank? " + topBank.isNodeDescendant(childBank));
        System.out.println("Is Top bank ancestor of child? " + childBank.isNodeAncestor(topBank));
        System.out.println("\nDepth at Top bank = " + topBank.getDepth());
        System.out.println("Depth at Mid bank = " + midBank.getDepth());
        System.out.println("Depth at Child bank = " + childBank.getDepth());
        System.out.println("\nLevel at top bank = " + topBank.getLevel());
        System.out.println("Level at child = " + childBank.getLevel());

        System.out.println("\nRemove child from midBank:");
        midBank.remove(childBank);
        System.out.println("midBank = " + midBank.toString());
        System.out.println("Is child descendant of top bank? " + topBank.isNodeDescendant(childBank));
        System.out.println("Is top bank ancestor of child? " + childBank.isNodeAncestor(topBank));

        // add child again
        System.out.println("\nAdd child back to midBank:");
        midBank.insert(childBank);
        System.out.println("midBank = " + midBank);
        midBank.removeAllChildren();
        System.out.println("Remove all children from midBank:");
        System.out.println("midBank = " + midBank);

        // add child again
        System.out.println("\nAdd 1 child back to midBank:");
        midBank.insert(childBank);
        System.out.println("midBank = " + midBank);
        childBank.removeFromParent();
        System.out.println("Remove child from parent:");
        System.out.println("midBank = " + midBank);

        // add child again
        System.out.println("\nAdd child back to midBank:");
        midBank.insert(childBank);
        System.out.println("Level at top bank = " + topBank.getLevel());
        System.out.println("Level at child = " + childBank.getLevel());
        System.out.println("Level at mid bank 1 = " + midBank.getLevel());

        System.out.println("\nCALL sharedAncestor for both mid banks");
        BaseStructure strc = midBank2.getSharedAncestor(midBank);
        if (strc != null) {
            System.out.println("shared ancestor of midBank 1&2 = " + strc);
        }
        else {
            System.out.println("shared ancestor of midBank 1&2 = NONE");
        }

        BaseStructure[] path = childBank.getPath();
        System.out.println("\nPath of child bank:");
        for (BaseStructure str : path) {
            System.out.println("     -  " + str);
        }

        int kidCount = topBank.getChildCount();
        System.out.println("\ntopBank has " + kidCount + " children");
        for (int i=0; i < kidCount; i++) {
            System.out.println("   child at index " + i + " = " + topBank.getChildAt(i));
            System.out.println("       child getIndex = " + topBank.getIndex(topBank.getChildAt(i)));
        }

        System.out.println("\ninsert another child into topBank at index = 2");
        EvioBank midBank3 = new EvioBank(3, DataType.BANK, 3);
        topBank.insert(midBank3, 2);
        System.out.println("\ntopBank = " + topBank);

        try {
            System.out.println("\ninsert another child into topBank at index = 4");
            EvioBank midBank33 = new EvioBank(33, DataType.BANK, 33);
            topBank.insert(midBank33, 4);
            System.out.println("\ntopBank = " + topBank);
        }
        catch (IndexOutOfBoundsException e) {
            // out of range?
            System.out.println("ERROR: index out of bounds");
        }

        System.out.println("\niterate breadth-first thru topBank");
        Enumeration<TreeNode> breadthFirst = topBank.breadthFirstEnumeration();
        while (breadthFirst.hasMoreElements()) {
            System.out.println("  kid = " + breadthFirst.nextElement());
        }

        // Note: Java & C++ depth-first definitions are different
        System.out.println("\niterate depth-first thru topBank");
        Enumeration<TreeNode> depththFirst = topBank.depthFirstEnumeration();
        while (depththFirst.hasMoreElements()) {
            System.out.println("  kid = " + depththFirst.nextElement());
        }

        System.out.println("\niterate thru topBank children");
        Vector<BaseStructure> children = topBank.getChildren();
        Enumeration<BaseStructure> en = children.elements();
        while (en.hasMoreElements()) {
            System.out.println("  kid = " + en.nextElement());
        }

        System.out.println("\nRemove topBank's first child");
        topBank.remove(0);
        System.out.println("    topBank has " + topBank.getChildCount() + " children");
        System.out.println("    topBank = " + topBank);
        // reinsert
        System.out.println("Add topBank's first child back");
        topBank.insert( midBank, 0);

        BaseStructure parent = topBank.getParent();
        if (parent == null) {
            System.out.println("\nParent of topBank is = null");
        }
        else {
            System.out.println("\nParent of topBank is = " + topBank.getParent());
        }

        parent = childBank.getParent();
        if (parent == null) {
            System.out.println("\nParent of childBank is = nullptr");
        }
        else {
            System.out.println("\nParent of childBank is = " + parent);
        }

        BaseStructure root = childBank.getRoot();
        System.out.println("\nRoot of childBank is = " + root);
        root = topBank.getRoot();
        System.out.println("Root of topBank is = " + root);

        System.out.println("\nIs childBank root = " + childBank.isRoot());
        System.out.println("Is topBank root = " + topBank.isRoot() );

        BaseStructure node = topBank;
        System.out.println("\nStarting from root:");
        do {
            node = node.getNextNode();
            if (node == null) {
                System.out.println("  next node = null");
            }
            else {
                System.out.println("  next node = " + node);
            }
        } while (node != null);


        node = midBank2;
        System.out.println("\nStarting from midBank2:");
        do {
            node = node.getNextNode();
            if (node == null) {
                System.out.println("  next node = nullptr");
            }
            else {
                System.out.println("  next node = " + node);
            }
        } while (node != null);


        node = midBank3;
        System.out.println("\nStarting from midBank3:");
        do {
            node = node.getPreviousNode();
            if (node == null) {
                System.out.println("  prev node = nullptr");
            }
            else {
                System.out.println("  prev node = " + node);
            }
        } while (node != null);

        System.out.println("\nIs childBank child of topBank = " + topBank.isNodeChild(childBank));
        System.out.println("Is midBank3 child of topBank = " + topBank.isNodeChild(midBank3));

        System.out.println("\nfirst child of topBank = " + topBank.getFirstChild().toString());
        System.out.println("last child of topBank = " + topBank.getLastChild().toString());
        System.out.println("child after midBank2 = " + topBank.getChildAfter(midBank2).toString());
        System.out.println("child before midBank3 = " + topBank.getChildBefore(midBank3).toString());

        System.out.println("\nis midBank sibling of midBank3 = " + midBank.isNodeSibling(midBank3));
        System.out.println("sibling count of midBank3 = " + midBank3.getSiblingCount());
        System.out.println("next sibling of midBank = " + midBank.getNextSibling().toString());
        System.out.println("prev sibling of midBank2 = " + midBank2.getPreviousSibling().toString());
        System.out.println("prev sibling of midBank = " + midBank.getPreviousSibling());


        System.out.println("\nAdd 2 children to midBank2 & and 1 child to 3");
        EvioBank childBank2 = new EvioBank(5, DataType.INT32, 5);
        EvioBank childBank3 = new EvioBank(6, DataType.INT32, 6);
        EvioBank childBank4 = new EvioBank(7, DataType.SHORT16, 7);

        // Child's data
        int[] iData = new int[] {3,4,5};
        childBank2.setIntData(iData);

        int[] iData2 = new int[] {6,7,8};
        childBank3.setIntData(iData2);

        short[] iData3 = new short[] {10,11,12};
        childBank4.setShortData(iData3);

        // add to tree
        midBank2.insert(childBank2);
        midBank2.insert(childBank3);
        midBank3.insert(childBank4);


        System.out.println("\nchildBank isLeaf = " + childBank.isLeaf());
        System.out.println("topBank isLeaf = " + topBank.isLeaf());
        System.out.println("topBank leaf count = " + topBank.getLeafCount());
        System.out.println("midBank2 leaf count = " + midBank2.getLeafCount());
        System.out.println("topBank first Leaf = " + topBank.getFirstLeaf().toString());
        System.out.println("topBank last Leaf = " + topBank.getLastLeaf().toString());
        System.out.println("midBank2 next Leaf = " + midBank2.getNextLeaf().toString());
        System.out.println("childBank2 prev Leaf = " + childBank2.getPreviousLeaf().toString());
        System.out.println("childBank prev Leaf = " + childBank.getPreviousLeaf());


        System.out.println("\nAdd 1 child to topBank with same tag (4) as first leaf but num = 20");
        EvioBank midBank4 = new EvioBank(4, DataType.BANK, 20);
        topBank.insert(midBank4);

        //////////////////////////////////////////////////////
        // FINDING STRUCTURES
        //////////////////////////////////////////////////////

        System.out.println("Search for all banks of tag = 4 / num = 4 Using StructureFinder, got the following:");
        int tag = 4;
        int  num = 4;

        List<BaseStructure> vec = StructureFinder.getMatchingBanks(topBank, tag, num);
        for (BaseStructure n : vec) {
            System.out.println("  bank = " + n);
        }
        vec.clear();


        System.out.println("Search for all banks of tag = 4, got the following:");

        // Define a filter to select structures tag=7 & num=0.
        class myFilter implements IEvioFilter {
            int tag;
            myFilter(int tag) {this.tag = tag;}
            public boolean accept(StructureType type, IEvioStructure struct){
                return ((type == StructureType.BANK) &&
                        (tag == struct.getHeader().getTag()));
            }
        };

        // Create the defined filter
        myFilter filter = new myFilter(4);

        vec = topBank.getMatchingStructures(filter);
        for (BaseStructure n : vec) {
            System.out.println("  bank = " + n);
        }

        System.out.println("\nSearch again for all banks of tag = 4, and execute listener:");

        class myListener implements IEvioListener {
            public void gotStructure(BaseStructure topStructure, IEvioStructure structure) {
                System.out.println("  TOP struct = " + topStructure);
                System.out.println("  GOT struct = " + structure);
            }

            public void startEventParse(BaseStructure structure) {
                System.out.println("  start parsing struct = " + structure);
            }

            public void endEventParse(BaseStructure structure) {
                System.out.println("  end parsing struct = " + structure);
                System.out.println("Ended event parse");
            }
        };

        myListener listener = new myListener();
        topBank.visitAllStructures(listener, filter);

    };

}
