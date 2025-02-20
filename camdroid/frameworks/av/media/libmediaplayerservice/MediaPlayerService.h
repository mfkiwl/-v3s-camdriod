/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ANDROID_MEDIAPLAYERSERVICE_H
#define ANDROID_MEDIAPLAYERSERVICE_H

#include <arpa/inet.h>

#include <utils/Log.h>
#include <utils/threads.h>
#include <utils/List.h>
#include <utils/Errors.h>
#include <utils/KeyedVector.h>
#include <utils/String8.h>
#include <utils/Vector.h>

#include <media/IMediaPlayerService.h>
#include <media/MediaPlayerInterface.h>
#include <media/Metadata.h>
#include <media/stagefright/foundation/ABase.h>

#include <system/audio.h>

namespace android {

class AudioTrack;
class IMediaRecorder;
class IMediaMetadataRetriever;
class IMediaServerCaller;
//class IOMX;
class IRemoteDisplay;
class IRemoteDisplayClient;
class MediaRecorderClient;
class MediaVideoResizerClient;

#define CALLBACK_ANTAGONIZER 0
#if CALLBACK_ANTAGONIZER
class Antagonizer {
public:
    Antagonizer(notify_callback_f cb, void* client);
    void start() { mActive = true; }
    void stop() { mActive = false; }
    void kill();
private:
    static const int interval;
    Antagonizer();
    static int callbackThread(void* cookie);
    Mutex               mLock;
    Condition           mCondition;
    bool                mExit;
    bool                mActive;
    void*               mClient;
    notify_callback_f   mCb;
};
#endif

class MediaPlayerService : public BnMediaPlayerService
{
    class Client;

    class AudioOutput : public MediaPlayerBase::AudioSink
    {
        class CallbackData;

     public:
                                AudioOutput(int sessionId);
        virtual                 ~AudioOutput();

        virtual bool            ready() const { return mTrack != NULL; }
        virtual bool            realtime() const { return true; }
        virtual ssize_t         bufferSize() const;
        virtual ssize_t         frameCount() const;
        virtual ssize_t         channelCount() const;
        virtual ssize_t         frameSize() const;
        virtual uint32_t        latency() const;
        virtual float           msecsPerFrame() const;
        virtual status_t        getPosition(uint32_t *position) const;
        virtual status_t        getFramesWritten(uint32_t *frameswritten) const;
        virtual int             getSessionId() const;

        virtual status_t        open(
                uint32_t sampleRate, int channelCount, audio_channel_mask_t channelMask,
                audio_format_t format, int bufferCount,
                AudioCallback cb, void *cookie,
                audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE);

        virtual void            start();
        virtual ssize_t         write(const void* buffer, size_t size);
        virtual void            stop();
        virtual void            flush();
        virtual void            pause();
        virtual void            close();
                void            setAudioStreamType(audio_stream_type_t streamType) { mStreamType = streamType; }
                void            setVolume(float left, float right);
        virtual status_t        setPlaybackRatePermille(int32_t ratePermille);
                status_t        setAuxEffectSendLevel(float level);
                status_t        attachAuxEffect(int effectId);
        virtual status_t        dump(int fd, const Vector<String16>& args) const;

        static bool             isOnEmulator();
        static int              getMinBufferCount();
                void            setNextOutput(const sp<AudioOutput>& nextOutput);
                void            switchToNextOutput();
        virtual bool            needsTrailingPadding() { return mNextOutput == NULL; }

    private:
        static void             setMinBufferCount();
        static void             CallbackWrapper(
                int event, void *me, void *info);

        AudioTrack*             mTrack;
        AudioTrack*             mRecycledTrack;
        sp<AudioOutput>         mNextOutput;
        AudioCallback           mCallback;
        void *                  mCallbackCookie;
        CallbackData *          mCallbackData;
        uint64_t                mBytesWritten;
        audio_stream_type_t     mStreamType;
        float                   mLeftVolume;
        float                   mRightVolume;
        int32_t                 mPlaybackRatePermille;
        uint32_t                mSampleRateHz; // sample rate of the content, as set in open()
        float                   mMsecsPerFrame;
        int                     mSessionId;
        float                   mSendLevel;
        int                     mAuxEffectId;
        static bool             mIsOnEmulator;
        static int              mMinBufferCount;  // 12 for emulator; otherwise 4
        audio_output_flags_t    mFlags;

