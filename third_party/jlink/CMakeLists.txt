#
#  Copyright (c) 2020, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

string(REPLACE "-Wundef" "" OT_CFLAGS "${OT_CFLAGS}")
add_library(jlinkrtt
    SEGGER_RTT_V640/RTT/SEGGER_RTT.c
)

if(OT_PLATFORM STREQUAL "nrf52811")
    target_compile_options(jlinkrtt PRIVATE
        -DNRF52811_XXAA
    )
elseif(OT_PLATFORM STREQUAL "nrf52833")
    target_compile_options(jlinkrtt PRIVATE
        -DNRF52833_XXAA
    )
else()
    target_compile_options(jlinkrtt PRIVATE
        -DNRF52840_XXAA
    )
endif()

option(RTT_BUFFER_SIZE "sets the RTT up buffer size")
if(RTT_BUFFER_SIZE)
    target_compile_options(jlinkrtt PRIVATE
        -DSEGGER_RTT_CONFIG_BUFFER_SIZE_UP=${RTT_BUFFER_SIZE}
    )
endif()

target_compile_options(jlinkrtt PRIVATE
    -DSEGGER_RTT_CONFIG_H=\"${PROJECT_SOURCE_DIR}/third_party/NordicSemiconductor/segger_rtt/SEGGER_RTT_Conf.h\"
    -DUSE_APP_CONFIG=1
)

target_include_directories(jlinkrtt PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party/NordicSemiconductor/cmsis
    ${PROJECT_SOURCE_DIR}/third_party/NordicSemiconductor/config
    ${PROJECT_SOURCE_DIR}/third_party/NordicSemiconductor/config/${NRF_PLATFORM}/config
    ${PROJECT_SOURCE_DIR}/third_party/NordicSemiconductor/dependencies
    ${PROJECT_SOURCE_DIR}/third_party/NordicSemiconductor/libraries/app_error
    ${PROJECT_SOURCE_DIR}/third_party/NordicSemiconductor/nrfx/mdk
)
