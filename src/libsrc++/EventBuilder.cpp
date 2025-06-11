//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EventBuilder.h"


namespace evio {


    /**
     * This is the constructor to use for an EventBuilder object that will operate on a new, empty event.
     * @param tag the tag for the event header (which is just a bank header).
     * @param dataType the data type for the event object--which again is just the type for the outer most
     * bank. Often an event is a bank of banks, so typically this will be DataType.BANK, or 0xe (14).
     * @param num often an ordinal enumeration.
     */
    EventBuilder::EventBuilder(uint16_t tag, DataType const & dataType, uint8_t num) {
        // create an event with the correct header data
        event = EvioEvent::getInstance(tag, dataType, num);
    }


    /**
     * This is the constructor to use when you want to manipulate an existing event.
     * @param ev the event to manipulate.
     */
    EventBuilder::EventBuilder(std::shared_ptr<EvioEvent> ev) : event(ev) {}


    /**
     * This goes through the event recursively, and makes sure all the length fields
     * in the headers are properly set.
     */
    void EventBuilder::setAllHeaderLengths() {
        try {
            event->setAllHeaderLengths();
        }
        catch (EvioException & e) {
            std::cout << e.what() << std::endl;
        }
    }


    /**
     * This clears all the data fields in a structure, but not the parent or the children. This keeps the
     * existing tree structure intact. To remove a structure (and, consequently, all its descendants) from the
     * tree, use <code>remove</code>
     * @param structure the segment to clear.
     */
    void EventBuilder::clearData(std::shared_ptr<BaseStructure> structure) {
        if (structure != nullptr) {
            structure->clearData();
        }
    }


    /**
     * Add a child to a parent structure.
     *
     * @param parent the parent structure.
     * @param child the child structure.
     * @throws EvioException if parent or child is null, child has wrong byte order,
     *                       is wrong structure type, or parent is not a container
     */
    void EventBuilder::addChild(std::shared_ptr<BaseStructure> parent, std::shared_ptr<BaseStructure> child) {

            if (child == nullptr || parent == nullptr) {
                throw EvioException("Null child or parent arg.");
            }

            if (child->getByteOrder() != event->getByteOrder()) {
                throw EvioException("Attempt to add child with opposite byte order.");
            }

            // the child must be consistent with the data type of the parent. For example, if the child
            // is a BANK, then the data type of the parent must be BANK.
            DataType const & parentDataType = parent->header->getDataType();

            if (parentDataType.isStructure()) {
                if (parentDataType == DataType::BANK ||
                    parentDataType == DataType::ALSOBANK) {
                    if (child->getStructureType() != StructureType::STRUCT_BANK) {
                        std::string errStr = "Type mismatch in addChild. Parent content type: " +
                                parentDataType.getName() +
                                " child type: " + child->getStructureType().getName();
                        throw EvioException(errStr);
                    }
                }
                else if (parentDataType == DataType::SEGMENT ||
                         parentDataType == DataType::ALSOSEGMENT) {
                    if (child->getStructureType() != StructureType::STRUCT_SEGMENT) {
                        std::string errStr = "Type mismatch in addChild. Parent content type: " +
                                parentDataType.getName() +
                                " child type: " + child->getStructureType().getName();
                        throw EvioException(errStr);
                    }
                }
                else if (parentDataType == DataType::TAGSEGMENT) {
                    if (child->getStructureType() != StructureType::STRUCT_TAGSEGMENT) {
                        std::string errStr = "Type mismatch in addChild. Parent content type: " +
                                parentDataType.getName() +
                                " child type: " + child->getStructureType().getName();
                        throw EvioException(errStr);
                    }
                }
            }
            else { //parent is not a container--it is expecting to hold primitives and cannot have children
                std::string errStr = "Type mismatch in addChild. Parent content type: " +
                        parentDataType.getName() + " cannot have children.";
                throw EvioException(errStr);
            }


            parent->insert(child, parent->getChildCount());
            child->setParent(parent);
            setAllHeaderLengths();
    }