        // CallbackData is what is passed to the AudioTrack as the "user" data.
        // We need to be able to target this to a different Output on the fly,
        // so we can't use the Output itself for this.
        class CallbackData {
        public:
            CallbackData(AudioOutput *cookie) {
                mData = cookie;
                mSwitching = false;
            }
            AudioOutput *   getOutput() { return mData;}
            void            setOutput(AudioOutput* newcookie) { mData = newcookie; }
            // lock/unlock are used by the callback before accessing the payload of this object
            void            lock() { mLock.lock(); }
            void            unlock() { mLock.unlock(); }
            // beginTrackSwitch/endTrackSwitch are used when this object is being handed over
            // to the next sink.
            void            beginTrackSwitch() { mLock.lock(); mSwitching = true; }
            void            endTrackSwitch() {
                if (mSwitching) {
                    mLock.unlock();
                }
                mSwitching = false;
            }
        private:
            AudioOutput *   mData;
            mutable Mutex   mLock;
            bool            mSwitching;
            DISALLOW_EVIL_CONSTRUCTORS(CallbackData);
        };

    }; // AudioOutput


    class AudioCache : public MediaPlayerBase::AudioSink
    {
    public:
                                AudioCache(const char* name);
        virtual                 ~AudioCache() {}

        virtual bool            ready() const { return (mChannelCount > 0) && (mHeap->getHeapID() > 0); }
        virtual bool            realtime() const { return false; }
        virtual ssize_t         bufferSize() const { return frameSize() * mFrameCount; }
        virtual ssize_t         frameCount() const { return mFrameCount; }
        virtual ssize_t         channelCount() const { return (ssize_t)mChannelCount; }
        virtual ssize_t         frameSize() const { return ssize_t(mChannelCount * ((mFormat == AUDIO_FORMAT_PCM_16_BIT)?sizeof(int16_t):sizeof(u_int8_t))); }
        virtual uint32_t        latency() const;
        virtual float           msecsPerFrame() const;
        virtual status_t        getPosition(uint32_t *position) const;
        virtual status_t        getFramesWritten(uint32_t *frameswritten) const;
        virtual int             getSessionId() const;

        virtual status_t        open(
                uint32_t sampleRate, int channelCount, audio_channel_mask_t channelMask,
                audio_format_t format, int bufferCount = 1,
                AudioCallback cb = NULL, void *cookie = NULL,
                audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE);

        virtual void            start();
        virtual ssize_t         write(const void* buffer, size_t size);
        virtual void            stop();
        virtual void            flush() {}
        virtual void            pause() {}
        virtual void            close() {}
                void            setAudioStreamType(audio_stream_type_t streamType) {}
                void            setVolume(float left, float right) {}
        virtual status_t        setPlaybackRatePermille(int32_t ratePermille) { return INVALID_OPERATION; }
                uint32_t        sampleRate() const { return mSampleRate; }
                audio_format_t  format() const { return mFormat; }
                size_t          size() const { return mSize; }
                status_t        wait();

                sp<IMemoryHeap> getHeap() const { return mHeap; }

        static  void            notify(void* cookie, int msg,
                                       int ext1, int ext2, const Parcel *obj);
        virtual status_t        dump(int fd, const Vector<String16>& args) const;

    private:
                                AudioCache();

        Mutex               mLock;
        Condition           mSignal;
        sp<MemoryHeapBase>  mHeap;
        float               mMsecsPerFrame;
        uint16_t            mChannelCount;
        audio_format_t      mFormat;
        ssize_t             mFrameCount;
        uint32_t            mSampleRate;
        uint32_t            mSize;
        int                 mError;
        bool                mCommandComplete;

