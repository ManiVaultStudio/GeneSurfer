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

    // Specify the feature, correction, p-value cutoff, and the number of top terms to return
    QJsonArray categories;
    QJsonObject molecularFunction;
    molecularFunction["Type"] = "GeneOntologyMolecularFunction";
    molecularFunction["PValue"] = 0.05;
    //molecularFunction["MaxResults"] = 10;
    molecularFunction["Correction"] = "Bonferroni";

    QJsonObject biologicalProcess;
    biologicalProcess["Type"] = "GeneOntologyBiologicalProcess";
    biologicalProcess["PValue"] = 0.05;
    //biologicalProcess["MaxResults"] = 10;
    biologicalProcess["Correction"] = "Bonferroni";

    QJsonObject cellularComponent;
    cellularComponent["Type"] = "GeneOntologyCellularComponent";
    cellularComponent["PValue"] = 0.05;
    //cellularComponent["MaxResults"] = 10;
    cellularComponent["Correction"] = "Bonferroni";

    categories.append(molecularFunction);
    categories.append(biologicalProcess);
    categories.append(cellularComponent);

    json["Categories"] = categories;

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

        //qDebug() << "annotationsArray size" << annotationsArray.size();

        // previous code for filtering the reply + //if (GOCategories.contains(category)) {} in the loop
        /*const QStringList GOCategories = {"GeneOntologyMolecularFunction",
                                          "GeneOntologyBiologicalProcess",
                                          "GeneOntologyCellularComponent"};*/

        // map full category names to abbreviations
        QMap<QString, QString> categoryMap = {
            {"GeneOntologyMolecularFunction", "GO:MF"},
            {"GeneOntologyBiologicalProcess", "GO:BP"},
            {"GeneOntologyCellularComponent", "GO:CC"}
        };


        // output the first maxCount terms
        int count = 0;
        //const int maxCount = 80;
        QVariantList outputList;
  
        for (int i = 0; i < annotationsArray.size(); ++i) {
            const QJsonValue& value = annotationsArray[i];
            QJsonObject annotationObject = value.toObject();
            QString category = annotationObject["Category"].toString();

            if (categoryMap.contains(category)) {
                category = categoryMap[category];
            }

            /*if (count >= maxCount) {
                qDebug() << "More terms from ToppGene are not showing...";
                break;
            }*/

            QString name = annotationObject["Name"].toString();
            //double pValue = annotationObject["pValue"].toDouble();
            //double QValueFDRBH = annotationObject["QValueFDRBH"].toDouble();
            double QValueBonferroni = annotationObject["QValueBonferroni"].toDouble();

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
            //dataMap["QValueFDRBH"] = QValueFDRBH;
            dataMap["QValueBonferroni"] = QValueBonferroni;
            dataMap["Symbol"] = geneSymbols.join(",");
            outputList.append(dataMap);

            count++;
        }

        // prepare to send the data to plugin
        if (count != 0) {
            // sort the outputList by QValueBonferroni
            std::sort(outputList.begin(), outputList.end(), [](const QVariant& a, const QVariant& b) {
                double qValueA = a.toMap()["QValueBonferroni"].toDouble();
                double qValueB = b.toMap()["QValueBonferroni"].toDouble();
                return qValueA < qValueB;
                });

            emit enrichmentDataReady(outputList);
        } else  {
            qDebug() << "ToppGene reply warning: no GO terms found";
            emit enrichmentDataNotExists();
        }
   
        //qDebug() << "EnrichmentAnalysis::handleEnrichmentReplyToppGene end...";

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

void EnrichmentAnalysis::postGeneGprofiler(const QStringList& query, const QStringList& background, const QString& species) { 
    //qDebug() << "gprofiler begin...";

    QUrl url("https://biit.cs.ut.ee/gprofiler/api/gost/profile/");

    QJsonObject json;
    json["organism"] = species; // TO DO: hard-coded for mouse dataset
    //json["organism"] = "hsapiens"; // TO DO: hard-coded for human dataset
    json["sources"] = QJsonArray({ "GO" }); // Gene Ontology categories
    json["query"] = QJsonArray::fromStringList(query);
    json["significance_threshold_method"] = "bonferroni";
   
    if (!background.isEmpty()) {
        json["domain_scope"] = "custom";
        json["background"] = QJsonArray::fromStringList(background);
    }   // else background is empty 

    QJsonDocument document(json);
    QByteArray jsonData = document.toJson();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = networkManager->post(request, jsonData);
    connect(reply, &QNetworkReply::finished, this, &EnrichmentAnalysis::handleEnrichmentReplyGprofiler);
}

void EnrichmentAnalysis::handleEnrichmentReplyGprofiler() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    //qDebug() << "start to handle gprofiler reply...";

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
                    QString source = resultObj["source"].toString();

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
                    dataMap["Category"] = source;
                    dataMap["Name"] = name;
                    dataMap["Padj_bonferroni"] = pValue;
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

