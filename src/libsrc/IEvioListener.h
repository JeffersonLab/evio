//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_IEVIOLISTENER_H
#define EVIO_6_0_IEVIOLISTENER_H


#include <memory>


namespace evio {

    // forward declaration so we can compile
    class BaseStructure;

    /**
     * In SAX like behavior, implementors will listen for structures encountered when an event is parsed.
     *
     * @author heddle (Original Java class)
     * @author timmer
     */
    class IEvioListener {

    public:

        /**
         * Called after a structure is read in while parsing or searching an event
         * and any filter has accepted it.
         *
         * NOTE: the user should NOT modify the arguments.
         *
         * @param topStructure the evio structure at the top of the search/parse
         * @param structure the full structure, including header
         */
        virtual void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                                  std::shared_ptr<BaseStructure> structure) = 0;

        /**
         * Starting to parse a new event structure.
         * @param structure the event structure in question.
         */
        virtual void startEventParse(std::shared_ptr<BaseStructure> structure) = 0;

        /**
         * Done parsing a new event structure.
         * @param structure the event structure in question.
         */
        virtual void endEventParse(std::shared_ptr<BaseStructure> structure) = 0;

    };

}

#endif //EVIO_6_0_IEVIOLISTENER_H