        sp<Thread>          mCallbackThread;
    }; // AudioCache

public:
    static  void                instantiate();

    // IMediaPlayerService interface
    virtual sp<IMediaRecorder>  createMediaRecorder(pid_t pid);
    void    removeMediaRecorderClient(wp<MediaRecorderClient> client);
    virtual sp<IMediaMetadataRetriever> createMetadataRetriever(pid_t pid);
    sp<IMediaServerCaller> createMediaServerCaller(pid_t pid);

    virtual sp<IMediaPlayer>    create(pid_t pid, const sp<IMediaPlayerClient>& client, int audioSessionId);

    virtual sp<IMemory>         decode(const char* url, uint32_t *pSampleRate, int* pNumChannels, audio_format_t* pFormat);
    virtual sp<IMemory>         decode(int fd, int64_t offset, int64_t length, uint32_t *pSampleRate, int* pNumChannels, audio_format_t* pFormat);
    //virtual sp<IOMX>            getOMX();
    virtual sp<ICrypto>         makeCrypto();
    virtual sp<IHDCP>           makeHDCP();

    virtual sp<IRemoteDisplay> listenForRemoteDisplay(const sp<IRemoteDisplayClient>& client,
            const String8& iface);
    virtual status_t            dump(int fd, const Vector<String16>& args);

            void                removeClient(wp<Client> client);

    virtual sp<IMediaVideoResizer>  createMediaVideoResizer(pid_t pid);
    void    removeMediaVideoResizerClient(wp<MediaVideoResizerClient> client);

    // For battery usage tracking purpose
    struct BatteryUsageInfo {
        // how many streams are being played by one UID
        int     refCount;
        // a temp variable to store the duration(ms) of audio codecs
        // when we start a audio codec, we minus the system time from audioLastTime
        // when we pause it, we add the system time back to the audioLastTime
        // so after the pause, audioLastTime = pause time - start time
        // if multiple audio streams are played (or recorded), then audioLastTime
        // = the total playing time of all the streams
        int32_t audioLastTime;
        // when all the audio streams are being paused, we assign audioLastTime to
        // this variable, so this value could be provided to the battery app
        // in the next pullBatteryData call
        int32_t audioTotalTime;

        int32_t videoLastTime;
        int32_t videoTotalTime;
    };
    KeyedVector<int, BatteryUsageInfo>    mBatteryData;

    enum {
        SPEAKER,
        OTHER_AUDIO_DEVICE,
        SPEAKER_AND_OTHER,
        NUM_AUDIO_DEVICES
    };

    struct BatteryAudioFlingerUsageInfo {
        int refCount; // how many audio streams are being played
        int deviceOn[NUM_AUDIO_DEVICES]; // whether the device is currently used
        int32_t lastTime[NUM_AUDIO_DEVICES]; // in ms
        // totalTime[]: total time of audio output devices usage
        int32_t totalTime[NUM_AUDIO_DEVICES]; // in ms
    };

    // This varialble is used to record the usage of audio output device
    // for battery app
    BatteryAudioFlingerUsageInfo mBatteryAudio;

    // Collect info of the codec usage from media player and media recorder
    virtual void                addBatteryData(uint32_t params);
    // API for the Battery app to pull the data of codecs usage
    virtual status_t            pullBatteryData(Parcel* reply);
    /* add by Gary. start {{----------------------------------- */
//    virtual status_t            setScreen(int screen);
//    virtual status_t            getScreen(int *screen);
    virtual status_t            isPlayingVideo(int *playing);
    /* add by Gary. end   -----------------------------------}} */

    /* add by Gary. start {{----------------------------------- */
    /* 2011-11-14 */
    /* support adjusting colors while playing video */
//    virtual status_t            setVppGate(bool enableVpp);
//    virtual bool                getVppGate();
//    virtual status_t            setLumaSharp(int value);
//    virtual int                 getLumaSharp();
//    virtual status_t            setChromaSharp(int value);
//    virtual int                 getChromaSharp();
//    virtual status_t            setWhiteExtend(int value);
//    virtual int                 getWhiteExtend();
//    virtual status_t            setBlackExtend(int value);
//    virtual int                 getBlackExtend();
    /* add by Gary. end   -----------------------------------}} */

