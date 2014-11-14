/** @file
    @brief Implementation

    @date 2014

    @author
    Ryan Pavlik
    <ryan@sensics.com>
    <http://sensics.com>
*/

// Copyright 2014 Sensics, Inc.
//
// All rights reserved.
//
// (Final version intended to be licensed under
// the Apache License, Version 2.0)

// Internal Includes
#include <ogvr/ClientKit/InterfaceCallbackC.h>
#include <ogvr/Client/ClientInterface.h>

// Library/third-party includes
// - none

// Standard includes
// - none

OGVR_ReturnCode ogvrRegisterPoseCallback(OGVR_ClientInterface iface,
                                         OGVR_PoseCallback cb, void *userdata) {
    iface->registerCallback(cb, userdata);
    return OGVR_RETURN_SUCCESS;
}