    /**
     * This removes a structure (and all its descendants) from the tree.
     * @param child the child structure to remove.
     * @throws EvioException if arg is null or its parent is null
     */
    void EventBuilder::remove(std::shared_ptr<BaseStructure> child) {
        if (child == nullptr ) {
            throw EvioException("Attempt to remove null child.");
        }

        auto parent = child->getParent();

        // the only orphan structure is the event itself, which cannot be removed.
        if (parent == nullptr ) {
            throw EvioException("Attempt to remove root node, i.e., the event. Don't remove an event. Just discard it.");
        }

        child->removeFromParent();
        child->setParent(std::weak_ptr<BaseStructure>());
        setAllHeaderLengths();
    }


    /**
     * Set int data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of ints) to write.
     * @param count number of ints to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setIntData(std::shared_ptr<BaseStructure> structure, int32_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::INT32);

        auto & vect = structure->getIntData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateIntData();
        setAllHeaderLengths();
    }


    /**
     * Set unsigned int data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of uints) to write.
     * @param count number of ints to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setUIntData(std::shared_ptr<BaseStructure> structure, uint32_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::UINT32);

        auto & vect = structure->getUIntData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateUIntData();
        setAllHeaderLengths();
    }


    /**
     * Set short data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of shorts) to write.
     * @param count number of shorts to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setShortData(std::shared_ptr<BaseStructure> structure, int16_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::SHORT16);

        auto & vect = structure->getShortData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateShortData();
        setAllHeaderLengths();
    }


    /**
     * Set unsigned short data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of ushorts) to write.
     * @param count number of shorts to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setUShortData(std::shared_ptr<BaseStructure> structure, uint16_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::USHORT16);

        auto & vect = structure->getUShortData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateUShortData();
        setAllHeaderLengths();
    }


    /**
     * Set long data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of longs) to write.
     * @param count number of longs to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setLongData(std::shared_ptr<BaseStructure> structure, int64_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::LONG64);

        auto & vect = structure->getLongData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateLongData();
        setAllHeaderLengths();
    }


    /**
     * Set unsigned long data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of ulongs) to write.
     * @param count number of longs to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setULongData(std::shared_ptr<BaseStructure> structure, uint64_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::ULONG64);

        auto & vect = structure->getULongData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateULongData();
        setAllHeaderLengths();
    }


    /**
     * Set byte data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of int8_t) to write.
     * @param count number of bytes to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setByteData(std::shared_ptr<BaseStructure> structure, int8_t* data, size_t count) {
        setUCharData(structure, reinterpret_cast<unsigned char *>(data), count);
    }

    /**
     * Set unsigned byte data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of uint8_t) to write.
     * @param count number of bytes to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setUByteData(std::shared_ptr<BaseStructure> structure, uint8_t* data, size_t count) {
        setCharData(structure, reinterpret_cast<char *>(data), count);
    }


    /**
     * Set char data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of chars) to write.
     * @param count number of bytes to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setCharData(std::shared_ptr<BaseStructure> structure, char* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::CHAR8);

        auto & vect = structure->getCharData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateCharData();
        setAllHeaderLengths();
    }


    /**
     * Set unsigned char data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of uchars) to write.
     * @param count number of bytes to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setUCharData(std::shared_ptr<BaseStructure> structure, unsigned char* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::UCHAR8);

        auto & vect = structure->getUCharData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateUCharData();
        setAllHeaderLengths();
    }


    /**
     * Set float data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of floats) to write.
     * @param count number of floats to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setFloatData(std::shared_ptr<BaseStructure> structure, float* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::FLOAT32);

        auto & vect = structure->getFloatData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateFloatData();
        setAllHeaderLengths();
    }


    /**
     * Set double data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of doubles) to write.
     * @param count number of doubles to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setDoubleData(std::shared_ptr<BaseStructure> structure, double* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::DOUBLE64);

        auto & vect = structure->getDoubleData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateDoubleData();
        setAllHeaderLengths();
    }


    /**
     * Set string data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of strings) to write.
     * @param count number of strings to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setStringData(std::shared_ptr<BaseStructure> structure, std::string* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::CHARSTAR8);

        auto & vect = structure->getStringData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateStringData();
        setAllHeaderLengths();
    }


    /**
     * Set composite data in the structure. If the structure has data, it is overwritten
     * even if the existing data is of a different type.
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of shared pointers of composite type) to write.
     * @param count number of (shared pointer of) composites to write.
     * @throws EvioException if structure or data arg(s) is null.
     */
    void EventBuilder::setCompositeData(std::shared_ptr<BaseStructure> structure,
                                        std::shared_ptr<CompositeData> *data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        structure->getHeader()->setDataType(DataType::COMPOSITE);

        auto & vect = structure->getCompositeData();
        vect.clear();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateCompositeData();
        setAllHeaderLengths();
    }


///////////////////////////////////////////////////////////////////////////////////////


