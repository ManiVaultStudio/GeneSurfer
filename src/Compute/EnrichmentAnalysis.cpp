#include "EnrichmentAnalysis.h"


EnrichmentAnalysis::EnrichmentAnalysis(QObject* parent)
    : QObject(parent), networkManager(new QNetworkAccessManager(this))
{
}

void EnrichmentAnalysis::lookupSymbolsToppGene(const QStringList& symbols) {

    QUrl url("https://toppgene.cchmc.org/API/lookup");
    QJsonObject json;
    QJsonArray jsonSymbols;

    for (const QString& symbol : symbols) {
        jsonSymbols.append(symbol);
    }

    json["Symbols"] = jsonSymbols;
    QJsonDocument document(json);
    QByteArray jsonData = document.toJson();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = networkManager->post(request, jsonData);
    connect(reply, &QNetworkReply::finished, this, &EnrichmentAnalysis::handleLookupReplyToppGene);

}

void EnrichmentAnalysis::handleLookupReplyToppGene() {

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    if (reply && reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();

        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();
        QJsonArray genesArray = jsonObject["Genes"].toArray();

        QJsonArray entrezIds;
        for (const QJsonValue& value : genesArray) {
            QJsonObject geneObject = value.toObject();
            entrezIds.append(geneObject["Entrez"].toInt());
        }

        postGeneToppGene(entrezIds);
    }
    else {
        qDebug() << "ToppGene lookup reply error:" << reply->errorString();
    }
    reply->deleteLater();
}

void EnrichmentAnalysis::postGeneToppGene(const QJsonArray& entrezIds)
{
    QUrl url("https://toppgene.cchmc.org/API/enrich");
    QJsonObject json;
    json["Genes"] = entrezIds;
    QJsonDocument document(json);
    QByteArray jsonData = document.toJson();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = networkManager->post(request, jsonData);
    connect(reply, &QNetworkReply::finished, this, &EnrichmentAnalysis::handleEnrichmentReplyToppGene);
}

void EnrichmentAnalysis::handleEnrichmentReplyToppGene() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    if (reply && reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
       
        // output the entire response
        /*QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        qDebug() << jsonDoc.toJson(QJsonDocument::Indented);*/
        
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();

        QJsonArray annotationsArray = jsonObject["Annotations"].toArray();

        qDebug() << "annotationsArray size" << annotationsArray.size();

        // output the first maxCount terms
        int count = 0;
        const int maxCount = 80;
        QVariantList outputList;
  
        for (int i = 0; i < annotationsArray.size(); ++i) {
            const QJsonValue& value = annotationsArray[i];

            QJsonObject annotationObject = value.toObject();

            QString category = annotationObject["Category"].toString();

            if (category == "GeneOntologyBiologicalProcess" || category == "GeneOntologyMolecularFunction" || category == "GeneOntologyCellularComponent") {
                if (count >= maxCount) {
                    qDebug() << "More terms from ToppGene are not showing...";
                    break;
                }
                QString name = annotationObject["Name"].toString();
                //double pValue = annotationObject["pValue"].toDouble();
                double QValueFDRBH = annotationObject["QValueFDRBH"].toDouble();
                //double QValueBonferroni = annotationObject["QValueBonferroni"].toDouble();

                /*qDebug() << QString("Category: %1, Name: %2, Bonferroni: %3")
                    .arg(category)
                    .arg(name)
                    .arg(QValueBonferroni, 0, 'g', 4);*/

                // extract gene symbols in the GO term
                QStringList geneSymbols;
                QJsonArray genesArray = annotationObject["Genes"].toArray();

                for (int i = 0; i < genesArray.size(); ++i) {
                    QJsonObject geneObject = genesArray[i].toObject();
                    geneSymbols.append(geneObject["Symbol"].toString());
                }

                QVariantMap dataMap;
                dataMap["Category"] = category;
                dataMap["Name"] = name;
                //dataMap["QValueBonferroni"] = QValueBonferroni;
                dataMap["QValueFDRBH"] = QValueFDRBH;
                dataMap["Symbol"] = geneSymbols.join(",");
                outputList.append(dataMap);

                count++;
            }            
        }
        // prepare to send the data to plugin
        if (count != 0) {
            // Go terms exist
            if (count < maxCount) {
                qDebug() << "ToppGene reply warning: Go terms less than " << maxCount << " terms found. Also show terms in other categories";
                for (const QJsonValue& value : annotationsArray) {
                    if (count >= maxCount) break; // Stop if max count reached

                    QJsonObject annotationObject = value.toObject();
                    QString category = annotationObject["Category"].toString();

                    // Skip if already a priority category
                    if (category == "GeneOntologyBiologicalProcess" || category == "GeneOntologyMolecularFunction" || category == "GeneOntologyCellularComponent") continue;

                    QString name = annotationObject["Name"].toString();
                    double QValueFDRBH = annotationObject["QValueFDRBH"].toDouble();
                    QStringList geneSymbols;
                    QJsonArray genesArray = annotationObject["Genes"].toArray();
                    for (int i = 0; i < genesArray.size(); ++i) {
                        QJsonObject geneObject = genesArray[i].toObject();
                        geneSymbols.append(geneObject["Symbol"].toString());
                    }

                    QVariantMap dataMap;
                    dataMap["Category"] = category;
                    dataMap["Name"] = name;
                    dataMap["QValueFDRBH"] = QValueFDRBH;
                    dataMap["Symbol"] = geneSymbols.join(",");
                    outputList.append(dataMap);

                    count++;
                }
            }
            emit enrichmentDataReady(outputList);
        }
        else if (annotationsArray.size() != 0)  {
            qDebug() << "ToppGene reply warning: no GeneOntology result found, show terms in other categories";               
            // output max count items in other categories
            for (int i = 0; i < annotationsArray.size(); ++i) {
                const QJsonValue& value = annotationsArray[i];
                QJsonObject annotationObject = value.toObject();
                QString category = annotationObject["Category"].toString();

                if (i >= maxCount) {
                    qDebug() << "More terms from ToppGene are not showing...";
                    break;
                }
                QString name = annotationObject["Name"].toString();
                double QValueBonferroni = annotationObject["QValueBonferroni"].toDouble();

                QStringList geneSymbols;
                QJsonArray genesArray = annotationObject["Genes"].toArray();

                for (int i = 0; i < genesArray.size(); ++i) {
                    QJsonObject geneObject = genesArray[i].toObject();
                    geneSymbols.append(geneObject["Symbol"].toString());
                }

                QVariantMap dataMap;
                dataMap["Category"] = category;
                dataMap["Name"] = name;
                dataMap["pValueBonferroni"] = QValueBonferroni;
                dataMap["Symbol"] = geneSymbols.join(",");
                outputList.append(dataMap);
            }
            emit enrichmentDataReady(outputList);

        } else if (annotationsArray.size() == 0) {
            qDebug() << "ToppGene reply warning: no result found";
            emit enrichmentDataNotExists();
        }
   
        qDebug() << "EnrichmentAnalysis::handleEnrichmentReplyToppGene end...";

        //output all items in certain categories
        /*for (const QJsonValue& value : annotationsArray) {
            QJsonObject annotationObject = value.toObject();
            QString category = annotationObject["Category"].toString();
            QString name = annotationObject["Name"].toString();
            double pValue = annotationObject["PValue"].toDouble();

            if (category == "GeneOntologyMolecularFunction" || category == "MousePheno") {
                qDebug() << QString("Category: %1, Name: %2, PValue: %3")
                    .arg(category)
                    .arg(name)
                    .arg(pValue, 0, 'g', 4);
            }           
        }*/
       
    }
    else {
        qDebug() << "ToppGene reply error:" << reply->errorString();
    }
    reply->deleteLater();
}

