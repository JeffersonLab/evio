//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIOEXCEPTION_H
#define EVIO_6_0_EVIOEXCEPTION_H


#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <exception>


namespace evio {

    /**
     * Exception class for Evio software package.
     * @date 04/10/2019
     * @author timmer
     */
    class EvioException : public std::runtime_error {

    public:

        /**
         * Constructor.
         * @param msg error message.
         */
        explicit EvioException(const std::string & msg) noexcept : std::runtime_error(msg) {}

        /**
         * Constructor.
         * @param ex exception that caused this one.
         */
        explicit EvioException(const std::exception & ex) noexcept : std::runtime_error(ex.what()) {}

        /**
         * Constructor.
         * @param msg error message.
         * @param file name of file exception occurred.
         * @param line file line exception occurred.
         */
        EvioException(const std::string & msg, const char *file, int line) noexcept : std::runtime_error(msg) {
            std::ostringstream o;
            o << file << ":" << line << ":" << msg;
            std::cerr << o.str() << std::endl;
        }

    };

/** Macro that throws an exception with given message and file, line info. */
#define throwEvioLine(arg) throw EvioException(arg, __FILE__, __LINE__);

}

#endif //EVIO_6_0_EVIOEXCEPTION_H
