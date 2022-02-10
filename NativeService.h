#ifndef ANDROID_ZPSERVICE_H
#define ANDROID_ZPSERVICE_H

#include <utils/RefBase.h>
#ifndef CHANNEL_SOCKET
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#endif

#define BUF_SIZE (4096)
#define NO_ERROR 0

namespace android
{
#ifdef CHANNEL_SOCKET
    #pragma pack(push)
    #pragma pack(1)
    typedef struct _tagADAPT_PARCEL
    {
        unsigned int code;
        unsigned int dataSize;
        unsigned int replySize;
        char *data;
    }ADAPT_PARCEL, *LPADAPT_PARCEL_T;
    #pragma pack(pop)

    class Parcel {
        private:
            int mDataSize;
            int mPos;
            jbyte mData[BUF_SIZE];

        public:
            Parcel();
            void setData(char *b, int len);
            void write(char *b, int len);
            void read(char *b, int len);
            int dataSize();
            void writeInt32(int i);
            int readInt32();
            void setDataPosition(int i);
            void recycle();
            jbyte readByte();
            void writeByte(jbyte value);
    };

    class NativeService
    {
    private:
        //mutable Mutex m_Lock;
        //int32_t m_NextConnId;

    public:
        static int Instance();
        NativeService();
        virtual ~NativeService();
        virtual int onTransact(uint32_t, Parcel&, Parcel*, uint32_t);
    };
#else
    class NativeService : public BBinder
    {
    private:
        //mutable Mutex m_Lock;
        //int32_t m_NextConnId;

    public:
        static int Instance();
        NativeService();
        virtual ~NativeService();
        virtual status_t onTransact(uint32_t, const Parcel&, Parcel*, uint32_t);
    };
#endif
}
#endif
