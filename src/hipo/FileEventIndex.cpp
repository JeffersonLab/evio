//
// Created by Carl Timmer on 2019-04-25.
//

#include "FileEventIndex.h"

using namespace std;




/** Clear the entire object. */
void FileEventIndex::clear() {
    currentEvent = currentRecord = currentRecordEvent = 0;
    recordIndex.clear();
}

/**
 * Resets the current index to 0. The corresponding
 * record number is recalculated by calling the setEvent() method.
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
 * {@link #retreat()} or {@link #setEvent(int)} (which also sets
 * the record number that the event belongs to).
 * @return current event number.
 */
uint32_t FileEventIndex::getEventNumber() const {return currentEvent;}

/**
 * Gets the current record number which is set by {@link #setEvent(int)},
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
bool FileEventIndex::canAdvance() const {return (currentEvent < (getMaxEvents()-1));}

/**
 * Checks if the event index can retreat (decrease). Convenience function.
 * @return true if the event index can be lowered by one.
 */
bool FileEventIndex::canRetreat() const {return (currentEvent > 0);}


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
        cout << "advance(): Warning, no entries in recordIndex!" << endl;
        return false;
    }

    if (currentEvent+1 < recordIndex[currentRecord+1]) {
        currentEvent++;
        currentRecordEvent++;
        return false;
    }

    // Trying to advance beyond the limit of list
    if (recordIndex.size() < currentRecord + 2 + 1) {
        cout << "advance(): Warning, reached recordIndex limit!" << endl;
        return false;
    }

    currentEvent++;
    currentRecord++;
    currentRecordEvent = 0;

    return true;
}

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
    cout << "[FILERECORDINDEX] number of records    : " << recordIndex.size() << endl;
    cout << "[FILERECORDINDEX] max number of events : " << getMaxEvents() << endl;

    for (int i = 0; i < recordIndex.size(); i++) {
        cout << setw(6) << recordIndex[i];
        if ((i+1)%15 == 0) {
            cout << endl;
        }
    }
    cout << "\n--\n" << endl;
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
        cout << "[record-index] ** error ** can't change event to " <<
                event << ". Choose value [ 0 - " << (getMaxEvents()-1) << " ]" << endl;
        return false;
    }

    // The search returns an iterator pointing to the first element in the range [first, last)
    // that is not less than (i.e. greater or equal to) event, or last if no such element is found.
    // This is exactly what the java Collections.binarySearch(recordIndex, event) does, although
    // the java return value is more complicated.
    auto it = std::lower_bound(recordIndex.cbegin(), recordIndex.cend(), event);
    uint32_t index = *it;

    if (index >= 0) {
        if (currentRecord == index) {
            hasRecordChanged = false;
        }
        currentRecord = index;
        currentRecordEvent = 0;
        currentEvent = event;
    }
    else {
        if (currentRecord == (-index-2)) {
            hasRecordChanged = false;
        }
        // One less than index into recordIndex
        currentRecord = -index-2;
        currentEvent  = event;
        currentRecordEvent = currentEvent - recordIndex[currentRecord];
    }
    return hasRecordChanged;
}

string FileEventIndex::toString() {

    stringstream ss;

    ss << "n events = " << setw(6) << getMaxEvents();
    ss << ", event = "  << setw(6) << currentEvent;
    ss << ", record = " << setw(5) << currentRecord;
    ss << ", offset = " << setw(6) << currentRecordEvent;

    return ss.str();
}

int FileEventIndex::main(int argc, char **argv) {

    FileEventIndex index;
    int nevents = 10;
    index.addEventSize(nevents);
    for(int i = 0; i < 5; i++){
        //int nevents = (int) (Math.random()*40+120);
        index.addEventSize(5+i*2);
    }

    index.show();
    index.setEvent(0);

    cout << index.toString() << endl;
    cout << " **** START ADVANCING ****" << endl;
    for(int i = 0 ; i < 60; i++) {
        bool status = index.advance();
        cout << index.toString() + ", status = " + to_string(status) << endl;
    }
    cout << " **** START RETREATING ****" << endl;
    for(int i = 0 ; i < 54; i++) {
        bool status = index.retreat();
        cout << index.toString() + ", status = " + to_string(status) << endl;
    }

    cout << " **** START SETTING EVENT NUMBER ****" << endl;
    for(int i = 0 ; i < 55; i++){
        bool status = index.setEvent(i);
        cout << index.toString() + ", status = " + to_string(status) << endl;
    }
}

