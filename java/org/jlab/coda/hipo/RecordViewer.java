/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.awt.BorderLayout;
import java.util.ArrayList;
import java.util.List;
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.table.AbstractTableModel;

/**
 *
 * @author gavalian
 */
public class RecordViewer extends JPanel {
    
    private List<FileRecordEntry>  fileRecords = new ArrayList<FileRecordEntry>();
    JTable recordTable = null;
    RecordTableModel model = new RecordTableModel();
    
    public RecordViewer(){
        this.setLayout(new BorderLayout());
        recordTable = new JTable(model);
        JScrollPane scrollPane = new JScrollPane(recordTable);
        this.add(scrollPane,BorderLayout.CENTER);
    }
    
    
    public void openFile(String file){
        
    }
    
    public static void main(String[] args){
        JFrame frame = new JFrame();
        frame.setSize(500, 500);
        RecordViewer viewer = new RecordViewer();
        frame.add(viewer);
        frame.pack();
        frame.setVisible(true);
    }
    
    
    public static class RecordTableModel extends AbstractTableModel {

        String[] columnNames = new String[]{"#","offset","length","header","index"};
        
        private List<FileRecordEntry> recordEntries = new ArrayList<>();
        
        public RecordTableModel(){
            
        }
        
        public List<FileRecordEntry> getRecordEntries(){ return this.recordEntries;}
        
        @Override
        public int getRowCount() {
            return recordEntries.size();
        }

        @Override
        public int getColumnCount() {
            return 5;
        }
        
        @Override
        public String getColumnName(int col) {
            return columnNames[col];
        }

        @Override
        public Object getValueAt(int rowIndex, int columnIndex) {
            
            return "7";
        }
        
    }
    
    public static class FileRecordEntry {
        
        private long  position;
        private int   dataLength;
        private int   recordLength;
        private int   indexLength;
        private int   compressedLength;
        private int   headerLength;
        private int   userHeaderLength;
        
        public FileRecordEntry(){
            
        }
        
        public FileRecordEntry setDataLength(int len){
            this.dataLength = len; return this;
        }
        
        public FileRecordEntry setRecordLength(int len){
            this.recordLength = len; return this;
        }
        
        public FileRecordEntry setCompressedLength(int len){
            this.compressedLength = len; return this;
        }
        
        public FileRecordEntry setIndexLength(int len){
            this.indexLength = len; return this;
        }

        public int getRecordLength(){ return this.recordLength;}
        public int getDataLength(){ return this.dataLength;}
        public int getCompressedLength(){return this.compressedLength;}
        public int getIndexLength(){return this.indexLength;}
        public long getPosition(){return this.position;}
    }
}
