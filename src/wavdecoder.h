#ifndef WAVDECODER_H
#define WAVDECODER_H

/* Ported from Wav.cpp from QTau http://github.com/qtau-devgroup/editor by digited, BSD license */

#include <QIODevice>
#include <QBuffer>
#include <QPair>
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

class WavDecoder : public QObject
{
    Q_OBJECT

public:

    WavDecoder(QObject *parent);

    // should read all contents of file/socket and decode it to PCM in buf
    virtual bool cacheAll(QIODevice *);

    QPair<f32, f32> GetWaveformPeaksForRange(i32 begin, i32 end);

    i32 SampleRate() { return fmt.sampleRate; }
    i32 GetLengthMs() { return mData.size() / (fmt.sampleSize / 8) * 1000 / fmt.sampleRate; }

    void clear() { mData.clear(); }
    i32 size() { return mData.size(); }


protected:
    WavFormat fmt; // format of that raw PCM data

    bool findFormatChunk(QDataStream &reader);
    bool findDataChunk(QDataStream &reader);

    QByteArray mData;

protected:
    QIODevice *dev;
    quint64 _data_chunk_location;  // bytes
    int     _data_chunk_length;    // in frames

};

#endif // WAVDECODER_H
