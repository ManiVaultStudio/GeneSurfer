#pragma once

#include <QTableWidget>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QTableWidgetSelectionRange>

class MyTableWidget : public QTableWidget {
    Q_OBJECT

public:
    using QTableWidget::QTableWidget;

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->matches(QKeySequence::Copy)) {
            copySelectedCellsToClipboard();
        } else {
            QTableWidget::keyPressEvent(event);
        }
    }

private:
    void copySelectedCellsToClipboard();
};