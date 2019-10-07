/*
 * Copyright (c) 2019, Jefferson Science Associates, all rights reserved.
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000 Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 */

//
// Created by timmer on 4/10/19.
//

#ifndef EVIO_6_0_HIPOEXCEPTION_H
#define EVIO_6_0_HIPOEXCEPTION_H

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

class HipoException : public std::runtime_error {

private:

    std::string errorMsg;

public:

    explicit HipoException(const std::string & msg) noexcept : std::runtime_error(msg) {
        errorMsg = msg;
    }


    HipoException(const std::string & msg, const char *file, int line)
                                                    noexcept : std::runtime_error(msg) {
        std::ostringstream o;
        o << file << ":" << line << ":" << msg;
        errorMsg = o.str();
    }

     ~HipoException() override = default;

    const char* what() const noexcept override {return errorMsg.c_str();}

};

#define throwHipoLine(arg) throw HipoException(arg, __FILE__, __LINE__);


#endif //EVIO_6_0_HIPOEXCEPTION_H
