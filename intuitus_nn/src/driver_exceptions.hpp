#ifndef DRIVER_EXPCEPTIONS_HPP
#define DRIVER_EXPCEPTIONS_HPP

#include <iostream>
#include <string>
#include <errno.h>
#include <stdio.h>
#include <string.h>

using namespace std;

class DMA_Exception
{

public:
    DMA_Exception(int err_no, const char* msg) : msg_(msg), code(err_no) {}
    ~DMA_Exception() {}

    const char* getMessage() const { return (msg_); }
    int getCode() const { return (code); }

private:
    const char* msg_;
    int code;
};

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#ifdef NDEBUG // If no debugging is enabled, remove all debugging statements
#define debug(msg, ...)
#else
/**
 * @brief Prints a debug message. Suppress it by defining the NDEBUG preprocess
 * constant @param msg The message that should be printed, including optional
 * parameters
 */
#define debug(msg, ...) \
    fprintf(stderr, "[DEBUG] %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

/**
 * @brief Returns the compile time name of the passed variable as string
 * @param Variable The variable
 */
#define GET_VARIABLE_NAME(Variable) #Variable

#ifdef _MSC_VER // Work around for Windows
#define clean_errno() (errno == 0 ? "None" : "strerror() not available")
#else
#define clean_errno() (errno == 0 ? "None" : strerror(errno))
#endif // _MSC_VER

#define log_err(C,M, ...) \
    fprintf(stderr, "[ERROR] (%s:%d: errno: %d) %s\n", __FILE__, __LINE__, C,M,##__VA_ARGS__)
#define log_warn(C,M, ...) \
    fprintf(stderr, "[WARN] (%s:%d: errno: %d) %s\n", __FILE__, __LINE__, C,M,##__VA_ARGS__)
#define log_info(M, ...) \
    fprintf(stderr, "[INFO] (%s:%d) %s\n", __FILE__, __LINE__,M,##__VA_ARGS__)

/**
 * @brief Check if a condition holds and goto an error routine if not
 * @param A The condition that should be checked
 * @param C The code that is return on exit 
 * @param M The message that should be printed in case of an error
 */
#define CHECK(A, C, M, ...)                   \
    try                                       \
    {                                         \
        if (!(A))                             \
        {                                     \
            throw(DMA_Exception(C, M));       \
        }                                     \
    }                                         \
    catch (DMA_Exception & e)                 \
    {                                         \
        log_err(e.getCode(),e.getMessage(), ##__VA_ARGS__); \
        return e.getCode();                   \
    }

/**
 * @brief Check if a condition holds and goto an error routine if not
 * @param A The condition that should be checked
 * @param C The code that is return on exit 
 * @param M The message that should be printed in case of an error
 */
#define CHECK_AND_EXIT(A, C, M, ...)          \
    try                                       \
    {                                         \
        if (!(A))                             \
        {                                     \
            throw(DMA_Exception(C, M));       \
        }                                     \
    }                                         \
    catch (DMA_Exception & e)                 \
    {                                         \
        log_err(e.getCode(),e.getMessage(), ##__VA_ARGS__); \
        exit(e.getCode());                    \
    }

/**
 * @brief Check if a variable is not null. If it is null, an error message is
 * printed and the program exits with error code C  
 */
#define CHECK_NOT_NULL(var, C) CHECK((var) != NULL, C, "%s must not be null", GET_VARIABLE_NAME(var))

/**
 * @brief Check if a condition holds and log warning message 
 * @note Same as "CHECK" but with the difference, that the message is only
 * printed and the program does not exit 
 * @param A The condition that should be checked ´
 * @param C The code that is return on exit 
 * @param M The message that should be printed
 */
#define CHECK_WARNING(A, C, M, ...)            \
    try                                        \
    {                                          \
        if (!(A))                              \
        {                                      \
            throw(DMA_Exception(C, M));        \
        }                                      \
    }                                          \
    catch (DMA_Exception & e)                  \
    {                                          \
        log_warn(e.getCode(),e.getMessage(), ##__VA_ARGS__); \
    }
#define CHECK_INFO(A, C, M, ...)               \
    try                                        \
    {                                          \
        if (!(A))                              \
        {                                      \
            throw(DMA_Exception(C, M));        \
        }                                      \
    }                                          \
    catch (DMA_Exception & e)                  \
    {                                          \
        log_info(e.getMessage(), ##__VA_ARGS__); \
    }

#endif