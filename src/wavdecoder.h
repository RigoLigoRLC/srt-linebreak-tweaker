#ifndef WAVDECODER_H
#define WAVDECODER_H

/* Ported from Wav.cpp from QTau http://github.com/qtau-devgroup/editor by digited, BSD license */

#include <QIODevice>
#include <QBuffer>
#include <rint.h>

struct WavFormat
{
    i32 channelCount,
        sampleRate,
        bytesPerFrame;
    u64 sampleSize;
    enum _TByteOrder{
        LittleEndian, BigEndian
    } byteOrder;
    enum _TSampleType{
        Int8, Int16, Float32
    } sampleType;
};

union uichar {
    char    c[4];
    quint32 i;
};

class WavDecoder : public QBuffer
{
    Q_OBJECT
    friend class qtauWavCodecFactory;

public:

    WavDecoder(QObject *parent);

    // should read all contents of file/socket and decode it to PCM in buf
    virtual bool cacheAll();

protected:
    WavFormat fmt; // format of that raw PCM data

protected:
    QIODevice *dev;

    bool findFormatChunk(QDataStream &reader);
    bool findDataChunk(QDataStream &reader);

    quint64 _data_chunk_location;  // bytes
    int     _data_chunk_length;    // in frames

};

#endif // WAVDECODER_H
