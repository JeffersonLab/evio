/*
 *   Copyright (c) 2018.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
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

    /**
     * Each entry corresponds to a record.
     * The value of each entry is the total number of events in
     * the file up to and including the record of that entry.
     * The only exception is the first entry which corresponds to no
     * record and its value is always 0. Thus, an index of 1 in this
     * list corresponds to the first record.
     */
    private List<Integer>       recordIndex  = new ArrayList<Integer>();
    /** Index number of the current event in the file. */
    private int                 currentEvent = 0;
    /** Index number of the current record.
     *  First record has value of 0. Add one to use with recordIndex. */
    private int                currentRecord = 0;
    /** Index number of the current event in the current record. */
    private int           currentRecordEvent = 0;
    
    public FileEventIndex(){}

    /**
     * Resets the current index to 0. The corresponding
     * record number is recalculated by calling setEvent() method.
     */
    public void resetIndex() {currentEvent = 0; setEvent(currentEvent);}

    /**
     * Adds the number of events in the next record to the index of records.
     * Internally, what is stored is the total number of events in
     * the file up to and including the record of this entry.
     * @param size number of events in the next record.
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
     * Gets the current event number which is set by {@link #advance()},
     * {@link #retreat()} or {@link #setEvent(int)} (which also sets
     * the record number that the event belongs to).
     * @return current event number.
     */
    public int getEventNumber() {return currentEvent;}

    /**
     * Gets the current record number which is set by {@link #setEvent(int)},
     * or by using {@link #advance()} or {@link #retreat()} (which set the event
     * number to the next available or previous available respectively).
     * @return current record number.
     */
    public int getRecordNumber() {return currentRecord;}

    /**
     * Gets the event number inside the record that corresponds
     * to the current global event number from the file.
     * @return event offset in the record.
     */
    public int getRecordEventNumber() {return currentRecordEvent;}

    /**
     * Checks to see if the event counter reached the end.
     * @return true if there are more events to advance to, false otherwise.
     */
    public boolean canAdvance() {
        //System.out.println(" current event = " + currentEvent + " MAX = " + getMaxEvents());
        return (currentEvent < (getMaxEvents()-1));
    }

    /**
     * Advances the current event number by one. If the event is not from current
     * record, the record number will also be changed.
     * If calling this would advance the current event number beyond its maximum limit,
     * nothing is done.
     * @return false if the record number is the same,
     *         and true if the record number has changed.
     */
    public boolean advance() {
        // If no data entered into recordIndex yet ...
        if (recordIndex.isEmpty()) {
            System.out.println("advance(): Warning, no entries in recordIndex!");
            return false;
        }

        if (currentEvent+1 < recordIndex.get(currentRecord+1)) {
            currentEvent++;
            currentRecordEvent++;
            return false;
        }
        
        // Trying to advance beyond the limit of list
        if (recordIndex.size() < currentRecord + 2 + 1) {
            System.out.println("advance(): Warning, reached recordIndex limit!");
            return false;
        }

        currentEvent++;
        currentRecord++;
        currentRecordEvent = 0;

        return true;
    }

    /**
     * Checks if the event index can retreat (decrease). Convenience function.
     * @return true if the event index can be lowered by one.
     */
    public boolean canRetreat() {return (currentEvent > 0);}
    
    /**
     * Reduces current event number by one.
     * If the record number changes, it returns true.
     * @return false if the record number is the same,
     *         and true is the record number has changed.
     */
    public boolean retreat() { 
        if (currentEvent == 0) return false;
        currentEvent--;
        if(currentRecordEvent > 0){
            currentRecordEvent--;
            return false;
        } else {
            currentRecord--;
            currentRecordEvent = currentEvent - recordIndex.get(currentRecord);
        }
        return true; 
    }

    /**
     * Gets the total number of events in file.
     * @return returns the number of events corresponding to the all records.
     */
    public int getMaxEvents() {
        return recordIndex.isEmpty() ? 0 : recordIndex.get(recordIndex.size()-1);
    }

    /** Prints the content of the event index array on the screen. */
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
     * Set the current event to the desired position. The current record and event
     * offset inside of the record are updated as well.
     * @param event event number in the stream, must be 0 to getMaxEvents()-1.
     * @return true if record is different from previous one, false if it is the same.
     */
    public boolean setEvent(int event){
        
        boolean hasRecordChanged = true;
        if(event < 0 || event >= getMaxEvents()) {
            System.out.println("[record-index] ** error ** can't change event "
            + " to " + event + ". choose value [ 0-" + (getMaxEvents()-1) + " ]");
            return false;
        }
        
        int index = Collections.binarySearch(recordIndex, event);
        if(index >= 0){
            if(currentRecord == index) hasRecordChanged = false;
            currentRecord = index;
            currentRecordEvent = 0;
            currentEvent = event;
        } else {
            if(currentRecord == (-index-2)) hasRecordChanged = false;
            // One less than index into recordIndex
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
        index.addEventSize(nevents);
        for(int i = 0; i < 5; i++){
            //int nevents = (int) (Math.random()*40+120);
            index.addEventSize(5+i*2);
        }
        
        index.show();
        index.setEvent(0);
        System.out.println(index);
        System.out.println(" **** START ADVANCING ****");
        for(int i = 0 ; i < 60; i++){
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
