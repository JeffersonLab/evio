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

#include <exception>

class HipoException : public std::exception {

protected:

    std::string errorMsg;

public:

    explicit HipoException(const std::string& msg) : errorMsg(msg) {}

    const char * what() {return errorMsg.c_str();}

};


#endif //EVIO_6_0_HIPOEXCEPTION_H
