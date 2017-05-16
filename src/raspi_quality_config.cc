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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "limits.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

#include "raspi_quality_config.h"
#include "default_config.h"


static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 35;
static const int kPacketLossThreshold = 8;   // Approximately 3.1%
static const int kRttMaxThreshold = 200;     // 200 ms maximum
static const int kAverageDuration =  3 * 30; // Approximately 30 samples per second 

// 
static const float kKushGaugeConstant = 0.07;
static const int kMaxMontionFactor = 3;  
static const int kMinMontionFactor = 1;   //  original value is 1 ~ 4
                                          // but quality config will use up to 3

static const int kMaxpFrameRate = 30; // using 30 as raspberry pi max FPS


QualityConfig::ResolutionConfigEntry::ResolutionConfigEntry (int width, 
        int height, int min_fps, int max_fps) 
    : width_(width), height_(height), max_fps_(max_fps), min_fps_(min_fps) {

    max_bandwidth_  = static_cast<int>((width_ * height_ * max_fps * 
                kKushGaugeConstant * kMaxMontionFactor )/1000);
    min_bandwidth_  = static_cast<int>((width_ * height_ * min_fps * 
                kKushGaugeConstant)/1000);
    average_bandwidth_ = static_cast<int>((max_bandwidth_+min_bandwidth_)/2);
}

QualityConfig::QualityConfig() 
    : target_framerate_(0), target_bitrate_(0),
    packet_loss_(3 * 30), rtt_(3 * 30), average_qp_(3 * 30) {

    use_dynamic_resolution_  =  default_config::use_dynamic_video_resolution;
    use_initial_resolution_  =  default_config::use_initial_video_resolution;
    use_4_3_resolution_  =  default_config::resolution_4_3_enable;
    //  TODO Need to check these resolution have same FOV between resolutions
    if( use_4_3_resolution_ ) {
        // 4:3 resolution
        resolution_config_.push_back(ResolutionConfigEntry(320,240,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(400,300,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(512,384,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(640,480,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1024,768,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1152,864,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1296,972,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1640,1232,20,30));
    }
    else {  
        // 16:9 resolution
        resolution_config_.push_back(ResolutionConfigEntry(384,216,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(512,288,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(640,360,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(768,432,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(896,504,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1024,576,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1152,648,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1280,720,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1408,864,20,30));
        resolution_config_.push_back(ResolutionConfigEntry(1920,1080,20,30));
    }
}


QualityConfig::~QualityConfig(){
}


void QualityConfig::ReportQP(int qp){
    average_qp_.AddSample(qp);
    const rtc::Optional<int> avg_qp = average_qp_.GetAverage();
    if( avg_qp ){
        if (*avg_qp > kHighH264QpThreshold ) {
            adaptation_up_ = true;
        }
        else if (*avg_qp < kHighH264QpThreshold ) {
            adaptation_down_ = true;
        }
    }
}

void QualityConfig::ReportChannelParameters(uint32_t packet_loss, uint64_t rtt ){
    packet_loss_.AddSample(packet_loss);
    rtt_.AddSample(rtt);

    const rtc::Optional<int> packet_loss_avg = packet_loss_.GetAverage();
    if( packet_loss_avg ){
        if( *packet_loss_avg > kPacketLossThreshold ) {
            adaptation_down_ = true;
            return;
        }
    }

    const rtc::Optional<int> rtt_avg = rtt_.GetAverage();
    if( rtt_avg ){
        if( *rtt_avg > kRttMaxThreshold ) {
            adaptation_down_ = true;
            return;
        }
    }
}

void QualityConfig::ReportFrameRate(int framerate) {
    if( target_framerate_ == framerate ) return;
    LOG(INFO) << "FrameRate changed from " << target_framerate_ << ", to " 
        << framerate;
    target_framerate_ = framerate;
}

void QualityConfig::ReportMaxBitrate(int bitrate) {
    LOG(INFO) << "Setting Max Bitrate : " << bitrate;
    max_bitrate_ = bitrate;
}

void QualityConfig::ReportTargetBitrate(int bitrate) {
    if( bitrate == target_bitrate_ ) return;
    LOG(INFO) << "Bitrate changed from " << target_bitrate_ 
        << ", to " << bitrate ;
    target_bitrate_ = bitrate;
}

void QualityConfig::Reset() {
    packet_loss_.Reset();
    rtt_.Reset();
    average_qp_.Reset();
}

bool QualityConfig::IsAdaptationRequired() {
    return adaptation_up_ || adaptation_up_;
}

int QualityConfig::GetFrameRate(){
    if( target_framerate_ > kMaxpFrameRate ) 
        return kMaxpFrameRate;
    else 
        return target_framerate_;
}

int QualityConfig::GetBitrate(){
    return target_bitrate_;
}

bool QualityConfig::GetBestMatch(QualityConfig::Resolution& resolution) {
    return GetBestMatch(target_bitrate_, resolution);
}

bool QualityConfig::GetInitialBestMatch(QualityConfig::Resolution& resolution) {
    Resolution candidate;
    
    // The initial resolution will be used 
    // if the use default resolution flag is on.
    if( use_initial_resolution_ == true) {
        candidate.width_ = default_config::initial_video_resolution.width_;
        candidate.height_ = default_config::initial_video_resolution.height_;
        resolution = current_res_ = candidate;
        return true;
    }
    
    return GetBestMatch(target_bitrate_, resolution);
}

///////////////////////////////////////////////////////////////////////////////
// At present, it is based on the average value of Kush Gauge's 1 and 3 moving 
// factors, but there is no evaluation as to whether it is appropriate. 
// Simply find the nearest target_bitrate at the expected bitrate and change 
// the resolution to the resolution.
//
// TODO: Evalution this method is appropriate
// TODO: QP/LOSS/RTT 
//       Currently, Google is working on a lot of BWE related work, 
//       so it needs to be modified or implemented 
//       according to the implementation status of WebRTC Native Package.
//
////////////////////////////////////////////////////////////////////////////////
bool QualityConfig::GetBestMatch(int target_bitrate, 
        QualityConfig::Resolution& resolution) {
    Resolution candidate;
    int last_diff =  std::numeric_limits<int>::max();
    int diff = 0;

    target_bitrate_ = target_bitrate;

    if( use_dynamic_resolution_ == false ) {
        // The encoder does not use the Bitrate Estimation delivered by BWE, 
        // but keeps the initially generated resolution.
        resolution = current_res_;
        return false;   // Do not change resoltuion 
    };

    for( std::list<ResolutionConfigEntry>::iterator iter = resolution_config_.begin(); 
            iter != resolution_config_.end(); iter++) {
        diff = abs(iter->average_bandwidth_ - target_bitrate );
        if( last_diff > diff ) {    
            candidate.width_ = iter->width_;
            candidate.height_ = iter->height_;
            last_diff = diff;
        }
        else {
            resolution = candidate;
            if( resolution.width_ != current_res_.width_ ||
                resolution.height_ != current_res_.height_ ) {
                current_res_ = resolution;
                LOG(INFO) << "BestMatch Resolution for bitrate " 
                    << target_bitrate_ << " : "
                    << candidate.width_ << "x" << candidate.height_ ;
                return true;
            }
            else
                return false;
        }
    }
    LOG(INFO) << "BestMatch Resolution (end of loop): " 
        << candidate.width_ << "x" << candidate.height_ ;
    current_res_ = resolution;
    return true;
}
