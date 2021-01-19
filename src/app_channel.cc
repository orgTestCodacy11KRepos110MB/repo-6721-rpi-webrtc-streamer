/*
Copyright (c) 2017, rpi-webrtc-streamer Lyu,KeunChang

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "app_channel.h"

#include <iostream>
#include <vector>

#include "utils.h"
#include "websocket_server.h"

////////////////////////////////////////////////////////////////////////////////
//
// Application Channel
//
////////////////////////////////////////////////////////////////////////////////
AppChannel::AppChannel() : is_inited_(false) {}

bool AppChannel::AppInitialize(StreamerProxy* proxy,
                               ConfigStreamer& config_streamer,
                               ConfigMotion& config_motion) {
    std::string rws_url_path;
    int port_num;
    std::string web_root;

    // LibWebSocket debug log
    if (config_streamer.GetLwsDebugEnable()) {
        RTC_LOG(INFO) << "enabling debug logging message of websocket library";
        LogLevel(LibWebSocketServer::DEBUG_LEVEL_ALL);
    };

    // need to initialize the motion mount after WebRoot initialization.
    web_root = config_streamer.GetWebRootPath();
    RTC_LOG(INFO) << "Using http file mapping : " << web_root;
    RTC_LOG(INFO) << "Using motion video mapping : "
                  << config_motion.GetDirectory();
    AddHttpWebMount(config_motion.GetDetectionEnable(), web_root,
                    config_motion.GetDirectory());

    port_num = config_streamer.GetWebSocketPort();
    RTC_LOG(INFO) << "WebSocket port num : " << port_num;
    if (Init(port_num) == false) return false;

    ws_client_.reset(new AppWsClient(proxy));

    rws_url_path = config_streamer.GetRwsWsUrlPath();
    RTC_LOG(INFO) << "Using RWS WS client url : " << rws_url_path;
    ws_client_->RegisterWebSocketMessage(this);
    ws_client_->RegisterConfigStreamer(&config_streamer);
    AddWebSocketHandler(rws_url_path, SINGLE_INSTANCE, ws_client_.get());

    is_inited_ = true;
    return true;
}

AppChannel::~AppChannel() {}
