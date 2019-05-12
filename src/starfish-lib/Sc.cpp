/* The MIT License (MIT)
 *
 * Copyright (c) 2018, 2019 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Sc.h"

#include <QNetworkReply>
#include <QDebug>


#include <brotli/decode.h>
#include <zlib.h>
#include <string.h>

QNetworkRequest
Sc::makeRequest(const QUrl& url)
{
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:60.0) Gecko/20100101 Firefox/60.0 " STARFISH_APP_NAME "/" QT_STRINGIFY(STARFISH_VERSION_MAJOR) "." QT_STRINGIFY(STARFISH_VERSION_MINOR) "." QT_STRINGIFY(STARFISH_VERSION_PATCH));
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.5");
    request.setRawHeader("Accept-Encoding", "gzip, deflate, br");
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Upgrade-Insecure-Requests", "1");

    return request;
}


QByteArray
Sc::gzipDecompress(const QByteArray& data, bool* ok)
{
    //https://stackoverflow.com/questions/2690328/qt-quncompress-gzip-data
    if (ok) *ok = false;

//    if (data.size() <= 4) {
//        qWarning("gUncompress: Input data is truncated");
//        return QByteArray();
//    }

    QByteArray result;
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    char decompressed[1024];


    /* allocate inflate state */
    strm.avail_in = (uInt)data.size();
    strm.next_in = (Bytef*)data.data();

    int ret = inflateInit2(&strm, 15 +  32); // gzip decoding
    if (ret != Z_OK) {
        return QByteArray();
    }

    // run inflate()
    do {
        strm.avail_out = (uInt)sizeof(decompressed);
        strm.next_out = (Bytef*)(decompressed);

        ret = inflate(&strm, Z_NO_FLUSH);
        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;     // and fall through
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            (void)inflateEnd(&strm);
            return QByteArray();
        }

        result.append(decompressed, (int)(sizeof(decompressed) - strm.avail_out));
    } while (strm.avail_out == 0);

    // clean up and return
    inflateEnd(&strm);

    if (ok) *ok = true;
    return result;
}

QByteArray
Sc::decodeContent(QNetworkReply* reply, bool* ok)
{
    Q_ASSERT(reply);

    if (ok) *ok = false;

    auto response = reply->readAll();
    auto encoding = reply->rawHeader("Content-Encoding");
    if (encoding.isEmpty()) {
        if (ok) *ok = true;
        return response;
    }

    if (encoding == "br") {
        auto state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
        if (state) {
            BrotliDecoderSetParameter(state, BROTLI_DECODER_PARAM_LARGE_WINDOW, 1u);
            QByteArray result;
            result.resize(qMax<size_t>(4096, response.size()*16));
            size_t availableOut = result.size();
            size_t availableIn = response.size();
            const quint8* inPtr = reinterpret_cast<const quint8*>(response.data());
            quint8* outPtr = reinterpret_cast<quint8*>(result.data());
            for (bool done = false; !done; ) {
                auto error = BrotliDecoderDecompressStream(state, &availableIn, &inPtr, &availableOut, &outPtr, nullptr);
                switch (error) {
                case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT: {
                    size_t offset = result.size() - availableOut;
                    availableOut += result.size();
                    result.resize(result.size() * 2);
                    outPtr = reinterpret_cast<quint8*>(result.data()) + offset;
                } break;
                case BROTLI_DECODER_RESULT_SUCCESS:
                    if (ok) *ok = true;
                    done = true;
                    result.resize(result.size() - availableOut);
                    break;
                default:
                    result.clear();
                    done = true;
                    break;
                }
            }


            BrotliDecoderDestroyInstance(state);

            return result;
        }


    } else if (encoding == "gzip") {
        return gzipDecompress(response, ok);
    } else if (encoding == "deflate") {
        qWarning() << "deflate not implemented";
    }

    return QByteArray();
}
