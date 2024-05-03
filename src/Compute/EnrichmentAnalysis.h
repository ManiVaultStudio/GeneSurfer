#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>

class EnrichmentAnalysis : public QObject
{
    Q_OBJECT

public:
    explicit EnrichmentAnalysis(QObject* parent = nullptr);

    // ToppGene
    void enrichmentToppGene(const QStringList& symbols, const QStringList& backgroundSymbols);
    void lookupSymbolsToppGene(const QStringList& symbols);
    void lookupSymbolsToppGeneBackground(const QStringList& backgroundSymbols);
    void postGeneToppGene(const QJsonArray& entrezIds, const QJsonArray& backgroundEntrezIds);

    // gProfiler
    void postGeneGprofiler(const QStringList& query, const QStringList& background); // const QStringList& query, const QStringList& background – gprofiler

signals:
    void enrichmentDataReady(const QVariantList& outputList);
    void enrichmentDataNotExists();

private slots:
    // ToppGene
    void handleLookupReplyToppGene();
    void handleLookupReplyToppGeneBackground();
    void handleEnrichmentReplyToppGene();

    // gProfiler
    void handleEnrichmentReplyGprofiler();

private:
    QNetworkAccessManager* networkManager;  

    // for ToppGene
    bool        _backgroundInitialized = false;
    QJsonArray  _backgroundEntrezIds;
    QStringList _currentSymbols;
    QStringList _currentBackgroundSymbols;
};