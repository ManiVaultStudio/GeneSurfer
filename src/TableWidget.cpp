#include "TableWidget.h"


void MyTableWidget::copySelectedCellsToClipboard() {
    QString selectedText;
    QList<QTableWidgetSelectionRange> ranges = selectedRanges();

    for (const QTableWidgetSelectionRange& range : ranges) {
        for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
            QStringList rowContents;
            for (int col = range.leftColumn(); col <= range.rightColumn(); ++col) {
                // Check if the current cell is selected
                if (selectionModel()->isSelected(indexFromItem(item(row, col)))) {
                    QTableWidgetItem* currentItem = item(row, col);
                    rowContents.append(currentItem ? currentItem->text() : "");
                }
                else {
                    rowContents.append("");
                }
            }
            selectedText.append(rowContents.join("\t")).append("\n");
        }
    }

    QApplication::clipboard()->setText(selectedText.trimmed());
}