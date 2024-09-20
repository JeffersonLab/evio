//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "FileEventIndex.h"


namespace evio {


    /** Clear the entire object. */
    void FileEventIndex::clear() {
        currentEvent = currentRecord = currentRecordEvent = 0;
        recordIndex.clear();
    }


    /**
     * Resets the current index to 0. The corresponding
     * record number is recalculated by calling the {@link #setEvent(uint32_t)} method.
     */
    void FileEventIndex::resetIndex() {
        currentEvent = 0;
        setEvent(currentEvent);
    }


    /**
     * Adds the number of events in the next record to the index of records.
     * Internally, what is stored is the total number of events in
     * the file up to and including the record of this entry.
     * @param size number of events in the next record.
     */
    void FileEventIndex::addEventSize(uint32_t size){
        if (recordIndex.empty()) {
            recordIndex.push_back(0);
            recordIndex.push_back(size);
        }
        else {
            uint32_t cz = recordIndex[recordIndex.size()-1] + size;
            recordIndex.push_back(cz);
        }
    }


    /**
     * Gets the current event number which is set by {@link #advance()},
     * {@link #retreat()} or {@link #setEvent(uint32_t)} (which also sets
     * the record number that the event belongs to).
     * @return current event number.
     */
    uint32_t FileEventIndex::getEventNumber() const {return currentEvent;}


    /**
     * Gets the current record number which is set by {@link #setEvent(uint32_t)},
     * or by using {@link #advance()} or {@link #retreat()} (which set the event
     * number to the next available or previous available respectively).
     * @return current record number.
     */
    uint32_t FileEventIndex::getRecordNumber() const {return currentRecord;}


    /**
     * Gets the event number inside the record that corresponds
     * to the current global event number from the file.
     * @return event offset in the record.
     */
    uint32_t FileEventIndex::getRecordEventNumber() const {return currentRecordEvent;}


    /**
     * Gets the total number of events in file.
     * @return returns the number of events corresponding to the all records.
     */
    uint32_t FileEventIndex::getMaxEvents() const {
        return recordIndex.empty() ? 0 : recordIndex[recordIndex.size()-1];
    }


    /**
     * Checks to see if the event counter reached the end.
     * @return true if there are more events to advance to, false otherwise.
     */
    bool FileEventIndex::canAdvance() const {
        //std::cout << " current event = " << currentEvent << " MAX = " << getMaxEvents() << std::endl;
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
    bool FileEventIndex::advance() {
        // If no data entered into recordIndex yet ...
        if (recordIndex.empty()) {
            //std::cout << "advance(): Warning, no entries in recordIndex!" << std::endl;
            return false;
        }

        if (currentEvent+1 < recordIndex[currentRecord+1]) {
            currentEvent++;
            currentRecordEvent++;
            return false;
        }

        // Trying to advance beyond the limit of list
        if (recordIndex.size() < currentRecord + 2 + 1) {
            //std::cout << "advance(): Warning, reached recordIndex limit!" << std::endl;
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
    bool FileEventIndex::canRetreat() const {return (currentEvent > 0);}


    /**
     * Reduces current event number by one.
     * If the record number changes, it returns true.
     * @return false if the record number is the same,
     *         and true is the record number has changed.
     */
    bool FileEventIndex::retreat() {
        if (currentEvent == 0) {
            return false;
        }

        currentEvent--;
        if (currentRecordEvent > 0) {
            currentRecordEvent--;
            return false;
        }
        else {
            currentRecord--;
            currentRecordEvent = currentEvent - recordIndex[currentRecord];
        }
        return true;
    }


    /** Prints the content of the event index array on the screen. */
    void FileEventIndex::show() {
        std::cout << "[FILERECORDINDEX] number of records    : " << (recordIndex.size() - 1) << std::endl;
        std::cout << "[FILERECORDINDEX] max number of events : " << getMaxEvents() << std::endl;

        for (size_t i = 0; i < recordIndex.size(); i++) {
            std::cout << std::setw(6) << recordIndex[i];
            if ((i+1)%15 == 0) {
                std::cout << std::endl;
            }
        }
        std::cout << "\n--\n" << std::endl;
    }


    /**
     * Set the current event to the desired position. The current record and event
     * offset inside of the record are updated as well.
     * @param event event number in the stream, must be 0 to getMaxEvents()-1.
     * @return true if record is different from previous one, false if it is the same.
     */
    bool FileEventIndex::setEvent(uint32_t event) {

        bool hasRecordChanged = true;
        if (event >= getMaxEvents()) {
            std::cout << "[record-index] ** error ** can't change event to " <<
                 event << ". Choose value [ 0 - " << (getMaxEvents()-1) << " ]" << std::endl;
            return false;
        }

        //    std::cout << "RecordIndex vector: size = " << recordIndex.size() << std::endl;
        //    for (int i=0; i < recordIndex.size(); i++) {
        //        std::cout << "    element " << i << " = " << recordIndex[i] << std::endl;
        //    }

        // The search returns an iterator pointing to the first element in the range [first, last)
        // that is greater than event, or last if no such element is found.
        auto it = std::upper_bound(recordIndex.cbegin(), recordIndex.cend(), event);
        uint32_t index = std::distance(recordIndex.cbegin(), it);
        // The first element in recordIndex is fake, shift by 1 for index into recordPositions of Reader
        index--;
//std::cout << "  setEvent: event #" << event << " found at index " << index << std::endl;

        if (currentRecord == index) {
            hasRecordChanged = false;
        }
        currentRecord = index;
        currentEvent = event;
        currentRecordEvent = currentEvent - recordIndex[currentRecord];

        return hasRecordChanged;
    }


    /**
     * Get a string representation of this object.
     * @return a string representation of this object.
     */
    std::string FileEventIndex::toString() {

        std::stringstream ss;

        ss << "n events = " << std::setw(6) << getMaxEvents();
        ss << ", event = "  << std::setw(6) << currentEvent;
        ss << ", record = " << std::setw(5) << currentRecord;
        ss << ", offset = " << std::setw(6) << currentRecordEvent;

        return ss.str();
    }


    int FileEventIndex::main(int argc, char **argv) {

        FileEventIndex index;
        int nevents = 10;
        index.addEventSize(nevents);
        for (int i = 0; i < 5; i++) {
            index.addEventSize(5+i*2);
        }

        index.show();
        index.setEvent(0);

        std::cout << index.toString() << std::endl;
        std::cout << " **** START ADVANCING ****" << std::endl;
        for (int i = 0 ; i < 60; i++) {
            bool status = index.advance();
            std::cout << index.toString() + ", status = " + std::to_string(status) << std::endl;
        }
        std::cout << " **** START RETREATING ****" << std::endl;
        for (int i = 0 ; i < 54; i++) {
            bool status = index.retreat();
            std::cout << index.toString() + ", status = " + std::to_string(status) << std::endl;
        }

        std::cout << " **** START SETTING EVENT NUMBER ****" << std::endl;
        for (int i = 0 ; i < 55; i++) {
            bool status = index.setEvent(i);
            std::cout << index.toString() + ", status = " + std::to_string(status) << std::endl;
        }

        return 0;
    }

}

