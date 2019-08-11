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

#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QNetworkConfigurationManager>

Q_DECLARE_METATYPE(QSqlDatabase)

class ScApp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(bool isOnBroadband READ isOnBroadband NOTIFY isOnBroadbandChanged)
    Q_PROPERTY(bool isOnMobile READ isOnMobile NOTIFY isOnMobileChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY isOnlineChanged)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
    Q_PROPERTY(QString appDir READ appDir CONSTANT)
    Q_PROPERTY(QString logDir READ logDir CONSTANT)
public:
    ScApp(QObject* parent = nullptr);

public:
    QString version() const;
    QString displayName() const;
    QString dataDir() const;
    QString appDir() const;
    QString logDir() const;
    bool isOnBroadband() const;
    bool isOnMobile() const;
    bool isOnline() const;
    Q_INVOKABLE bool isUrl(const QString& str) const;
    Q_INVOKABLE bool unlink(const QString& filePath) const;
    Q_INVOKABLE bool copy(const QString& srcFilePath, const QString& dstFilePath) const;
    Q_INVOKABLE bool move(const QString& srcFilePath, const QString& dstFilePath) const;
    Q_INVOKABLE bool isDir(const QString& str) const;
    Q_INVOKABLE bool isVodKey(const QVariant& var) const;
    Q_INVOKABLE bool isUrlKey(const QVariant& var) const;
    Q_INVOKABLE QString filename(const QString& value) const;
    Q_INVOKABLE QString makeTemporaryFile(const QString& pathTemplate) const;
    static QString staticLogDir();
    static bool IsVodKey(const QVariant& var);
    static bool IsUrlKey(const QVariant& var);
    static QString MakeTemporaryFile(const QString& pathTemplate);


signals:
    void isOnBroadbandChanged();
    void isOnMobileChanged();
    void isOnlineChanged();

private slots:
    void onOnlineStateChanged(bool online);

private:
    QNetworkConfigurationManager m_NetworkConfigurationManager;
};
