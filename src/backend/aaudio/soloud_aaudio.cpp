/*
SoLoud audio engine
Copyright (c) 2024 Larry Zeng

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/
#include <stdlib.h>
#include <math.h>
#include <memory.h>

#include "soloud.h"
#include "soloud_thread.h"


#if defined( __ANDROID__ )

#include <aaudio/AAudio.h>
#include <android/log.h>

// Error logging.
#define LOG_ERROR(_msg) \
   __android_log_print( ANDROID_LOG_ERROR, "SoLoud_AAudio", _msg )
#define LOG_INFO(_msg) \
   __android_log_print( ANDROID_LOG_INFO, "SoLoud_AAudio", _msg )

#else

#error "AAudio is only supported on Android."

#endif



#if !defined(WITH_AAUDIO)
namespace SoLoud {
    result aaudio_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate,
                       unsigned int aBuffer) {
        LOG_ERROR("AAudio is not enabled.")
        return NOT_IMPLEMENTED;
    }
}
#else

#define NUM_BUFFERS 2

namespace SoLoud {
    struct AAudioData {
        AAudioStream *stream;
        unsigned int bufferSize;
        unsigned int channels;
        short *outputBuffers[NUM_BUFFERS];

        AAudioData() : stream(nullptr), bufferSize(0), channels(0) {
            memset(outputBuffers, 0, sizeof(outputBuffers));
        }

        ~AAudioData() {
            if (stream) {
                AAudioStream_close(stream);
            }
            for (int i = 0; i < 2; ++i) {
                delete[] outputBuffers[i];
            }
        }
    };


    void soloud_aaudio_deinit(SoLoud::Soloud *aSoloud) {
        delete static_cast<AAudioData *>(aSoloud->mBackendData);
        aSoloud->mBackendData = nullptr;
    }

    static aaudio_data_callback_result_t dataCallback(
            AAudioStream *stream,
            void *userData,
            void *audioData,
            int32_t numFrames) {
        Soloud *soloud = static_cast<Soloud *>(userData);
        soloud->mixSigned16(static_cast<short *>(audioData), numFrames);
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    result aaudio_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate,
                       unsigned int aBuffer, unsigned int aChannels) {
        AAudioData *data = new AAudioData();
        data->bufferSize = aBuffer;
        data->channels = aChannels;

        LOG_INFO("Initialized AAudio Data");

        AAudioStreamBuilder *builder;
        AAudio_createStreamBuilder(&builder);
        AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setChannelCount(builder, aChannels);
        AAudioStreamBuilder_setSampleRate(builder, aSamplerate);
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setDataCallback(builder, dataCallback, aSoloud);

        LOG_INFO("Initialized AAudio Stream Builder");

        aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &data->stream);
        if (result != AAUDIO_OK) {
            LOG_ERROR("Failed to open stream");
            AAudioStreamBuilder_delete(builder);
            delete data;
            return UNKNOWN_ERROR;
        }
        LOG_INFO("Opened AAudio Stream");

        AAudioStreamBuilder_delete(builder);

        LOG_INFO("Deleted AAudio Stream Builder");

        aSoloud->mBackendData = data;
        aSoloud->mBackendCleanupFunc = soloud_aaudio_deinit;
        aSoloud->mBackendString = "AAudio";

        LOG_INFO("Set backend data and cleanup function");

        aSoloud->postinit_internal(aSamplerate, data->bufferSize, aFlags, aChannels);

        LOG_INFO("Postinit internal");

        AAudioStream_requestStart(data->stream);

        LOG_INFO("AAudio initialized");

        return SO_NO_ERROR;
    }
};
#endif
