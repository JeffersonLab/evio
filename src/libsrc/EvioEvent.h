//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIOEVENT_H
#define EVIO_6_0_EVIOEVENT_H

#include <cstring>
#include <sstream>
#include <memory>

#include "ByteBuffer.h"
#include "DataType.h"
#include "BaseStructure.h"
#include "BaseStructureHeader.h"
#include "EvioBank.h"
#include "BankHeader.h"
#include "StructureType.h"


namespace evio {

    /**
     * An event is really just the outer, primary bank. That is, the first structure in an event
     * (aka logical record, aka buffer) and is <b>usually</b> a bank of banks.
     *
     * The Event trivially extends bank, just so there can be a distinct <code>EvioEvent</code> class, for clarity.
     *
     * @author heddle (original Java file).
     * @author timmer
     * @date 5/19/2020
     */
    class EvioEvent : public EvioBank {

    private:

        /** Constructor. */
        EvioEvent() : EvioBank() {}

       /**
        * Constructor.
        * @param head bank header.
        */
        explicit EvioEvent(std::shared_ptr<BankHeader> const & head) : EvioBank(head) {}

    public:

        /**
          * Method to return a shared pointer to a constructed object of this class.
          * Header is filled with default values.
          * @return shared pointer to new EvioBank.
          */
        static std::shared_ptr<EvioEvent> getInstance() {
            std::shared_ptr<EvioEvent> pNode(new EvioEvent());
            return pNode;
        }

        /**
        * Method to return a shared pointer to a constructed object of this class.
        * @param head bank header.
        * @return shared pointer to new EvioEvent.
        */
        static std::shared_ptr<EvioEvent> getInstance(std::shared_ptr<BankHeader> const & head) {
            std::shared_ptr<EvioEvent> pNode(new EvioEvent(head));
            return pNode;
        }

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param tag bank tag.
         * @param typ bank data type.
         * @param num bank num.
         * @return shared pointer to new EvioEvent.
         */
        static std::shared_ptr<EvioEvent> getInstance(uint16_t tag, DataType const & typ, uint8_t num) {
            std::shared_ptr<BankHeader> head(new BankHeader(tag, typ, num));
            std::shared_ptr<EvioEvent> pNode(new EvioEvent(head));
            return pNode;
        }

    private:

        /**
         * This is not the "num" field from the header, but rather a number [1..] indicating
         * which event this was in the event file from which it was read.
         */
        size_t eventNumber = 0;

        /** There may be a dictionary in xml associated with this event. Or there may not. */
        std::string dictionaryXML{""};

    protected:

        /** Has this been parsed yet or not? */
        bool parsed = false;

    public:

        /**
         * Method to set if this event has been parsed or not.
         * @param p true if parsed, else false.
         */
        void setParsed(bool p) {parsed = p;}

        /**
         * Has this object been parsed or not.
         * @return true if this object has been parsed, else false.
         */
        bool isParsed() const {return parsed;}

        /**
         * Is there an XML dictionary associated with this event?
         * @return <code>true</code> if there is an XML dictionary associated with this event,
         *         else <code>false</code>
         */
        bool hasDictionaryXML() const {return !dictionaryXML.empty();}

        /**
         * Get the XML dictionary associated with this event if there is one.
         * @return the XML dictionary associated with this event if there is one, else null
         */
        std::string getDictionaryXML() const {return dictionaryXML;}

        /**
         * Set the XML dictionary associated with this event.
         * @param xml the XML dictionary associated with this event.
         */
        void setDictionaryXML(std::string &xml) {dictionaryXML = xml;}

        /**
         * Obtain a string representation of the structure.
         * @return a string representation of the structure.
         */
        std::string toString() const override {

            std::stringstream ss;

            // show 0x for hex
            ss << std::showbase;

            StructureType stype = getStructureType();
            DataType dtype = header->getDataType();

            std::string sb;
            sb.reserve(100);

            ss << "<Event> has " << dtype.toString() << "s:  tag=" << header->getTag();
            ss << std::hex << "(" << header->getTag() << ")" << std::dec;

            if (stype == StructureType::STRUCT_BANK) {
                ss << "  num=" << (int)(header->getNumber()) << std::hex << "(" << (int)(header->getNumber()) << ")" << std::dec;
            }

            if (rawBytes.empty()) {
                ss << "  dataLen=" << ((header->getLength() - (header->getHeaderLength() - 1))/4);
            }
            else {
                ss << "  dataLen=" << (rawBytes.size()/4);
            }

            if (header->getPadding() != 0) {
                ss << "  pad=" << (int)(header->getPadding());
            }

            int numChildren = children.size();

            if (numChildren > 0) {
                ss << "  children=" << numChildren;
            }

            return ss.str();
        }

        /**
         * This returns a number [1..] indicating which event this was in the event file from which
         * it was read. It is not the "num" field from the header.
         * @return a number [1..] indicating which event this was in the event file from which
         * it was read.
         */
        size_t getEventNumber() const {return eventNumber;}

        /**
         * This sets a number [1..] indicating which event this was in the event file from which
         * it was read. It is not the "num" field from the header.
         * @param evNum a number [1..] indicating which event this was in the event file from which
         * it was read.
         */
        void setEventNumber(size_t evNum) {eventNumber = evNum;}

    };
}

#endif //EVIO_6_0_EVIOEVENT_H
