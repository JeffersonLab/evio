//
// Created by Carl Timmer on 2019-04-25.
//

#ifndef EVIO_6_0_FILEEVENTINDEX_H
#define EVIO_6_0_FILEEVENTINDEX_H


#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <vector>

using std::string;
using std::vector;

/**
 * Class used to handle event indexes in the context
 * of a file and having to change records.
 * @author timmer
 */
class FileEventIndex {

private:

    /** Index number of the current event in the file. */
    uint32_t currentEvent = 0;

    /** Index number of the current record.
     *  First record has value of 0. Add one to use with recordIndex. */
    uint32_t currentRecord = 0;

    /** Index number of the current event in the current record. */
    uint32_t currentRecordEvent = 0;

    /**
      * Each entry corresponds to a record.
      * The value of each entry is the total number of events in
      * the file up to and including the record of that entry.
      * The only exception is the first entry which corresponds to no
      * record and its value is always 0. Thus, an index of 1 in this
      * vector corresponds to the first record.
      */
    vector<uint32_t> recordIndex;

public:

    /** Constructor. */
    FileEventIndex() = default;

    void clear();
    void resetIndex();
    void show();
    void addEventSize(uint32_t size);

    uint32_t getEventNumber()       const;
    uint32_t getRecordNumber()      const;
    uint32_t getRecordEventNumber() const;
    uint32_t getMaxEvents()         const;

    bool canAdvance() const;
    bool canRetreat() const;
    bool advance();
    bool retreat();
    bool setEvent(uint32_t event);

    string toString();

    int main(int argc, char **argv);

    // TODO: overwrite = operator ???

};

#endif //EVIO_6_0_FILEEVENTINDEX_H
