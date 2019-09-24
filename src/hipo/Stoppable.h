//
// Created by Carl Timmer on 2019-05-15.
//

#ifndef EVIO_6_0_STOPPABLE_H
#define EVIO_6_0_STOPPABLE_H


#include <chrono>
#include <iostream>
#include <future>

/*
 * Class that encapsulates promise and future objects in order to
 * provide an API to set a signal to stop a thread.
 */
class Stoppable  {

private:

    std::promise<void> exitSignal;
    std::future<void>  futureObj;

public:

    Stoppable() : futureObj(exitSignal.get_future()) {}

    Stoppable(Stoppable && obj) noexcept :
            exitSignal(std::move(obj.exitSignal)), futureObj(std::move(obj.futureObj)) {
        //std::cout << "Move Constructor is called" << std::endl;
    }

    Stoppable & operator=(Stoppable && obj) noexcept {
        // Avoid self assignment ...
        if (this != &obj) {
            //std::cout << "Move Assignment is called" << std::endl;
            exitSignal = std::move(obj.exitSignal);
            futureObj  = std::move(obj.futureObj);
        }
        return *this;
    }

    // Task needed to provide definition for this function
    // which will be called by thread function.
    virtual void run() = 0;

    // Thread function to be executed by thread
    void operator()() {
        run();
    }

    /** Checks if thread has been asked to stop. */
    bool stopRequested() {
        // checks if value in future object is available
        if (futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
            return false;
        return true;
    }

    /** Request to stop the thread by setting value in promise object. */
    void stop() {
        exitSignal.set_value();
    }
};

//---------------------------------------
// Example of how this class is used
//---------------------------------------


//
///*
// * A Task class that extends the Stoppable Task
// */
//class MyTask: public Stoppable
//{
//public:
//    // Function to be executed by thread function
//    void run()
//    {
//        std::cout << "Task Start" << std::endl;
//
//        // Check if thread is requested to stop ?
//        while (stopRequested() == false)
//        {
//            std::cout << "Doing Some Work" << std::endl;
//            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//
//        }
//        std::cout << "Task End" << std::endl;
//    }
//};
//
//int main()
//{
//    // Creating our Task
//    MyTask task;
//
//    //Creating a thread to execute our task
//    std::thread th([&]()
//                   {
//                       task.run();
//                   });
//
//    std::this_thread::sleep_for(std::chrono::seconds(10));
//
//    std::cout << "Asking Task to Stop" << std::endl;
//    // Stop the Task
//    task.stop();
//
//    //Waiting for thread to join
//    th.join();
//    std::cout << "Thread Joined" << std::endl;
//    std::cout << "Exiting Main Function" << std::endl;
//    return 0;
//}

#endif //EVIO_6_0_STOPPABLE_H