    /**
     * Append int data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of ints) to append.
     * @param count number of ints to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not int.
     */
    void EventBuilder::appendIntData(std::shared_ptr<BaseStructure> structure, int32_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::INT32) {
            throw EvioException("cannot append ints to structure of type " +
                                structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getIntData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateIntData();
        setAllHeaderLengths();
    }


    /**
     * Append unsigned int data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of uints) to append.
     * @param count number of ints to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not unsigned int.
     */
    void EventBuilder::appendUIntData(std::shared_ptr<BaseStructure> structure, uint32_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::UINT32) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getUIntData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateUIntData();
        setAllHeaderLengths();
    }


    /**
     * Append short data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of shorts) to append.
     * @param count number of shorts to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not short.
     */
    void EventBuilder::appendShortData(std::shared_ptr<BaseStructure> structure, int16_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::SHORT16) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getShortData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateShortData();
        setAllHeaderLengths();
    }


    /**
     * Append unsigned short data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of ushorts) to append.
     * @param count number of shorts to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not unsigned short.
     */
    void EventBuilder::appendUShortData(std::shared_ptr<BaseStructure> structure, uint16_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::USHORT16) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getUShortData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateUShortData();
        setAllHeaderLengths();
    }


    /**
     * Append long data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of longs) to append.
     * @param count number of longs to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not long.
     */
    void EventBuilder::appendLongData(std::shared_ptr<BaseStructure> structure, int64_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::LONG64) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getLongData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateLongData();
        setAllHeaderLengths();
    }


    /**
     * Append unsigned long data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of ulongs) to append.
     * @param count number of longs to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not unsigned long.
     */
    void EventBuilder::appendULongData(std::shared_ptr<BaseStructure> structure, uint64_t* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::ULONG64) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getULongData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateULongData();
        setAllHeaderLengths();
    }


    /**
     * Append char data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of chars) to append.
     * @param count number of chars to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not char.
     */
    void EventBuilder::appendCharData(std::shared_ptr<BaseStructure> structure, char* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::CHAR8) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getCharData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateCharData();
        setAllHeaderLengths();
    }


    /**
     * Append unsigned char data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of uchars) to append.
     * @param count number of chars to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not unsigned char.
     */
    void EventBuilder::appendUCharData(std::shared_ptr<BaseStructure> structure, unsigned char* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::UCHAR8) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getUCharData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateUCharData();
        setAllHeaderLengths();
    }


    /**
     * Append byte data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of int8_t) to append.
     * @param count number of chars to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not char.
     */
    void EventBuilder::appendByteData(std::shared_ptr<BaseStructure> structure, int8_t* data, size_t count) {
        appendCharData(structure, reinterpret_cast<char *>(data), count);
    }


    /**
     * Append unsigned byte data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of uint8_t) to append.
     * @param count number of chars to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not unsigned char.
     */
    void EventBuilder::appendUByteData(std::shared_ptr<BaseStructure> structure, uint8_t* data, size_t count) {
        appendUCharData(structure, reinterpret_cast<unsigned char *>(data), count);
    }


    /**
     * Append float data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of floats) to append.
     * @param count number of floats to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not float.
     */
    void EventBuilder::appendFloatData(std::shared_ptr<BaseStructure> structure, float* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::FLOAT32) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getFloatData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateFloatData();
        setAllHeaderLengths();
    }


    /**
     * Append double data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of doubles) to append.
     * @param count number of doubles to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not double.
     */
    void EventBuilder::appendDoubleData(std::shared_ptr<BaseStructure> structure, double* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::DOUBLE64) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getDoubleData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateDoubleData();
        setAllHeaderLengths();
    }


    /**
     * Append string data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of string) to append.
     * @param count number of strings to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not string.
     */
    void EventBuilder::appendStringData(std::shared_ptr<BaseStructure> structure, std::string* data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::CHARSTAR8) {
            throw EvioException("cannot append ints to structure of type " + structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getStringData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateStringData();
        setAllHeaderLengths();
    }


    /**
     * Append composite data to the structure. If the structure has no data, then this
     * is the same as setting the data (except the the data type may not be changed).
     * @param structure the structure to receive the data.
     * @param data pointer to data (array of CompositeData shared pointers) to append.
     * @param count number of strings to append.
     * @throws EvioException if structure or data arg(s) is null, data type is not string.
     */
    void EventBuilder::appendCompositeData(std::shared_ptr<BaseStructure> structure,
                                           std::shared_ptr<CompositeData> * data, size_t count) {
        if (structure == nullptr && data != nullptr) {
            throw EvioException("either structure or data arg is null");
        }

        if (structure->getHeader()->getDataType() != DataType::COMPOSITE) {
            throw EvioException("cannot append ints to structure of type " +
                                structure->getHeader()->getDataType().getName());
        }

        auto & vect = structure->getCompositeData();
        for (size_t i=0; i < count; i++) {
            vect.push_back(data[i]);
        }
        structure->updateCompositeData();
        setAllHeaderLengths();
    }


    /**
     * Get the underlying event.
     * @return the underlying event.
     */
    std::shared_ptr<EvioEvent> EventBuilder::getEvent() {return event;}

    /**
     * Set the underlying event. As far as this event builder is concerned, the
     * previous underlying event is lost, and all subsequent calls will affect
     * the newly supplied event.
     * @param ev the new underlying event.
     */
    void EventBuilder::setEvent(std::shared_ptr<EvioEvent> ev) {event = ev;}


    //---------------------------------------------------------------------------------


    /**
     * Main program for testing.
     * @param argc ignored command line count.
     * @param argv ignored command line arguments.
     */
    int EventBuilder::main(int argc, char **argv) {
        //create an event writer to write out the test events.
        std::string outfile = "C:\\Documents and Settings\\heddle\\My Documents\\test.ev";
        EventWriter eventWriter(outfile);

        //count the events we make for testing
        uint8_t eventNumber = 1;

        //use a tag of 11 for events--for no particular reason
        uint16_t tag = 11;

        try {
            // first event-- a trivial event containing an array of ints.
            EventBuilder eventBuilder(tag, DataType::INT32, eventNumber++);
            auto event1 = eventBuilder.getEvent();
            // should end up with int array 1..25,1..10
            uint32_t intData[25];
            fakeIntArray(intData, 25);
            eventBuilder.appendUIntData(event1, intData, 25);
            eventBuilder.appendUIntData(event1, intData, 10);
            eventWriter.writeEvent(static_cast<std::shared_ptr<EvioBank>>(event1));

            // second event, more traditional bank of banks
            EventBuilder eventBuilder2(tag, DataType::BANK, eventNumber++);
            auto event2 = eventBuilder2.getEvent();

            // add a bank of doubles
            auto bank1 = EvioBank::getInstance(22, DataType::DOUBLE64, 0);
            double dData[10];
            fakeDoubleArray(dData, 10);
            eventBuilder2.appendDoubleData(bank1, dData, 10);
            eventBuilder2.addChild(event2, bank1);
            eventWriter.writeEvent(event2);

            // lets modify event2
            event2->getHeader()->setNumber(eventNumber++);
            auto bank2 = EvioBank::getInstance(33, DataType::BANK, 0);
            eventBuilder.addChild(event2, bank2);

            auto subBank1 = EvioBank::getInstance(34, DataType::SHORT16, 1);
            eventBuilder.addChild(bank2, subBank1);
            uint16_t sData[5];
            fakeShortArray(sData, 5);
            eventBuilder.appendUShortData(subBank1, sData, 5);



            //now add a bank of segments
            auto subBank2 = EvioBank::getInstance(33, DataType::SEGMENT, 0);
            eventBuilder.addChild(bank2, subBank2);

            auto segment1 = EvioSegment::getInstance(34, DataType::SHORT16);
            eventBuilder.addChild(subBank2, segment1);
            uint16_t ssData[7];
            fakeShortArray(ssData, 7);
            eventBuilder.appendUShortData(segment1, ssData, 7);

            auto segment2 = EvioSegment::getInstance(34, DataType::SHORT16);
            eventBuilder.addChild(subBank2, segment2);
            uint16_t sssData[10];
            fakeShortArray(sssData, 10);
            eventBuilder.appendUShortData(segment2, sssData, 10);


            //now add a bank of tag segments
            auto subBank3 = EvioBank::getInstance(45, DataType::TAGSEGMENT, 0);
            eventBuilder.addChild(bank2, subBank3);

            auto tagsegment1 = EvioTagSegment::getInstance(34, DataType::INT32);
            eventBuilder.addChild(subBank3, tagsegment1);
            uint32_t iiData[3];
            fakeIntArray(iiData, 3);
            eventBuilder.appendUIntData(tagsegment1, iiData, 3);

            auto tagsegment2 = EvioTagSegment::getInstance(34, DataType::CHARSTAR8);
            eventBuilder.addChild(subBank3, tagsegment2);
            std::string *strData = fakeStringArray();
            eventBuilder.appendStringData(tagsegment2, strData, 7);


//			System.err.println("EVENT2: " + event2.getHeader());
//			System.err.println("BANK1: " + bank1.getHeader());
//			System.err.println("BANK2: " + bank2.getHeader());
//			System.err.println("SUBBANK1: " + subBank1.getHeader());
//			System.err.println("SUBBANK2: " + subBank2.getHeader());
//			System.err.println("segment1: " + segment1.getHeader());
//			System.err.println("segment2: " + segment2.getHeader());
//			System.err.println("SUBBANK3: " + subBank3.getHeader());
//			System.err.println("tagseg1: " + tagsegment1.getHeader());


            //write the event
            eventWriter.writeEvent(event2);

            //all done
            eventWriter.close();

        }
        catch (EvioException & e) {
            std::cout << e.what() << std::endl;
        }

        std::cout << "Test completed" << std::endl;
        return 0;
    }


    /**
     * Fill array of ints, sequential, 1..size, for test purposes.
     * @param array pointer to int array.
     * @param size the size of the array.
     */
    void EventBuilder::fakeIntArray(uint32_t* array, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            array[i] = i+1;
        }
    }


    /**
     * Fill array of shorts, sequential, 1..size, for test purposes.
     * @param array pointer to short array.
     * @param size the size of the array.
     */
    void EventBuilder::fakeShortArray(uint16_t* array, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            array[i] = (short)(i+1);
        }
    }


    /**
     * Return array of strings for test purposes. Size is 7 strings.
     * @param the static string array.
     */
    std::string* EventBuilder::fakeStringArray() {
        static std::string array[] = {"This", " ", "is" , " ", "string", " ", "data"};
        return array;
    }


    /**
     * Fill array of doubles, sequential, 1..size, for test purposes.
     * @param array pointer to double array.
     * @param size the size of the array.
     */
    void EventBuilder::fakeDoubleArray(double *array, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            array[i] = i+1;
        }
    }

}

