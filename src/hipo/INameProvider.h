/**
 * Copyright (c) 2020, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/1/2020
 * @author heddle
 * @author timmer
 */


#include <string>
#include "BaseStructure.h"

#ifndef EVIO_6_0_INAMEPROVIDER_H
#define EVIO_6_0_INAMEPROVIDER_H


namespace evio {


    /**
     * This interface must be implemented by dictionary readers. For example, a dictionary reader that parses standard CODA
     * dictionary plain text files, or a dictionary reader that processes the xml dictionary Maurizio uses for GEMC.
     *
     * @author heddle (original java)
     * @author timmer
     *
     */
    class INameProvider {

    public:

            /**
             * A string used to indicate that no name can be determined.
             */
            static const std::string NO_NAME_STRING;

            /**
             * Returns the pretty name of some evio structure. Typically this is involve
             * the use of the "tag" and, if present, "num" fields. There may also be a hierarchical
             * dependence.
             *
             * @param structure the structure to find the name of.
             * @return a descriptive name, e.g., "Edep".
             */
            virtual std::string getName(BaseStructure & structure) = 0;
    };

    const std::string INameProvider::NO_NAME_STRING = "???";
}


#endif //EVIO_6_0_INAMEPROVIDER_H
