//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

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
     * (aka logical record, aka buffer) and must be a bank of banks.
     *
     * The Event trivially extends bank, just so there can be a distinct <code>EvioEvent</code> class, for clarity.
     *
     * @author heddle (original Java file).
     * @author timmer
     * @date 5/19/2020
     */
    class EvioEvent : public EvioBank {

    private:

        explicit EvioEvent(std::shared_ptr<BankHeader> const & head) : EvioBank(head) {
                std::cout << "EvioEvent's private constructor" << std::endl;
        }

    public:

        static std::shared_ptr<EvioEvent> getInstance(std::shared_ptr<BankHeader> const & head) {
            std::shared_ptr<EvioEvent> pNode(new EvioEvent(head));
            return pNode;
        }

        static std::shared_ptr<EvioEvent> getInstance(uint32_t tag, DataType typ, uint32_t num) {
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
        string dictionaryXML{""};

    protected:

        /** Has this been parsed yet or not? */
        bool parsed = false;
    public:
        void setParsed(bool p) {parsed = p;}
        bool isParsed() const {return parsed;}

    public:

//        /**
//         * Deep clone of BaseStructure does most of the work.
//         * Leave EvioBank's attachment as a reference. Only
//         * thing we need to do get rid of any cloned treeModel.
//         */
//        Object clone() {
//            EvioEvent ev = (EvioEvent) super.clone();
//
//            // A treeModel is only created & used if this event was parsed into existence.
//            // To avoid keeping this unused and transient object, just get rid of it.
//            treeModel = null;
//
//            return ev;
//        }

        /**
         * Is there an XML dictionary associated with this event?
         * @return <code>true</code> if there is an XML dictionary associated with this event,
         *         else <code>false</code>
         */
        bool hasDictionaryXML() {return !dictionaryXML.empty();}

        /**
         * Get the XML dictionary associated with this event if there is one.
         * @return the XML dictionary associated with this event if there is one, else null
         */
        string getDictionaryXML() {return dictionaryXML;}

        /**
         * Set the XML dictionary associated with this event.
         * @param xml the XML dictionary associated with this event.
         */
        void setDictionaryXML(string &xml) {dictionaryXML = xml;}

        /**
         * Obtain a string representation of the structure.
         * @return a string representation of the structure.
         */
        string toString() {

            stringstream ss;

            // show 0x for hex
            ss << showbase;

            StructureType stype = getStructureType();
            DataType dtype = header->getDataType();

            string description = getDescription();
            // TODO::::
//    if (INameProvider.NO_NAME_STRING.equals(description)) {
//        description = "";
//    }

            string sb;
            sb.reserve(100);

            if (!description.empty()) {
                ss << "<html><b>" << description << "</b>";
            }
            else {
                ss << "<Event> has " << dtype.toString() << "s:  tag=" << header->getTag();
                ss << hex << "(" << header->getTag() << ")" << dec;

                if (stype == StructureType::STRUCT_BANK) {
                    ss << "  num=" << header->getNumber() << hex << "(" << header->getNumber() << ")" << dec;
                }
            }

            if (rawBytes.empty()) {
                ss << "  dataLen=" << ((header->getLength() - (header->getHeaderLength() - 1))/4);
            }
            else {
                ss << "  dataLen=" << (rawBytes.size()/4);
            }

            if (header->getPadding() != 0) {
                ss << "  pad=" << header->getPadding();
            }

            int numChildren = children.size();

            if (numChildren > 0) {
                ss << "  children=" << numChildren;
            }

            if (!description.empty()) {
                ss << "</html>";
            }

            return ss.str();
        }

//        /**
//         * Inserts a child structure into the event's tree. This is called when the event is being parsed,
//         * and when an event is being created.
//         * @param child the child structure being added to the tree.
//         * @param parent the parent structure of the new child.
//         */
//        void insert(std::shared_ptr<BaseStructure> const & child,
//                    std::shared_ptr<BaseStructure> const & parent) {
//            parent->insert(child, parent->getChildCount());
//        }

        /**
         * This returns a number [1..] indicating which event this was in the event file from which
         * it was read. It is not the "num" field from the header.
         * @return a number [1..] indicating which event this was in the event file from which
         * it was read.
         */
        size_t getEventNumber() {return eventNumber;}

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
