ISO-TP (ISO 15765-2) Support Library in C
================================

**This project is inspired by [openxc isotp-c](https://github.com/openxc/isotp-c), but the code has been completely re-written.**

This is a platform agnostic C library that implements the [ISO 15765-2](https://en.wikipedia.org/wiki/ISO_15765-2) (also known as ISO-TP) protocol, which runs over a CAN bus. Quoting Wikipedia:

>ISO 15765-2, or ISO-TP, is an international standard for sending data packets over a CAN-Bus.
>The protocol allows for the transport of messages that exceed the eight byte maximum payload of CAN frames. 
>ISO-TP segments longer messages into multiple frames, adding metadata that allows the interpretation of individual frames and reassembly 
>into a complete message packet by the recipient. It can carry up to 4095 bytes of payload per message packet.

This library doesn't assume anything about the source of the ISO-TP messages or the underlying interface to CAN. It uses dependency injection to give you complete control.

**The current version supports [ISO-15765-2](https://en.wikipedia.org/wiki/ISO_15765-2) single and multiple frame transmition, and works in Full-duplex mode.**

## Builds

### Master Build
[![CMake](https://github.com/SimonCahill/isotp-c/actions/workflows/cmake.yml/badge.svg)](https://github.com/SimonCahill/isotp-c/actions/workflows/cmake.yml)

## Contributors

It's at this point where I'd like to point out all the fantastic contributions made to this fork by the amazing people using it!
[List of contributors](https://raw.githubusercontent.com/SimonCahill/isotp-c/master/CONTRIBUTORS.md)

Thank you all!

## Building ISOTP-C

This library may be built using either straight Makefiles, or using CMake.

### make
To build this library using Make, simply call:

```bash
$ make all
```

### CMake

The CMake build system allows for more flexibility at generation and build time, so it is recommended you use this for building this library.  
Of course, if your project does not use CMake, you don't *have* to use it.
If your projects use a different build system, you are more than welcome to include it in this repository.

The Makefile generator for isotpc will automatically detect whether or not your build system is using the `Debug` or `Release` build type and will adjust compiler parameters accordingly.

#### Debug Build
If your project is configured to build as `Debug`, then the library will be compiled with **no** optimisations and **with** debug symbols.  
`-DCMAKE_BUILD_TYPE=Debug`

#### Release Build
If your project is configured to build as `Release`, then the library code will be **optimised** using `-O2` and will be **stripped**.  
`-DCMAKE_BUILD_TYPE=Release`

#### External Include Directories
It is generally considered good practice to segregate header files from each other, depending on the project. For this reason, you may opt in to this behaviour for this library.  

If you pass `-Disotpc_USE_INCLUDE_DIR=ON` on the command-line, or you set `set(isotpc_USE_INCLUDE_DIR ON CACHE BOOL "Use external include dir for isotp-c")` in your CMakeLists.txt, then a separate `include/` directory will
be added to the project.  
This happens at generation time, and the CMake project will automatically reference `${CMAKE_CURRENT_BINARY_DIR}/include` as the include directory for the project. This will be propagated to your projects, too.

In your code:

```c
// if -Disotpc_USE_INCLUDE_DIR=ON
#include <isotp/isotp.h>

// else
#include <isotp.h>
```

#### Static Library
In some cases, it is required that a static library be used instead of a shared library.
isotp-c supports this also, via options.

Either pass `-Disotpc_STATIC_LIBRARY=ON` via command-line or `set(isotpc_STATIC_LIBRARY ON CACHE BOOL "Enable static library for isotp-c")` in your CMakeLists.txt and the library will be built as a static library (`*.a|*.lib`) for your project to include.

#### Inclusion in your CMake project
```cmake
###
# Set your desired options
###
set(isotpc_USE_INCLUDE_DIR ON CACHE BOOL "Use external include directory for isotp-c") # optional
set(isotpc_STATIC_LIBRARY ON CACHE BOOL "Build isotp-c as a static library instead of shared") # optional

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/path/to/isotp-c) # add to current project

target_link_libraries(
    mytarget

    # ... other libs
    simon_cahill::isotp_c
)
```


## Usage

First, create some [shim](https://en.wikipedia.org/wiki/Shim_(computing)) functions to let this library use your lower level system:

```C
    /* required, this must send a single CAN message with the given arbitration
     * ID (i.e. the CAN message ID) and data. The size will never be more than 8
     * bytes. Should return ISOTP_RET_OK if frame sent successfully.
     * May return ISOTP_RET_NOSPACE if the frame could not be sent but may be
     * retried later. Should return ISOTP_RET_ERROR in case frame could not be sent.
     */
    int  isotp_user_send_can(const uint32_t arbitration_id,
                             const uint8_t* data, const uint8_t size) {
        // ...
    }

    /* required, return system tick, unit is micro-second */
    uint32_t isotp_user_get_us(void) {
        // ...
    }
    
    /* optional, provide to receive debugging log messages */
    void isotp_user_debug(const char* message, ...) {
        // ...
    }
```

### API

You can use isotp-c in the following way:

```C
    /* Alloc IsoTpLink statically in RAM */
    static IsoTpLink g_link;

	/* Alloc send and receive buffer statically in RAM */
    static uint8_t g_isotpRecvBuf[ISOTP_BUFSIZE];
    static uint8_t g_isotpSendBuf[ISOTP_BUFSIZE];
	
    int main(void) {
        /* Initialize CAN and other peripherals */
        
        /* Initialize link, 0x7TT is the CAN ID you send with */
        isotp_init_link(&g_link, 0x7TT,
						g_isotpSendBuf, sizeof(g_isotpSendBuf), 
						g_isotpRecvBuf, sizeof(g_isotpRecvBuf));
        
        while(1) {
        
            /* If receive any interested can message, call isotp_on_can_message to handle message */
            ret = can_receive(&id, &data, &len);
            
            /* 0x7RR is CAN ID you want to receive */
            if (RET_OK == ret && 0x7RR == id) {
                isotp_on_can_message(&g_link, data, len);
            }
            
            /* Poll link to handle multiple frame transmition */
            isotp_poll(&g_link);
            
            /* You can receive message with isotp_receive.
               payload is upper layer message buffer, usually UDS;
               payload_size is payload buffer size;
               out_size is the actuall read size;
               */
            ret = isotp_receive(&g_link, payload, payload_size, &out_size);
            if (ISOTP_RET_OK == ret) {
                /* Handle received message */
            }
            
            /* And send message with isotp_send */
            ret = isotp_send(&g_link, payload, payload_size);
            if (ISOTP_RET_OK == ret) {
                /* Send ok */
            } else {
                /* An error occured */
            }
            
            /* In case you want to send data w/ functional addressing, use isotp_send_with_id */
            ret = isotp_send_with_id(&g_link, 0x7df, payload, payload_size);
            if (ISOTP_RET_OK == ret) {
                /* Send ok */
            } else {
                /* Error occur */
            }
        }

        return;
    }
```
    
You can call isotp_poll as frequently as you want, as it internally uses isotp_user_get_ms to measure timeout occurences.
If you need handle functional addressing, you must use two separate links, one for each.

```C
    /* Alloc IsoTpLink statically in RAM */
    static IsoTpLink g_phylink;
    static IsoTpLink g_funclink;

	/* Allocate send and receive buffer statically in RAM */
	static uint8_t g_isotpPhyRecvBuf[512];
	static uint8_t g_isotpPhySendBuf[512];
	/* currently functional addressing is not supported with multi-frame messages */
	static uint8_t g_isotpFuncRecvBuf[8];
	static uint8_t g_isotpFuncSendBuf[8];	
	
    int main(void) {
        /* Initialize CAN and other peripherals */
        
        /* Initialize link, 0x7TT is the CAN ID you send with */
        isotp_init_link(&g_phylink, 0x7TT,
						g_isotpPhySendBuf, sizeof(g_isotpPhySendBuf), 
						g_isotpPhyRecvBuf, sizeof(g_isotpPhyRecvBuf));
        isotp_init_link(&g_funclink, 0x7TT,
						g_isotpFuncSendBuf, sizeof(g_isotpFuncSendBuf), 
						g_isotpFuncRecvBuf, sizeof(g_isotpFuncRecvBuf));
        
        while(1) {
        
            /* If any CAN messages are received, which are of interest, call isotp_on_can_message to handle the message */
            ret = can_receive(&id, &data, &len);
            
            /* 0x7RR is CAN ID you want to receive */
            if (RET_OK == ret) {
                if (0x7RR == id) {
                    isotp_on_can_message(&g_phylink, data, len);
                } else if (0x7df == id) {
                    isotp_on_can_message(&g_funclink, data, len);
                }
            } 
            
            /* Poll link to handle multiple frame transmition */
            isotp_poll(&g_phylink);
            isotp_poll(&g_funclink);
            
            /* You can receive message with isotp_receive.
               payload is upper layer message buffer, usually UDS;
               payload_size is payload buffer size;
               out_size is the actuall read size;
               */
            ret = isotp_receive(&g_phylink, payload, payload_size, &out_size);
            if (ISOTP_RET_OK == ret) {
                /* Handle physical addressing message */
            }
            
            ret = isotp_receive(&g_funclink, payload, payload_size, &out_size);
            if (ISOTP_RET_OK == ret) {
                /* Handle functional addressing message */
            }            
            
            /* And send message with isotp_send */
            ret = isotp_send(&g_phylink, payload, payload_size);
            if (ISOTP_RET_OK == ret) {
                /* Send ok */
            } else {
                /* An error occured */
            }
        }

        return;
    }
```

## Authors

Please view [Contributors](#contributors) to see a list of all contributors.

## License

Licensed under the MIT license.