    /* add by Gary. start {{----------------------------------- */
    /* 2012-03-12 */
    /* add the global interfaces to control the subtitle gate  */
//    virtual status_t            setGlobalSubGate(bool showSub);
//    virtual bool                getGlobalSubGate();
    /* add by Gary. end   -----------------------------------}} */

    /* add by Gary. start {{----------------------------------- */
    /* 2012-4-24 */
    /* add two general interfaces for expansibility */
//    virtual status_t            generalGlobalInterface(int cmd, int int1, int int2, int int3, void *p);
    /* add by Gary. end   -----------------------------------}} */
private:

    class Client : public BnMediaPlayer {
        // IMediaPlayer interface
        virtual void            disconnect();
//        virtual status_t        setVideoSurfaceTexture(
//                                        const sp<ISurfaceTexture>& surfaceTexture);
        virtual status_t        setVideoSurfaceTexture(
                                        const unsigned int hlay);
        virtual status_t        prepareAsync();
        virtual status_t        start();
        virtual status_t        stop();
        virtual status_t        pause();
        virtual status_t        isPlaying(bool* state);
        virtual status_t        seekTo(int msec);
        virtual status_t        getCurrentPosition(int* msec);
        virtual status_t        getDuration(int* msec);
        virtual status_t        reset();
        virtual status_t        setAudioStreamType(audio_stream_type_t type);
        virtual status_t        setLooping(int loop);
        virtual status_t        setVolume(float leftVolume, float rightVolume);
        virtual status_t        invoke(const Parcel& request, Parcel *reply);
        virtual status_t        setMetadataFilter(const Parcel& filter);
        virtual status_t        getMetadata(bool update_only,
                                            bool apply_filter,
                                            Parcel *reply);
        virtual status_t        setAuxEffectSendLevel(float level);
        virtual status_t        attachAuxEffect(int effectId);
        virtual status_t        setParameter(int key, const Parcel &request);
        virtual status_t        getParameter(int key, Parcel *reply);
        virtual status_t        setRetransmitEndpoint(const struct sockaddr_in* endpoint);
        virtual status_t        getRetransmitEndpoint(struct sockaddr_in* endpoint);
        virtual status_t        setNextPlayer(const sp<IMediaPlayer>& player);
        /* add by Gary. start {{----------------------------------- */
        //virtual status_t        setScreen(int screen);
        virtual status_t        isPlayingVideo(int *playing);
        /* add by Gary. end   -----------------------------------}} */

        /* add by Gary. start {{----------------------------------- */
        /* 2011-9-15 15:39:01 */
        /* expend interfaces about subtitle, track and so on */
//        virtual int             getSubCount();
//        virtual int             getSubList(MediaPlayer_SubInfo *infoList, int count);
//        virtual int             getCurSub();
//        virtual status_t        switchSub(int index);
//        virtual status_t        setSubGate(bool showSub);
//        virtual bool            getSubGate();
//        virtual status_t        setSubColor(int color);
//        virtual int             getSubColor();
//        virtual status_t        setSubFrameColor(int color);
//        virtual int             getSubFrameColor();
//        virtual status_t        setSubFontSize(int size);
//        virtual int             getSubFontSize();
        virtual status_t        setSubCharset(const char *charset);
        virtual status_t        getSubCharset(char *charset);
//        virtual status_t        setSubPosition(int percent);
//        virtual int             getSubPosition();
        virtual status_t        setSubDelay(int time);
        virtual int             getSubDelay();
//        virtual int             getTrackCount();
//        virtual int             getTrackList(MediaPlayer_TrackInfo *infoList, int count);
//        virtual int             getCurTrack();
//        virtual status_t        switchTrack(int index);
//        virtual status_t        setInputDimensionType(int type);
//        virtual int             getInputDimensionType();
//        virtual status_t        setOutputDimensionType(int type);
//        virtual int             getOutputDimensionType();
//        virtual status_t        setAnaglaghType(int type);
//        virtual int             getAnaglaghType();
//        virtual status_t        getVideoEncode(char *encode);
//        virtual int             getVideoFrameRate();
//        virtual status_t        getAudioEncode(char *encode);
//        virtual int             getAudioBitRate();
//        virtual int             getAudioSampleRate();
        /* add by Gary. end   -----------------------------------}} */
        