void EnrichmentAnalysis::postGeneGprofiler(const QStringList& query, const QStringList& background) { //const QStringList& query, const QStringList& background
    qDebug() << "gprofiler begin...";

    QUrl url("https://biit.cs.ut.ee/gprofiler/api/gost/profile/");

    QJsonObject json;
    json["organism"] = "mmusculus"; // TO DO: hard-coded for mouse dataset
    json["query"] = QJsonArray::fromStringList(query);
   
    if (!background.isEmpty()) {
        json["domain_scope"] = "custom";
        json["background"] = QJsonArray::fromStringList(background);
    }   // else background is empty 

    json["significance_threshold_method"] = "fdr";

    QJsonDocument document(json);
    QByteArray jsonData = document.toJson();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = networkManager->post(request, jsonData);
    connect(reply, &QNetworkReply::finished, this, &EnrichmentAnalysis::handleEnrichmentReplyGprofiler);
}

void EnrichmentAnalysis::handleEnrichmentReplyGprofiler() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    qDebug() << "start to handle gprofiler reply...";

    if (reply && reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        // qDebug() << jsonDoc.toJson(QJsonDocument::Indented); // output the entire response

        QJsonObject jsonObject = jsonDoc.object();

        QJsonArray queryArray = jsonObject["meta"].toObject()["query_metadata"].toObject()["queries"].toObject()["query_1"].toArray();
        QStringList queries;
        for (const QJsonValue& value : queryArray) {
            queries.append(value.toString());
        }

        QVariantList outputList;// for sending to plugin

        if (jsonObject.contains("result") && jsonObject["result"].isArray()) {
            QJsonArray resultsArray = jsonObject["result"].toArray();

            bool validResultsFound = false;

            for (const QJsonValue& value : resultsArray) {
                QJsonObject resultObj = value.toObject();

                if (resultObj.contains("name") && resultObj.contains("p_value")) {
                    QString name = resultObj["name"].toString();
                    double pValue = resultObj["p_value"].toDouble();

                    //qDebug() << "g:Profiler Name:" << name << ", P-Value:" << pValue;

                    validResultsFound = true;

                    // Process intersections
                    QJsonArray intersections = resultObj["intersections"].toArray();
                    QStringList geneSymbols;
                    for (int i = 0; i < intersections.size(); ++i) {
                        if (!intersections[i].toArray().isEmpty()) {
                            geneSymbols.append(queries[i]);
                        }
                    }
                    //qDebug() << "Gene Symbols:" << geneSymbols.join(", ");

                    QVariantMap dataMap;
                    dataMap["Name"] = name;
                    dataMap["PValueFDR"] = pValue;
                    dataMap["Symbol"] = geneSymbols.join(",");
                    outputList.append(dataMap);
                }
            }

            if (validResultsFound) {
                emit enrichmentDataReady(outputList);           
            }
            else {
                qDebug() << "Gprofiler reply warning: no result found";
                emit enrichmentDataNotExists();
            }
        }
        else {
            // The 'result' key does not exist or is not an array
            qDebug() << "Gprofiler reply warning: no result key found";
        }
    }
    else {
        qDebug() << "Gprofiler reply error:" << reply->errorString();
    }
    reply->deleteLater();
}

