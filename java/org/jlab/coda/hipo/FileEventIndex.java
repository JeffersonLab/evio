/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 *
 * @author gavalian
 */
public class FileEventIndex {
    
    private List<Integer>       recordIndex  = new ArrayList<Integer>();
    private int                 currentEvent = 0;
    private int                currentRecord = 0;
    private int           currentRecordEvent = 0;
    
    public FileEventIndex(){
        
    }
    /**
     * resets the current index, sets it to 0. the corresponding
     * record number is recalculated by calling setEvent() method.
     */
    public void resetIndex(){ currentEvent = 0; setEvent(currentEvent);}
    /**
     * adds another event with size = size to the index
     * of records.
     * @param size 
     */
    public void addEventSize(int size){
        if(recordIndex.isEmpty()){
            recordIndex.add(0);
            recordIndex.add(size);
        } else {
            int cz = recordIndex.get(recordIndex.size()-1) + size;
            recordIndex.add(cz);
        }
    }
    /**
     * returns the current event number, the event number is either 
     * set by using setEvent() method, which will also determine 
     * the record number that the event belongs to.
     * @return current event number
     */
    public int getEventNumber()  { return currentEvent;  }
    /**
     * returns the current record number, this is determined automatically
     * by either setting the event number using setEvent() method, or
     * by using methods advance() or retreat() which will set the event number
     * to the next available one or previous available one respectively.
     * @return current record number.
     */
    public int getRecordNumber() { return currentRecord; }
    /**
     * returns the event number inside the record that corresponds
     * to the global event number from the file.
     * @return event offset in the record.
     */
    public int getRecordEventNumber(){
        return this.currentRecordEvent;
    }
    /**
     * checks if the event counter reached the end of the array.
     * @return true is there are more events to advance, false otherwise.
     */
    public boolean canAdvance(){
        return (currentEvent<getMaxEvents());
    }
    /**
     * advances the current event number, if the event is not from current
     * record the record number will also be changed.
     * @return false if the record number is the same, and true is the record number has changed
     */
    public boolean advance() {
        if(currentEvent+1<recordIndex.get(currentRecord+1)){
            currentEvent++;
            currentRecordEvent++;
            return false;
        } else {
            currentEvent++;
            currentRecord++;
            currentRecordEvent = 0;
        }
        return true; 
    }
    /**
     * checks if the event index can retreat (decrease). convenience function.
     * @return true is the event index can be lowered by one.
     */
    public boolean canRetreat(){
        return (currentEvent>0);
    }
    /**
     * retreats one event backwards, if the record number changes it returns true.
     * @return false if the record number is the same, and true is the record number has changed
     */
    public boolean retreat() { 
        if(currentEvent==0) return false;
        currentEvent--;
        if(currentRecordEvent>0){
            currentRecordEvent--;
            return false;
        } else {
            currentRecord--;
            currentRecordEvent = currentEvent - recordIndex.get(currentRecord);
        }
        return true; 
    }
    /**
     * returns maximum number of events.
     * @return returns the number of events corresponding to the all records.
     */
    public int getMaxEvents() {
        return recordIndex.isEmpty()?0:recordIndex.get(recordIndex.size()-1);
    }
    /**
     * Prints the content of the event index array on the screen.
     */
    public void show(){
        System.out.println("[FILERECORDINDEX] number of records    : " + recordIndex.size());
        System.out.println("[FILERECORDINDEX] max number of events : " + getMaxEvents());
        for(int i = 0; i < recordIndex.size(); i++){
            System.out.print(String.format("%6d ", recordIndex.get(i)));
            if( (i+1)%15==0) System.out.println();
        }
        System.out.println("\n--\n");
    }
    
    /**
     * set the current event to the desired position. the current record and event
     * offset inside of the record is updated as well. 
     * @param event event number in the stream, must be 0-getMaxEvents()
     * @return true if record is different from previous one, false if it is the same
     */
    
    public boolean setEvent(int event){
        
        boolean hasRecordChanged = true;
        if(event<0||event>=this.getMaxEvents()){
            System.out.println("[record-index] ** error ** can't chage event "
            + " to " + event + ". choose value [ 0-"+getMaxEvents()+" ]");
            return false;
        }
        
        int index = Collections.binarySearch(recordIndex, event);
        if(index>=0){
            if(currentRecord==index) hasRecordChanged=false;
            currentRecord = index;
            currentRecordEvent = 0;
            currentEvent = event;
        } else {
            if(currentRecord==(-index-2)) hasRecordChanged=false;
            currentRecord = -index-2;
            currentEvent  = event;
            currentRecordEvent = currentEvent - recordIndex.get(currentRecord);
        }
        return hasRecordChanged;
    }
    
    @Override
    public String toString(){
        StringBuilder str = new StringBuilder();
        str.append(String.format("n events %6d ", this.getMaxEvents()));
        str.append(String.format(" event = %6d,", currentEvent));
        str.append(String.format(" record = %5d,", currentRecord));
        str.append(String.format(" offset = %6d", currentRecordEvent));
        return str.toString();
    }
    
    public static void main(String[] args){
        
        FileEventIndex index = new FileEventIndex();
        int nevents = 10;
        index.addEventSize(10);
        for(int i = 0; i < 5; i++){
            //int nevents = (int) (Math.random()*40+120);
            index.addEventSize(5+i*2);
        }
        
        index.show();
        index.setEvent(0);
        System.out.println(index);
        System.out.println(" **** START ADVANCING ****");
        for(int i = 0 ; i < 54; i++){
            boolean status = index.advance();
            System.out.println(index + " status = " + status);
        }
        System.out.println(" **** START RETREATING ****");
        for(int i = 0 ; i < 54; i++){
            boolean status = index.retreat();
            System.out.println(index + " status = " + status);
        }
        
        System.out.println(" **** START SETTING EVENT NUMBER ****");
        for(int i = 0 ; i < 55; i++){
            boolean status = index.setEvent(i);
            System.out.println(index + " status = " + status);
        }
    }
}