        /* add by Gary. start {{----------------------------------- */
        /* 2011-11-14 */
        /* support scale mode */
        virtual status_t        enableScaleMode(bool enable, int width, int height);
        /* add by Gary. end   -----------------------------------}} */
        
        /* add by Gary. start {{----------------------------------- */
        /* 2011-11-14 */
        /* support adjusting colors while playing video */
//        virtual status_t        setVppGate(bool enableVpp);
//        virtual status_t        setLumaSharp(int value);
//        virtual status_t        setChromaSharp(int value);
//        virtual status_t        setWhiteExtend(int value);
//        virtual status_t        setBlackExtend(int value);
        /* add by Gary. end   -----------------------------------}} */

        /* add by Gary. start {{----------------------------------- */
        /* 2012-03-07 */
        /* set audio channel mute */
        virtual status_t        setChannelMuteMode(int muteMode);
        virtual int             getChannelMuteMode();
        /* add by Gary. end   -----------------------------------}} */

        /* add by Gary. start {{----------------------------------- */
        /* 2012-4-24 */
        /* add two general interfaces for expansibility */
        virtual status_t        generalInterface(int cmd, int int1, int int2, int int3, void *p);
        /* add by Gary. end   -----------------------------------}} */
        virtual status_t        setDataSource(const sp<IStreamSource> &source, int type);
        
        sp<MediaPlayerBase>     createPlayer(player_type playerType);

        virtual status_t        setDataSource(
                        const char *url,
                        const KeyedVector<String8, String8> *headers);

        virtual status_t        setDataSource(int fd, int64_t offset, int64_t length);

        virtual status_t        setDataSource(const sp<IStreamSource> &source);

        sp<MediaPlayerBase>     setDataSource_pre(player_type playerType);
        void                    setDataSource_post(const sp<MediaPlayerBase>& p,
                                                   status_t status);

        static  void            notify(void* cookie, int msg,
                                       int ext1, int ext2, const Parcel *obj);

                pid_t           pid() const { return mPid; }
        virtual status_t        dump(int fd, const Vector<String16>& args) const;

                int             getAudioSessionId() { return mAudioSessionId; }

    private:
        friend class MediaPlayerService;
                                Client( const sp<MediaPlayerService>& service,
                                        pid_t pid,
                                        int32_t connId,
                                        const sp<IMediaPlayerClient>& client,
                                        int audioSessionId,
                                        uid_t uid);
                                Client();
        virtual                 ~Client();

                void            deletePlayer();

        sp<MediaPlayerBase>     getPlayer() const { Mutex::Autolock lock(mLock); return mPlayer; }



        // @param type Of the metadata to be tested.
        // @return true if the metadata should be dropped according to
        //              the filters.
        bool shouldDropMetadata(media::Metadata::Type type) const;

        // Add a new element to the set of metadata updated. Noop if
        // the element exists already.
        // @param type Of the metadata to be recorded.
        void addNewMetadataUpdate(media::Metadata::Type type);

        // Disconnect from the currently connected ANativeWindow.
        void disconnectNativeWindow();

        mutable     Mutex                       mLock;
                    sp<MediaPlayerBase>         mPlayer;
                    sp<MediaPlayerService>      mService;
                    sp<IMediaPlayerClient>      mClient;
                    sp<AudioOutput>             mAudioOutput;
                    pid_t                       mPid;
                    status_t                    mStatus;
                    bool                        mLoop;
                    int32_t                     mConnId;
                    int                         mAudioSessionId;
                    uid_t                       mUID;
//                    sp<ANativeWindow>           mConnectedWindow;
//                    sp<IBinder>                 mConnectedWindowBinder;
                    unsigned int                mConnectedVideoLayerId;
                    struct sockaddr_in          mRetransmitEndpoint;
                    bool                        mRetransmitEndpointValid;
                    sp<Client>                  mNextClient;

                    /* add by Gary. start {{----------------------------------- */
                    int                         mHasSurface;
                    int 						mMsg; //add by lszhang for play during<1 song
                    /* add by Gary. end   -----------------------------------}} */

                    /* add by Gary. start {{----------------------------------- */
                    /* 2011-9-28 16:28:24 */
                    /* save properties before creating the real player */
//                    bool                        mSubGate;
//                    int                         mSubColor;
//                    int                         mSubFrameColor;
//                    int                         mSubPosition;
                    int                         mSubDelay;
//                    int                         mSubFontSize;
                    char                        mSubCharset[MEDIAPLAYER_NAME_LEN_MAX];
//					int                         mSubIndex;
//				    int                         mTrackIndex;
                    int                         mMuteMode;   // 2012-03-07, set audio channel mute
                    /* add by Gary. end   -----------------------------------}} */

                    /* add by Gary. start {{----------------------------------- */
                    /* 2011-11-14 */
                    /* support scale mode */
                    bool                        mEnableScaleMode;
                    int                         mScaleWidth;
                    int                         mScaleHeight;
                    /* add by Gary. end   -----------------------------------}} */

                    /* add by Gary. start {{----------------------------------- */
                    /* 2011-11-30 */
                    /* fix the bug about setting global attibute */
//                    int                         mScreen;
//                    bool                        mVppGate;
//                    int                         mLumaSharp;
//                    int                         mChromaSharp;
//                    int                         mWhiteExtend;
//                    int                         mBlackExtend;
                    /* add by Gary. end   -----------------------------------}} */

        // Metadata filters.
        media::Metadata::Filter mMetadataAllow;  // protected by mLock
        media::Metadata::Filter mMetadataDrop;  // protected by mLock

        // Metadata updated. For each MEDIA_INFO_METADATA_UPDATE
        // notification we try to update mMetadataUpdated which is a
        // set: no duplicate.
        // getMetadata clears this set.
        media::Metadata::Filter mMetadataUpdated;  // protected by mLock

#if CALLBACK_ANTAGONIZER
                    Antagonizer*                mAntagonizer;
#endif
    }; // Client

// ----------------------------------------------------------------------------

                            MediaPlayerService();
    virtual                 ~MediaPlayerService();

    mutable     Mutex                       mLock;
                SortedVector< wp<Client> >  mClients;
                SortedVector< wp<MediaRecorderClient> > mMediaRecorderClients;
                SortedVector< wp<MediaVideoResizerClient> > mMediaVideoResizerClients;
                int32_t                     mNextConnId;
                //sp<IOMX>                    mOMX;
                sp<ICrypto>                 mCrypto;
                /* add by Gary. start {{----------------------------------- */
//                int                         mScreen;
                /* add by Gary. end   -----------------------------------}} */

                /* add by Gary. start {{----------------------------------- */
                /* 2011-11-14 */
                /* support adjusting colors while playing video */
//                bool                        mVppGate;
//                int                         mLumaSharp;
//                int                         mChromaSharp;
//                int                         mWhiteExtend;
//                int                         mBlackExtend;
//                bool                        mGlobalSubGate;  // 2012-03-12, add the global interfaces to control the subtitle gate
                /* add by Gary. end   -----------------------------------}} */
                
                /*Start by Bevis. Detect http data source from other application.*/
                wp<Client> mDetectClient;
                /*Start by Bevis. Detect http data source from other application.*/

                /*Add by eric_wang. record hdmi state, 20130318 */
                //static int                  mHdmiPlugged;   //1:hdmi plugin, 0:hdmi plugout
                /*Add by eric_wang. record hdmi state, 20130318, end ---------- */
};

// ----------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_MEDIAPLAYERSERVICE_H
